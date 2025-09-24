#include "App2AppProvider.h"

namespace Plugin {

    App2AppProvider::App2AppProvider()
        : PluginHost::JSONRPC(std::vector<uint8_t>{1})
        , _service(nullptr)
        , _sessions(Core::make_unique<SessionRegistry>())
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

        // Register all JSON-RPC endpoints
        RegisterAll();
    }

    App2AppProvider::~App2AppProvider() {
        // Ensure proper unregistration if Deinitialize was not called
        UnregisterAll();
    }

    void App2AppProvider::RegisterAll() {
        Register<Core::JSON::VariantContainer, PingResult>(_T("ping"), &App2AppProvider::Ping, this);
        Register<RegisterAppParams, RegisterAppResult>(_T("register"), &App2AppProvider::RegisterApp, this);
        Register<UnregisterAppParams, UnregisterAppResult>(_T("unregister"), &App2AppProvider::UnregisterApp, this);
        Register<GetPeersParams, PeersArray>(_T("getpeers"), &App2AppProvider::GetPeers, this);
        Register<UpdateCapabilitiesParams, UpdateCapabilitiesResult>(_T("updatecapabilities"), &App2AppProvider::UpdateCapabilities, this);
        Register<SendMessageParams, MessageResult>(_T("sendmessage"), &App2AppProvider::SendMessage, this);
        Register<BroadcastParams, MessageResult>(_T("broadcast"), &App2AppProvider::Broadcast, this);

        // Events: "peerup", "peerdown", "capabilitiesupdated", "messagereceived"
        // Clients subscribe via standard JSON-RPC register calls.
    }

    void App2AppProvider::UnregisterAll() {
        Unregister(_T("ping"));
        Unregister(_T("register"));
        Unregister(_T("unregister"));
        Unregister(_T("getpeers"));
        Unregister(_T("updatecapabilities"));
        Unregister(_T("sendmessage"));
        Unregister(_T("broadcast"));
    }

    const string App2AppProvider::Initialize(PluginHost::IShell* service) {
        ASSERT(service != nullptr);
        _service = service;
        _service->AddRef();

        // Activate JSON-RPC dispatcher so Notify() can be used
        Activate(_service);

        // Parse configuration
        _config.FromString(_service->ConfigLine());

        // Log initial state
        SYSLOG(Logging::Information, (_T("App2AppProvider initialized. Registered peers: %u"), _sessions->Count()));

        return string(); // empty => success
    }

    void App2AppProvider::Deinitialize(PluginHost::IShell* service) {
        ASSERT(service == _service);

        // Deactivate JSON-RPC dispatcher (clears observers)
        Deactivate();

        // Release service pointer
        if (_service != nullptr) {
            _service->Release();
            _service = nullptr;
        }

        // Unregister methods
        UnregisterAll();

        // Clear state
        _sessions.reset(new SessionRegistry());
    }

    string App2AppProvider::Information() const {
        // Minimal metadata; could be expanded with build/version info
        return _T("{\"name\":\"App2AppProvider\",\"version\":\"1.0.0\"}");
    }

    uint32_t App2AppProvider::Ping(const Core::JSON::VariantContainer&, PingResult& out) {
        out.Time = NowISO8601();
        return Core::ERROR_NONE;
    }

    uint32_t App2AppProvider::RegisterApp(const RegisterAppParams& in, RegisterAppResult& out) {
        const string id = in.Id.Value();
        if (id.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        AppInfo info;
        info.id = id;
        info.name = in.Name.Value();
        info.version = in.Version.Value();
        info.capabilities = in.Capabilities;

        string session;
        bool isNew = false;
        if (!_sessions->RegisterApp(info, session, isNew)) {
            return Core::ERROR_GENERAL;
        }
        out.Session = session;

        if (isNew) {
            EmitPeerUp(info);
        } else {
            // Consider an update as a capabilities update (if capabilities present)
            if (in.Capabilities.IsSet() == true) {
                EmitCapabilitiesUpdated(info.id, info.capabilities);
            }
        }

        return Core::ERROR_NONE;
    }

    uint32_t App2AppProvider::UnregisterApp(const UnregisterAppParams& in, UnregisterAppResult& out) {
        const string id = in.Id.Value();
        if (id.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        AppInfo removed;
        const bool ok = _sessions->UnregisterById(id, &removed);
        out.Removed = ok;
        if (ok) {
            EmitPeerDown(id);
            return Core::ERROR_NONE;
        }
        return Core::ERROR_UNKNOWN_KEY;
    }

    uint32_t App2AppProvider::GetPeers(const GetPeersParams& in, PeersArray& out) {
        const string excludeId = in.Id.Value();
        const auto peers = _sessions->ListPeers(excludeId);
        for (const auto& p : peers) {
            AppInfoJSON json;
            json.From(p);
            out.Peers.Add(json);
        }
        return Core::ERROR_NONE;
    }

    uint32_t App2AppProvider::UpdateCapabilities(const UpdateCapabilitiesParams& in, UpdateCapabilitiesResult& out) {
        const string id = in.Id.Value();
        if (id.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }
        const bool ok = _sessions->UpdateCapabilities(id, in.Capabilities);
        out.Updated = ok;
        if (!ok) {
            return Core::ERROR_UNKNOWN_KEY;
        }
        EmitCapabilitiesUpdated(id, in.Capabilities);
        return Core::ERROR_NONE;
    }

    uint32_t App2AppProvider::SendMessage(const SendMessageParams& in, MessageResult& out) {
        const string from = in.From.Value();
        const string to   = in.To.Value();
        const string type = in.Type.Value();
        const string corr = in.CorrelationId.Value();

        if (from.empty() || to.empty() || type.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        // Validate sender and receiver exist
        AppInfo dummy;
        if (!_sessions->FindById(from, dummy)) {
            return Core::ERROR_UNKNOWN_KEY;
        }
        if (!_sessions->FindById(to, dummy)) {
            return Core::ERROR_UNKNOWN_KEY;
        }

        // Optional payload size check
        if (_config.MaxPayloadBytes.IsSet() && _config.MaxPayloadBytes.Value() > 0) {
            // Conservative check by stringifying payload
            string payloadStr;
            in.Payload.ToString(payloadStr);
            if (payloadStr.size() > _config.MaxPayloadBytes.Value()) {
                SYSLOG(Logging::Warning, (_T("Payload exceeds MaxPayloadBytes: %u"), _config.MaxPayloadBytes.Value()));
                return Core::ERROR_INVALID_INPUT_LENGTH;
            }
        }

        out.Accepted = true;
        out.CorrelationId = corr;

        EmitMessage(from, to, type, in.Payload, corr);
        return Core::ERROR_NONE;
    }

    uint32_t App2AppProvider::Broadcast(const BroadcastParams& in, MessageResult& out) {
        const string from = in.From.Value();
        const string type = in.Type.Value();
        const string corr = in.CorrelationId.Value();

        if (from.empty() || type.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        // Validate sender exists
        AppInfo dummy;
        if (!_sessions->FindById(from, dummy)) {
            return Core::ERROR_UNKNOWN_KEY;
        }

        // Optional payload size check
        if (_config.MaxPayloadBytes.IsSet() && _config.MaxPayloadBytes.Value() > 0) {
            string payloadStr;
            in.Payload.ToString(payloadStr);
            if (payloadStr.size() > _config.MaxPayloadBytes.Value()) {
                SYSLOG(Logging::Warning, (_T("Payload exceeds MaxPayloadBytes: %u"), _config.MaxPayloadBytes.Value()));
                return Core::ERROR_INVALID_INPUT_LENGTH;
            }
        }

        out.Accepted = true;
        out.CorrelationId = corr;

        // Emit a message event for each peer except the sender
        const auto peers = _sessions->ListPeers(from);
        for (const auto& peer : peers) {
            EmitMessage(from, peer.id, type, in.Payload, corr);
        }
        return Core::ERROR_NONE;
    }

    void App2AppProvider::EmitPeerUp(const AppInfo& info) {
        PeerUpEvent ev;
        ev.Id = info.id;
        ev.Name = info.name;
        ev.Version = info.version;
        ev.Capabilities = info.capabilities;
        ev.Timestamp = NowISO8601();
        Notify(_T("peerup"), ev);
    }

    void App2AppProvider::EmitPeerDown(const string& id) {
        PeerDownEvent ev;
        ev.Id = id;
        ev.Timestamp = NowISO8601();
        Notify(_T("peerdown"), ev);
    }

    void App2AppProvider::EmitCapabilitiesUpdated(const string& id, const Core::JSON::VariantContainer& capabilities) {
        CapabilitiesUpdatedEvent ev;
        ev.Id = id;
        ev.Capabilities = capabilities;
        ev.Timestamp = NowISO8601();
        Notify(_T("capabilitiesupdated"), ev);
    }

    void App2AppProvider::EmitMessage(const string& from, const string& to, const string& type,
                                      const Core::JSON::VariantContainer& payload, const string& correlationId) {
        MessageEvent ev;
        ev.From = from;
        ev.To = to;
        ev.Type = type;
        ev.Payload = payload;
        if (!correlationId.empty()) {
            ev.CorrelationId = correlationId;
        }
        ev.Timestamp = NowISO8601();
        Notify(_T("messagereceived"), ev);
    }

    string App2AppProvider::NowISO8601() {
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
