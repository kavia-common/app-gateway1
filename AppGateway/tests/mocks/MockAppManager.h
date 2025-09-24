#pragma once

#include "interfaces/IAppManager.h"
#include <unordered_map>
#include <vector>

namespace WPEFramework {
namespace Testing {

    class MockAppManager final : public Exchange::IAppManager {
    public:
        MockAppManager()
            : _refCount(1)
        {
            // Default some loaded apps list for tests
            _loadedApps = { "com.example.A", "com.example.B" };
        }

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
        void* QueryInterface(const uint32_t id) override {
            if (id == Exchange::IAppManager::ID) {
                AddRef();
                return static_cast<Exchange::IAppManager*>(this);
            }
            return nullptr;
        }

        // Exchange::IAppManager
        uint32_t PreloadApp(const string& appId, const string& launchArgs, string& backendError) override {
            lastPreload.appId = appId;
            lastPreload.launchArgs = launchArgs;
            // Optionally expose a backend detail
            backendError = simulatedPreloadError;
            // Track as loaded if not present
            if (std::find(_loadedApps.begin(), _loadedApps.end(), appId) == _loadedApps.end()) {
                _loadedApps.push_back(appId);
            }
            return preloadReturnCode;
        }

        uint32_t LaunchApp(const string& appId, const string& intent, const string& launchArgs) override {
            lastLaunch.appId = appId;
            lastLaunch.intent = intent;
            lastLaunch.launchArgs = launchArgs;
            if (std::find(_loadedApps.begin(), _loadedApps.end(), appId) == _loadedApps.end()) {
                _loadedApps.push_back(appId);
            }
            return launchReturnCode;
        }

        uint32_t CloseApp(const string& appId) override {
            lastClose.appId = appId;
            // Simulate remove from loaded
            auto it = std::find(_loadedApps.begin(), _loadedApps.end(), appId);
            if (it != _loadedApps.end()) {
                _loadedApps.erase(it);
            }
            return closeReturnCode;
        }

        uint32_t TerminateApp(const string& appId) override {
            lastTerminate.appId = appId;
            // Simulate remove from loaded
            auto it = std::find(_loadedApps.begin(), _loadedApps.end(), appId);
            if (it != _loadedApps.end()) {
                _loadedApps.erase(it);
            }
            return terminateReturnCode;
        }

        uint32_t GetLoadedApps(string& result) override {
            // Return as a JSON array string, forwarded opaquely by plugin
            Core::JSON::ArrayType<Core::JSON::String> arr;
            for (const auto& a : _loadedApps) {
                Core::JSON::String v;
                v = a;
                arr.Add(v);
            }
            string json;
            arr.ToString(json);
            result = json;
            return getLoadedAppsReturnCode;
        }

        uint32_t SetAppProperty(const string& appId, const string& key, const string& value) override {
            _props[appId][key] = value;
            lastSetProperty.appId = appId;
            lastSetProperty.key = key;
            lastSetProperty.value = value;
            return setPropertyReturnCode;
        }

        uint32_t GetAppProperty(const string& appId, const string& key, string& value) override {
            lastGetProperty.appId = appId;
            lastGetProperty.key = key;
            auto ait = _props.find(appId);
            if (ait != _props.end()) {
                auto kit = ait->second.find(key);
                if (kit != ait->second.end()) {
                    value = kit->second;
                    return getPropertyReturnCode;
                }
            }
            return Core::ERROR_UNKNOWN_KEY;
        }

        uint32_t SendIntent(const string& appId, const string& intent) override {
            lastIntent.appId = appId;
            lastIntent.intent = intent;
            return sendIntentReturnCode;
        }

        // Control and inspection helpers for tests
        struct PreloadInfo { string appId; string launchArgs; } lastPreload;
        struct LaunchInfo { string appId; string intent; string launchArgs; } lastLaunch;
        struct CloseInfo { string appId; } lastClose;
        struct TerminateInfo { string appId; } lastTerminate;
        struct SetPropInfo { string appId; string key; string value; } lastSetProperty;
        struct GetPropInfo { string appId; string key; } lastGetProperty;
        struct IntentInfo { string appId; string intent; } lastIntent;

        std::vector<string> _loadedApps;
        std::unordered_map<string, std::unordered_map<string, string>> _props;

        // Simulated backend behaviors
        string simulatedPreloadError;
        uint32_t preloadReturnCode { Core::ERROR_NONE };
        uint32_t launchReturnCode { Core::ERROR_NONE };
        uint32_t closeReturnCode { Core::ERROR_NONE };
        uint32_t terminateReturnCode { Core::ERROR_NONE };
        uint32_t getLoadedAppsReturnCode { Core::ERROR_NONE };
        uint32_t setPropertyReturnCode { Core::ERROR_NONE };
        uint32_t getPropertyReturnCode { Core::ERROR_NONE };
        uint32_t sendIntentReturnCode { Core::ERROR_NONE };

    private:
        mutable uint32_t _refCount;
    };

} // namespace Testing
} // namespace WPEFramework
