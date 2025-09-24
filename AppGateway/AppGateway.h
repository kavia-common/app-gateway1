#pragma once

#include "Module.h"
#include <interfaces/IAppManager.h>

namespace WPEFramework {
namespace Plugin {

    // API version information for SERVICE_REGISTRATION and Metadata
    constexpr uint8_t API_VERSION_NUMBER_MAJOR = 1;
    constexpr uint8_t API_VERSION_NUMBER_MINOR = 0;
    constexpr uint8_t API_VERSION_NUMBER_PATCH = 0;

    class AppGateway final
        : public PluginHost::IPlugin
        , public PluginHost::JSONRPC {
    public:
        AppGateway(const AppGateway&) = delete;
        AppGateway& operator=(const AppGateway&) = delete;

        AppGateway();
        ~AppGateway() override;

        // PUBLIC_INTERFACE
        /**
         * Initialize the AppGateway Thunder plugin.
         * Reads configuration, acquires Exchange::IAppManager backend, and registers JSON-RPC methods.
         *
         * @param service IShell pointer provided by the Thunder host.
         * @return string Empty on success, error message on failure.
         */
        const string Initialize(PluginHost::IShell* service) override;

        // PUBLIC_INTERFACE
        /**
         * Deinitialize the AppGateway Thunder plugin.
         * Unregisters JSON-RPC methods, releases backend references, and terminates remote connection if any.
         *
         * @param service IShell pointer provided by the Thunder host.
         */
        void Deinitialize(PluginHost::IShell* service) override;

        // PUBLIC_INTERFACE
        /**
         * Returns plugin information string.
         * @return string Human-readable information text (may be empty).
         */
        string Information() const override;

    private:
        class Notification;

        // Configuration model for the plugin
        class Config : public Core::JSON::Container {
        public:
            Config(const Config&) = delete;
            Config& operator=(const Config&) = delete;

            Config()
                : Core::JSON::Container()
                , BackendCallsign(_T("org.rdk.AppManager"))
                , BackendImplementation(_T("AppManagerImplementation"))
                , ReturnPreloadErrorField(false)
            {
                Add(_T("backendCallsign"), &BackendCallsign);
                Add(_T("backendImplementation"), &BackendImplementation);
                Add(_T("returnPreloadErrorField"), &ReturnPreloadErrorField);
            }

            ~Config() override = default;

            Core::JSON::String BackendCallsign;           // default "org.rdk.AppManager"
            Core::JSON::String BackendImplementation;     // default "AppManagerImplementation"
            Core::JSON::Boolean ReturnPreloadErrorField;  // default false
        };

        // JSON models for method parameters
        class AppIdParam : public Core::JSON::Container {
        public:
            AppIdParam()
                : Core::JSON::Container()
            {
                Add(_T("appId"), &AppId);
            }
            ~AppIdParam() override = default;

            Core::JSON::String AppId; // required
        };

        class PreloadParams : public Core::JSON::Container {
        public:
            PreloadParams()
                : Core::JSON::Container()
            {
                Add(_T("appId"), &AppId);
                Add(_T("launchArgs"), &LaunchArgs);
            }
            ~PreloadParams() override = default;

            Core::JSON::String AppId;      // required
            Core::JSON::String LaunchArgs; // optional
        };

        class LaunchParams : public Core::JSON::Container {
        public:
            LaunchParams()
                : Core::JSON::Container()
            {
                Add(_T("appId"), &AppId);
                Add(_T("intent"), &Intent);
                Add(_T("launchArgs"), &LaunchArgs);
            }
            ~LaunchParams() override = default;

            Core::JSON::String AppId;      // required
            Core::JSON::String Intent;     // optional
            Core::JSON::String LaunchArgs; // optional
        };

        class AppPropertySetParams : public Core::JSON::Container {
        public:
            AppPropertySetParams()
                : Core::JSON::Container()
            {
                Add(_T("appId"), &AppId);
                Add(_T("key"), &Key);
                Add(_T("value"), &Value);
            }
            ~AppPropertySetParams() override = default;

            Core::JSON::String AppId;  // required
            Core::JSON::String Key;    // required
            Core::JSON::String Value;  // required
        };

        class AppPropertyGetParams : public Core::JSON::Container {
        public:
            AppPropertyGetParams()
                : Core::JSON::Container()
            {
                Add(_T("appId"), &AppId);
                Add(_T("key"), &Key);
            }
            ~AppPropertyGetParams() override = default;

            Core::JSON::String AppId;  // required
            Core::JSON::String Key;    // required
        };

        class IntentParams : public Core::JSON::Container {
        public:
            IntentParams()
                : Core::JSON::Container()
            {
                Add(_T("appId"), &AppId);
                Add(_T("intent"), &Intent);
            }
            ~IntentParams() override = default;

            Core::JSON::String AppId;   // required
            Core::JSON::String Intent;  // required
        };

        // JSON-RPC Handlers

        // PUBLIC_INTERFACE
        /**
         * JSON-RPC: preloadApp
         * Preload an application without activating it.
         * Params: { appId: string, launchArgs?: string }
         * Result: null on success (no typed result).
         */
        uint32_t preloadApp(const PreloadParams& params);

        // PUBLIC_INTERFACE
        /**
         * JSON-RPC: launchApp
         * Launch an application (optionally with intent and launchArgs).
         * Params: { appId: string, intent?: string, launchArgs?: string }
         * Result: null on success.
         */
        uint32_t launchApp(const LaunchParams& params);

        // PUBLIC_INTERFACE
        /**
         * JSON-RPC: closeApp
         * Gracefully close an application.
         * Params: { appId: string }
         * Result: null on success.
         */
        uint32_t closeApp(const AppIdParam& params);

        // PUBLIC_INTERFACE
        /**
         * JSON-RPC: terminateApp
         * Forcefully terminate an application.
         * Params: { appId: string }
         * Result: null on success.
         */
        uint32_t terminateApp(const AppIdParam& params);

        // PUBLIC_INTERFACE
        /**
         * JSON-RPC: getLoadedApps
         * Retrieve the list of loaded applications.
         * Params: {}
         * Result: string (opaque JSON string from backend)
         */
        uint32_t getLoadedApps(const Core::JSON::VariantContainer& /*params*/, Core::JSON::String& result);

        // PUBLIC_INTERFACE
        /**
         * JSON-RPC: setAppProperty
         * Set a property on an application.
         * Params: { appId: string, key: string, value: string }
         * Result: null on success.
         */
        uint32_t setAppProperty(const AppPropertySetParams& params);

        // PUBLIC_INTERFACE
        /**
         * JSON-RPC: getAppProperty
         * Get a property value for an application.
         * Params: { appId: string, key: string }
         * Result: string (property value)
         */
        uint32_t getAppProperty(const AppPropertyGetParams& params, Core::JSON::String& result);

        // PUBLIC_INTERFACE
        /**
         * JSON-RPC: sendIntent
         * Deliver an intent to a target application.
         * Params: { appId: string, intent: string }
         * Result: null on success.
         */
        uint32_t sendIntent(const IntentParams& params);

        // Helper utilities
        inline bool IsEmpty(const Core::JSON::String& s) const {
            return (s.IsSet() == false) || (s.Value().empty());
        }

        uint32_t ValidateNonEmptyAppId(const Core::JSON::String& appId) const;

        // Internal state
        PluginHost::IShell* _service;
        Exchange::IAppManager* _appManager;
        uint32_t _connectionId;

        // Configuration fields (cached)
        Core::string _backendCallsign;
        Core::string _backendImplementation;
        bool _returnPreloadErrorField;

        // Remote connection notification sink
        class Notification : public RPC::IRemoteConnection::INotification {
        public:
            explicit Notification(AppGateway& parent)
                : _parent(parent) {
            }

            Notification(const Notification&) = delete;
            Notification& operator=(const Notification&) = delete;

            // PUBLIC_INTERFACE
            /**
             * RPC remote connection deactivation notification.
             * If the backend remote connection ends, the plugin may log and clear connection tracking.
             */
            void Deactivated(RPC::IRemoteConnection* connection) override;

            void Activated(RPC::IRemoteConnection* /*connection*/) override { /* not used */ }

        private:
            AppGateway& _parent;
        };

        Core::Sink<Notification> _notification;
    };

} // namespace Plugin
} // namespace WPEFramework
