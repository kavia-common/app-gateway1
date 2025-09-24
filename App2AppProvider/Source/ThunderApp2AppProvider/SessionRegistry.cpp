#include "SessionRegistry.h"

#include <sstream>

namespace Plugin {

    SessionRegistry::SessionRegistry() = default;
    SessionRegistry::~SessionRegistry() = default;

    bool SessionRegistry::RegisterApp(const AppInfo& infoIn, string& sessionOut, bool& isNew) {
        if (!infoIn.IsValid()) {
            return false;
        }
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _apps.find(infoIn.id);
        if (it == _apps.end()) {
            AppInfo copy = infoIn;
            copy.session = NextSessionId(infoIn.id);
            _apps.emplace(copy.id, copy);
            sessionOut = copy.session;
            isNew = true;
            return true;
        }
        // Update existing
        it->second.name = infoIn.name;
        it->second.version = infoIn.version;
        it->second.capabilities = infoIn.capabilities;
        sessionOut = it->second.session;
        isNew = false;
        return true;
    }

    bool SessionRegistry::UnregisterById(const string& id, AppInfo* removedOut) {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _apps.find(id);
        if (it == _apps.end()) {
            return false;
        }
        if (removedOut != nullptr) {
            *removedOut = it->second;
        }
        _apps.erase(it);
        return true;
    }

    bool SessionRegistry::UpdateCapabilities(const string& id, const Core::JSON::VariantContainer& caps) {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _apps.find(id);
        if (it == _apps.end()) {
            return false;
        }
        it->second.capabilities = caps;
        return true;
    }

    bool SessionRegistry::FindById(const string& id, AppInfo& out) const {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _apps.find(id);
        if (it == _apps.end()) {
            return false;
        }
        out = it->second;
        return true;
    }

    std::vector<AppInfo> SessionRegistry::ListPeers(const string& excludeId) const {
        std::lock_guard<std::mutex> guard(_adminLock);
        std::vector<AppInfo> out;
        out.reserve(_apps.size());
        for (const auto& kv : _apps) {
            if (!excludeId.empty() && kv.first == excludeId) {
                continue;
            }
            out.push_back(kv.second);
        }
        return out;
    }

    uint32_t SessionRegistry::Count() const {
        std::lock_guard<std::mutex> guard(_adminLock);
        return static_cast<uint32_t>(_apps.size());
    }

    string SessionRegistry::NextSessionId(const string& id) {
        static std::atomic<uint64_t> counter{0};
        uint64_t value = ++counter;
        std::ostringstream ss;
        ss << id << "#sess-" << value;
        return ss.str();
        // PUBLIC_INTERFACE
    }

} // namespace Plugin
