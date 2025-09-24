#pragma once
/*
    Test override of WPEFramework::Exchange::IAppManager to match AppGateway plugin expectations for unit/integration tests.

    This header intentionally diverges from the installed interfaces/IAppManager.h to provide the methods
    used by AppGateway:
      - PreloadApp(appId, launchArgs, backendErrorOut)
      - GetLoadedApps(string& result)
      - GetAppProperty(appId, key, string& valueOut)
      - SetAppProperty(appId, key, value)
      - LaunchApp, CloseApp, TerminateApp
      - SendIntent(appId, intent)
*/

#include <WPEFramework/core/core.h>
#include <WPEFramework/com/Ids.h>

namespace WPEFramework {
namespace Exchange {

    // PUBLIC_INTERFACE
    struct EXTERNAL IAppManager : virtual public Core::IUnknown {
        /** Interface ID is aligned with standard Thunder IDs. */
        enum { ID = RPC::ID_APP_MANAGER };

        // Minimal notification interface placeholder (not used by tests)
        struct EXTERNAL INotification : virtual public Core::IUnknown {
            enum { ID = RPC::ID_APP_MANAGER_NOTIFICATION };
            virtual void OnAppInstalled(const string&, const string&) = 0;
            virtual void OnAppUninstalled(const string&) = 0;
            virtual void OnAppStateChanged(const string&, const string&) = 0;
            virtual void OnAppLaunchRequest(const string&, const string&, const string&) = 0;
        };

        // PUBLIC_INTERFACE
        virtual uint32_t PreloadApp(const string& appId, const string& launchArgs, string& backendError) = 0;

        // PUBLIC_INTERFACE
        virtual uint32_t LaunchApp(const string& appId, const string& intent, const string& launchArgs) = 0;

        // PUBLIC_INTERFACE
        virtual uint32_t CloseApp(const string& appId) = 0;

        // PUBLIC_INTERFACE
        virtual uint32_t TerminateApp(const string& appId) = 0;

        // PUBLIC_INTERFACE
        virtual uint32_t GetLoadedApps(string& result /* opaque JSON string */) = 0;

        // PUBLIC_INTERFACE
        virtual uint32_t SetAppProperty(const string& appId, const string& key, const string& value) = 0;

        // PUBLIC_INTERFACE
        virtual uint32_t GetAppProperty(const string& appId, const string& key, string& value /* out */) = 0;

        // PUBLIC_INTERFACE
        virtual uint32_t SendIntent(const string& appId, const string& intent) = 0;
    };

} // namespace Exchange
} // namespace WPEFramework
