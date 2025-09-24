# LaunchDelegate Thunder Plugin

LaunchDelegate provides a unified JSON-RPC interface for launching and activating applications or intents across the platform. It resolves a target (appId, URI, or intent) to a provider and delegates execution to that provider. It also tracks launches, supports cancellation, exposes provider capabilities, and emits status events.

- Callsign: LaunchDelegate
- Interface: LaunchDelegate.1
- Methods:
  - launch
  - resolve
  - cancel
  - getStatus
  - getCapabilities
  - getProviders
  - activate
- Events:
  - launchstatus
  - foregroundchanged
  - providerstatus (optional)

Configuration example (Thunder service entry):
{
  "callsign": "LaunchDelegate",
  "classname": "LaunchDelegate",
  "locator": "libWPEFrameworkLaunchDelegate.so",
  "autostart": true,
  "configuration": {
    "enabled": true,
    "defaultTimeoutMs": 15000,
    "allowedApps": ["com.example.player", "com.example.gallery"],
    "allowedSchemes": ["https", "app", "video"],
    "providerChain": ["local-appmgr", "deeplink"],
    "routing": [
      {"match": "scheme:https", "providerId": "deeplink"},
      {"match": "appId:*", "providerId": "local-appmgr"}
    ],
    "providers": {
      "local-appmgr": { "type": "local" },
      "deeplink": { "type": "deeplink" }
    }
  }
}

Note on COM-RPC:
- This implementation follows Thunder JSON-RPC best practices. The provider abstraction allows plugging a COM-RPC backend if/when an Exchange::ILaunch interface is defined. The current stub providers are in-process adapters meant for development and unit testing.

Security:
- Use `allowedApps` and `allowedSchemes` to limit what can be launched via configuration. For production, integrate token-based authorization through JSONRPC TokenCheckFunction (pattern used in other plugins in this repository).

Design reference documents:
- kavia-docs/plugin-design-LaunchDelegate.md
- kavia-docs/plugin-design-AppGateway.md
- kavia-docs/plugin-design-App2AppProvider.md
- kavia-docs/plugin-design-AppNotification.md
- kavia-docs/thunder-plugins-architecture.md
