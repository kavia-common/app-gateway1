# App2AppProvider Thunder Plugin

App2AppProvider offers a lightweight, event-driven app-to-app communication provider using Thunder JSON-RPC. It maintains a registry of participating applications and provides message routing via broadcast and direct send primitives.

Key features:
- JSON-RPC methods (v1): ping, register, unregister, getpeers, updatecapabilities, sendmessage, broadcast.
- Events: peerup, peerdown, capabilitiesupdated, messagereceived.
- Configuration via Thunder Controller: enableTokenCheck, jsonrpcVersion, maxPayloadBytes.
- SessionRegistry abstraction to manage app registration and capability updates.
- Logging via Thunder SYSLOG.

Directory layout:
- Source/ThunderApp2AppProvider/ — C++ sources for the plugin and session registry.
- config/App2AppProvider.conf.in — Default configuration template.
- CMakeLists.txt — Build system for generating the plugin shared library.

JSON-RPC API (v1):

- Method: ping()
  - Input: {}
  - Output: { "time": "<ISO8601 UTC>" }

- Method: register(params)
  - Input:
    {
      "id": "app-id",
      "name": "App Name",
      "version": "1.0.0",
      "capabilities": { ... } // arbitrary JSON
    }
  - Output: { "session": "<session-id>" }
  - Emits: peerup (on first registration), capabilitiesupdated (on updates with capabilities)

- Method: unregister(params)
  - Input: { "id": "app-id" }
  - Output: { "removed": true|false }
  - Emits: peerdown (if removed)

- Method: getpeers(params)
  - Input: { "id": "app-id" } // optional; excludes this id from the list
  - Output: { "peers": [ { "id","name","version","capabilities","session" }, ... ] }

- Method: updatecapabilities(params)
  - Input: { "id": "app-id", "capabilities": { ... } }
  - Output: { "updated": true|false }
  - Emits: capabilitiesupdated

- Method: sendmessage(params)
  - Input:
    {
      "from": "app-id",
      "to": "peer-id",
      "type": "msg-type",
      "payload": { ... },
      "correlationId": "optional"
    }
  - Output: { "accepted": true, "correlationId": "..." }
  - Emits: messagereceived (one event with fields: from, to, type, payload, correlationId, timestamp)

- Method: broadcast(params)
  - Input:
    {
      "from": "app-id",
      "type": "msg-type",
      "payload": { ... },
      "correlationId": "optional"
    }
  - Output: { "accepted": true, "correlationId": "..." }
  - Emits: messagereceived (one event per peer; `to` field set accordingly)

Events:
- peerup: { "id","name","version","capabilities","timestamp" }
- peerdown: { "id","timestamp" }
- capabilitiesupdated: { "id","capabilities","timestamp" }
- messagereceived: { "from","to","type","payload","correlationId","timestamp" }

Notes:
- The plugin uses Thunder's JSON-RPC event subscription model; consumers must use the Controller to register for events.
- The payload size can be constrained via `maxPayloadBytes` (0 or unset means unlimited).
- In production deployments, consider adding authentication via `enableTokenCheck` and relying on a security agent token.

Build & Install:
- Requires Thunder/WPEFramework (Core, Plugins, Tracing targets).
- `cmake -S app-gateway1/App2AppProvider -B build && cmake --build build`
- The library installs into `lib/Thunder/Plugins` and the config template into `share/Thunder/Plugins`.

