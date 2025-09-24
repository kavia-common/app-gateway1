#pragma once

#include "Module.h"
#include <unordered_map>
#include <mutex>

namespace Plugin {

    // Normalized app state names consistent with the design document.
    namespace AppState {
        static constexpr const char* Launching = "launching";
        static constexpr const char* Running   = "running";
        static constexpr const char* Suspended = "suspended";
        static constexpr const char* Stopping  = "stopping";
        static constexpr const char* Stopped   = "stopped";
        static constexpr const char* Error     = "error";
        static constexpr const char* Unknown   = "unknown";
    }

    // A development stub emulating a LaunchDelegate connector.
    // Replace with a COM-RPC SmartConnector to Exchange::ILaunch in production.
    class LaunchClient {
    public:
        LaunchClient() = default;
        ~LaunchClient() = default;

        bool Open() {
            // In production: Connect to LaunchDelegate Exchange interface
            return true;
        }

        void Close() {
            // In production: Close COM-RPC connector
            std::lock_guard<std::mutex> guard(_adminLock);
            _states.clear();
        }

        // Launch an app. Produces a synthetic handle and sets state to launching then running.
        // Returns Core::ERROR_NONE on simulated success.
        uint32_t Launch(const string& id,
                        const string& intent,
                        const Core::JSON::VariantContainer& params,
                        string& handleOut,
                        string& stateOut)
        {
            (void)intent;
            (void)params;

            std::lock_guard<std::mutex> guard(_adminLock);
            // Generate a simple increasing handle suffix
            uint32_t& seq = _sequence[id];
            ++seq;
            handleOut = id + _T("#") + std::to_string(seq);

            _states[id].pid = 1000 + (seq % 100);
            _states[id].state = AppState::Launching;

            stateOut = _states[id].state;

            // Simulate that it quickly becomes running
            _states[id].state = AppState::Running;

            return Core::ERROR_NONE;
        }

        // Terminate an app by id. Returns final state.
        uint32_t Terminate(const string& id, string& stateOut) {
            std::lock_guard<std::mutex> guard(_adminLock);
            auto it = _states.find(id);
            if (it == _states.end()) {
                stateOut = AppState::Unknown;
                return Core::ERROR_UNKNOWN_KEY;
            }
            it->second.state = AppState::Stopped;
            it->second.pid = 0;
            stateOut = it->second.state;
            return Core::ERROR_NONE;
        }

        // Query app state. Fills stateOut and pidOut.
        uint32_t State(const string& id, string& stateOut, uint32_t& pidOut) const {
            std::lock_guard<std::mutex> guard(_adminLock);
            auto it = _states.find(id);
            if (it == _states.end()) {
                stateOut = AppState::Unknown;
                pidOut = 0;
                return Core::ERROR_UNKNOWN_KEY;
            }
            stateOut = it->second.state;
            pidOut = it->second.pid;
            return Core::ERROR_NONE;
        }

    private:
        struct StateInfo {
            string state { AppState::Unknown };
            uint32_t pid { 0 };
        };

        mutable std::mutex _adminLock;
        std::unordered_map<string, StateInfo> _states;
        std::unordered_map<string, uint32_t> _sequence;
    };

} // namespace Plugin
