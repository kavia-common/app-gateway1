#pragma once

// Thunder/WPEFramework consolidated module include
// In real builds, <WPEFramework/Module.h> includes core, plugins, tracing, and other essentials.
#ifdef __has_include
  #if __has_include(<WPEFramework/Module.h>)
    #include <WPEFramework/Module.h>
  #else
    // Fallback to common Thunder headers if consolidated header is not present
    #include <core/core.h>
    #include <plugins/plugins.h>
    #include <tracing/tracing.h>
  #endif
#else
  #include <WPEFramework/Module.h>
#endif

#ifndef MODULE_NAME
#define MODULE_NAME AppGateway
#endif

// Ensure we have the proper namespaces available
using namespace WPEFramework;
