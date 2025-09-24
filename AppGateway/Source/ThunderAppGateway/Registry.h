#pragma once

#include "Module.h"
#include <unordered_map>
#include <vector>
#include <mutex>

namespace Plugin {

    // App record describes a discoverable application and its metadata
    struct AppRecord {
        string id;
        string name;
        string version;
        std::vector<string> categories;
        Core::JSON::VariantContainer capabilities;

        AppRecord()
            : capabilities() {}

        bool IsValid() const {
            return !id.empty();
        }
    };

    // JSON schema for serializing/deserializing AppRecord
    class AppRecordJSON : public Core::JSON::Container {
    public:
        AppRecordJSON()
            : Core::JSON::Container()
            , Id()
            , Name()
            , Version()
            , Categories()
            , Capabilities()
        {
            Add(_T("id"), &Id);
            Add(_T("name"), &Name);
            Add(_T("version"), &Version);
            Add(_T("categories"), &Categories);
            Add(_T("capabilities"), &Capabilities);
        }

        Core::JSON::String Id;
        Core::JSON::String Name;
        Core::JSON::String Version;
        Core::JSON::ArrayType<Core::JSON::String> Categories;
        Core::JSON::VariantContainer Capabilities;

        void From(const AppRecord& in) {
            Id = in.id;
            Name = in.name;
            Version = in.version;
            Categories.Clear();
            for (const auto& c : in.categories) {
                Core::JSON::String v;
                v = c;
                Categories.Add(v);
            }
            Capabilities = in.capabilities;
        }

        AppRecord To() const {
            AppRecord out;
            out.id = Id.Value();
            out.name = Name.Value();
            out.version = Version.Value();
            out.categories.clear();
            for (uint32_t i = 0; i < Categories.Length(); ++i) {
                out.categories.push_back(Categories[i].Value());
            }
            out.capabilities = Capabilities;
            return out;
        }
    };

    // Registry stores application metadata and supports queries
    class Registry {
    public:
        Registry();
        ~Registry();

        // Thread-safe loading from a file (JSON: { "apps": [ {id, name, ...}, ... ] })
        bool LoadFromFile(const string& path, string& errorOut);

        // List all apps
        std::vector<AppRecord> List() const;

        // Find app by id; returns true if found and fills out
        bool Find(const string& id, AppRecord& out) const;

        // Update capabilities of an app in the registry; returns false if not found
        bool UpdateCapabilities(const string& id, const Core::JSON::VariantContainer& caps);

    private:
        struct RegistryJSON : public Core::JSON::Container {
            RegistryJSON()
                : Core::JSON::Container()
                , Apps()
            {
                Add(_T("apps"), &Apps);
            }
            Core::JSON::ArrayType<AppRecordJSON> Apps;
        };

        mutable std::mutex _adminLock;
        std::unordered_map<string, AppRecord> _apps;
    };

} // namespace Plugin
