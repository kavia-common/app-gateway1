#pragma once

#include "Module.h"

#include <unordered_map>
#include <mutex>
#include <atomic>

namespace Plugin {

    struct SessionEntry {
        string launchId;
        string appId;
        string providerId;
        string state;
        uint64_t lastUpdatedMs {0};
        string message;
    };

    class LaunchSessionTracker {
    public:
        LaunchSessionTracker();
        ~LaunchSessionTracker();

        // Create a new session and return its launchId.
        string Create(const string& appId, const string& providerId);

        // Update state and message.
        void Update(const string& launchId, const string& state, const string& message, uint64_t tsMs);

        // Retrieve session entry; returns false if not found.
        bool Get(const string& launchId, SessionEntry& out) const;

        // Cancel session; returns false if not found.
        bool Cancel(const string& launchId, const string& reason, uint64_t tsMs);

        // Count active launches (non-terminal states).
        uint32_t ActiveCount() const;

        // Total sessions regardless of state.
        uint32_t TotalCount() const;

    private:
        static string NextLaunchId();

        mutable std::mutex _adminLock;
        std::unordered_map<string, SessionEntry> _byId;
    };

} // namespace Plugin
