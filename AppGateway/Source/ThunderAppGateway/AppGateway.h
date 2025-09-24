#pragma once

#include "Module.h"
#include "Registry.h"
#include "LaunchClient.h"

#include <mutex>
#include <memory>

namespace Plugin {

    // JSON containers for API inputs/outputs

    // Ping result
    class PingResult : public Core::JSON::Container {
    public:
        PingResult() : Time() { Add(_T("time"), &Time); }
        Core::JSON::String Time;
    };

    // List result
    class AppsArray : public Core::JSON::Container {
    public:
        AppsArray() : Apps() { Add(_T("apps"), &Apps); }
        Core::JSON::ArrayType<AppRecordJSON> Apps;
    };

    // Launch params/result
    class LaunchParams : public Core::JSON::Container {
    public:
        LaunchParams()
            : Id()
            , Entry()
            , Intent()
            , Params()
            , Flags()
        {
            Add(_T("id"), &Id);
            Add(_T("entry"), &Entry);
            Add(_T("intent"), &Intent);
            Add(_T("params"), &Params);
            Add(_T("flags"), &Flags);
        }
        Core::JSON::String Id;
        Core::JSON::String Entry;
        Core::JSON::String Intent;
        Core::JSON::VariantContainer Params;
        Core::JSON::ArrayType<Core::JSON::String> Flags;
    };
    class LaunchResult : public Core::JSON::Container {
    public:
        LaunchResult()
            : Handle()
            , State()
        {
            Add(_T("handle"), &Handle);
            Add(_T("state"), &State);
        }
        Core::JSON::String Handle;
        Core::JSON::String State;
    };

    // Stop params/result
    class StopParams : public Core::JSON::Container {
    public:
        StopParams() : Id() { Add(_T("id"), &Id); }
        Core::JSON::String Id;
    };
    class StopResult : public Core::JSON::Container {
    public:
        StopResult() : State() { Add(_T("state"), &State); }
        Core::JSON::String State;
    };

    // Status params/result
    class StatusParams : public Core::JSON::Container {
    public:
        StatusParams() : Id() { Add(_T("id"), &Id); }
        Core::JSON::String Id;
    };
    class StatusResult : public Core::JSON::Container {
    public:
        StatusResult()
            : State()
            , Pid()
            , Info()
        {
            Add(_T("state"), &State);
            Add(_T("pid"), &Pid);
            Add(_T("info"), &Info);
        }
        Core::JSON::String State;
        Core::JSON::DecUInt32 Pid;
        Core::JSON::VariantContainer Info;
    };

    // GetCapabilities params/result
    class CapabilitiesParams : public Core::JSON::Container {
    public:
        CapabilitiesParams() : Id() { Add(_T("id"), &Id); }
        Core::JSON::String Id;
    };
    class CapabilitiesResult : public Core::JSON::Container {
    public:
        CapabilitiesResult() : Capabilities() { Add(_T("capabilities"), &Capabilities); }
        Core::JSON::VariantContainer Capabilities;
    };

    // Event payloads
    class StateChangedEvent : public Core::JSON::Container {
    public:
        StateChangedEvent()
            : Id()
            , State()
            , Reason()
            , Timestamp()
        {
            Add(_T("id"), &Id);
            Add(_T("state"), &State);
            Add(_T("reason"), &Reason);
            Add(_T("timestamp"), &Timestamp);
        }
        Core::JSON::String Id;
        Core::JSON::String State;
        Core::JSON::String Reason;
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
    class AppGatewayConfig : public Core::JSON::Container {
    public:
        class Root : public Core::JSON::Container {
        public:
            Root() : Mode() {
                Add(_T("mode"), &Mode);
            }
            Core::JSON::String Mode; // Off, Local, Container, Distributed
        };
        class RegistryCfg : public Core::JSON::Container {
        public:
            RegistryCfg() : Source(), Path() {
                Add(_T("source"), &Source);
                Add(_T("path"), &Path);
            }
            Core::JSON::String Source;
            Core::JSON::String Path;
        };

        AppGatewayConfig()
            : JSONRPCVersion()
            , RootNode()
            , RegistryNode()
            , LaunchTimeoutMs()
            , EnableTokenCheck()
        {
            // Optional override for JSON-RPC version; default 1
            Add(_T("jsonrpcVersion"), &JSONRPCVersion);
            Add(_T("root"), &RootNode);
            Add(_T("registry"), &RegistryNode);
            Add(_T("launchTimeoutMs"), &LaunchTimeoutMs);
            Add(_T("enableTokenCheck"), &EnableTokenCheck);
        }

        Core::JSON::DecUInt8 JSONRPCVersion;
        Root RootNode;
        RegistryCfg RegistryNode;
        Core::JSON::DecUInt32 LaunchTimeoutMs;
        Core::JSON::Boolean EnableTokenCheck;
    };

    class AppGateway : public PluginHost::IPlugin, public PluginHost::JSONRPC {
    public:
        AppGateway();
        ~AppGateway() override;

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
        uint32_t List(const Core::JSON::VariantContainer& in, AppsArray& out);

        // PUBLIC_INTERFACE
        uint32_t Launch(const LaunchParams& in, LaunchResult& out);

        // PUBLIC_INTERFACE
        uint32_t Stop(const StopParams& in, StopResult& out);

        // PUBLIC_INTERFACE
        uint32_t Status(const StatusParams& in, StatusResult& out);

        // PUBLIC_INTERFACE
        uint32_t GetCapabilities(const CapabilitiesParams& in, CapabilitiesResult& out);

    private:
        void RegisterAll();
        void UnregisterAll();

        void EmitStateChanged(const string& id, const string& state, const string& reason = string());
        void EmitCapabilitiesUpdated(const string& id, const Core::JSON::VariantContainer& capabilities);

        static string NowISO8601();

    private:
        PluginHost::IShell* _service;
        std::unique_ptr<Registry> _registry;
        std::unique_ptr<LaunchClient> _launch;
        AppGatewayConfig _config;
        mutable std::mutex _adminLock;
    };

} // namespace Plugin
