#include "LaunchClient.h"
#include "Module.h"

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

TTEST(LaunchClientTests, OpenLaunchStateTerminateFlow) {
    LaunchClient client;
    TEXPECT_TRUE(client.Open());

    std::string handle;
    std::string state;
    Core::JSON::VariantContainer params;
    uint32_t rc = client.Launch("com.example.VideoApp", "play", params, handle, state);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    TEXPECT_FALSE(handle.empty());
    TEXPECT_EQ(state, std::string(AppState::Launching) ); // immediate launching returned

    // State should quickly become running per stub implementation
    std::string state2;
    uint32_t pid = 0;
    rc = client.State("com.example.VideoApp", state2, pid);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    TEXPECT_EQ(state2, std::string(AppState::Running));
    TEXPECT_TRUE(pid > 0);

    // Terminate
    std::string finalState;
    rc = client.Terminate("com.example.VideoApp", finalState);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    TEXPECT_EQ(finalState, std::string(AppState::Stopped));

    // After terminate, state should be stopped and pid 0
    std::string state3;
    uint32_t pid2 = 1234;
    rc = client.State("com.example.VideoApp", state3, pid2);
    TEXPECT_EQ(rc, Core::ERROR_NONE);
    TEXPECT_EQ(state3, std::string(AppState::Stopped));
    TEXPECT_EQ(pid2, 0u);

    client.Close();
}

TTEST(LaunchClientTests, UnknownIdHandling) {
    LaunchClient client;
    TEXPECT_TRUE(client.Open());

    std::string state;
    uint32_t rc = client.Terminate("unknown.app.id", state);
    TEXPECT_EQ(rc, Core::ERROR_UNKNOWN_KEY);
    TEXPECT_EQ(state, std::string(AppState::Unknown));

    std::string state2;
    uint32_t pid = 42;
    rc = client.State("unknown.app.id", state2, pid);
    TEXPECT_EQ(rc, Core::ERROR_UNKNOWN_KEY);
    TEXPECT_EQ(state2, std::string(AppState::Unknown));
    TEXPECT_EQ(pid, 0u);

    client.Close();
}
