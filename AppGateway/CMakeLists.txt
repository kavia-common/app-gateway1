cmake_minimum_required(VERSION 3.10)

project(AppGateway CXX)

# Find Thunder (WPEFramework) and ThunderInterfaces packages.
# These package names are common in RDK/Thunder-based environments.
find_package(Thunder REQUIRED)
find_package(ThunderInterfaces REQUIRED)

set(PLUGIN_NAME AppGateway)

# Sources
set(PLUGIN_SOURCES
    Module.cpp
    AppGateway.cpp
)

set(PLUGIN_HEADERS
    Module.h
    AppGateway.h
)

add_library(${PLUGIN_NAME} SHARED
    ${PLUGIN_SOURCES}
    ${PLUGIN_HEADERS}
)

target_include_directories(${PLUGIN_NAME}
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Link with Thunder libraries
target_link_libraries(${PLUGIN_NAME}
    PRIVATE
        Thunder::Core
        Thunder::Plugins
        Thunder::Interfaces
)

# Standard definitions used by Thunder plugins
target_compile_definitions(${PLUGIN_NAME}
    PRIVATE
        MODULE_NAME=${PLUGIN_NAME}
)

set_target_properties(${PLUGIN_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    FRAMEWORK FALSE
    POSITION_INDEPENDENT_CODE ON
)

# Installation path follows typical Thunder plugin layout.
install(TARGETS ${PLUGIN_NAME}
    LIBRARY DESTINATION lib/wpeframework/plugins
)
