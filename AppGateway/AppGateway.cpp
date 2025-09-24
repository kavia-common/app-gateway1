#include "AppGateway.h"

namespace WPEFramework {
namespace Plugin {

    namespace {
        // Metadata registration enables Thunder to know the plugin interface version and capabilities.
        static Metadata<AppGateway> metadata(
            API_VERSION_NUMBER_MAJOR,
            API_VERSION_NUMBER_MINOR,
            API_VERSION_NUMBER_PATCH,
            // Preconditions, Terminations, Controls can be extended later if needed:
            {},
            {},
            {}
        );
    }

    SERVICE_REGISTRATION(AppGateway, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    AppGateway::AppGateway()
        : PluginHost::JSONRPC()
        , _service(nullptr)
        , _appManager(nullptr)
        , _connectionId(0)
        , _backendCallsign(_T("org.rdk.AppManager"))
        , _backendImplementation(_T("AppManagerImplementation"))
        , _returnPreloadErrorField(false)
        , _notification(*this)
    {
    }

    AppGateway::~AppGateway() {
        // Ensure resources are released if Deinitialize was not called (defensive).
        // Thunder host should call Deinitialize(), but we guard anyway.
        if (_appManager != nullptr) {
            _appManager->Release();
            _appManager = nullptr;
        }
    }

    const string AppGateway::Initialize(PluginHost::IShell* service) {
        ASSERT(service != nullptr);

        _service = service;
        _service->AddRef();

        // Load plugin configuration
        Config config;
        config.FromString(_service->ConfigLine());

        if (config.BackendCallsign.IsSet() && (config.BackendCallsign.Value().empty() == false)) {
            _backendCallsign = config.BackendCallsign.Value();
        }
        if (config.BackendImplementation.IsSet() && (config.BackendImplementation.Value().empty() == false)) {
            _backendImplementation = config.BackendImplementation.Value();
        }
        _returnPreloadErrorField = config.ReturnPreloadErrorField.Value();

        // Acquire backend: Preferred is by callsign to an existing plugin exposing Exchange::IAppManager
        _appManager = _service->QueryInterfaceByCallsign<Exchange::IAppManager>(_backendCallsign.c_str());

        // Fallback: Try to acquire via direct implementation root (spawns/attaches to implementation)
        if (_appManager == nullptr) {
            // Root<T> fills _connectionId if the implementation is out-of-process.
            _appManager = _service->Root<Exchange::IAppManager>(_connectionId, 2000, _backendImplementation.c_str());
        }

        if (_appManager == nullptr) {
            string message = _T("AppGateway: Failed to acquire Exchange::IAppManager (callsign=");
            message += _backendCallsign.c_str();
            message += _T(", implementation=");
            message += _backendImplementation.c_str();
            message += _T(")");
            return message;
        }

        // If we have an out-of-process connection, register for remote connection notifications
        if (_connectionId != 0) {
            _service->Register(&_notification);
        }

        // Register JSON-RPC methods.
        // Lifecycle
        Register<PreloadParams, void>(_T("preloadApp"), &AppGateway::preloadApp, this);
        Register<LaunchParams, void>(_T("launchApp"), &AppGateway::launchApp, this);
        Register<AppIdParam, void>(_T("closeApp"), &AppGateway::closeApp, this);
        Register<AppIdParam, void>(_T("terminateApp"), &AppGateway::terminateApp, this);

        // Listings
        Register<Core::JSON::VariantContainer, Core::JSON::String>(_T("getLoadedApps"), &AppGateway::getLoadedApps, this);

        // Properties
        Register<AppPropertySetParams, void>(_T("setAppProperty"), &AppGateway::setAppProperty, this);
        Register<AppPropertyGetParams, Core::JSON::String>(_T("getAppProperty"), &AppGateway::getAppProperty, this);

        // Intents
        Register<IntentParams, void>(_T("sendIntent"), &AppGateway::sendIntent, this);

        // Note: AppGateway intentionally does not produce events; see AppNotifications plugin for events.

        return string();
    }

    void AppGateway::Deinitialize(PluginHost::IShell* service) {
        ASSERT(service == _service);

        // Unregister JSON-RPC methods (safe if not registered)
        Unregister(_T("preloadApp"));
        Unregister(_T("launchApp"));
        Unregister(_T("closeApp"));
        Unregister(_T("terminateApp"));
        Unregister(_T("getLoadedApps"));
        Unregister(_T("setAppProperty"));
        Unregister(_T("getAppProperty"));
        Unregister(_T("sendIntent"));

        // Deregister remote notifications if used
        if (_connectionId != 0) {
            _service->Unregister(&_notification);
        }

        // Release backend interface
        if (_appManager != nullptr) {
            _appManager->Release();
            _appManager = nullptr;
        }

        // Terminate remote connection, if applicable
        if (_connectionId != 0) {
            RPC::IRemoteConnection* connection = _service->RemoteConnection(_connectionId);
            if (connection != nullptr) {
                // Terminate the remote end if it's still active
                connection->Terminate();
                connection->Release();
            }
            _connectionId = 0;
        }

        if (_service != nullptr) {
            _service->Release();
            _service = nullptr;
        }
    }

    string AppGateway::Information() const {
        // Optional: return a short info string.
        return string(_T("AppGateway plugin: Application lifecycle & intent facade over Exchange::IAppManager"));
    }

    uint32_t AppGateway::ValidateNonEmptyAppId(const Core::JSON::String& appId) const {
        return (IsEmpty(appId) ? Core::ERROR_BAD_REQUEST : Core::ERROR_NONE);
    }

    uint32_t AppGateway::preloadApp(const PreloadParams& params) {
        RETURN_IF(_appManager == nullptr, Core::ERROR_UNAVAILABLE);

        // Validate input
        uint32_t status = ValidateNonEmptyAppId(params.AppId);
        if (status != Core::ERROR_NONE) {
            return status;
        }

        // Map to Exchange::IAppManager::PreloadApp
        // If an error string is returned alongside success, we may optionally log it.
        string backendError;
        status = _appManager->PreloadApp(params.AppId.Value(), params.LaunchArgs.Value(), backendError);

        if ((status == Core::ERROR_NONE) && _returnPreloadErrorField && (backendError.empty() == false)) {
            // We opted not to change the result schema (which is null) in this implementation.
            // For observability, log the backend error detail.
            SYSLOG(Warning, (_T("PreloadApp reported detail: %s"), backendError.c_str()));
        }

        return status;
    }

    uint32_t AppGateway::launchApp(const LaunchParams& params) {
        RETURN_IF(_appManager == nullptr, Core::ERROR_UNAVAILABLE);

        uint32_t status = ValidateNonEmptyAppId(params.AppId);
        if (status != Core::ERROR_NONE) {
            return status;
        }

        const string appId = params.AppId.Value();
        const string intent = params.Intent.IsSet() ? params.Intent.Value() : string();
        const string launchArgs = params.LaunchArgs.IsSet() ? params.LaunchArgs.Value() : string();

        status = _appManager->LaunchApp(appId, intent, launchArgs);
        return status;
    }

    uint32_t AppGateway::closeApp(const AppIdParam& params) {
        RETURN_IF(_appManager == nullptr, Core::ERROR_UNAVAILABLE);

        uint32_t status = ValidateNonEmptyAppId(params.AppId);
        if (status != Core::ERROR_NONE) {
            return status;
        }

        status = _appManager->CloseApp(params.AppId.Value());
        return status;
    }

    uint32_t AppGateway::terminateApp(const AppIdParam& params) {
        RETURN_IF(_appManager == nullptr, Core::ERROR_UNAVAILABLE);

        uint32_t status = ValidateNonEmptyAppId(params.AppId);
        if (status != Core::ERROR_NONE) {
            return status;
        }

        status = _appManager->TerminateApp(params.AppId.Value());
        return status;
    }

    uint32_t AppGateway::getLoadedApps(const Core::JSON::VariantContainer& /*params*/, Core::JSON::String& result) {
        RETURN_IF(_appManager == nullptr, Core::ERROR_UNAVAILABLE);

        string jsonApps;
        uint32_t status = _appManager->GetLoadedApps(jsonApps);
        if (status == Core::ERROR_NONE) {
            result = jsonApps;
        }
        return status;
    }

    uint32_t AppGateway::setAppProperty(const AppPropertySetParams& params) {
        RETURN_IF(_appManager == nullptr, Core::ERROR_UNAVAILABLE);

        // Validate parameters
        if (IsEmpty(params.AppId) || IsEmpty(params.Key) || IsEmpty(params.Value)) {
            return Core::ERROR_BAD_REQUEST;
        }

        uint32_t status = _appManager->SetAppProperty(params.AppId.Value(), params.Key.Value(), params.Value.Value());
        return status;
    }

    uint32_t AppGateway::getAppProperty(const AppPropertyGetParams& params, Core::JSON::String& result) {
        RETURN_IF(_appManager == nullptr, Core::ERROR_UNAVAILABLE);

        // Validate parameters
        if (IsEmpty(params.AppId) || IsEmpty(params.Key)) {
            return Core::ERROR_BAD_REQUEST;
        }

        string value;
        uint32_t status = _appManager->GetAppProperty(params.AppId.Value(), params.Key.Value(), value);
        if (status == Core::ERROR_NONE) {
            result = value;
        }
        return status;
    }

    uint32_t AppGateway::sendIntent(const IntentParams& params) {
        RETURN_IF(_appManager == nullptr, Core::ERROR_UNAVAILABLE);

        if (IsEmpty(params.AppId) || IsEmpty(params.Intent)) {
            return Core::ERROR_BAD_REQUEST;
        }

        uint32_t status = _appManager->SendIntent(params.AppId.Value(), params.Intent.Value());
        return status;
    }

    void AppGateway::Notification::Deactivated(RPC::IRemoteConnection* connection) {
        // Called by Thunder when an out-of-process implementation terminates.
        ASSERT(connection != nullptr);
        if (connection != nullptr) {
            const uint32_t id = connection->Id();
            if ((id != 0) && (id == _parent._connectionId)) {
                SYSLOG(Warning, (_T("AppGateway: Backend remote connection %u deactivated."), id));
                // Clear connection tracking; Deinitialize will handle full cleanup.
                _parent._connectionId = 0;
            }
        }
    }

} // namespace Plugin
} // namespace WPEFramework
