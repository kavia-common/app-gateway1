#pragma once

#include "Module.h"
#include "Config.h"

#include <functional>
#include <vector>

namespace Plugin {

    namespace LaunchState {
        static constexpr const char* Accepted   = "accepted";
        static constexpr const char* Started    = "started";
        static constexpr const char* Foreground = "foreground";
        static constexpr const char* Background = "background";
        static constexpr const char* Failed     = "failed";
        static constexpr const char* Terminated = "terminated";
        static constexpr const char* Timeout    = "timeout";
        static constexpr const char* Canceled   = "canceled";
    }

    struct LaunchEvent {
        string launchId;
        string state;       // see LaunchState
        string appId;       // may be empty
        string providerId;
        string message;     // optional details
        uint64_t timestampMs {0};
    };

    struct ProviderInfo {
        string id;
        string name;
        std::vector<string> schemes;
        std::vector<string> intents;
        bool supportsBackground {false};
        string notes;
    };

    class ILaunchProvider {
    public:
        virtual ~ILaunchProvider() = default;

        // PUBLIC_INTERFACE
        virtual uint32_t Start(const Config& config) = 0;

        // PUBLIC_INTERFACE
        virtual void Stop() = 0;

        // PUBLIC_INTERFACE
        virtual bool SupportsAppId() const = 0;

        // PUBLIC_INTERFACE
        virtual bool SupportsScheme(const string& scheme) const = 0;

        // PUBLIC_INTERFACE
        virtual uint32_t Launch(const string& launchId,
                                const string& appId,
                                const string& uri,
                                const string& intent,
                                const Core::JSON::VariantContainer& parameters,
                                const bool background,
                                const bool bringToFront,
                                const uint32_t timeoutMs,
                                LaunchEvent& initialEvent) = 0;

        // PUBLIC_INTERFACE
        virtual uint32_t Cancel(const string& launchId, const string& reason) = 0;

        // PUBLIC_INTERFACE
        virtual uint32_t Activate(const string& appId, const Core::JSON::VariantContainer& options) = 0;

        // PUBLIC_INTERFACE
        virtual ProviderInfo Info(const string& providerId) const = 0;

        // Register callbacks for status/foreground changes
        using StatusCallback = std::function<void(const LaunchEvent&)>;
        using ForegroundCallback = std::function<void(const string& appId, const string& state /*foreground|background*/, const string& providerId, uint64_t tsMs)>;

        // PUBLIC_INTERFACE
        virtual void OnStatus(StatusCallback cb) = 0;

        // PUBLIC_INTERFACE
        virtual void OnForegroundChanged(ForegroundCallback cb) = 0;
    };

} // namespace Plugin
