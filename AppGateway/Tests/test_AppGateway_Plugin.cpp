#include "AppGateway.h"
#include "Registry.h"
#include "LaunchClient.h"
#include "Module.h"

#include <memory>
#include <string>

// Minimal Thunder stubs sufficient for Initialize/Deactivate path.
// We include only the minimal interface used in AppGateway.cpp: IShell, ISubSystem.
namespace WPEFramework {
namespace PluginHost {

class SubSystemStub : public ISubSystem {
public:
    SubSystemStub() = default;
    ~SubSystemStub() override = default;

    // Mark all queried subsystems as inactive unless toggled.
    bool IsActive(const subsystem type) const override {
        (void)type;
        return false;
    }
    void Set(const subsystem, ISubsystem*) override {}
    ISubsystem* Get(const subsystem) override { return nullptr; }
    const ISubsystem* Get(const subsystem) const override { return nullptr; }
    void Unset(const subsystem) override {}
    BEGIN_INTERFACE_MAP(SubSystemStub)
    INTERFACE_ENTRY(ISubSystem)
    END_INTERFACE_MAP
};

class ShellStub : public IShell {
public:
    ShellStub(const std::string& configLine, ISubSystem* subs)
        : _refCount(1), _config(configLine), _subs(subs) {}

    ~ShellStub() override = default;

    // IUnknown
    BEGIN_INTERFACE_MAP(ShellStub)
    INTERFACE_ENTRY(IShell)
    END_INTERFACE_MAP

    void AddRef() const override { ++_refCount; }
    uint32_t Release() const override {
        uint32_t result = 0;
        if (_refCount > 0) {
            result = --_refCount;
        }
        return result;
    }
    void* Aquire(const string&) override { return nullptr; }

    // IShell (we implement only methods used by AppGateway)
    string ConfigLine() const override { return _config; }
    ISubSystem* SubSystems() override { return _subs; }
    const ISubSystem* SubSystems() const override { return _subs; }

    // Unused methods - provide no-op/placeholder implementations
    void* QueryInterfaceByCallsign(const uint32_t, const string&, const uint32_t) override { return nullptr; }
    string VolatilePath() const override { return string(); }
    string PersistentPath() const override { return string(); }
    string DataPath() const override { return string(); }
    string ProxyStubPath() const override { return string(); }
    string SystemPath() const override { return string(); }
    string AppPath() const override { return string(); }
    string Hash() const override { return string(); }
    string ConfigFile() const override { return string(); }
    string Callsign() const override { return string(_T("AppGateway")); }
    string Locator() const override { return string(); }
    bool IsActive() const override { return true; }
    void EnableWebServer(const string&) override {}
    void DisableWebServer() override {}
    void Notify(const string&) override {}
    void Register(IPlugin::INotification*) override {}
    void Unregister(IPlugin::INotification*) override {}
    void Register(ISubSystem::INotification*) override {}
    void Unregister(ISubSystem::INotification*) override {}
    void Register(PluginHost::IStateControl::INotification*) override {}
    void Unregister(PluginHost::IStateControl::INotification*) override {}
    void Register(PluginHost::ISecurity*, const string&) override {}
    void Unregister(PluginHost::ISecurity*) override {}
    void Background(bool) override {}
    void Enable(const bool) override {}
    void Suspend(const bool) override {}
    state State() const override { return state::ACTIVATED; }
    pid_t ConnectorPID() const override { return 0; }
    IStateControl* QueryInterfaceByCallsign(const string&) override { return nullptr; }
    string Accessor() const override { return string(); }
    void* Root() override { return nullptr; }
    void Relinquish(const uint32_t, const string&, void*) override {}
    void* Instantiate(const uint32_t, const string&, const string&) override { return nullptr; }
    void Allow(const string&, const string&) override {}
    uint32_t Submit(const uint32_t, Core::IConfiguration*, const string&) override { return Core::ERROR_NONE; }
    string Substitute(const string&) const override { return string(); }
    uint8_t Major() const override { return 1; }
    uint8_t Minor() const override { return 0; }
    bool HasSubsystem(const ISubSystem::subsystem) const override { return false; }
    void ActivateProxy(const ProxyType) override {}
    void DeactivateProxy() override {}

private:
    mutable uint32_t _refCount;
    std::string _config;
    ISubSystem* _subs;
};

} // namespace PluginHost
} // namespace WPEFramework

using namespace Plugin;

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <gtest/gtest.h>
#define TTEST(TESTSUITE, NAME) TEST(TESTSUITE, NAME)
#define TEXPECT_TRUE(x) EXPECT_TRUE(x)
#define TEXPECT_FALSE(x) EXPECT_FALSE(x)
#define TEXPECT_EQ(a,b) EXPECT_EQ(a,b)
#else
#include "test_basic_runner.cpp"
#define TTEST(TESTSUITE, NAME) TEST(TESTSUITE, NAME)
#define TEXPECT_TRUE(x) EXPECT_TRUE(x)
#define TEXPECT_FALSE(x) EXPECT_FALSE(x)
#define TEXPECT_EQ(a,b) EXPECT_EQ(a,b)
#endif

// Helper to seed registry through a temporary file and point config at it.
static std::string createRegistryFile() {
    const std::string json = R"JSON(
{ "apps": [
  { "id":"com.example.VideoApp","name":"Video App","version":"2.3.1","categories":["video"],"capabilities":{"intents":["play"]} }
]}
)JSON";
    std::string path = "/tmp/appgateway_registry_plugin_test.json";
    std::ofstream ofs(path);
    ofs << json;
    ofs.close();
    return path;
}

TTEST(AppGatewayPluginTests, InitializeDeinitializeAndPing) {
    AppGateway plugin;
    WPEFramework::PluginHost::SubSystemStub subs;
    // Point registry source to file and provide a path; enable token check to exercise code path (no real check enforced in constructor).
    std::string regPath = createRegistryFile();
    std::string config = std::string("{\"registry\":{\"source\":\"file\",\"path\":\"") + regPath + "\"},\"enableTokenCheck\":false}";

    WPEFramework::PluginHost::ShellStub shell(config, &subs);
    auto initResult = plugin.Initialize(&shell);
    TEXPECT_TRUE(initResult.empty());

    Plugin::PingResult pingOut;
    Core::JSON::VariantContainer dummy;
    uint32_t rc = plugin.Ping(dummy, pingOut);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    TEXPECT_FALSE(pingOut.Time.Value().empty());

    plugin.Deinitialize(&shell);
}

TTEST(AppGatewayPluginTests, ListCapabilitiesLaunchStatusStop) {
    AppGateway plugin;
    WPEFramework::PluginHost::SubSystemStub subs;
    std::string regPath = createRegistryFile();
    std::string config = std::string("{\"registry\":{\"source\":\"file\",\"path\":\"") + regPath + "\"}}";
    WPEFramework::PluginHost::ShellStub shell(config, &subs);
    auto initResult = plugin.Initialize(&shell);
    TEXPECT_TRUE(initResult.empty());

    // List
    Plugin::AppsArray listOut;
    Core::JSON::VariantContainer noParams;
    uint32_t rc = plugin.List(noParams, listOut);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    TEXPECT_EQ(listOut.Apps.Length(), 1u);

    // GetCapabilities
    Plugin::CapabilitiesParams capIn;
    capIn.Id = "com.example.VideoApp";
    Plugin::CapabilitiesResult capOut;
    rc = plugin.GetCapabilities(capIn, capOut);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    const Core::JSON::Variant* intents = capOut.Capabilities.Get(_T("intents"));
    TEXPECT_TRUE(intents != nullptr);

    // Status for unknown should error if not launched yet: our LaunchClient returns UNKNOWN for absent states; AppGateway forwards return from State.
    Plugin::StatusParams statusIn;
    statusIn.Id = "unknown.id";
    Plugin::StatusResult statusOut;
    rc = plugin.Status(statusIn, statusOut);
    TEXPECT_EQ(rc, Core::ERROR_UNKNOWN_KEY);

    // Launch known id
    Plugin::LaunchParams launchIn;
    launchIn.Id = "com.example.VideoApp";
    launchIn.Intent = "play";
    Plugin::LaunchResult launchOut;
    rc = plugin.Launch(launchIn, launchOut);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    TEXPECT_FALSE(launchOut.Handle.Value().empty());
    // According to stub, immediate return reports 'launching' and event will set running afterwards.
    TEXPECT_EQ(launchOut.State.Value(), std::string(AppState::Launching));

    // Status after launch should be running
    statusIn.Id = "com.example.VideoApp";
    rc = plugin.Status(statusIn, statusOut);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    TEXPECT_EQ(statusOut.State.Value(), std::string(AppState::Running));
    TEXPECT_TRUE(statusOut.Pid.Value() > 0);

    // Stop
    Plugin::StopParams stopIn;
    stopIn.Id = "com.example.VideoApp";
    Plugin::StopResult stopOut;
    rc = plugin.Stop(stopIn, stopOut);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    TEXPECT_EQ(stopOut.State.Value(), std::string(AppState::Stopped));

    // Status after stop -> stopped and pid 0
    rc = plugin.Status(statusIn, statusOut);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    TEXPECT_EQ(statusOut.State.Value(), std::string(AppState::Stopped));
    TEXPECT_EQ(statusOut.Pid.Value(), 0u);

    plugin.Deinitialize(&shell);
}

TTEST(AppGatewayPluginTests, ErrorCodesForBadRequestsAndUnknownIds) {
    AppGateway plugin;
    WPEFramework::PluginHost::SubSystemStub subs;
    // Empty registry config: registry Find will return false -> UNKNOWN_KEY for launch/stop/getCapabilities
    std::string config = "{}";
    WPEFramework::PluginHost::ShellStub shell(config, &subs);
    auto initResult = plugin.Initialize(&shell);
    TEXPECT_TRUE(initResult.empty());

    // Launch without id -> BAD_REQUEST
    Plugin::LaunchParams launchInBad;
    Plugin::LaunchResult launchOut;
    uint32_t rc = plugin.Launch(launchInBad, launchOut);
    TEXPECT_EQ(rc, Core::ERROR_BAD_REQUEST);

    // Launch unknown id with empty registry -> UNKNOWN_KEY
    launchInBad.Id = "not.present";
    rc = plugin.Launch(launchInBad, launchOut);
    TEXPECT_EQ(rc, Core::ERROR_UNKNOWN_KEY);

    // Stop unknown id -> UNKNOWN_KEY
    Plugin::StopParams stopIn;
    stopIn.Id = "not.present";
    Plugin::StopResult stopOut;
    rc = plugin.Stop(stopIn, stopOut);
    TEXPECT_EQ(rc, Core::ERROR_UNKNOWN_KEY);

    // GetCapabilities unknown id -> UNKNOWN_KEY
    Plugin::CapabilitiesParams capIn;
    capIn.Id = "not.present";
    Plugin::CapabilitiesResult capOut;
    rc = plugin.GetCapabilities(capIn, capOut);
    TEXPECT_EQ(rc, Core::ERROR_UNKNOWN_KEY);

    // Status with missing id -> BAD_REQUEST
    Plugin::StatusParams statusInBad;
    Plugin::StatusResult statusOut;
    rc = plugin.Status(statusInBad, statusOut);
    TEXPECT_EQ(rc, Core::ERROR_BAD_REQUEST);

    plugin.Deinitialize(&shell);
}
