#pragma once

#include "Module.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <ctime>

namespace Plugin {

    // In-memory representation of a notification.
    struct Notification {
        string id;
        string from;  // appId/source
        string type;  // notification topic/type
        string title;
        string body;
        Core::JSON::VariantContainer data; // arbitrary payload
        uint8_t priority {0};              // 0-255
        bool requiresAck {false};
        std::vector<string> ackedBy;       // subscriber ids that acked
        string timestamp;                  // ISO8601 UTC when published
        string expiresAt;                  // ISO8601 UTC when expires (optional)
        std::vector<string> tags;          // optional tags

        // Internal: expiry epoch in seconds (0 = no expiry configured)
        uint64_t expireEpoch {0};

        Notification()
            : data()
        {
        }

        bool IsExpired(const uint64_t nowEpoch) const {
            return (expireEpoch != 0) && (nowEpoch >= expireEpoch);
        }
    };

    // JSON schema for serializing/deserializing Notification
    class NotificationJSON : public Core::JSON::Container {
    public:
        NotificationJSON()
            : Core::JSON::Container()
            , Id()
            , From()
            , Type()
            , Title()
            , Body()
            , Data()
            , Priority()
            , RequiresAck()
            , AckedBy()
            , Timestamp()
            , ExpiresAt()
            , Tags()
        {
            Add(_T("id"), &Id);
            Add(_T("from"), &From);
            Add(_T("type"), &Type);
            Add(_T("title"), &Title);
            Add(_T("body"), &Body);
            Add(_T("data"), &Data);
            Add(_T("priority"), &Priority);
            Add(_T("requiresAck"), &RequiresAck);
            Add(_T("ackedBy"), &AckedBy);
            Add(_T("timestamp"), &Timestamp);
            Add(_T("expiresAt"), &ExpiresAt);
            Add(_T("tags"), &Tags);
        }

        Core::JSON::String Id;
        Core::JSON::String From;
        Core::JSON::String Type;
        Core::JSON::String Title;
        Core::JSON::String Body;
        Core::JSON::VariantContainer Data;
        Core::JSON::DecUInt8 Priority;
        Core::JSON::Boolean RequiresAck;
        Core::JSON::ArrayType<Core::JSON::String> AckedBy;
        Core::JSON::String Timestamp;
        Core::JSON::String ExpiresAt;
        Core::JSON::ArrayType<Core::JSON::String> Tags;

        void From(const Notification& in) {
            Id = in.id;
            From = in.from;
            Type = in.type;
            Title = in.title;
            Body = in.body;
            Data = in.data;
            Priority = in.priority;
            RequiresAck = in.requiresAck;
            AckedBy.Clear();
            for (const auto& who : in.ackedBy) {
                Core::JSON::String s;
                s = who;
                AckedBy.Add(s);
            }
            Timestamp = in.timestamp;
            if (!in.expiresAt.empty()) {
                ExpiresAt = in.expiresAt;
            }
            Tags.Clear();
            for (const auto& t : in.tags) {
                Core::JSON::String s;
                s = t;
                Tags.Add(s);
            }
        }

        Notification To() const {
            Notification out;
            out.id = Id.Value();
            out.from = From.Value();
            out.type = Type.Value();
            out.title = Title.Value();
            out.body = Body.Value();
            out.data = Data;
            out.priority = static_cast<uint8_t>(Priority.Value());
            out.requiresAck = RequiresAck.Value();
            out.ackedBy.clear();
            for (uint32_t i = 0; i < AckedBy.Length(); ++i) {
                out.ackedBy.push_back(AckedBy[i].Value());
            }
            out.timestamp = Timestamp.Value();
            out.expiresAt = ExpiresAt.Value();
            out.tags.clear();
            for (uint32_t i = 0; i < Tags.Length(); ++i) {
                out.tags.push_back(Tags[i].Value());
            }
            out.expireEpoch = ParseISO8601UTC(out.expiresAt);
            return out;
        }

        static uint64_t ParseISO8601UTC(const string& iso) {
            if (iso.empty()) return 0;
            // Expect simple form: YYYY-MM-DDTHH:MM:SSZ
            std::tm tm{};
            if (iso.size() < 20) {
                return 0;
            }
            // Basic parsing with atoi; not locale dependent.
            auto to_int = [](const char* b, size_t n)->int {
                int v = 0;
                for (size_t i = 0; i < n; ++i) {
                    char c = b[i];
                    if (c < '0' || c > '9') return 0;
                    v = v * 10 + (c - '0');
                }
                return v;
            };
            tm.tm_year = to_int(&iso[0], 4) - 1900;
            tm.tm_mon  = to_int(&iso[5], 2) - 1;
            tm.tm_mday = to_int(&iso[8], 2);
            tm.tm_hour = to_int(&iso[11], 2);
            tm.tm_min  = to_int(&iso[14], 2);
            tm.tm_sec  = to_int(&iso[17], 2);
            // Convert to epoch assuming UTC (timegm or _mkgmtime)
        #if defined(_WIN32)
            return static_cast<uint64_t>(_mkgmtime(&tm));
        #else
            return static_cast<uint64_t>(timegm(&tm));
        #endif
        }
    };

    // Thread-safe storage of notifications with optional TTL and retention limits.
    class NotificationStore {
    public:
        NotificationStore();
        ~NotificationStore();

        // Configure default retention behavior
        void Configure(const uint32_t defaultTTLSeconds, const uint32_t maxItems);

        // Add a notification. Returns assigned id. ttlSeconds=0 uses default.
        // evictedCountOut returns number of notifications evicted due to retention/TTL.
        string Add(const Notification& base, uint32_t ttlSeconds, uint32_t& evictedCountOut);

        // Acknowledge a notification by id and subscriber.
        bool Ack(const string& id, const string& by);

        // Clear a single notification by id. Returns true if removed.
        bool ClearById(const string& id);

        // Clear notifications by filter and return number removed.
        uint32_t ClearByFilter(const string& appId, const string& type, const bool onlyUnacked);

        // List notifications using filters; if limit==0, returns all matches.
        std::vector<Notification> List(const string& appId, const string& type, const bool onlyUnacked, uint32_t limit) const;

        // Persistence
        bool LoadFromFile(const string& path, string& errorOut);
        bool SaveToFile(const string& path, string& errorOut) const;

    private:
        static string NextId();
        static string NowISO8601();
        static uint64_t NowEpoch();
        void CleanupUnlocked(const uint64_t nowEpoch, uint32_t& evictedOut);

        mutable std::mutex _adminLock;
        std::unordered_map<string, Notification> _byId; // id -> Notification
        std::vector<string> _order; // insertion order for deterministic listing
        uint32_t _defaultTTL {0}; // seconds, 0=no expiry
        uint32_t _maxItems {0};   // 0=no limit
    };

} // namespace Plugin
