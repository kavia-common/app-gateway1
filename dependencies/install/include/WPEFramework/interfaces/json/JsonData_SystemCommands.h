// C++ classes for System Commands API JSON-RPC API.
// Generated automatically from 'SystemCommands.json'. DO NOT EDIT.

// Note: This code is inherently not thread safe. If required, proper synchronisation must be added.

#pragma once

#include <core/JSON.h>

namespace WPEFramework {

namespace JsonData {

    namespace SystemCommands {

        // Method params/result classes
        //

        class UsbresetParamsData : public Core::JSON::Container {
        public:
            UsbresetParamsData()
                : Core::JSON::Container()
            {
                Add(_T("device"), &Device);
            }

            bool IsValid() const
            {
                return (true);
            }

            UsbresetParamsData(const UsbresetParamsData&) = delete;
            UsbresetParamsData& operator=(const UsbresetParamsData&) = delete;

        public:
            Core::JSON::String Device; // USB device to reset
        }; // class UsbresetParamsData

    } // namespace SystemCommands

} // namespace JsonData

}

