# AppNotifications Thunder Plugin

AppNotifications provides a lightweight, event-driven notification service for applications using Thunder JSON-RPC. Apps can publish notifications, subscribe/unsubscribe with optional filters, list/clear notifications, and acknowledge notifications. The plugin supports in-memory storage or file-based persistence with retention controls (TTL, max items).

Key features:
- JSON-RPC methods (v1): ping, publish, subscribe, unsubscribe, list, clear, ack, getsubscribers.
- Events: notification, ack, subscriberup, subscriberdrop, cleared.
- Configuration via Thunder Controller: `storage` (memory/file), `retention` (ttlSeconds, maxNotifications), `enableTokenCheck`, `jsonrpcVersion`, `maxPayloadBytes`.
- NotificationStore abstraction for thread-safe storage with TTL and retention.
- Logging via Thunder SYSLOG.

Directory layout:
- Source/ThunderAppNotifications/ — C++ sources for the plugin and storage.
- config/AppNotifications.conf.in — Default configuration template.
- CMakeLists.txt — Build file for generating the plugin shared library.

JSON-RPC API (v1):

- Method: ping()
  - Input: {}
  - Output: { "time": "<ISO8601 UTC>" }

- Method: publish(params)
  - Input:
    {
      "from": "app-id",
      "type": "topic",
      "title": "Title text",
      "body": "Optional longer text",
      "data": { ... },            // arbitrary JSON
      "priority": 0,              // 0..255
      "requiresAck": false,
      "ttlSeconds": 0,            // optional; 0 uses default
      "tags": ["optional","tags"]
    }
  - Output: { "id": "<notification-id>" }
  - Emits: notification (payload is the full notification object)
  - May also emit: cleared (if retention/TTL evicted older notifications)

- Method: subscribe(params)
  - Input:
    {
      "id": "subscriber-id",
      "types": ["topicA","topicB"],  // optional
      "apps": ["app-a","app-b"]      // optional
    }
  - Output: { "subscribed": true }
  - Emits: subscriberup

- Method: unsubscribe(params)
  - Input: { "id": "subscriber-id" }
  - Output: { "removed": true|false }
  - Emits: subscriberdrop (if removed)

- Method: list(params)
  - Input: { "appId": "app-id", "type": "topic", "onlyUnacked": false, "limit": 0 }
  - Output: { "notifications": [ { "id","from","type","title","body","data","priority","requiresAck","ackedBy","timestamp","expiresAt","tags" }, ... ] }

- Method: clear(params)
  - Input options:
    - By id: { "id": "<notification-id>" }
    - By filter: { "appId": "app-id", "type": "topic", "onlyUnacked": false }
  - Output: { "removed": <count> }
  - Emits: cleared

- Method: ack(params)
  - Input: { "id": "<notification-id>", "by": "subscriber-id" }
  - Output: { "acked": true|false }
  - Emits: ack

- Method: getsubscribers()
  - Input: {}
  - Output:
    {
      "subscribers": [
        { "id": "...", "types": [...], "apps": [...] },
        ...
      ]
    }

Events:
- notification: { "id","from","type","title","body","data","priority","requiresAck","ackedBy","timestamp","expiresAt","tags" }
- ack: { "id","by","timestamp" }
- subscriberup: { "id","timestamp" }
- subscriberdrop: { "id","timestamp" }
- cleared: { "removed","appId","type","onlyUnacked","timestamp" }

Notes:
- The plugin uses Thunder's JSON-RPC event subscription model; consumers must use the Controller to register for events.
- When `storage.source` is "file" the store loads at initialization and saves on updates and deinitialization. Paths support `%persistentpath%` and `%datapath%` placeholders (resolved via THUNDER_PERSISTENT_PATH/THUNDER_DATA_PATH environment variables in this code drop).
- `maxPayloadBytes` constrains approximate publish payload size (title+body+type+data+tags lengths).
- In production deployments, consider enabling authentication via `enableTokenCheck` and relying on a security agent token.

Build & Install:
- Requires Thunder/WPEFramework (Core, Plugins, Tracing targets).
- Build example:
  - `cmake -S app-gateway1/AppNotifications -B build-notify && cmake --build build-notify`
- The library installs into `lib/Thunder/Plugins` and the config template into `share/Thunder/Plugins`.

Sample usage (via Thunder Controller JSON-RPC):
1) Publish:
```
{
  "jsonrpc":"2.0","id":1,"method":"AppNotifications.1.publish",
  "params": {
    "from":"com.example.app","type":"update","title":"Update Available",
    "body":"Version 2.0 now available","data":{"version":"2.0"},
    "priority":5,"requiresAck":true,"ttlSeconds":3600
  }
}
```

2) List:
```
{
  "jsonrpc":"2.0","id":2,"method":"AppNotifications.1.list",
  "params":{"appId":"com.example.app","type":"update","onlyUnacked":true}
}
```

3) Ack:
```
{
  "jsonrpc":"2.0","id":3,"method":"AppNotifications.1.ack",
  "params":{"id":"ntf-1","by":"subscriber-xyz"}
}
```

4) Subscribe (then register for event "notification" using Controller):
```
{
  "jsonrpc":"2.0","id":4,"method":"AppNotifications.1.subscribe",
  "params":{"id":"subscriber-xyz","types":["update"],"apps":["com.example.app"]}
}
```
