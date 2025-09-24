#pragma once

#include "Module.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>

namespace Plugin {

    // Session/app information for app-to-app communication.
    struct AppInfo {
        string id;
        string name;
        string version;
        Core::JSON::VariantContainer capabilities;
        string session; // generated on registration

        AppInfo() : capabilities() {}

        bool IsValid() const { return !id.empty(); }
    };

    // JSON view for AppInfo (peer list entries)
    class AppInfoJSON : public Core::JSON::Container {
    public:
        AppInfoJSON()
            : Core::JSON::Container()
            , Id()
            , Name()
            , Version()
            , Capabilities()
            , Session()
        {
            Add(_T("id"), &Id);
            Add(_T("name"), &Name);
            Add(_T("version"), &Version);
            Add(_T("capabilities"), &Capabilities);
            Add(_T("session"), &Session);
        }

        Core::JSON::String Id;
        Core::JSON::String Name;
        Core::JSON::String Version;
        Core::JSON::VariantContainer Capabilities;
        Core::JSON::String Session;

        void From(const AppInfo& in) {
            Id = in.id;
            Name = in.name;
            Version = in.version;
            Capabilities = in.capabilities;
            Session = in.session;
        }

        AppInfo To() const {
            AppInfo out;
            out.id = Id.Value();
            out.name = Name.Value();
            out.version = Version.Value();
            out.capabilities = Capabilities;
            out.session = Session.Value();
            return out;
        }
    };

    // Thread-safe registry of sessions and peers.
    class SessionRegistry {
    public:
        SessionRegistry();
        ~SessionRegistry();

        // Register a new app. If already exists, refresh its info and return existing session.
        bool RegisterApp(const AppInfo& infoIn, string& sessionOut, bool& isNew);

        // Unregister by id. Returns true if removed.
        bool UnregisterById(const string& id, AppInfo* removedOut = nullptr);

        // Update capabilities for an app. Returns false if not found.
        bool UpdateCapabilities(const string& id, const Core::JSON::VariantContainer& caps);

        // Find app by id. Returns true if found.
        bool FindById(const string& id, AppInfo& out) const;

        // Return list of peers. If excludeId is set, skips that id.
        std::vector<AppInfo> ListPeers(const string& excludeId = string()) const;

        // Return number of registered apps.
        uint32_t Count() const;

    private:
        static string NextSessionId(const string& id);

        mutable std::mutex _adminLock;
        std::unordered_map<string, AppInfo> _apps; // id -> AppInfo
    };

} // namespace Plugin
