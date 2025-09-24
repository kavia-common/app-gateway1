#include "AppNotifications.h"

#include <cstdlib>

namespace Plugin {

    AppNotifications::AppNotifications()
        : PluginHost::JSONRPC(std::vector<uint8_t>{1})
        , _service(nullptr)
        , _store(Core::make_unique<NotificationStore>())
        , _config()
        , _adminLock()
        , _subscriptions()
        , _storagePath()
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

    AppNotifications::~AppNotifications() {
        // Ensure proper unregistration if Deinitialize was not called
        UnregisterAll();
    }

    void AppNotifications::RegisterAll() {
        Register<Core::JSON::VariantContainer, PingResult>(_T("ping"), &AppNotifications::Ping, this);
        Register<PublishParams, PublishResult>(_T("publish"), &AppNotifications::Publish, this);
        Register<SubscribeParams, SubscribeResult>(_T("subscribe"), &AppNotifications::Subscribe, this);
        Register<UnsubscribeParams, UnsubscribeResult>(_T("unsubscribe"), &AppNotifications::Unsubscribe, this);
        Register<ListParams, NotificationsArray>(_T("list"), &AppNotifications::List, this);
        Register<ClearParams, ClearResult>(_T("clear"), &AppNotifications::Clear, this);
        Register<AckParams, AckResult>(_T("ack"), &AppNotifications::Ack, this);
        Register<Core::JSON::VariantContainer, GetSubscribersResult>(_T("getsubscribers"), &AppNotifications::GetSubscribers, this);

        // Events:
        // - "notification"
        // - "ack"
        // - "subscriberup"
        // - "subscriberdrop"
        // - "cleared"
    }

    void AppNotifications::UnregisterAll() {
        Unregister(_T("ping"));
        Unregister(_T("publish"));
        Unregister(_T("subscribe"));
        Unregister(_T("unsubscribe"));
        Unregister(_T("list"));
        Unregister(_T("clear"));
        Unregister(_T("ack"));
        Unregister(_T("getsubscribers"));
    }

    const string AppNotifications::Initialize(PluginHost::IShell* service) {
        ASSERT(service != nullptr);
        _service = service;
        _service->AddRef();

        // Activate JSON-RPC dispatcher so Notify() can be used
        Activate(_service);

        // Parse configuration
        _config.FromString(_service->ConfigLine());

        // Configure store retention
        uint32_t ttl = _config.RetentionNode.TTLSeconds.IsSet() ? _config.RetentionNode.TTLSeconds.Value() : 0;
        uint32_t maxItems = _config.RetentionNode.MaxNotifications.IsSet() ? _config.RetentionNode.MaxNotifications.Value() : 0;
        _store->Configure(ttl, maxItems);

        // Resolve storage path and optionally load
        if (ShouldPersist()) {
            _storagePath = ResolvePath(_config.StorageNode.Path.Value());
            string error;
            if (!_store->LoadFromFile(_storagePath, error)) {
                SYSLOG(Logging::Warning, (_T("AppNotifications storage load failed: %s"), error.c_str()));
            }
        }

        return string(); // empty => success
    }

    void AppNotifications::Deinitialize(PluginHost::IShell* service) {
        ASSERT(service == _service);

        // Deactivate JSON-RPC dispatcher (clears observers)
        Deactivate();

        // Persist on shutdown if configured
        if (ShouldPersist()) {
            TrySave();
        }

        // Release service pointer
        if (_service != nullptr) {
            _service->Release();
            _service = nullptr;
        }

        // Unregister methods
        UnregisterAll();

        // Clear state
        std::lock_guard<std::mutex> guard(_adminLock);
        _subscriptions.clear();
    }

    string AppNotifications::Information() const {
        // Minimal metadata; could be expanded with build/version info
        return _T("{\"name\":\"AppNotifications\",\"version\":\"1.0.0\"}");
    }

    uint32_t AppNotifications::Ping(const Core::JSON::VariantContainer&, PingResult& out) {
        out.Time = NowISO8601();
        return Core::ERROR_NONE;
    }

    static size_t ApproxSizeBytes(const PublishParams& in) {
        // Approximate payload size for enforcement: title + body + type + data JSON + tags
        size_t total = 0;
        total += in.Title.Value().size() + in.Body.Value().size() + in.Type.Value().size();
        string dataStr;
        in.Data.ToString(dataStr);
        total += dataStr.size();
        for (uint32_t i = 0; i < in.Tags.Length(); ++i) {
            total += in.Tags[i].Value().size();
        }
        return total;
    }

    uint32_t AppNotifications::Publish(const PublishParams& in, PublishResult& out) {
        const string from = in.From.Value();
        const string type = in.Type.Value();

        if (from.empty() || type.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        // Optional payload size check
        if (_config.MaxPayloadBytes.IsSet() && _config.MaxPayloadBytes.Value() > 0) {
            const size_t approx = ApproxSizeBytes(in);
            if (approx > _config.MaxPayloadBytes.Value()) {
                SYSLOG(Logging::Warning, (_T("Publish payload exceeds MaxPayloadBytes: %u"), _config.MaxPayloadBytes.Value()));
                return Core::ERROR_INVALID_INPUT_LENGTH;
            }
        }

        Notification n;
        n.from = from;
        n.type = type;
        n.title = in.Title.Value();
        n.body = in.Body.Value();
        n.data = in.Data;
        n.priority = static_cast<uint8_t>(in.Priority.Value());
        n.requiresAck = in.RequiresAck.Value();
        n.timestamp = NowISO8601();
        n.tags.clear();
        for (uint32_t i = 0; i < in.Tags.Length(); ++i) {
            n.tags.push_back(in.Tags[i].Value());
        }

        uint32_t evicted = 0;
        const uint32_t ttl = in.TTLSeconds.Value();
        const string id = _store->Add(n, ttl, evicted);
        out.Id = id;

        // Emit events
        // Emit cleared if any were evicted due to retention/TTL (optional informational)
        if (evicted > 0) {
            EmitCleared(evicted, string(), string(), false);
        }
        // Emit the notification event
        NotificationJSON json;
        json.From(n);
        json.Id = id; // ensure assigned id visible
        EmitNotification(json.To());

        // Persist
        if (ShouldPersist()) {
            TrySave();
        }

        return Core::ERROR_NONE;
    }

    uint32_t AppNotifications::Subscribe(const SubscribeParams& in, SubscribeResult& out) {
        const string id = in.Id.Value();
        if (id.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        Subscription s;
        for (uint32_t i = 0; i < in.Types.Length(); ++i) {
            s.types.push_back(in.Types[i].Value());
        }
        for (uint32_t i = 0; i < in.Apps.Length(); ++i) {
            s.apps.push_back(in.Apps[i].Value());
        }

        {
            std::lock_guard<std::mutex> guard(_adminLock);
            _subscriptions[id] = std::move(s);
        }

        out.Subscribed = true;
        EmitSubscriberUp(id);
        return Core::ERROR_NONE;
    }

    uint32_t AppNotifications::Unsubscribe(const UnsubscribeParams& in, UnsubscribeResult& out) {
        const string id = in.Id.Value();
        if (id.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        bool removed = false;
        {
            std::lock_guard<std::mutex> guard(_adminLock);
            auto it = _subscriptions.find(id);
            if (it != _subscriptions.end()) {
                _subscriptions.erase(it);
                removed = true;
            }
        }
        out.Removed = removed;
        if (removed) {
            EmitSubscriberDown(id);
            return Core::ERROR_NONE;
        }
        return Core::ERROR_UNKNOWN_KEY;
    }

    uint32_t AppNotifications::List(const ListParams& in, NotificationsArray& out) {
        const string appId = in.AppId.Value();
        const string type = in.Type.Value();
        const bool onlyUnacked = in.OnlyUnacked.Value();
        const uint32_t limit = in.Limit.Value();

        const auto list = _store->List(appId, type, onlyUnacked, limit);
        for (const auto& n : list) {
            NotificationJSON json;
            json.From(n);
            out.Notifications.Add(json);
        }
        return Core::ERROR_NONE;
    }

    uint32_t AppNotifications::Clear(const ClearParams& in, ClearResult& out) {
        uint32_t removed = 0;
        if (in.Id.IsSet() && !in.Id.Value().empty()) {
            removed = _store->ClearById(in.Id.Value()) ? 1 : 0;
        } else {
            const string appId = in.AppId.Value();
            const string type = in.Type.Value();
            const bool onlyUnacked = in.OnlyUnacked.Value();
            removed = _store->ClearByFilter(appId, type, onlyUnacked);
        }
        out.Removed = removed;

        if (removed > 0) {
            EmitCleared(removed, in.AppId.Value(), in.Type.Value(), in.OnlyUnacked.Value());
            if (ShouldPersist()) {
                TrySave();
            }
        }

        return (removed > 0) ? Core::ERROR_NONE : Core::ERROR_UNKNOWN_KEY;
    }

    uint32_t AppNotifications::Ack(const AckParams& in, AckResult& out) {
        const string id = in.Id.Value();
        const string by = in.By.Value();
        if (id.empty() || by.empty()) {
            return Core::ERROR_BAD_REQUEST;
        }

        const bool ok = _store->Ack(id, by);
        out.Acked = ok;
        if (!ok) {
            return Core::ERROR_UNKNOWN_KEY;
        }

        EmitAck(id, by);

        if (ShouldPersist()) {
            TrySave();
        }

        return Core::ERROR_NONE;
    }

    uint32_t AppNotifications::GetSubscribers(const Core::JSON::VariantContainer&, GetSubscribersResult& out) {
        std::lock_guard<std::mutex> guard(_adminLock);
        for (const auto& kv : _subscriptions) {
            SubscriptionJSON json;
            json.Id = kv.first;
            for (const auto& t : kv.second.types) {
                Core::JSON::String v; v = t; json.Types.Add(v);
            }
            for (const auto& a : kv.second.apps) {
                Core::JSON::String v; v = a; json.Apps.Add(v);
            }
            out.Subscribers.Add(json);
        }
        return Core::ERROR_NONE;
    }

    void AppNotifications::EmitNotification(const Notification& n) {
        NotificationJSON ev;
        ev.From(n);
        Notify(_T("notification"), ev);
    }

    void AppNotifications::EmitAck(const string& id, const string& by) {
        AckEvent ev;
        ev.Id = id;
        ev.By = by;
        ev.Timestamp = NowISO8601();
        Notify(_T("ack"), ev);
    }

    void AppNotifications::EmitSubscriberUp(const string& id) {
        SubscriberEvent ev;
        ev.Id = id;
        ev.Timestamp = NowISO8601();
        Notify(_T("subscriberup"), ev);
    }

    void AppNotifications::EmitSubscriberDown(const string& id) {
        SubscriberEvent ev;
        ev.Id = id;
        ev.Timestamp = NowISO8601();
        Notify(_T("subscriberdrop"), ev);
    }

    void AppNotifications::EmitCleared(uint32_t removed, const string& appId, const string& type, bool onlyUnacked) {
        ClearedEvent ev;
        ev.Removed = removed;
        if (!appId.empty()) ev.AppId = appId;
        if (!type.empty()) ev.Type = type;
        ev.OnlyUnacked = onlyUnacked;
        ev.Timestamp = NowISO8601();
        Notify(_T("cleared"), ev);
    }

    string AppNotifications::NowISO8601() {
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

    bool AppNotifications::ShouldPersist() const {
        return (_config.StorageNode.Source.IsSet()
                && _config.StorageNode.Source.Value() == _T("file")
                && _config.StorageNode.Path.IsSet()
                && !_config.StorageNode.Path.Value().empty());
    }

    string AppNotifications::ResolvePath(const string& pathTemplate) const {
        string resolved = pathTemplate;
        const char* persistent = ::getenv("THUNDER_PERSISTENT_PATH");
        const char* data = ::getenv("THUNDER_DATA_PATH");
        if (persistent != nullptr) {
            const string key = "%persistentpath%";
            const size_t pos = resolved.find(key);
            if (pos != string::npos) {
                resolved.replace(pos, key.length(), persistent);
            }
        }
        if (data != nullptr) {
            const string key = "%datapath%";
            const size_t pos = resolved.find(key);
            if (pos != string::npos) {
                resolved.replace(pos, key.length(), data);
            }
        }
        return resolved;
    }

    void AppNotifications::TrySave() {
        if (_storagePath.empty()) return;
        string error;
        if (!_store->SaveToFile(_storagePath, error)) {
            SYSLOG(Logging::Warning, (_T("AppNotifications storage save failed: %s"), error.c_str()));
        }
    }

} // namespace Plugin
