cmake_minimum_required(VERSION 3.10)

project(AppGatewayTests CXX)

# Find installed Thunder packages for linking.
find_package(Thunder REQUIRED)
find_package(ThunderInterfaces REQUIRED)

set(TESTS_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(PLUGIN_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
set(REPO_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)

# Common sources: compile the plugin sources into the test executables so we can control include precedence
set(PLUGIN_SOURCES
    ${PLUGIN_DIR}/Module.cpp
    ${PLUGIN_DIR}/AppGateway.cpp
)

# Include order is critical to ensure our test override of interfaces/IAppManager.h is used by the plugin.
# Place mocks first.
set(TEST_INCLUDE_DIRS
    ${TESTS_BASE_DIR}/mocks
    ${PLUGIN_DIR}
    ${REPO_ROOT}/dependencies/install/include/WPEFramework
    ${REPO_ROOT}/dependencies/install/include
)

# Unit tests executable
add_executable(AppGatewayUnitTests
    ${PLUGIN_SOURCES}
    ${TESTS_BASE_DIR}/unit/AppGatewayUnitTests.cpp
    ${TESTS_BASE_DIR}/mocks/MockAppManager.h
    ${TESTS_BASE_DIR}/mocks/MockShell.h
)

target_include_directories(AppGatewayUnitTests PRIVATE ${TEST_INCLUDE_DIRS})

target_link_libraries(AppGatewayUnitTests
    PRIVATE
        Thunder::Core
        Thunder::Plugins
        Thunder::Interfaces
)

target_compile_definitions(AppGatewayUnitTests PRIVATE MODULE_NAME=AppGateway)
set_target_properties(AppGatewayUnitTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

# Integration tests executable (JSON-RPC invocation patterns)
add_executable(AppGatewayJsonRpcTests
    ${PLUGIN_SOURCES}
    ${TESTS_BASE_DIR}/integration/AppGatewayJsonRpcTests.cpp
    ${TESTS_BASE_DIR}/mocks/MockAppManager.h
    ${TESTS_BASE_DIR}/mocks/MockShell.h
)

target_include_directories(AppGatewayJsonRpcTests PRIVATE ${TEST_INCLUDE_DIRS})

target_link_libraries(AppGatewayJsonRpcTests
    PRIVATE
        Thunder::Core
        Thunder::Plugins
        Thunder::Interfaces
)

target_compile_definitions(AppGatewayJsonRpcTests PRIVATE MODULE_NAME=AppGateway)
set_target_properties(AppGatewayJsonRpcTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

# Optionally enable CTest registration if project uses it
include(CTest)
if(BUILD_TESTING)
    add_test(NAME AppGatewayUnitTestsRun COMMAND AppGatewayUnitTests)
    add_test(NAME AppGatewayJsonRpcTestsRun COMMAND AppGatewayJsonRpcTests)
endif()
