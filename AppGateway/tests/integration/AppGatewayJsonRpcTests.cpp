#include "MockShell.h"
#include "MockAppManager.h"
#include "../../AppGateway.h"

#include <WPEFramework/core/core.h>
#include <WPEFramework/plugins/JSONRPC.h>
#include <cstdio>
#include <cstdlib>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using namespace WPEFramework::Testing;

static int failed = 0;
static int total = 0;

#define CHECK_TRUE(cond, msg) do { total++; if(!(cond)) { failed++; printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } } while(0)
#define CHECK_EQ(a,b,msg) do { total++; if(!((a)==(b))) { failed++; printf("FAIL: %s (expected %d, got %d) at %s:%d\n", msg, (int)(b), (int)(a), __FILE__, __LINE__); } } while(0)

static uint32_t Invoke(JSONRPC& rpc, const string& method, const string& params, string& response) {
    return rpc.Invoke(777 /*channel*/, 1001 /*id*/, string() /*token*/, method, params, response);
}

int main() {
    auto* shell = new MockShell();
    auto* backend = new MockAppManager();
    shell->Backend(backend);

    // Provide config to exercise returnPreloadErrorField (even though plugin only logs)
    shell->Config(R"({"backendCallsign":"org.rdk.AppManager","backendImplementation":"AppManagerImplementation","returnPreloadErrorField":true})");

    AppGateway plugin;
    string init = plugin.Initialize(shell);
    CHECK_TRUE(init.empty(), "Initialize should succeed");

    JSONRPC& rpc = plugin;

    // Activate JSONRPC with shell to allow callsign-annotated JSON-RPC method names
    rpc.Activate(shell);

    // Method exists check
    {
        string response;
        uint32_t code = Invoke(rpc, "exists", R"("launchApp")", response);
        CHECK_EQ(code, Core::ERROR_NONE, "exists call should return ERROR_NONE");
        // response "0" indicates Core::ERROR_NONE presence per JSONRPC::Invoke
        CHECK_TRUE(response == "0", "exists should report method exists");
    }

    // Callsign-qualified invocation
    {
        string response;
        uint32_t code = Invoke(rpc, "org.rdk.AppGateway.1.launchApp", R"({"appId":"com.example.C","intent":"play","launchArgs":"{}"})", response);
        CHECK_EQ(code, Core::ERROR_NONE, "Callsign-qualified launchApp should succeed");
    }

    // Event register/unregister should succeed but no events are emitted by plugin (per design)
    {
        // Subscribe to a dummy event name; expect registration success (response "0")
        Core::JSON::String callsign; callsign = "org.rdk.AppGateway";
        Core::JSON::String event; event = "onAppLifecycleStateChanged";
        PluginHost::JSONRPC::Registration reg;
        reg.Event = event;
        reg.Callsign = callsign;
        string regPayload; reg.ToString(regPayload);

        string response;
        uint32_t code = Invoke(rpc, "register", regPayload, response);
        CHECK_EQ(code, Core::ERROR_NONE, "register should succeed via JSONRPC base");
        CHECK_TRUE(response == "0", "register returns 0 on success");

        // Unregister
        string response2;
        code = Invoke(rpc, "unregister", regPayload, response2);
        CHECK_EQ(code, Core::ERROR_NONE, "unregister should succeed via JSONRPC base");
        CHECK_TRUE(response2 == "0", "unregister returns 0 on success");
    }

    // Error path propagation from backend
    {
        backend->launchReturnCode = Core::ERROR_GENERAL;
        string response;
        uint32_t code = Invoke(rpc, "org.rdk.AppGateway.1.launchApp", R"({"appId":"com.example.D"})", response);
        CHECK_EQ(code, Core::ERROR_GENERAL, "Backend error should propagate through JSON-RPC");
        backend->launchReturnCode = Core::ERROR_NONE;
    }

    // getLoadedApps shape check (opaque string wrapped)
    {
        backend->_loadedApps = { "a1", "a2", "a3" };
        string response;
        uint32_t code = Invoke(rpc, "org.rdk.AppGateway.1.getLoadedApps", "{}", response);
        CHECK_EQ(code, Core::ERROR_NONE, "getLoadedApps ok");
        CHECK_TRUE(response.find(R"(["a1","a2","a3"])") != string::npos, "getLoadedApps returns quoted backend JSON string");
    }

    plugin.Deinitialize(shell);
    shell->Release();
    backend->Release();

    printf("Integration tests completed: %d total, %d failed\n", total, failed);
    return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
