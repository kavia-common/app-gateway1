#include "DeepLinkProvider.h"

namespace Plugin {

    DeepLinkProvider::DeepLinkProvider()
        : _statusCb()
        , _fgCb()
        , _running(false)
        , _schemes({"http", "https", "video"}) {
    }

    DeepLinkProvider::~DeepLinkProvider() {
        Stop();
    }

    uint32_t DeepLinkProvider::Start(const Config&) {
        _running = true;
        return Core::ERROR_NONE;
    }

    void DeepLinkProvider::Stop() {
        _running = false;
    }

    bool DeepLinkProvider::SupportsAppId() const {
        return false;
    }

    bool DeepLinkProvider::SupportsScheme(const string& scheme) const {
        for (const auto& s : _schemes) {
            if (s == scheme) return true;
        }
        return false;
    }

    uint32_t DeepLinkProvider::Launch(const string& launchId,
                                      const string& appId,
                                      const string& uri,
                                      const string& intent,
                                      const Core::JSON::VariantContainer&,
                                      const bool background,
                                      const bool bringToFront,
                                      const uint32_t,
                                      LaunchEvent& initialEvent) {
        if (!_running) {
            return Core::ERROR_UNAVAILABLE;
        }
        (void)appId; (void)intent;

        // Validate URI scheme
        if (uri.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }
        const auto pos = uri.find(':');
        const string scheme = pos == string::npos ? string() : uri.substr(0, pos);
        if (!SupportsScheme(scheme)) {
            return Core::ERROR_UNKNOWN_KEY;
        }

        // Initial accepted event
        initialEvent.launchId = launchId;
        initialEvent.appId.clear();
        initialEvent.providerId = "deeplink";
        initialEvent.state = LaunchState::Accepted;
        initialEvent.message = "Deep link accepted";
        initialEvent.timestampMs = Core::Time::Now().Ticks();
        if (_statusCb) _statusCb(initialEvent);

        // Optionally bring to front
        if (bringToFront && _fgCb) {
            _fgCb(string(), "foreground", "deeplink", Core::Time::Now().Ticks());
        }

        // Started + Foreground/Background
        if (_statusCb) {
            LaunchEvent started = initialEvent;
            started.state = LaunchState::Started;
            started.message = "Opening deep link target";
            started.timestampMs = Core::Time::Now().Ticks();
            _statusCb(started);

            LaunchEvent st = initialEvent;
            st.state = background ? LaunchState::Background : LaunchState::Foreground;
            st.message = background ? "Deep link handled in background" : "Deep link handled in foreground";
            st.timestampMs = Core::Time::Now().Ticks();
            _statusCb(st);
        }

        return Core::ERROR_NONE;
    }

    uint32_t DeepLinkProvider::Cancel(const string& launchId, const string& reason) {
        if (!_running) return Core::ERROR_UNAVAILABLE;
        if (_statusCb) {
            LaunchEvent ev;
            ev.launchId = launchId;
            ev.state = LaunchState::Canceled;
            ev.providerId = "deeplink";
            ev.message = reason;
            ev.timestampMs = Core::Time::Now().Ticks();
            _statusCb(ev);
        }
        return Core::ERROR_NONE;
    }

    uint32_t DeepLinkProvider::Activate(const string&, const Core::JSON::VariantContainer&) {
        // For links, activation may not be meaningful; treat as success.
        return _running ? Core::ERROR_NONE : Core::ERROR_UNAVAILABLE;
    }

    ProviderInfo DeepLinkProvider::Info(const string& providerId) const {
        ProviderInfo info;
        info.id = providerId;
        info.name = "Deep Link Provider";
        info.schemes = _schemes;
        info.intents = {"view", "open"};
        info.supportsBackground = true;
        info.notes = "Handles URI/intent-based launches by scheme.";
        return info;
    }

    void DeepLinkProvider::OnStatus(StatusCallback cb) {
        _statusCb = std::move(cb);
    }

    void DeepLinkProvider::OnForegroundChanged(ForegroundCallback cb) {
        _fgCb = std::move(cb);
    }

} // namespace Plugin
