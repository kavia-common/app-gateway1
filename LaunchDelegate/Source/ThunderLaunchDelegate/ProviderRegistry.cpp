#include "ProviderRegistry.h"

#include <algorithm>

namespace Plugin {

    ProviderRegistry::ProviderRegistry()
        : _providers()
        , _statusCb()
        , _fgCb() {
    }

    ProviderRegistry::~ProviderRegistry() {
        StopAll();
    }

    bool ProviderRegistry::Register(const string& id, std::unique_ptr<ILaunchProvider> provider) {
        std::lock_guard<std::mutex> guard(_adminLock);
        if (_providers.find(id) != _providers.end()) {
            return false;
        }
        if (_statusCb) provider->OnStatus(_statusCb);
        if (_fgCb) provider->OnForegroundChanged(_fgCb);
        _providers.emplace(id, std::move(provider));
        return true;
    }

    bool ProviderRegistry::Unregister(const string& id) {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _providers.find(id);
        if (it == _providers.end()) {
            return false;
        }
        it->second->Stop();
        _providers.erase(it);
        return true;
    }

    uint32_t ProviderRegistry::StartAll(const Config& config) {
        std::lock_guard<std::mutex> guard(_adminLock);
        for (auto& kv : _providers) {
            const uint32_t rc = kv.second->Start(config);
            if (rc != Core::ERROR_NONE) {
                return rc;
            }
        }
        return Core::ERROR_NONE;
    }

    void ProviderRegistry::StopAll() {
        std::lock_guard<std::mutex> guard(_adminLock);
        for (auto& kv : _providers) {
            kv.second->Stop();
        }
    }

    bool ProviderRegistry::Resolve(const Config& config,
                                   const string& appId,
                                   const string& uri,
                                   const string& intent,
                                   const string& hint,
                                   Resolution& out) const
    {
        (void)hint;

        // Try routing rules first
        if (!uri.empty()) {
            const auto pos = uri.find(':');
            if (pos != string::npos) {
                const string scheme = uri.substr(0, pos);
                // Enforce allowedSchemes if configured
                if (!config.IsSchemeAllowed(scheme)) {
                    return false;
                }
                // Find a provider that supports this scheme
                if (ResolveByScheme(scheme, out)) {
                    out.capability = _T("scheme:") + scheme;
                    return true;
                }
            }
        }

        if (!appId.empty()) {
            // Enforce allowedApps if configured
            if (!config.IsAppAllowed(appId)) {
                return false;
            }
            if (ResolveByAppId(out)) {
                out.capability = _T("appId");
                return true;
            }
        }

        // Intent-based routing: choose the first provider that declares intents
        if (!intent.empty()) {
            std::lock_guard<std::mutex> guard(_adminLock);
            for (const auto& kv : _providers) {
                const ProviderInfo info = kv.second->Info(kv.first);
                if (std::find(info.intents.begin(), info.intents.end(), intent) != info.intents.end()) {
                    out.providerId = kv.first;
                    out.capability = _T("intent:") + intent;
                    out.confidence = 80;
                    return true;
                }
            }
        }

        // Fall back to first entry in providerChain if set
        if (config.ProviderChain.Length() > 0) {
            out.providerId = config.ProviderChain[0].Value();
            out.capability = _T("default");
            out.confidence = 50;
            return (_providers.find(out.providerId) != _providers.end());
        }

        // Or first registered provider
        {
            std::lock_guard<std::mutex> guard(_adminLock);
            if (!_providers.empty()) {
                out.providerId = _providers.begin()->first;
                out.capability = _T("default");
                out.confidence = 50;
                return true;
            }
        }

        return false;
    }

    ILaunchProvider* ProviderRegistry::Get(const string& id) const {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _providers.find(id);
        return (it == _providers.end() ? nullptr : it->second.get());
    }

    std::vector<ProviderInfo> ProviderRegistry::Capabilities() const {
        std::vector<ProviderInfo> list;
        std::lock_guard<std::mutex> guard(_adminLock);
        for (const auto& kv : _providers) {
            list.push_back(kv.second->Info(kv.first));
        }
        return list;
    }

    std::vector<ProviderState> ProviderRegistry::GetProviders() const {
        std::vector<ProviderState> list;
        std::lock_guard<std::mutex> guard(_adminLock);
        for (const auto& kv : _providers) {
            ProviderState s;
            s.id = kv.first;
            const ProviderInfo info = kv.second->Info(kv.first);
            s.name = info.name;
            s.registered = true;
            s.healthy = true; // Stub: providers report healthy after Start()
            list.push_back(s);
        }
        return list;
    }

    void ProviderRegistry::SetStatusCallback(ILaunchProvider::StatusCallback cb) {
        std::lock_guard<std::mutex> guard(_adminLock);
        _statusCb = std::move(cb);
        for (auto& kv : _providers) {
            kv.second->OnStatus(_statusCb);
        }
    }

    void ProviderRegistry::SetForegroundCallback(ILaunchProvider::ForegroundCallback cb) {
        std::lock_guard<std::mutex> guard(_adminLock);
        _fgCb = std::move(cb);
        for (auto& kv : _providers) {
            kv.second->OnForegroundChanged(_fgCb);
        }
    }

    bool ProviderRegistry::ResolveByScheme(const string& scheme, Resolution& out) const {
        std::lock_guard<std::mutex> guard(_adminLock);
        for (const auto& kv : _providers) {
            if (kv.second->SupportsScheme(scheme)) {
                out.providerId = kv.first;
                out.confidence = 100;
                return true;
            }
        }
        return false;
    }

    bool ProviderRegistry::ResolveByAppId(Resolution& out) const {
        std::lock_guard<std::mutex> guard(_adminLock);
        for (const auto& kv : _providers) {
            if (kv.second->SupportsAppId()) {
                out.providerId = kv.first;
                out.confidence = 100;
                return true;
            }
        }
        return false;
    }

} // namespace Plugin
