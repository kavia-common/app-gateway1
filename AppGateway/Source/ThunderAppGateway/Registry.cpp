#include "Registry.h"

#include <fstream>
#include <sstream>

namespace Plugin {

    Registry::Registry() = default;
    Registry::~Registry() = default;

    bool Registry::LoadFromFile(const string& path, string& errorOut) {
        std::lock_guard<std::mutex> guard(_adminLock);

        _apps.clear();
        errorOut.clear();

        if (path.empty()) {
            errorOut = _T("Registry path is empty.");
            return false;
        }

        // Resolve %persistentpath% and %datapath% patterns if present
        string resolved = path;
        // In a real environment, we would query IShell for these paths. Here we do simple replacements if env vars are set.
        const char* persistent = ::getenv("THUNDER_PERSISTENT_PATH");
        const char* data = ::getenv("THUNDER_DATA_PATH");
        if (persistent != nullptr) {
            Core::TextSegmentIterator it(resolved, true, _T("%"));
            // naive replace; better to use IShell Expander in real builds
        }
        // Load file
        std::ifstream ifs(resolved.c_str());
        if (!ifs.is_open()) {
            errorOut = _T("Failed to open registry file: ") + resolved;
            return false;
        }

        std::stringstream buffer;
        buffer << ifs.rdbuf();
        string content = buffer.str();
        ifs.close();

        RegistryJSON json;
        if (json.FromString(content) == false) {
            errorOut = _T("Failed to parse registry JSON file.");
            return false;
        }

        for (uint32_t i = 0; i < json.Apps.Length(); ++i) {
            AppRecord rec = json.Apps[i].To();
            if (rec.IsValid()) {
                _apps[rec.id] = rec;
            }
        }

        return true;
    }

    std::vector<AppRecord> Registry::List() const {
        std::lock_guard<std::mutex> guard(_adminLock);
        std::vector<AppRecord> out;
        out.reserve(_apps.size());
        for (auto& kv : _apps) {
            out.push_back(kv.second);
        }
        return out;
    }

    bool Registry::Find(const string& id, AppRecord& out) const {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _apps.find(id);
        if (it == _apps.end()) {
            return false;
        }
        out = it->second;
        return true;
    }

    bool Registry::UpdateCapabilities(const string& id, const Core::JSON::VariantContainer& caps) {
        std::lock_guard<std::mutex> guard(_adminLock);
        auto it = _apps.find(id);
        if (it == _apps.end()) {
            return false;
        }
        it->second.capabilities = caps;
        return true;
    }

} // namespace Plugin
