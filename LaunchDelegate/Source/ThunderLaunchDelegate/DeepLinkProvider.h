#pragma once

#include "ILaunchProvider.h"

namespace Plugin {

    class DeepLinkProvider : public ILaunchProvider {
    public:
        DeepLinkProvider();
        ~DeepLinkProvider() override;

        // PUBLIC_INTERFACE
        uint32_t Start(const Config& config) override;

        // PUBLIC_INTERFACE
        void Stop() override;

        // PUBLIC_INTERFACE
        bool SupportsAppId() const override;

        // PUBLIC_INTERFACE
        bool SupportsScheme(const string& scheme) const override;

        // PUBLIC_INTERFACE
        uint32_t Launch(const string& launchId,
                        const string& appId,
                        const string& uri,
                        const string& intent,
                        const Core::JSON::VariantContainer& parameters,
                        const bool background,
                        const bool bringToFront,
                        const uint32_t timeoutMs,
                        LaunchEvent& initialEvent) override;

        // PUBLIC_INTERFACE
        uint32_t Cancel(const string& launchId, const string& reason) override;

        // PUBLIC_INTERFACE
        uint32_t Activate(const string& appId, const Core::JSON::VariantContainer& options) override;

        // PUBLIC_INTERFACE
        ProviderInfo Info(const string& providerId) const override;

        // PUBLIC_INTERFACE
        void OnStatus(StatusCallback cb) override;

        // PUBLIC_INTERFACE
        void OnForegroundChanged(ForegroundCallback cb) override;

    private:
        StatusCallback _statusCb;
        ForegroundCallback _fgCb;
        bool _running {false};
        std::vector<string> _schemes;
    };

} // namespace Plugin
