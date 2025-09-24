#include "AppGateway.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace Plugin {

    // Helper to safely get string from VariantContainer
    static string GetString(const Core::JSON::VariantContainer& vc, const TCHAR* key) {
        const Core::JSON::Variant* v = vc.Get(key);
        if (v == nullptr) {
            return string();
        }
        if (v->Content() == Core::JSON::Variant::type::STRING) {
            return static_cast<const Core::JSON::String&>(*v).Value();
        }
        return string();
    }

    AppGateway::AppGateway()
        : PluginHost::JSONRPC(std::vector<uint8_t>{1})
        , _service(nullptr)
        , _registry(Core::make_unique<Registry>())
        , _launch(Core::make_unique<LaunchClient>())
        , _config()
    {
        // Optional: Token-based authorization hook, configurable
        using Classification = PluginHost::JSONRPC::classification;
        PluginHost::JSONRPC::TokenCheckFunction validate = [this](const string& token, const string& method, const string&) -> Classification {
            if (_config.EnableTokenCheck.IsSet() && _config.EnableTokenCheck.Value() == true) {
                if (token.empty()) {
                    SYSLOG(Logging::Warning, (_T("Token missing for method %s"), method.c_str()));
                    return Classification::INVALID;
                }
            }
            return Classification::VALID;
        };

        // Reinitialize JSONRPC dispatcher with version and optional token validation
        // Note: If JSONRPC ctor with validation is not available in this environment,
        // the validate function is unused but preserved for reference.
        // Register all JSON-RPC endpoints
        RegisterAll();
    }

    AppGateway::~AppGateway() {
        // Ensure proper unregistration if Deinitialize was not called
        UnregisterAll();
    }

    void AppGateway::RegisterAll() {
        Register<Core::JSON::VariantContainer, PingResult>(_T("ping"), &AppGateway::Ping, this);
        Register<Core::JSON::VariantContainer, AppsArray>(_T("list"), &AppGateway::List, this);
        Register<LaunchParams, LaunchResult>(_T("launch"), &AppGateway::Launch, this);
        Register<StopParams, StopResult>(_T("stop"), &AppGateway::Stop, this);
        Register<StatusParams, StatusResult>(_T("status"), &AppGateway::Status, this);
        Register<CapabilitiesParams, CapabilitiesResult>(_T("getCapabilities"), &AppGateway::GetCapabilities, this);

        // Events: "statechanged", "capabilitiesupdated"
        // Clients subscribe via standard JSON-RPC register calls.
    }

    void AppGateway::UnregisterAll() {
        Unregister(_T("ping"));
        Unregister(_T("list"));
        Unregister(_T("launch"));
        Unregister(_T("stop"));
        Unregister(_T("status"));
        Unregister(_T("getCapabilities"));
    }

    const string AppGateway::Initialize(PluginHost::IShell* service) {
        ASSERT(service != nullptr);
        _service = service;
        _service->AddRef();

        // Activate JSON-RPC dispatcher so Notify() can be used
        Activate(_service);

        // Parse configuration
        _config.FromString(_service->ConfigLine());

        // Note: In real Thunder, IShell supports variable expansion for %persistentpath% etc.
        // Load registry if file source is configured
        if (_config.RegistryNode.Source.IsSet() && _config.RegistryNode.Source.Value() == _T("file")) {
            string error;
            const string path = _config.RegistryNode.Path.Value();
            if (!_registry->LoadFromFile(path, error)) {
                SYSLOG(Logging::Warning, (_T("AppGateway registry load failed: %s"), error.c_str()));
            }
        }

        // Attempt to open LaunchClient (stubbed here)
        if (!_launch->Open()) {
            SYSLOG(Logging::Error, (_T("Failed to open LaunchClient connector")));
        }

        // Subsystems (optional): log if not active yet
        if (auto* subs = _service->SubSystems()) {
            if (!subs->IsActive(PluginHost::ISubSystem::NETWORK)) {
                SYSLOG(Logging::Information, (_T("NETWORK subsystem not active at initialization.")));
            }
            if (!subs->IsActive(PluginHost::ISubSystem::GRAPHICS)) {
                SYSLOG(Logging::Information, (_T("GRAPHICS subsystem not active at initialization.")));
            }
        }

        return string(); // empty => success
    }

    void AppGateway::Deinitialize(PluginHost::IShell* service) {
        ASSERT(service == _service);

        // Close LaunchClient
        if (_launch) {
            _launch->Close();
        }

        // Deactivate JSON-RPC dispatcher (clears observers)
        Deactivate();

        // Release service pointer
        if (_service != nullptr) {
            _service->Release();
            _service = nullptr;
        }

        // Unregister methods
        UnregisterAll();
    }

    string AppGateway::Information() const {
        // Minimal metadata; could be expanded with build/version info
        return _T("{\"name\":\"AppGateway\",\"version\":\"1.0.0\"}");
    }

    uint32_t AppGateway::Ping(const Core::JSON::VariantContainer&, PingResult& out) {
        out.Time = NowISO8601();
        return Core::ERROR_NONE;
    }

    uint32_t AppGateway::List(const Core::JSON::VariantContainer&, AppsArray& out) {
        const auto apps = _registry->List();
        for (const auto& rec : apps) {
            AppRecordJSON json;
            json.From(rec);
            out.Apps.Add(json);
        }
        return Core::ERROR_NONE;
    }

    uint32_t AppGateway::Launch(const LaunchParams& in, LaunchResult& out) {
        const string id = in.Id.Value();
        if (id.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        // Validate app exists (if registry is populated)
        AppRecord found;
        if (!_registry->Find(id, found)) {
            // Not a hard error if registry is optional; map to UNKNOWN_KEY per design
            return Core::ERROR_UNKNOWN_KEY;
        }

        string handle;
        string state;
        uint32_t rc = _launch->Launch(id, in.Intent.Value(), in.Params, handle, state);
        if (rc != Core::ERROR_NONE) {
            EmitStateChanged(id, AppState::Error, _T("LaunchDelegate unavailable or launch failure"));
            return rc;
        }

        out.Handle = handle;
        out.State = state;

        // Emit launching immediately
        EmitStateChanged(id, AppState::Launching, _T("Launch accepted"));

        // For the stubbed LaunchClient, state transitions to running immediately
        EmitStateChanged(id, AppState::Running);

        return Core::ERROR_NONE;
    }

    uint32_t AppGateway::Stop(const StopParams& in, StopResult& out) {
        const string id = in.Id.Value();
        if (id.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        // Validate app known
        AppRecord found;
        if (!_registry->Find(id, found)) {
            return Core::ERROR_UNKNOWN_KEY;
        }

        string state;
        uint32_t rc = _launch->Terminate(id, state);
        if (rc != Core::ERROR_NONE) {
            EmitStateChanged(id, AppState::Error, _T("Terminate failed"));
            return rc;
        }

        out.State = state;
        EmitStateChanged(id, state, _T("Stop requested"));
        return Core::ERROR_NONE;
    }

    uint32_t AppGateway::Status(const StatusParams& in, StatusResult& out) {
        const string id = in.Id.Value();
        if (id.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        uint32_t pid = 0;
        string state;
        uint32_t rc = _launch->State(id, state, pid);
        if (rc != Core::ERROR_NONE) {
            return rc;
        }

        out.State = state;
        out.Pid = pid;
        // Info is optional; keep empty for now
        return Core::ERROR_NONE;
    }

    uint32_t AppGateway::GetCapabilities(const CapabilitiesParams& in, CapabilitiesResult& out) {
        const string id = in.Id.Value();
        if (id.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        AppRecord rec;
        if (!_registry->Find(id, rec)) {
            return Core::ERROR_UNKNOWN_KEY;
        }

        out.Capabilities = rec.capabilities;
        return Core::ERROR_NONE;
    }

    void AppGateway::EmitStateChanged(const string& id, const string& state, const string& reason) {
        StateChangedEvent ev;
        ev.Id = id;
        ev.State = state;
        ev.Timestamp = NowISO8601();
        if (!reason.empty()) {
            ev.Reason = reason;
        }
        Notify(_T("statechanged"), ev);
    }

    void AppGateway::EmitCapabilitiesUpdated(const string& id, const Core::JSON::VariantContainer& capabilities) {
        CapabilitiesUpdatedEvent ev;
        ev.Id = id;
        ev.Capabilities = capabilities;
        ev.Timestamp = NowISO8601();
        Notify(_T("capabilitiesupdated"), ev);
    }

    string AppGateway::NowISO8601() {
        // RFC3339 / ISO8601 UTC
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

} // namespace Plugin
