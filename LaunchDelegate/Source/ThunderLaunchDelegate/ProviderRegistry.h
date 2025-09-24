#pragma once

#include "Module.h"
#include "ILaunchProvider.h"
#include "Config.h"

#include <unordered_map>
#include <memory>
#include <mutex>

namespace Plugin {

    struct Resolution {
        string providerId;
        string capability; // e.g., "appId", "scheme:https", "intent:playback"
        uint8_t confidence {0}; // 0-100
        Core::JSON::VariantContainer route; // optional route metadata
    };

    struct ProviderState {
        string id;
        string name;
        bool registered {false};
        bool healthy {false};
    };

    class ProviderRegistry {
    public:
        ProviderRegistry();
        ~ProviderRegistry();

        // Register and own provider implementation. Returns false if already exists.
        bool Register(const string& id, std::unique_ptr<ILaunchProvider> provider);

        // Unregister by id.
        bool Unregister(const string& id);

        // Start all providers based on config.
        uint32_t StartAll(const Config& config);

        // Stop all providers and clear callbacks.
        void StopAll();

        // Resolve target based on appId/uri/intent and routing rules.
        bool Resolve(const Config& config,
                     const string& appId,
                     const string& uri,
                     const string& intent,
                     const string& hint,
                     Resolution& out) const;

        // Lookup provider by id (non-owning pointer).
        ILaunchProvider* Get(const string& id) const;

        // Capabilities listing.
        std::vector<ProviderInfo> Capabilities() const;

        // Provider states.
        std::vector<ProviderState> GetProviders() const;

        // Set callbacks for all current providers (called after Register and StartAll).
        void SetStatusCallback(ILaunchProvider::StatusCallback cb);
        void SetForegroundCallback(ILaunchProvider::ForegroundCallback cb);

    private:
        bool ResolveByScheme(const string& scheme, Resolution& out) const;
        bool ResolveByAppId(Resolution& out) const;

        mutable std::mutex _adminLock;
        std::unordered_map<string, std::unique_ptr<ILaunchProvider>> _providers;

        // callbacks fanned out to providers
        ILaunchProvider::StatusCallback _statusCb;
        ILaunchProvider::ForegroundCallback _fgCb;
    };

} // namespace Plugin
