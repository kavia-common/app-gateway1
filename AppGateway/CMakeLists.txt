cmake_minimum_required(VERSION 3.16)
project(AppGateway)

# Options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source/ThunderAppGateway)
set(SOURCES
    ${SRC_DIR}/AppGateway.cpp
    ${SRC_DIR}/Registry.cpp
    ${SRC_DIR}/LaunchClient.cpp
)

set(HEADERS
    ${SRC_DIR}/Module.h
    ${SRC_DIR}/AppGateway.h
    ${SRC_DIR}/Registry.h
    ${SRC_DIR}/LaunchClient.h
)

# Find Thunder/WPEFramework if available; allow building stubs if not found.
# In a real Thunder environment, WPEFramework provides imported targets:
#   WPEFramework::Core, WPEFramework::Plugins, WPEFramework::Tracing
find_package(WPEFramework QUIET)

add_library(${PROJECT_NAME} MODULE ${SOURCES} ${HEADERS})

# If WPEFramework targets are found, link against them. Otherwise, leave unlinked
# so this project can be used as a code drop-in.
if (TARGET WPEFramework::Core)
    target_link_libraries(${PROJECT_NAME}
        PUBLIC
            WPEFramework::Core
            WPEFramework::Plugins
            WPEFramework::Tracing
    )
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${SRC_DIR}
)

# Ensure MODULE_NAME macro is set for Thunder's Module.h usage
target_compile_definitions(${PROJECT_NAME} PRIVATE MODULE_NAME=${PROJECT_NAME})

# Installation paths are typical for Thunder plugin deployments; adapt as needed.
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib/Thunder/Plugins
)

# Install default configuration template
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config/AppGateway.conf.in
    DESTINATION share/Thunder/Plugins
)
