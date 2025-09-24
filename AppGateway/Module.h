#pragma once
/*
    Module.h
    Common module header for the AppGateway Thunder plugin.

    Provides:
    - Module-wide includes for Thunder Core/Plugins
    - Trace category convenience if needed
    - MODULE_NAME macro binding (from CMake)
*/

#include <core/core.h>
#include <plugins/Plugin.h>
#include <tracing/Tracing.h>

// Ensure MODULE_NAME is defined by the build system (CMakeLists passes it).
#ifndef MODULE_NAME
#define MODULE_NAME AppGateway
#endif

// Convenience trace macro if needed later in the plugin.
#ifndef TRACE_CATEGORY
#define TRACE_CATEGORY AppGateway
#endif
