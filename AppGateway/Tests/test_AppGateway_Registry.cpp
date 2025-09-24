#include "Registry.h"
#include "Module.h"

#include <fstream>
#include <cstdio>

using namespace Plugin;

static std::string writeTempRegistry(const std::string& content) {
    std::string path = "/tmp/appgateway_registry_test.json";
    std::ofstream ofs(path);
    ofs << content;
    ofs.close();
    return path;
}

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

TTEST(RegistryTests, LoadListFindAndUpdate) {
    // Prepare a small registry JSON following the expected schema: { "apps": [ AppRecordJSON, ... ] }
    const std::string json = R"JSON(
{
  "apps": [
    {
      "id": "com.example.VideoApp",
      "name": "Video App",
      "version": "2.3.1",
      "categories": ["video","entertainment"],
      "capabilities": { "intents": ["view","play"], "drm": ["widevine"] }
    },
    {
      "id": "com.example.MusicApp",
      "name": "Music App",
      "version": "1.0.0",
      "categories": ["music"],
      "capabilities": { "intents": ["play"] }
    }
  ]
}
)JSON";
    std::string error;
    std::string path = writeTempRegistry(json);

    Registry reg;
    bool ok = reg.LoadFromFile(path, error);
    TEXPECT_TRUE(ok);
    TEXPECT_TRUE(error.empty());

    auto apps = reg.List();
    TEXPECT_EQ(apps.size(), static_cast<size_t>(2));

    AppRecord rec;
    bool found = reg.Find("com.example.VideoApp", rec);
    TEXPECT_TRUE(found);
    TEXPECT_EQ(rec.id, "com.example.VideoApp");
    TEXPECT_EQ(rec.name, "Video App");
    TEXPECT_EQ(rec.version, "2.3.1");

    // Update capabilities
    Core::JSON::VariantContainer newCaps;
    Core::JSON::String key("supportsHDR");
    Core::JSON::Boolean val(true);
    newCaps.Set(_T("supportsHDR"), val);
    bool updated = reg.UpdateCapabilities("com.example.VideoApp", newCaps);
    TEXPECT_TRUE(updated);

    AppRecord rec2;
    TEXPECT_TRUE(reg.Find("com.example.VideoApp", rec2));
    // Verify that capabilities got updated (presence of key)
    const Core::JSON::Variant* v = rec2.capabilities.Get(_T("supportsHDR"));
    TEXPECT_TRUE(v != nullptr);
}

TTEST(RegistryTests, LoadFromMissingFileFails) {
    Registry reg;
    std::string error;
    bool ok = reg.LoadFromFile("/tmp/this_file_should_not_exist_12345.json", error);
    TEXPECT_FALSE(ok);
    TEXPECT_FALSE(error.empty());
}
