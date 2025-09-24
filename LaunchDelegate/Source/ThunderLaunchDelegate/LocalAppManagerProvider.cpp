#include "LocalAppManagerProvider.h"

namespace Plugin {

    LocalAppManagerProvider::LocalAppManagerProvider()
        : _statusCb()
        , _fgCb()
        , _running(false) {
    }

    LocalAppManagerProvider::~LocalAppManagerProvider() {
        Stop();
    }

    uint32_t LocalAppManagerProvider::Start(const Config&) {
        _running = true;
        return Core::ERROR_NONE;
    }

    void LocalAppManagerProvider::Stop() {
        _running = false;
    }

    bool LocalAppManagerProvider::SupportsAppId() const {
        return true;
    }

    bool LocalAppManagerProvider::SupportsScheme(const string&) const {
        return false;
    }

    uint32_t LocalAppManagerProvider::Launch(const string& launchId,
                                             const string& appId,
                                             const string& uri,
                                             const string& intent,
                                             const Core::JSON::VariantContainer&,
                                             const bool background,
                                             const bool bringToFront,
                                             const uint32_t,
                                             LaunchEvent& initialEvent) {
        if (!_running) {
            return Core::ERROR_UNAVAILABLE;
        }
        (void)uri; (void)intent;

        // Produce initial accepted event
        initialEvent.launchId = launchId;
        initialEvent.appId = appId;
        initialEvent.providerId = "local-appmgr";
        initialEvent.state = LaunchState::Accepted;
        initialEvent.message = background ? "Launching in background" : "Launching";
        initialEvent.timestampMs = Core::Time::Now().Ticks();

        if (_statusCb) {
            _statusCb(initialEvent);
        }

        // Synchronously indicate foreground if requested
        if (bringToFront && _fgCb) {
            _fgCb(appId, "foreground", "local-appmgr", Core::Time::Now().Ticks());
        }

        // Then signal started/foreground
        if (_statusCb) {
            LaunchEvent started = initialEvent;
            started.state = LaunchState::Started;
            started.message = "Process started";
            started.timestampMs = Core::Time::Now().Ticks();
            _statusCb(started);

            LaunchEvent fg = initialEvent;
            fg.state = background ? LaunchState::Background : LaunchState::Foreground;
            fg.message = background ? "Running in background" : "Running in foreground";
            fg.timestampMs = Core::Time::Now().Ticks();
            _statusCb(fg);
        }

        return Core::ERROR_NONE;
    }

    uint32_t LocalAppManagerProvider::Cancel(const string& launchId, const string& reason) {
        if (!_running) return Core::ERROR_UNAVAILABLE;
        if (_statusCb) {
            LaunchEvent ev;
            ev.launchId = launchId;
            ev.state = LaunchState::Canceled;
            ev.providerId = "local-appmgr";
            ev.message = reason;
            ev.timestampMs = Core::Time::Now().Ticks();
            _statusCb(ev);
        }
        return Core::ERROR_NONE;
    }

    uint32_t LocalAppManagerProvider::Activate(const string& appId, const Core::JSON::VariantContainer&) {
        if (!_running) return Core::ERROR_UNAVAILABLE;
        if (_fgCb) {
            _fgCb(appId, "foreground", "local-appmgr", Core::Time::Now().Ticks());
        }
        return Core::ERROR_NONE;
    }

    ProviderInfo LocalAppManagerProvider::Info(const string& providerId) const {
        ProviderInfo info;
        info.id = providerId;
        info.name = "Local App Manager";
        info.schemes = {}; // not a scheme provider
        info.intents = {"view", "play", "default"};
        info.supportsBackground = true;
        info.notes = "Launches apps by appId using local system app manager.";
        return info;
    }

    void LocalAppManagerProvider::OnStatus(StatusCallback cb) {
        _statusCb = std::move(cb);
    }

    void LocalAppManagerProvider::OnForegroundChanged(ForegroundCallback cb) {
        _fgCb = std::move(cb);
    }

} // namespace Plugin
