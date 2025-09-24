#pragma once

#include "Module.h"
#include "Config.h"
#include "ProviderRegistry.h"
#include "LaunchSessionTracker.h"

#include <mutex>
#include <memory>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace Plugin {

    // JSON API containers

    class LaunchFlags : public Core::JSON::Container {
    public:
        LaunchFlags() : Background(), BringToFront(), TimeoutMs() {
            Add(_T("background"), &Background);
            Add(_T("bringToFront"), &BringToFront);
            Add(_T("timeoutMs"), &TimeoutMs);
        }
        Core::JSON::Boolean Background;
        Core::JSON::Boolean BringToFront;
        Core::JSON::DecUInt32 TimeoutMs;
    };

    class LaunchParams : public Core::JSON::Container {
    public:
        LaunchParams()
            : AppId()
            , Uri()
            , Intent()
            , Parameters()
            , Flags()
            , Source()
        {
            Add(_T("appId"), &AppId);
            Add(_T("uri"), &Uri);
            Add(_T("intent"), &Intent);
            Add(_T("parameters"), &Parameters);
            Add(_T("flags"), &Flags);
            Add(_T("source"), &Source);
        }
        Core::JSON::String AppId;
        Core::JSON::String Uri;
        Core::JSON::String Intent;
        Core::JSON::VariantContainer Parameters;
        LaunchFlags Flags;
        Core::JSON::String Source;
    };

    class LaunchResult : public Core::JSON::Container {
    public:
        LaunchResult() : LaunchId(), Accepted(), ProviderId() {
            Add(_T("launchId"), &LaunchId);
            Add(_T("accepted"), &Accepted);
            Add(_T("providerId"), &ProviderId);
        }
        Core::JSON::String LaunchId;
        Core::JSON::Boolean Accepted;
        Core::JSON::String ProviderId;
    };

    class ResolveParams : public Core::JSON::Container {
    public:
        ResolveParams() : Target(), Hint() {
            Add(_T("target"), &Target);
            Add(_T("hint"), &Hint);
        }
        Core::JSON::String Target;
        Core::JSON::String Hint;
    };

    class ResolveResult : public Core::JSON::Container {
    public:
        ResolveResult() : ProviderId(), Capability(), Confidence(), Route() {
            Add(_T("providerId"), &ProviderId);
            Add(_T("capability"), &Capability);
            Add(_T("confidence"), &Confidence);
            Add(_T("route"), &Route);
        }
        Core::JSON::String ProviderId;
        Core::JSON::String Capability;
        Core::JSON::DecUInt8 Confidence;
        Core::JSON::VariantContainer Route;
    };

    class CancelParams : public Core::JSON::Container {
    public:
        CancelParams() : LaunchId(), Reason() {
            Add(_T("launchId"), &LaunchId);
            Add(_T("reason"), &Reason);
        }
        Core::JSON::String LaunchId;
        Core::JSON::String Reason;
    };

    class GetStatusParams : public Core::JSON::Container {
    public:
        GetStatusParams() : LaunchId() {
            Add(_T("launchId"), &LaunchId);
        }
        Core::JSON::String LaunchId; // optional
    };

    class GetStatusResult : public Core::JSON::Container {
    public:
        GetStatusResult()
            : LaunchId()
            , State()
            , AppId()
            , ProviderId()
            , LastUpdated()
            , Message()
            , Version()
            , ActiveLaunches()
            , Providers()
        {
            Add(_T("launchId"), &LaunchId);
            Add(_T("state"), &State);
            Add(_T("appId"), &AppId);
            Add(_T("providerId"), &ProviderId);
            Add(_T("lastUpdated"), &LastUpdated);
            Add(_T("message"), &Message);
            Add(_T("version"), &Version);
            Add(_T("activeLaunches"), &ActiveLaunches);
            Add(_T("providers"), &Providers);
        }
        Core::JSON::String LaunchId;
        Core::JSON::String State;
        Core::JSON::String AppId;
        Core::JSON::String ProviderId;
        Core::JSON::DecUInt64 LastUpdated;
        Core::JSON::String Message;

        Core::JSON::String Version; // populated when no launchId provided
        Core::JSON::DecUInt32 ActiveLaunches;
        Core::JSON::DecUInt32 Providers;
    };

    class ProviderInfoJSON : public Core::JSON::Container {
    public:
        ProviderInfoJSON() : Id(), Name(), Schemes(), Intents(), SupportsBackground(), Notes() {
            Add(_T("id"), &Id);
            Add(_T("name"), &Name);
            Add(_T("schemes"), &Schemes);
            Add(_T("intents"), &Intents);
            Add(_T("supportsBackground"), &SupportsBackground);
            Add(_T("notes"), &Notes);
        }
        Core::JSON::String Id;
        Core::JSON::String Name;
        Core::JSON::ArrayType<Core::JSON::String> Schemes;
        Core::JSON::ArrayType<Core::JSON::String> Intents;
        Core::JSON::Boolean SupportsBackground;
        Core::JSON::String Notes;

        void From(const ProviderInfo& info) {
            Id = info.id;
            Name = info.name;
            Schemes.Clear();
            for (const auto& s : info.schemes) {
                Core::JSON::String v; v = s; Schemes.Add(v);
            }
            Intents.Clear();
            for (const auto& s : info.intents) {
                Core::JSON::String v; v = s; Intents.Add(v);
            }
            SupportsBackground = info.supportsBackground;
            Notes = info.notes;
        }
    };

    class GetCapabilitiesResult : public Core::JSON::Container {
    public:
        GetCapabilitiesResult() : Providers() {
            Add(_T("providers"), &Providers);
        }
        Core::JSON::ArrayType<ProviderInfoJSON> Providers;
    };

    class ProviderStateJSON : public Core::JSON::Container {
    public:
        ProviderStateJSON() : Id(), Name(), Registered(), Healthy() {
            Add(_T("id"), &Id);
            Add(_T("name"), &Name);
            Add(_T("registered"), &Registered);
            Add(_T("healthy"), &Healthy);
        }
        Core::JSON::String Id;
        Core::JSON::String Name;
        Core::JSON::Boolean Registered;
        Core::JSON::Boolean Healthy;
    };

    class GetProvidersResult : public Core::JSON::Container {
    public:
        GetProvidersResult() : Providers() {
            Add(_T("providers"), &Providers);
        }
        Core::JSON::ArrayType<ProviderStateJSON> Providers;
    };

    class ActivateParams : public Core::JSON::Container {
    public:
        class Options : public Core::JSON::Container {
        public:
            Options() : BringToFront(true) { Add(_T("bringToFront"), &BringToFront); }
            Core::JSON::Boolean BringToFront;
        };
        ActivateParams() : AppId(), OptionsNode() {
            Add(_T("appId"), &AppId);
            Add(_T("options"), &OptionsNode);
        }
        Core::JSON::String AppId;
        Options OptionsNode;
    };

    // Events
    class LaunchStatusEvent : public Core::JSON::Container {
    public:
        LaunchStatusEvent() : LaunchId(), State(), AppId(), ProviderId(), Message(), Timestamp() {
            Add(_T("launchId"), &LaunchId);
            Add(_T("state"), &State);
            Add(_T("appId"), &AppId);
            Add(_T("providerId"), &ProviderId);
            Add(_T("message"), &Message);
            Add(_T("timestamp"), &Timestamp);
        }
        Core::JSON::String LaunchId;
        Core::JSON::String State;
        Core::JSON::String AppId;
        Core::JSON::String ProviderId;
        Core::JSON::String Message;
        Core::JSON::DecUInt64 Timestamp;
    };

    class ForegroundChangedEvent : public Core::JSON::Container {
    public:
        ForegroundChangedEvent() : AppId(), State(), ProviderId(), Timestamp() {
            Add(_T("appId"), &AppId);
            Add(_T("state"), &State);
            Add(_T("providerId"), &ProviderId);
            Add(_T("timestamp"), &Timestamp);
        }
        Core::JSON::String AppId;
        Core::JSON::String State; // "foreground" | "background"
        Core::JSON::String ProviderId;
        Core::JSON::DecUInt64 Timestamp;
    };

    class LaunchDelegate : public PluginHost::IPlugin, public PluginHost::JSONRPC {
    public:
        LaunchDelegate();
        ~LaunchDelegate() override;

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
        uint32_t Launch(const LaunchParams& in, LaunchResult& out);

        // PUBLIC_INTERFACE
        uint32_t ResolveTarget(const ResolveParams& in, ResolveResult& out);

        // PUBLIC_INTERFACE
        uint32_t Cancel(const CancelParams& in, Core::JSON::VariantContainer& out);

        // PUBLIC_INTERFACE
        uint32_t GetStatus(const GetStatusParams& in, GetStatusResult& out);

        // PUBLIC_INTERFACE
        uint32_t GetCapabilities(const Core::JSON::VariantContainer& in, GetCapabilitiesResult& out);

        // PUBLIC_INTERFACE
        uint32_t GetProviders(const Core::JSON::VariantContainer& in, GetProvidersResult& out);

        // PUBLIC_INTERFACE
        uint32_t Activate(const ActivateParams& in, Core::JSON::VariantContainer& out);

    private:
        void RegisterAll();
        void UnregisterAll();

        static string NowISO8601();
        static uint64_t NowMs();

        void EmitLaunchStatus(const LaunchEvent& ev);
        void EmitForegroundChanged(const string& appId, const string& state, const string& providerId, uint64_t tsMs);

    private:
        PluginHost::IShell* _service;
        std::unique_ptr<ProviderRegistry> _providers;
        std::unique_ptr<LaunchSessionTracker> _sessions;
        Config _config;
        mutable std::mutex _adminLock;
    };

} // namespace Plugin
