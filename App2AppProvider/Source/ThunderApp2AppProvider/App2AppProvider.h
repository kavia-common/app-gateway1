#pragma once

#include "Module.h"
#include "SessionRegistry.h"

#include <mutex>
#include <memory>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace Plugin {

    // JSON containers for API inputs/outputs

    // Ping result
    class PingResult : public Core::JSON::Container {
    public:
        PingResult() : Time() { Add(_T("time"), &Time); }
        Core::JSON::String Time;
    };

    // Register app params/result
    class RegisterAppParams : public Core::JSON::Container {
    public:
        RegisterAppParams()
            : Id()
            , Name()
            , Version()
            , Capabilities()
        {
            Add(_T("id"), &Id);
            Add(_T("name"), &Name);
            Add(_T("version"), &Version);
            Add(_T("capabilities"), &Capabilities);
        }
        Core::JSON::String Id;
        Core::JSON::String Name;
        Core::JSON::String Version;
        Core::JSON::VariantContainer Capabilities;
    };
    class RegisterAppResult : public Core::JSON::Container {
    public:
        RegisterAppResult() : Session() { Add(_T("session"), &Session); }
        Core::JSON::String Session;
    };

    // Unregister app params/result
    class UnregisterAppParams : public Core::JSON::Container {
    public:
        UnregisterAppParams() : Id() { Add(_T("id"), &Id); }
        Core::JSON::String Id;
    };
    class UnregisterAppResult : public Core::JSON::Container {
    public:
        UnregisterAppResult() : Removed() { Add(_T("removed"), &Removed); }
        Core::JSON::Boolean Removed;
    };

    // Get peers params/result
    class GetPeersParams : public Core::JSON::Container {
    public:
        GetPeersParams() : Id() { Add(_T("id"), &Id); } // optional: exclude this id
        Core::JSON::String Id;
    };
    class PeersArray : public Core::JSON::Container {
    public:
        PeersArray() : Peers() { Add(_T("peers"), &Peers); }
        Core::JSON::ArrayType<AppInfoJSON> Peers;
    };

    // Update capabilities params/result
    class UpdateCapabilitiesParams : public Core::JSON::Container {
    public:
        UpdateCapabilitiesParams() : Id(), Capabilities() {
            Add(_T("id"), &Id);
            Add(_T("capabilities"), &Capabilities);
        }
        Core::JSON::String Id;
        Core::JSON::VariantContainer Capabilities;
    };
    class UpdateCapabilitiesResult : public Core::JSON::Container {
    public:
        UpdateCapabilitiesResult() : Updated() { Add(_T("updated"), &Updated); }
        Core::JSON::Boolean Updated;
    };

    // Messaging params/result
    class SendMessageParams : public Core::JSON::Container {
    public:
        SendMessageParams()
            : From()
            , To()
            , Type()
            , Payload()
            , CorrelationId()
        {
            Add(_T("from"), &From);
            Add(_T("to"), &To);
            Add(_T("type"), &Type);
            Add(_T("payload"), &Payload);
            Add(_T("correlationId"), &CorrelationId);
        }
        Core::JSON::String From;
        Core::JSON::String To;
        Core::JSON::String Type;
        Core::JSON::VariantContainer Payload;
        Core::JSON::String CorrelationId; // optional
    };
    class BroadcastParams : public Core::JSON::Container {
    public:
        BroadcastParams()
            : From()
            , Type()
            , Payload()
            , CorrelationId()
        {
            Add(_T("from"), &From);
            Add(_T("type"), &Type);
            Add(_T("payload"), &Payload);
            Add(_T("correlationId"), &CorrelationId);
        }
        Core::JSON::String From;
        Core::JSON::String Type;
        Core::JSON::VariantContainer Payload;
        Core::JSON::String CorrelationId; // optional
    };
    class MessageResult : public Core::JSON::Container {
    public:
        MessageResult() : Accepted(), CorrelationId() {
            Add(_T("accepted"), &Accepted);
            Add(_T("correlationId"), &CorrelationId);
        }
        Core::JSON::Boolean Accepted;
        Core::JSON::String CorrelationId;
    };

    // Event payloads
    class PeerUpEvent : public Core::JSON::Container {
    public:
        PeerUpEvent()
            : Id()
            , Name()
            , Version()
            , Capabilities()
            , Timestamp()
        {
            Add(_T("id"), &Id);
            Add(_T("name"), &Name);
            Add(_T("version"), &Version);
            Add(_T("capabilities"), &Capabilities);
            Add(_T("timestamp"), &Timestamp);
        }
        Core::JSON::String Id;
        Core::JSON::String Name;
        Core::JSON::String Version;
        Core::JSON::VariantContainer Capabilities;
        Core::JSON::String Timestamp;
    };
    class PeerDownEvent : public Core::JSON::Container {
    public:
        PeerDownEvent()
            : Id()
            , Timestamp()
        {
            Add(_T("id"), &Id);
            Add(_T("timestamp"), &Timestamp);
        }
        Core::JSON::String Id;
        Core::JSON::String Timestamp;
    };
    class MessageEvent : public Core::JSON::Container {
    public:
        MessageEvent()
            : From()
            , To()
            , Type()
            , Payload()
            , CorrelationId()
            , Timestamp()
        {
            Add(_T("from"), &From);
            Add(_T("to"), &To);
            Add(_T("type"), &Type);
            Add(_T("payload"), &Payload);
            Add(_T("correlationId"), &CorrelationId);
            Add(_T("timestamp"), &Timestamp);
        }
        Core::JSON::String From;
        Core::JSON::String To;
        Core::JSON::String Type;
        Core::JSON::VariantContainer Payload;
        Core::JSON::String CorrelationId;
        Core::JSON::String Timestamp;
    };
    class CapabilitiesUpdatedEvent : public Core::JSON::Container {
    public:
        CapabilitiesUpdatedEvent()
            : Id()
            , Capabilities()
            , Timestamp()
        {
            Add(_T("id"), &Id);
            Add(_T("capabilities"), &Capabilities);
            Add(_T("timestamp"), &Timestamp);
        }
        Core::JSON::String Id;
        Core::JSON::VariantContainer Capabilities;
        Core::JSON::String Timestamp;
    };

    // Plugin configuration (parsed from service->ConfigLine())
    class App2AppProviderConfig : public Core::JSON::Container {
    public:
        App2AppProviderConfig()
            : JSONRPCVersion()
            , EnableTokenCheck()
            , MaxPayloadBytes()
        {
            // Optional override for JSON-RPC version; default 1
            Add(_T("jsonrpcVersion"), &JSONRPCVersion);
            Add(_T("enableTokenCheck"), &EnableTokenCheck);
            Add(_T("maxPayloadBytes"), &MaxPayloadBytes);
        }

        Core::JSON::DecUInt8 JSONRPCVersion; // defaults to 1 if not set
        Core::JSON::Boolean EnableTokenCheck;
        Core::JSON::DecUInt32 MaxPayloadBytes; // 0 or unset => unlimited
    };

    class App2AppProvider : public PluginHost::IPlugin, public PluginHost::JSONRPC {
    public:
        App2AppProvider();
        ~App2AppProvider() override;

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
        uint32_t RegisterApp(const RegisterAppParams& in, RegisterAppResult& out);

        // PUBLIC_INTERFACE
        uint32_t UnregisterApp(const UnregisterAppParams& in, UnregisterAppResult& out);

        // PUBLIC_INTERFACE
        uint32_t GetPeers(const GetPeersParams& in, PeersArray& out);

        // PUBLIC_INTERFACE
        uint32_t UpdateCapabilities(const UpdateCapabilitiesParams& in, UpdateCapabilitiesResult& out);

        // PUBLIC_INTERFACE
        uint32_t SendMessage(const SendMessageParams& in, MessageResult& out);

        // PUBLIC_INTERFACE
        uint32_t Broadcast(const BroadcastParams& in, MessageResult& out);

    private:
        void RegisterAll();
        void UnregisterAll();

        void EmitPeerUp(const AppInfo& info);
        void EmitPeerDown(const string& id);
        void EmitCapabilitiesUpdated(const string& id, const Core::JSON::VariantContainer& capabilities);
        void EmitMessage(const string& from, const string& to, const string& type,
                         const Core::JSON::VariantContainer& payload, const string& correlationId);

        static string NowISO8601();

    private:
        PluginHost::IShell* _service;
        std::unique_ptr<SessionRegistry> _sessions;
        App2AppProviderConfig _config;
        mutable std::mutex _adminLock;
    };

} // namespace Plugin
