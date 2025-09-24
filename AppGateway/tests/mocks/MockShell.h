#pragma once

#include <WPEFramework/plugins/IShell.h>
#include <WPEFramework/core/core.h>
#include <WPEFramework/plugins/JSONRPC.h>
#include "MockAppManager.h"

namespace WPEFramework {
namespace Testing {

    class MockRemoteConnection final : public RPC::IRemoteConnection {
    public:
        MockRemoteConnection(uint32_t id) : _id(id) {}
        uint32_t Id() const override { return _id; }
        virtual string Caller() const override { return string(); }
        virtual string Callee() const override { return string(); }
        void Terminate() override { /* no-op */ }
        void* Aquire(const uint32_t, const uint32_t, const uint32_t) override { return nullptr; }
        void Release() const override { delete this; }
    private:
        uint32_t _id { 0 };
    };

    class MockShell final : public PluginHost::IShell {
    public:
        MockShell()
            : _refCount(1)
            , _callsign(_T("org.rdk.AppGateway"))
            , _backendCallsign(_T("org.rdk.AppManager"))
        {
        }

        void Backend(Exchange::IAppManager* mgr) {
            _appManager = mgr;
            if (_appManager) { _appManager->AddRef(); }
        }
        void BackendCallsign(const string& cs) { _backendCallsign = cs; }
        void Config(const string& config) { _config = config; }

        // Core::IUnknown
        uint32_t AddRef() const override {
            return Core::InterlockedIncrement(const_cast<uint32_t&>(_refCount));
        }
        uint32_t Release() const override {
            uint32_t result = Core::InterlockedDecrement(const_cast<uint32_t&>(_refCount));
            if (result == 0) {
                delete this;
            }
            return result;
        }
        void* QueryInterface(const uint32_t /*id*/) override {
            return nullptr;
        }

        // WebServer stubs
        void EnableWebServer(const string&, const string&) override {}
        void DisableWebServer() override {}

        string Model() const override { return _T("MockModel"); }
        bool Background() const override { return false; }
        string Accessor() const override { return string(); }
        string WebPrefix() const override { return string(); }
        string Locator() const override { return string(); }
        string ClassName() const override { return _T("AppGateway"); }
        string Versions() const override { return _T("[1]"); }
        string Callsign() const override { return _callsign; }
        string PersistentPath() const override { return string(); }
        string VolatilePath() const override { return string(); }
        string DataPath() const override { return string(); }
        string ProxyStubPath() const override { return string(); }
        string SystemPath() const override { return string(); }
        string PluginPath() const override { return string(); }
        string SystemRootPath() const override { return string(); }
        Core::hresult SystemRootPath(const string&) override { return Core::ERROR_NONE; }
        PluginHost::IShell::startup Startup() const override { return PluginHost::IShell::startup::ACTIVATED; }
        Core::hresult Startup(const startup) override { return Core::ERROR_NONE; }
        string Substitute(const string& input) const override { return input; }
        bool Resumed() const override { return false; }
        Core::hresult Resumed(const bool) override { return Core::ERROR_NONE; }
        string HashKey() const override { return string(); }
        string ConfigLine() const override { return _config; }
        Core::hresult ConfigLine(const string& cfg) override { _config = cfg; return Core::ERROR_NONE; }
        Core::hresult Metadata(string& /*info*/) const override { return Core::ERROR_NONE; }
        bool IsSupported(const uint8_t /*version*/) const override { return true; }
        ISubSystem* SubSystems() override { return nullptr; }
        void Notify(const string&) override {}

        void Register(IPlugin::INotification* /*sink*/) override {}
        void Unregister(IPlugin::INotification* /*sink*/) override {}

        state State() const override { return state::ACTIVATED; }

        void* /* @interface:id */ QueryInterfaceByCallsign(const uint32_t id, const string& name) override {
            if ((id == Exchange::IAppManager::ID) && (name == _backendCallsign) && (_appManager != nullptr)) {
                _appManager->AddRef();
                return _appManager;
            }
            return nullptr;
        }

        Core::hresult Activate(const reason) override { return Core::ERROR_NONE; }
        Core::hresult Deactivate(const reason) override { return Core::ERROR_NONE; }
        Core::hresult Unavailable(const reason) override { return Core::ERROR_NONE; }
        Core::hresult Hibernate(const uint32_t) override { return Core::ERROR_NONE; }
        reason Reason() const override { return reason::REQUESTED; }

        // JSON responses are delivered via this in real host; here we just sink them.
        uint32_t Submit(const uint32_t /*Id*/, const Core::ProxyType<Core::JSON::IElement>& /*response*/) override {
            return Core::ERROR_NONE;
        }

        // Simple COMLink stub (not used in tests; Root fallback path is avoided by returning backend in QueryInterfaceByCallsign).
        class SimpleCOMLink final : public ICOMLink {
        public:
            void Register(RPC::IRemoteConnection::INotification* /*sink*/) override {}
            void Unregister(const RPC::IRemoteConnection::INotification* /*sink*/) override {}
            void Register(INotification* /*sink*/) override {}
            void Unregister(INotification* /*sink*/) override {}
            RPC::IRemoteConnection* RemoteConnection(const uint32_t connectionId) override {
                return (connectionId != 0 ? new MockRemoteConnection(connectionId) : nullptr);
            }
            void* Instantiate(const RPC::Object& /*object*/, const uint32_t /*waitTime*/, uint32_t& connectionId) override {
                connectionId = 0;
                return nullptr;
            }
        };

        ICOMLink* COMLink() override { return &_comLink; }

    private:
        mutable uint32_t _refCount;
        string _callsign;
        string _backendCallsign;
        string _config;
        Exchange::IAppManager* _appManager { nullptr };
        SimpleCOMLink _comLink;
    };

} // namespace Testing
} // namespace WPEFramework
