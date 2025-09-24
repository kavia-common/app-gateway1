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

static int g_failed = 0;
static int g_total = 0;

#define EXPECT_TRUE(cond, msg) do { g_total++; if(!(cond)) { g_failed++; printf("FAIL: %s (at %s:%d)\n", msg, __FILE__, __LINE__); } } while(0)
#define EXPECT_EQ(a,b,msg) do { g_total++; if(!((a)==(b))) { g_failed++; printf("FAIL: %s (expected %d, got %d) at %s:%d\n", msg, (int)(b), (int)(a), __FILE__, __LINE__); } } while(0)
#define EXPECT_STR_EQ(a,b,msg) do { g_total++; if(!((a)==(b))) { g_failed++; printf("FAIL: %s (expected '%s', got '%s') at %s:%d\n", msg, b.c_str(), a.c_str(), __FILE__, __LINE__); } } while(0)

static uint32_t Invoke(JSONRPC& rpc, const string& method, const string& params, string& response) {
    // Use Local dispatcher invoke
    return rpc.Invoke(1 /*channel*/, 1 /*id*/, string() /*token*/, method, params, response);
}

int main() {
    // Prepare mock shell and backend
    auto* shell = new MockShell();
    auto* backend = new MockAppManager();
    shell->Backend(backend);

    // Test Initialize success path
    AppGateway plugin;
    string initResult = plugin.Initialize(shell);
    EXPECT_TRUE(initResult.empty(), "Initialize() should succeed with backend available");

    JSONRPC& rpc = plugin; // access JSON-RPC dispatcher from plugin

    // Negative: preloadApp missing appId -> BAD_REQUEST
    {
        string response;
        uint32_t code = Invoke(rpc, "preloadApp", "{}", response);
        EXPECT_EQ(code, Core::ERROR_BAD_REQUEST, "preloadApp without appId should return BAD_REQUEST");
    }

    // Positive: preloadApp with appId and launchArgs
    {
        backend->simulatedPreloadError = _T("backend detail");
        string response;
        uint32_t code = Invoke(rpc, "preloadApp", R"({"appId":"com.example.A","launchArgs":"{\"foo\":1}"})", response);
        EXPECT_EQ(code, Core::ERROR_NONE, "preloadApp should return ERROR_NONE");
        EXPECT_TRUE(backend->lastPreload.appId == "com.example.A", "backend.PreloadApp should be called with appId");
    }

    // launchApp: missing appId -> BAD_REQUEST
    {
        string response;
        uint32_t code = Invoke(rpc, "launchApp", R"({"intent":"watch","launchArgs":"{}"})", response);
        EXPECT_EQ(code, Core::ERROR_BAD_REQUEST, "launchApp without appId should return BAD_REQUEST");
    }

    // launchApp: present appId only
    {
        string response;
        uint32_t code = Invoke(rpc, "launchApp", R"({"appId":"com.example.A"})", response);
        EXPECT_EQ(code, Core::ERROR_NONE, "launchApp should succeed with appId only");
        EXPECT_TRUE(backend->lastLaunch.appId == "com.example.A", "LaunchApp backend called");
        EXPECT_TRUE(backend->lastLaunch.intent.empty(), "intent should be empty if not supplied");
        EXPECT_TRUE(backend->lastLaunch.launchArgs.empty(), "launchArgs should be empty if not supplied");
    }

    // closeApp: missing appId -> BAD_REQUEST
    {
        string response;
        uint32_t code = Invoke(rpc, "closeApp", "{}", response);
        EXPECT_EQ(code, Core::ERROR_BAD_REQUEST, "closeApp missing appId should be BAD_REQUEST");
    }

    // closeApp: ok
    {
        string response;
        uint32_t code = Invoke(rpc, "closeApp", R"({"appId":"com.example.A"})", response);
        EXPECT_EQ(code, Core::ERROR_NONE, "closeApp should return ERROR_NONE");
        EXPECT_TRUE(backend->lastClose.appId == "com.example.A", "CloseApp backend called");
    }

    // terminateApp: missing appId -> BAD_REQUEST
    {
        string response;
        uint32_t code = Invoke(rpc, "terminateApp", "{}", response);
        EXPECT_EQ(code, Core::ERROR_BAD_REQUEST, "terminateApp missing appId should be BAD_REQUEST");
    }

    // terminateApp: ok
    {
        string response;
        uint32_t code = Invoke(rpc, "terminateApp", R"({"appId":"com.example.B"})", response);
        EXPECT_EQ(code, Core::ERROR_NONE, "terminateApp should return ERROR_NONE");
        EXPECT_TRUE(backend->lastTerminate.appId == "com.example.B", "TerminateApp backend called");
    }

    // getLoadedApps: returns opaque JSON string inside JSON string result
    {
        backend->_loadedApps = { "app.x", "app.y" };
        string response;
        uint32_t code = Invoke(rpc, "getLoadedApps", "{}", response);
        EXPECT_EQ(code, Core::ERROR_NONE, "getLoadedApps should return ERROR_NONE");
        // response is Core::JSON::String which serializes a JSON string -> quoted JSON
        EXPECT_TRUE(response.find(R"(["app.x","app.y"])") != string::npos, "getLoadedApps should wrap backend JSON string");
    }

    // setAppProperty: missing params -> BAD_REQUEST
    {
        string response;
        uint32_t code = Invoke(rpc, "setAppProperty", R"({"appId":"a","key":""})", response);
        EXPECT_EQ(code, Core::ERROR_BAD_REQUEST, "setAppProperty with empty key should be BAD_REQUEST");
    }

    // setAppProperty + getAppProperty positive
    {
        string response;
        uint32_t code = Invoke(rpc, "setAppProperty", R"({"appId":"a","key":"k","value":"v"})", response);
        EXPECT_EQ(code, Core::ERROR_NONE, "setAppProperty ok");

        string response2;
        code = Invoke(rpc, "getAppProperty", R"({"appId":"a","key":"k"})", response2);
        EXPECT_EQ(code, Core::ERROR_NONE, "getAppProperty ok");

        // response2 is a JSON-encoded string, so expect quotes around "v"
        EXPECT_TRUE(response2.find("\"v\"") != string::npos, "getAppProperty should return quoted value");
    }

    // getAppProperty: missing fields -> BAD_REQUEST
    {
        string response;
        uint32_t code = Invoke(rpc, "getAppProperty", R"({"appId":"a"})", response);
        EXPECT_EQ(code, Core::ERROR_BAD_REQUEST, "getAppProperty missing key should be BAD_REQUEST");
    }

    // sendIntent: missing params -> BAD_REQUEST
    {
        string response;
        uint32_t code = Invoke(rpc, "sendIntent", R"({"appId":"a"})", response);
        EXPECT_EQ(code, Core::ERROR_BAD_REQUEST, "sendIntent missing intent should be BAD_REQUEST");
    }

    // sendIntent: ok
    {
        string response;
        uint32_t code = Invoke(rpc, "sendIntent", R"({"appId":"a","intent":"test.intent"})", response);
        EXPECT_EQ(code, Core::ERROR_NONE, "sendIntent ok");
        EXPECT_TRUE(backend->lastIntent.appId == "a", "SendIntent appId");
        EXPECT_TRUE(backend->lastIntent.intent == "test.intent", "SendIntent intent");
    }

    // Exotic: whitespace appId (non-empty) should pass plugin's non-empty validation
    {
        string response;
        uint32_t code = Invoke(rpc, "launchApp", R"({"appId":"  "})", response);
        EXPECT_EQ(code, Core::ERROR_NONE, "launchApp accepts whitespace appId (non-empty per validation)");
    }

    // Deinitialize should unregister methods; invoking after deinit should fail with unknown method/range
    plugin.Deinitialize(shell);
    {
        string response;
        uint32_t code = Invoke(rpc, "launchApp", R"({"appId":"a"})", response);
        EXPECT_TRUE(code != Core::ERROR_NONE, "launchApp should not be invokable after Deinitialize");
    }

    // Initialize failure path: backend unavailable
    {
        AppGateway plugin2;
        auto* shell2 = new MockShell();
        // Do not provide backend
        string init2 = plugin2.Initialize(shell2);
        EXPECT_TRUE(init2.empty() == false, "Initialize() should return error string if backend is unavailable");
        plugin2.Deinitialize(shell2);
        shell2->Release();
    }

    // Cleanup
    shell->Release();
    backend->Release();

    printf("Unit tests completed: %d total, %d failed\n", g_total, g_failed);
    return (g_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
