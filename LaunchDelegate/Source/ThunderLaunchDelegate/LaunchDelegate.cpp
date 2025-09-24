#include "LaunchDelegate.h"
#include "LocalAppManagerProvider.h"
#include "DeepLinkProvider.h"

namespace Plugin {

    LaunchDelegate::LaunchDelegate()
        : PluginHost::JSONRPC(std::vector<uint8_t>{1})
        , _service(nullptr)
        , _providers(Core::make_unique<ProviderRegistry>())
        , _sessions(Core::make_unique<LaunchSessionTracker>())
        , _config()
    {
        RegisterAll();
    }

    LaunchDelegate::~LaunchDelegate() {
        UnregisterAll();
    }

    void LaunchDelegate::RegisterAll() {
        Register<LaunchParams, LaunchResult>(_T("launch"), &LaunchDelegate::Launch, this);
        Register<ResolveParams, ResolveResult>(_T("resolve"), &LaunchDelegate::ResolveTarget, this);
        Register<CancelParams, Core::JSON::VariantContainer>(_T("cancel"), &LaunchDelegate::Cancel, this);
        Register<GetStatusParams, GetStatusResult>(_T("getStatus"), &LaunchDelegate::GetStatus, this);
        Register<Core::JSON::VariantContainer, GetCapabilitiesResult>(_T("getCapabilities"), &LaunchDelegate::GetCapabilities, this);
        Register<Core::JSON::VariantContainer, GetProvidersResult>(_T("getProviders"), &LaunchDelegate::GetProviders, this);
        Register<ActivateParams, Core::JSON::VariantContainer>(_T("activate"), &LaunchDelegate::Activate, this);
        // Events:
        //  - "launchstatus"
        //  - "foregroundchanged"
        //  - "providerstatus" (notified on Register/Unregister in a real implementation)
    }

    void LaunchDelegate::UnregisterAll() {
        Unregister(_T("launch"));
        Unregister(_T("resolve"));
        Unregister(_T("cancel"));
        Unregister(_T("getStatus"));
        Unregister(_T("getCapabilities"));
        Unregister(_T("getProviders"));
        Unregister(_T("activate"));
    }

    const string LaunchDelegate::Initialize(PluginHost::IShell* service) {
        ASSERT(service != nullptr);
        _service = service;
        _service->AddRef();

        Activate(_service);

        // Parse configuration
        _config.FromString(_service->ConfigLine());

        // Register default providers
        _providers->Register("local-appmgr", Core::make_unique<LocalAppManagerProvider>());
        _providers->Register("deeplink", Core::make_unique<DeepLinkProvider>());

        // Wire callbacks
        _providers->SetStatusCallback([this](const LaunchEvent& ev) {
            // Update session and emit event
            _sessions->Update(ev.launchId, ev.state, ev.message, ev.timestampMs);
            EmitLaunchStatus(ev);
        });
        _providers->SetForegroundCallback([this](const string& appId, const string& state, const string& providerId, uint64_t tsMs) {
            EmitForegroundChanged(appId, state, providerId, tsMs);
        });

        // Start providers
        const uint32_t rc = _providers->StartAll(_config);
        if (rc != Core::ERROR_NONE) {
            SYSLOG(Logging::Error, (_T("LaunchDelegate: one or more providers failed to start: %u"), rc));
        }

        return string(); // success
    }

    void LaunchDelegate::Deinitialize(PluginHost::IShell* service) {
        ASSERT(service == _service);

        _providers->StopAll();

        Deactivate();

        if (_service != nullptr) {
            _service->Release();
            _service = nullptr;
        }

        UnregisterAll();
    }

    string LaunchDelegate::Information() const {
        return _T("{\"name\":\"LaunchDelegate\",\"version\":\"1.0.0\"}");
    }

    static bool IsEmpty(const Core::JSON::String& s) {
        return (!s.IsSet() || s.Value().empty());
    }

    uint32_t LaunchDelegate::Launch(const LaunchParams& in, LaunchResult& out) {
        const string appId = in.AppId.Value();
        const string uri = in.Uri.Value();
        const string intent = in.Intent.Value();

        if (appId.empty() && uri.empty() && intent.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        // Resolve provider
        Resolution res;
        const string hint = string();
        if (!_providers->Resolve(_config, appId, uri, intent, hint, res)) {
            return Core::ERROR_UNKNOWN_KEY;
        }

        ILaunchProvider* provider = _providers->Get(res.providerId);
        if (provider == nullptr) {
            return Core::ERROR_UNAVAILABLE;
        }

        // Create session and launchId
        const string launchId = _sessions->Create(appId, res.providerId);

        // Flags & timeout
        const bool background = in.Flags.Background.IsSet() ? in.Flags.Background.Value() : false;
        const bool bringToFront = in.Flags.BringToFront.IsSet() ? in.Flags.BringToFront.Value() : true;
        const uint32_t timeoutMs = in.Flags.TimeoutMs.IsSet() && in.Flags.TimeoutMs.Value() > 0 ? in.Flags.TimeoutMs.Value() : _config.TimeoutMs();

        LaunchEvent initial;
        const uint32_t rc = provider->Launch(launchId, appId, uri, intent, in.Parameters, background, bringToFront, timeoutMs, initial);
        if (rc != Core::ERROR_NONE) {
            _sessions->Update(launchId, "failed", "Provider launch failed", NowMs());
            return rc;
        }

        // Ensure initial accepted event updates the session (provider callback should have done it already)
        _sessions->Update(launchId, initial.state, initial.message, initial.timestampMs);

        out.LaunchId = launchId;
        out.Accepted = (initial.state == "accepted" || initial.state == "started" || initial.state == "foreground" || initial.state == "background");
        out.ProviderId = res.providerId;

        return Core::ERROR_NONE;
    }

    uint32_t LaunchDelegate::ResolveTarget(const ResolveParams& in, ResolveResult& out) {
        const string target = in.Target.Value();
        if (target.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        string appId, uri, intent;
        if (target.find("://") != string::npos) {
            uri = target;
        } else if (target.find(':') != string::npos) {
            // Consider "intent:playback"
            intent = target.substr(target.find(':') + 1);
        } else {
            appId = target;
        }

        Resolution res;
        if (!_providers->Resolve(_config, appId, uri, intent, in.Hint.Value(), res)) {
            return Core::ERROR_UNKNOWN_KEY;
        }

        out.ProviderId = res.providerId;
        out.Capability = res.capability;
        out.Confidence = res.confidence;
        out.Route = res.route;
        return Core::ERROR_NONE;
    }

    uint32_t LaunchDelegate::Cancel(const CancelParams& in, Core::JSON::VariantContainer&) {
        const string launchId = in.LaunchId.Value();
        if (launchId.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        SessionEntry se;
        if (!_sessions->Get(launchId, se)) {
            return Core::ERROR_UNKNOWN_KEY;
        }

        ILaunchProvider* provider = _providers->Get(se.providerId);
        if (provider == nullptr) {
            return Core::ERROR_UNAVAILABLE;
        }

        const string reason = in.Reason.Value();
        const uint32_t rc = provider->Cancel(launchId, reason);
        if (rc != Core::ERROR_NONE) {
            return rc;
        }

        _sessions->Cancel(launchId, reason, NowMs());
        return Core::ERROR_NONE;
    }

    uint32_t LaunchDelegate::GetStatus(const GetStatusParams& in, GetStatusResult& out) {
        const bool byLaunch = (in.LaunchId.IsSet() && !in.LaunchId.Value().empty());
        if (byLaunch) {
            SessionEntry se;
            if (!_sessions->Get(in.LaunchId.Value(), se)) {
                return Core::ERROR_UNKNOWN_KEY;
            }
            out.LaunchId = se.launchId;
            out.State = se.state;
            out.AppId = se.appId;
            out.ProviderId = se.providerId;
            out.LastUpdated = se.lastUpdatedMs;
            if (!se.message.empty()) {
                out.Message = se.message;
            }
        } else {
            out.Version = _T("1.0.0");
            out.ActiveLaunches = _sessions->ActiveCount();
            out.Providers = static_cast<uint32_t>(_providers->GetProviders().size());
        }
        return Core::ERROR_NONE;
    }

    uint32_t LaunchDelegate::GetCapabilities(const Core::JSON::VariantContainer&, GetCapabilitiesResult& out) {
        const auto caps = _providers->Capabilities();
        for (const auto& info : caps) {
            ProviderInfoJSON json;
            json.From(info);
            out.Providers.Add(json);
        }
        return Core::ERROR_NONE;
    }

    uint32_t LaunchDelegate::GetProviders(const Core::JSON::VariantContainer&, GetProvidersResult& out) {
        const auto list = _providers->GetProviders();
        for (const auto& s : list) {
            ProviderStateJSON json;
            json.Id = s.id;
            json.Name = s.name;
            json.Registered = s.registered;
            json.Healthy = s.healthy;
            out.Providers.Add(json);
        }
        return Core::ERROR_NONE;
    }

    uint32_t LaunchDelegate::Activate(const ActivateParams& in, Core::JSON::VariantContainer&) {
        const string appId = in.AppId.Value();
        if (appId.empty()) return Core::ERROR_BAD_REQUEST;

        // Choose provider: by appId support first
        Resolution res;
        if (!_providers->Resolve(_config, appId, string(), string(), string(), res)) {
            return Core::ERROR_UNKNOWN_KEY;
        }
        ILaunchProvider* provider = _providers->Get(res.providerId);
        if (provider == nullptr) {
            return Core::ERROR_UNAVAILABLE;
        }

        Core::JSON::VariantContainer opts;
        opts.Set(_T("bringToFront"), Core::JSON::Boolean(in.OptionsNode.BringToFront.Value()));
        return provider->Activate(appId, opts);
    }

    void LaunchDelegate::EmitLaunchStatus(const LaunchEvent& ev) {
        LaunchStatusEvent out;
        out.LaunchId = ev.launchId;
        out.State = ev.state;
        if (!ev.appId.empty()) out.AppId = ev.appId;
        out.ProviderId = ev.providerId;
        if (!ev.message.empty()) out.Message = ev.message;
        out.Timestamp = ev.timestampMs != 0 ? ev.timestampMs : NowMs();

        Notify(_T("launchstatus"), out);
    }

    void LaunchDelegate::EmitForegroundChanged(const string& appId, const string& state, const string& providerId, uint64_t tsMs) {
        ForegroundChangedEvent out;
        out.AppId = appId;
        out.State = state;
        out.ProviderId = providerId;
        out.Timestamp = tsMs != 0 ? tsMs : NowMs();

        Notify(_T("foregroundchanged"), out);
    }

    string LaunchDelegate::NowISO8601() {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
    #if defined(_WIN32)
        gmtime_s(&tm, &t);
    #else
        gmtime_r(&t, &tm);
    #endif
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    uint64_t LaunchDelegate::NowMs() {
        return Core::Time::Now().Ticks();
    }

} // namespace Plugin
