#pragma once

#include "Module.h"
#include "NotificationStore.h"

#include <mutex>
#include <memory>
#include <unordered_map>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Plugin {

    // PUBLIC_INTERFACE
    class PingResult : public Core::JSON::Container {
    public:
        /** This is a public function. Returns current UTC time in ISO8601. */
        PingResult() : Time() { Add(_T("time"), &Time); }
        Core::JSON::String Time;
    };

    // Publish API
    class PublishParams : public Core::JSON::Container {
    public:
        PublishParams()
            : From()
            , Type()
            , Title()
            , Body()
            , Data()
            , Priority()
            , RequiresAck()
            , TTLSeconds()
            , Tags()
        {
            Add(_T("from"), &From);
            Add(_T("type"), &Type);
            Add(_T("title"), &Title);
            Add(_T("body"), &Body);
            Add(_T("data"), &Data);
            Add(_T("priority"), &Priority);
            Add(_T("requiresAck"), &RequiresAck);
            Add(_T("ttlSeconds"), &TTLSeconds);
            Add(_T("tags"), &Tags);
        }
        Core::JSON::String From;
        Core::JSON::String Type;
        Core::JSON::String Title;
        Core::JSON::String Body;
        Core::JSON::VariantContainer Data;
        Core::JSON::DecUInt8 Priority;
        Core::JSON::Boolean RequiresAck;
        Core::JSON::DecUInt32 TTLSeconds; // optional; 0 uses default
        Core::JSON::ArrayType<Core::JSON::String> Tags;
    };
    class PublishResult : public Core::JSON::Container {
    public:
        PublishResult() : Id() { Add(_T("id"), &Id); }
        Core::JSON::String Id;
    };

    // Subscribe API
    class SubscribeParams : public Core::JSON::Container {
    public:
        SubscribeParams() : Id(), Types(), Apps() {
            Add(_T("id"), &Id);
            Add(_T("types"), &Types);
            Add(_T("apps"), &Apps);
        }
        Core::JSON::String Id;
        Core::JSON::ArrayType<Core::JSON::String> Types;
        Core::JSON::ArrayType<Core::JSON::String> Apps;
    };
    class SubscribeResult : public Core::JSON::Container {
    public:
        SubscribeResult() : Subscribed() { Add(_T("subscribed"), &Subscribed); }
        Core::JSON::Boolean Subscribed;
    };

    // Unsubscribe API
    class UnsubscribeParams : public Core::JSON::Container {
    public:
        UnsubscribeParams() : Id() { Add(_T("id"), &Id); }
        Core::JSON::String Id;
    };
    class UnsubscribeResult : public Core::JSON::Container {
    public:
        UnsubscribeResult() : Removed() { Add(_T("removed"), &Removed); }
        Core::JSON::Boolean Removed;
    };

    // List API
    class ListParams : public Core::JSON::Container {
    public:
        ListParams() : AppId(), Type(), OnlyUnacked(), Limit() {
            Add(_T("appId"), &AppId);
            Add(_T("type"), &Type);
            Add(_T("onlyUnacked"), &OnlyUnacked);
            Add(_T("limit"), &Limit);
        }
        Core::JSON::String AppId;
        Core::JSON::String Type;
        Core::JSON::Boolean OnlyUnacked;
        Core::JSON::DecUInt32 Limit;
    };
    class NotificationsArray : public Core::JSON::Container {
    public:
        NotificationsArray() : Notifications() { Add(_T("notifications"), &Notifications); }
        Core::JSON::ArrayType<NotificationJSON> Notifications;
    };

    // Clear API
    class ClearParams : public Core::JSON::Container {
    public:
        ClearParams() : Id(), AppId(), Type(), OnlyUnacked() {
            Add(_T("id"), &Id);
            Add(_T("appId"), &AppId);
            Add(_T("type"), &Type);
            Add(_T("onlyUnacked"), &OnlyUnacked);
        }
        Core::JSON::String Id; // if set, clear by id
        Core::JSON::String AppId;
        Core::JSON::String Type;
        Core::JSON::Boolean OnlyUnacked;
    };
    class ClearResult : public Core::JSON::Container {
    public:
        ClearResult() : Removed() { Add(_T("removed"), &Removed); }
        Core::JSON::DecUInt32 Removed;
    };

    // Ack API
    class AckParams : public Core::JSON::Container {
    public:
        AckParams() : Id(), By() {
            Add(_T("id"), &Id);
            Add(_T("by"), &By);
        }
        Core::JSON::String Id;
        Core::JSON::String By;
    };
    class AckResult : public Core::JSON::Container {
    public:
        AckResult() : Acked() { Add(_T("acked"), &Acked); }
        Core::JSON::Boolean Acked;
    };

    // GetSubscribers API
    class SubscriptionJSON : public Core::JSON::Container {
    public:
        SubscriptionJSON()
            : Id()
            , Types()
            , Apps()
        {
            Add(_T("id"), &Id);
            Add(_T("types"), &Types);
            Add(_T("apps"), &Apps);
        }
        Core::JSON::String Id;
        Core::JSON::ArrayType<Core::JSON::String> Types;
        Core::JSON::ArrayType<Core::JSON::String> Apps;
    };
    class GetSubscribersResult : public Core::JSON::Container {
    public:
        GetSubscribersResult() : Subscribers() { Add(_T("subscribers"), &Subscribers); }
        Core::JSON::ArrayType<SubscriptionJSON> Subscribers;
    };

    // Event payloads
    class AckEvent : public Core::JSON::Container {
    public:
        AckEvent() : Id(), By(), Timestamp() {
            Add(_T("id"), &Id);
            Add(_T("by"), &By);
            Add(_T("timestamp"), &Timestamp);
        }
        Core::JSON::String Id;
        Core::JSON::String By;
        Core::JSON::String Timestamp;
    };

    class SubscriberEvent : public Core::JSON::Container {
    public:
        SubscriberEvent() : Id(), Timestamp() {
            Add(_T("id"), &Id);
            Add(_T("timestamp"), &Timestamp);
        }
        Core::JSON::String Id;
        Core::JSON::String Timestamp;
    };

    class ClearedEvent : public Core::JSON::Container {
    public:
        ClearedEvent() : Removed(), AppId(), Type(), OnlyUnacked(), Timestamp() {
            Add(_T("removed"), &Removed);
            Add(_T("appId"), &AppId);
            Add(_T("type"), &Type);
            Add(_T("onlyUnacked"), &OnlyUnacked);
            Add(_T("timestamp"), &Timestamp);
        }
        Core::JSON::DecUInt32 Removed;
        Core::JSON::String AppId;
        Core::JSON::String Type;
        Core::JSON::Boolean OnlyUnacked;
        Core::JSON::String Timestamp;
    };

    // Plugin configuration (parsed from service->ConfigLine())
    class AppNotificationsConfig : public Core::JSON::Container {
    public:
        class Storage : public Core::JSON::Container {
        public:
            Storage() : Source(), Path() {
                Add(_T("source"), &Source); // "memory" or "file"
                Add(_T("path"), &Path);     // file path when source="file"
            }
            Core::JSON::String Source;
            Core::JSON::String Path;
        };
        class Retention : public Core::JSON::Container {
        public:
            Retention() : MaxNotifications(), TTLSeconds() {
                Add(_T("maxNotifications"), &MaxNotifications);
                Add(_T("ttlSeconds"), &TTLSeconds);
            }
            Core::JSON::DecUInt32 MaxNotifications; // 0 = no limit
            Core::JSON::DecUInt32 TTLSeconds;       // 0 = no expiry
        };

        AppNotificationsConfig()
            : JSONRPCVersion()
            , EnableTokenCheck()
            , MaxPayloadBytes()
            , StorageNode()
            , RetentionNode()
        {
            Add(_T("jsonrpcVersion"), &JSONRPCVersion);
            Add(_T("enableTokenCheck"), &EnableTokenCheck);
            Add(_T("maxPayloadBytes"), &MaxPayloadBytes);
            Add(_T("storage"), &StorageNode);
            Add(_T("retention"), &RetentionNode);
        }

        Core::JSON::DecUInt8 JSONRPCVersion; // default 1
        Core::JSON::Boolean EnableTokenCheck;
        Core::JSON::DecUInt32 MaxPayloadBytes; // optional; approximate limit
        Storage StorageNode;
        Retention RetentionNode;
    };

    class AppNotifications : public PluginHost::IPlugin, public PluginHost::JSONRPC {
    public:
        AppNotifications();
        ~AppNotifications() override;

        // IPlugin interface

        // PUBLIC_INTERFACE
        const string Initialize(PluginHost::IShell* service) override;
        // PUBLIC_INTERFACE
        void Deinitialize(PluginHost::IShell* service) override;
        // PUBLIC_INTERFACE
        string Information() const override;

    public:
        // JSON-RPC handlers

        // PUBLIC_INTERFACE
        uint32_t Ping(const Core::JSON::VariantContainer& in, PingResult& out);

        // PUBLIC_INTERFACE
        uint32_t Publish(const PublishParams& in, PublishResult& out);

        // PUBLIC_INTERFACE
        uint32_t Subscribe(const SubscribeParams& in, SubscribeResult& out);

        // PUBLIC_INTERFACE
        uint32_t Unsubscribe(const UnsubscribeParams& in, UnsubscribeResult& out);

        // PUBLIC_INTERFACE
        uint32_t List(const ListParams& in, NotificationsArray& out);

        // PUBLIC_INTERFACE
        uint32_t Clear(const ClearParams& in, ClearResult& out);

        // PUBLIC_INTERFACE
        uint32_t Ack(const AckParams& in, AckResult& out);

        // PUBLIC_INTERFACE
        uint32_t GetSubscribers(const Core::JSON::VariantContainer& in, GetSubscribersResult& out);

    private:
        void RegisterAll();
        void UnregisterAll();

        void EmitNotification(const Notification& n);
        void EmitAck(const string& id, const string& by);
        void EmitSubscriberUp(const string& id);
        void EmitSubscriberDown(const string& id);
        void EmitCleared(uint32_t removed, const string& appId, const string& type, bool onlyUnacked);

        static string NowISO8601();

        // Path helper: expand %persistentpath% and %datapath% via env in this minimal setup
        string ResolvePath(const string& pathTemplate) const;

        bool ShouldPersist() const;
        void TrySave();

    private:
        PluginHost::IShell* _service;
        std::unique_ptr<NotificationStore> _store;
        AppNotificationsConfig _config;
        mutable std::mutex _adminLock;

        struct Subscription {
            std::vector<string> types;
            std::vector<string> apps;
        };
        std::unordered_map<string, Subscription> _subscriptions; // id -> subscription

        string _storagePath;
    };

} // namespace Plugin
