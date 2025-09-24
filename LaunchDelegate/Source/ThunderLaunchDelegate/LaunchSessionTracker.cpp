#include "LaunchSessionTracker.h"

#include <sstream>

namespace Plugin {

    LaunchSessionTracker::LaunchSessionTracker() = default;
    LaunchSessionTracker::~LaunchSessionTracker() = default;

    string LaunchSessionTracker::NextLaunchId() {
        static std::atomic<uint64_t> counter{0};
        const uint64_t v = ++counter;
        std::ostringstream ss;
        ss << "ld-" << std::hex << v;
        return ss.str();
    }

    string LaunchSessionTracker::Create(const string& appId, const string& providerId) {
        const string id = NextLaunchId();
        std::lock_guard<std::mutex> guard(_adminLock);
        SessionEntry e;
        e.launchId = id;
        e.appId = appId;
        e.providerId = providerId;
        e.state = "accepted";
        e.lastUpdatedMs = Core::Time::Now().Ticks();
        _byId.emplace(id, std::move(e));
        return id;
    }

    void LaunchSessionTracker::Update(const string& launchId, const string& state, const string& message, uint64_t tsMs) {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _byId.find(launchId);
        if (it == _byId.end()) return;
        it->second.state = state;
        it->second.message = message;
        it->second.lastUpdatedMs = tsMs != 0 ? tsMs : Core::Time::Now().Ticks();
    }

    bool LaunchSessionTracker::Get(const string& launchId, SessionEntry& out) const {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _byId.find(launchId);
        if (it == _byId.end()) return false;
        out = it->second;
        return true;
    }

    bool LaunchSessionTracker::Cancel(const string& launchId, const string& reason, uint64_t tsMs) {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _byId.find(launchId);
        if (it == _byId.end()) return false;
        it->second.state = "canceled";
        it->second.message = reason;
        it->second.lastUpdatedMs = tsMs != 0 ? tsMs : Core::Time::Now().Ticks();
        return true;
    }

    uint32_t LaunchSessionTracker::ActiveCount() const {
        std::lock_guard<std::mutex> guard(_adminLock);
        uint32_t count = 0;
        for (const auto& kv : _byId) {
            const string& s = kv.second.state;
            if (s != "terminated" && s != "failed" && s != "timeout" && s != "canceled") {
                ++count;
            }
        }
        return count;
    }

    uint32_t LaunchSessionTracker::TotalCount() const {
        std::lock_guard<std::mutex> guard(_adminLock);
        return static_cast<uint32_t>(_byId.size());
    }

} // namespace Plugin
