#pragma once

#include "Module.h"

namespace Plugin {

    // Configuration schema for LaunchDelegate plugin
    class Config : public Core::JSON::Container {
    public:
        class Flags : public Core::JSON::Container {
        public:
            Flags()
                : Background(false)
                , BringToFront(true)
                , TimeoutMs(15000)
            {
                Add(_T("background"), &Background);
                Add(_T("bringToFront"), &BringToFront);
                Add(_T("timeoutMs"), &TimeoutMs);
            }
            Core::JSON::Boolean Background;
            Core::JSON::Boolean BringToFront;
            Core::JSON::DecUInt32 TimeoutMs;
        };

        class RoutingRule : public Core::JSON::Container {
        public:
            RoutingRule()
                : Match()
                , ProviderId()
            {
                Add(_T("match"), &Match);
                Add(_T("providerId"), &ProviderId);
            }
            Core::JSON::String Match;
            Core::JSON::String ProviderId;
        };

        class Providers : public Core::JSON::Container {
        public:
            Providers() : LocalAppMgr(), DeepLink() {
                Add(_T("local-appmgr"), &LocalAppMgr);
                Add(_T("deeplink"), &DeepLink);
            }
            Core::JSON::VariantContainer LocalAppMgr;
            Core::JSON::VariantContainer DeepLink;
        };

        Config()
            : Enabled(true)
            , DefaultTimeoutMs(15000)
            , AllowedApps()
            , AllowedSchemes()
            , ProviderChain()
            , Routing()
            , ProvidersNode()
        {
            Add(_T("enabled"), &Enabled);
            Add(_T("defaultTimeoutMs"), &DefaultTimeoutMs);
            Add(_T("allowedApps"), &AllowedApps);
            Add(_T("allowedSchemes"), &AllowedSchemes);
            Add(_T("providerChain"), &ProviderChain);
            Add(_T("routing"), &Routing);
            Add(_T("providers"), &ProvidersNode);
        }

        bool IsAppAllowed(const string& appId) const {
            if (AllowedApps.Length() == 0) return true;
            for (uint32_t i = 0; i < AllowedApps.Length(); ++i) {
                if (AllowedApps[i].Value() == appId) return true;
            }
            return false;
        }

        bool IsSchemeAllowed(const string& scheme) const {
            if (AllowedSchemes.Length() == 0) return true;
            for (uint32_t i = 0; i < AllowedSchemes.Length(); ++i) {
                if (AllowedSchemes[i].Value() == scheme) return true;
            }
            return false;
        }

        uint32_t TimeoutMs() const {
            return DefaultTimeoutMs.Value();
        }

        Core::JSON::Boolean Enabled;
        Core::JSON::DecUInt32 DefaultTimeoutMs;
        Core::JSON::ArrayType<Core::JSON::String> AllowedApps;
        Core::JSON::ArrayType<Core::JSON::String> AllowedSchemes;
        Core::JSON::ArrayType<Core::JSON::String> ProviderChain;
        Core::JSON::ArrayType<RoutingRule> Routing;
        Providers ProvidersNode;
    };

} // namespace Plugin
