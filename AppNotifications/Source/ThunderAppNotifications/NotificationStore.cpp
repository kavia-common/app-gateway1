#include "NotificationStore.h"

#include <sstream>
#include <fstream>
#include <iomanip>

namespace Plugin {

    namespace {
        class StoreJSON : public Core::JSON::Container {
        public:
            StoreJSON() : Notifications() {
                Add(_T("notifications"), &Notifications);
            }
            Core::JSON::ArrayType<NotificationJSON> Notifications;
        };

        static string ToISO8601UTC(const std::time_t t) {
            std::tm tm{};
        #if defined(_WIN32)
            gmtime_s(&tm, &t);
        #else
            gmtime_r(&t, &tm);
        #endif
            std::ostringstream ss;
            ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            return ss.str();
        }
    }

    NotificationStore::NotificationStore() = default;
    NotificationStore::~NotificationStore() = default;

    void NotificationStore::Configure(const uint32_t defaultTTLSeconds, const uint32_t maxItems) {
        std::lock_guard<std::mutex> guard(_adminLock);
        _defaultTTL = defaultTTLSeconds;
        _maxItems = maxItems;
    }

    string NotificationStore::NextId() {
        static std::atomic<uint64_t> counter{0};
        const uint64_t value = ++counter;
        std::ostringstream ss;
        ss << "ntf-" << value;
        return ss.str();
    }

    string NotificationStore::NowISO8601() {
        return ToISO8601UTC(static_cast<std::time_t>(NowEpoch()));
    }

    uint64_t NotificationStore::NowEpoch() {
        return static_cast<uint64_t>(std::time(nullptr));
    }

    void NotificationStore::CleanupUnlocked(const uint64_t nowEpoch, uint32_t& evictedOut) {
        // Remove expired
        size_t before = _byId.size();
        // Efficient remove by scanning order and erasing by id if expired
        std::vector<string> newOrder;
        newOrder.reserve(_order.size());
        for (const auto& id : _order) {
            auto it = _byId.find(id);
            if (it == _byId.end()) {
                continue;
            }
            if (it->second.IsExpired(nowEpoch)) {
                _byId.erase(it);
                continue;
            }
            newOrder.push_back(id);
        }
        _order.swap(newOrder);
        size_t after = _byId.size();
        if (after < before) {
            evictedOut += static_cast<uint32_t>(before - after);
        }

        // Enforce max items by evicting oldest
        if (_maxItems > 0 && _byId.size() > _maxItems) {
            const size_t target = _maxItems;
            size_t toRemove = _byId.size() - target;
            // remove from front of _order
            std::vector<string> newer;
            newer.reserve(target);
            for (const auto& id : _order) {
                if (toRemove > 0) {
                    _byId.erase(id);
                    --toRemove;
                    ++evictedOut;
                    continue;
                }
                newer.push_back(id);
            }
            _order.swap(newer);
        }
    }

    string NotificationStore::Add(const Notification& base, uint32_t ttlSeconds, uint32_t& evictedCountOut) {
        std::lock_guard<std::mutex> guard(_adminLock);

        evictedCountOut = 0;
        CleanupUnlocked(NowEpoch(), evictedCountOut);

        Notification copy = base;
        if (copy.id.empty()) {
            copy.id = NextId();
        }

        // Compute expiration
        uint32_t ttl = ttlSeconds != 0 ? ttlSeconds : _defaultTTL;
        if (ttl > 0) {
            const uint64_t now = NowEpoch();
            copy.expireEpoch = now + ttl;
            copy.expiresAt = ToISO8601UTC(static_cast<std::time_t>(copy.expireEpoch));
        } else {
            copy.expireEpoch = 0;
            copy.expiresAt.clear();
        }

        // Timestamp if not provided
        if (copy.timestamp.empty()) {
            copy.timestamp = NowISO8601();
        }

        // Insert new
        _byId[copy.id] = copy;
        _order.push_back(copy.id);

        // Enforce post-insert
        CleanupUnlocked(NowEpoch(), evictedCountOut);
        return copy.id;
    }

    bool NotificationStore::Ack(const string& id, const string& by) {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _byId.find(id);
        if (it == _byId.end()) {
            return false;
        }
        // Deduplicate
        for (const auto& who : it->second.ackedBy) {
            if (who == by) {
                return true;
            }
        }
        it->second.ackedBy.push_back(by);
        return true;
    }

    bool NotificationStore::ClearById(const string& id) {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _byId.find(id);
        if (it == _byId.end()) {
            return false;
        }
        _byId.erase(it);
        // Erase from order
        std::vector<string> newOrder;
        newOrder.reserve(_order.size());
        for (const auto& cur : _order) {
            if (cur != id) {
                newOrder.push_back(cur);
            }
        }
        _order.swap(newOrder);
        return true;
    }

    uint32_t NotificationStore::ClearByFilter(const string& appId, const string& type, const bool onlyUnacked) {
        std::lock_guard<std::mutex> guard(_adminLock);
        uint32_t removed = 0;
        std::unordered_map<string, Notification>::iterator it = _byId.begin();
        while (it != _byId.end()) {
            const Notification& n = it->second;
            if (!appId.empty() && n.from != appId) {
                ++it;
                continue;
            }
            if (!type.empty() && n.type != type) {
                ++it;
                continue;
            }
            if (onlyUnacked && !n.ackedBy.empty()) {
                ++it;
                continue;
            }
            it = _byId.erase(it);
            ++removed;
        }
        if (removed > 0) {
            std::vector<string> newOrder;
            newOrder.reserve(_order.size());
            for (const auto& id : _order) {
                if (_byId.find(id) != _byId.end()) {
                    newOrder.push_back(id);
                }
            }
            _order.swap(newOrder);
        }
        return removed;
    }

    std::vector<Notification> NotificationStore::List(const string& appId, const string& type, const bool onlyUnacked, uint32_t limit) const {
        std::lock_guard<std::mutex> guard(_adminLock);
        std::vector<Notification> out;
        for (const auto& id : _order) {
            auto it = _byId.find(id);
            if (it == _byId.end()) {
                continue;
            }
            const Notification& n = it->second;
            if (!appId.empty() && n.from != appId) {
                continue;
            }
            if (!type.empty() && n.type != type) {
                continue;
            }
            if (onlyUnacked && !n.ackedBy.empty()) {
                continue;
            }
            out.push_back(n);
            if (limit != 0 && out.size() >= limit) {
                break;
            }
        }
        return out;
    }

    bool NotificationStore::LoadFromFile(const string& path, string& errorOut) {
        std::lock_guard<std::mutex> guard(_adminLock);

        errorOut.clear();
        if (path.empty()) {
            errorOut = _T("Storage path is empty.");
            return false;
        }

        std::ifstream ifs(path.c_str());
        if (!ifs.is_open()) {
            // If file doesn't exist, treat as empty store
            return true;
        }
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        string content = buffer.str();
        ifs.close();

        StoreJSON json;
        if (json.FromString(content) == false) {
            errorOut = _T("Failed to parse notifications store JSON.");
            return false;
        }

        _byId.clear();
        _order.clear();
        for (uint32_t i = 0; i < json.Notifications.Length(); ++i) {
            Notification n = json.Notifications[i].To();
            if (n.id.empty()) {
                n.id = NextId();
            }
            _byId[n.id] = n;
            _order.push_back(n.id);
        }

        return true;
    }

    bool NotificationStore::SaveToFile(const string& path, string& errorOut) const {
        std::lock_guard<std::mutex> guard(_adminLock);

        errorOut.clear();
        if (path.empty()) {
            errorOut = _T("Storage path is empty.");
            return false;
        }

        StoreJSON json;
        for (const auto& id : _order) {
            auto it = _byId.find(id);
            if (it == _byId.end()) {
                continue;
            }
            NotificationJSON njson;
            njson.From(it->second);
            json.Notifications.Add(njson);
        }

        string output;
        json.ToString(output);
        std::ofstream ofs(path.c_str(), std::ios::trunc | std::ios::out);
        if (!ofs.is_open()) {
            errorOut = _T("Failed to open storage file for write: ") + path;
            return false;
        }
        ofs << output;
        ofs.close();
        return true;
    }

} // namespace Plugin
