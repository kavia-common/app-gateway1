# AppGateway Thunder Plugin

AppGateway centralizes application discovery and lifecycle operations behind a versioned Thunder JSON-RPC API. It delegates platform-specific launching and termination to a LaunchDelegate (via COM-RPC), while exposing consistent client-facing methods and lifecycle events.

Key features:
- JSON-RPC methods: ping, list, launch, stop, status, getCapabilities (version 1).
- Events: statechanged, capabilitiesupdated.
- Configuration via Thunder Controller: registry source/path, launch timeouts, execution mode, and optional token validation.
- Registry abstraction to manage app metadata and capabilities.
- LaunchClient stub included for development; replace with a COM-RPC connector to `Exchange::ILaunch` in production.

Directory layout:
- Source/ThunderAppGateway/ — C++ sources for the plugin, registry, and launch client.
- config/AppGateway.conf.in — Default configuration template.
- CMakeLists.txt — Build system for generating the plugin shared library.

Notes:
- This repository includes a stub LaunchClient that simulates state transitions to enable development without a running LaunchDelegate.
- In a full Thunder environment, link against `WPEFramework::Core`, `WPEFramework::Plugins`, and `WPEFramework::Tracing` and replace LaunchClient with a COM-RPC smart connector to LaunchDelegate.
