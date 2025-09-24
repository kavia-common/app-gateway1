cmake_minimum_required(VERSION 3.16)

project(AppGatewayTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Paths
set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../Source/ThunderAppGateway)

# Try to find GoogleTest. If not found, we will compile a basic assert-based runner.
find_package(GTest QUIET)

# Common sources from the plugin to link into tests (we test implementation classes directly).
set(APPGATEWAY_SOURCES
    ${SRC_DIR}/AppGateway.cpp
    ${SRC_DIR}/Registry.cpp
    ${SRC_DIR}/LaunchClient.cpp
)
set(APPGATEWAY_HEADERS
    ${SRC_DIR}/Module.h
    ${SRC_DIR}/AppGateway.h
    ${SRC_DIR}/Registry.h
    ${SRC_DIR}/LaunchClient.h
)

# Tests
set(TEST_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/test_AppGateway_Registry.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_AppGateway_LaunchClient.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test_AppGateway_Plugin.cpp
)

# Build test executable differently depending on GTest availability.
if (GTest_FOUND)
    add_executable(AppGateway_tests
        ${TEST_SOURCES}
        ${APPGATEWAY_SOURCES}
        ${APPGATEWAY_HEADERS}
    )
    target_include_directories(AppGateway_tests PUBLIC ${SRC_DIR})
    target_link_libraries(AppGateway_tests PRIVATE GTest::GTest GTest::Main)
    # If Thunder dev packages are available link to them; otherwise rely on headers-only uses in these tests.
    find_package(WPEFramework QUIET)
    if (TARGET WPEFramework::Core)
        target_link_libraries(AppGateway_tests PRIVATE
            WPEFramework::Core
            WPEFramework::Plugins
            WPEFramework::Tracing
        )
    endif()

    enable_testing()
    add_test(NAME AppGateway_tests COMMAND AppGateway_tests)
else()
    message(WARNING "GoogleTest not found. Building basic assert-based runner instead.")
    add_executable(AppGateway_tests_basic
        ${CMAKE_CURRENT_LIST_DIR}/test_basic_runner.cpp
        ${TEST_SOURCES}
        ${APPGATEWAY_SOURCES}
        ${APPGATEWAY_HEADERS}
    )
    target_include_directories(AppGateway_tests_basic PUBLIC ${SRC_DIR})
    find_package(WPEFramework QUIET)
    if (TARGET WPEFramework::Core)
        target_link_libraries(AppGateway_tests_basic PRIVATE
            WPEFramework::Core
            WPEFramework::Plugins
            WPEFramework::Tracing
        )
    endif()

    enable_testing()
    add_test(NAME AppGateway_tests_basic COMMAND AppGateway_tests_basic)
endif()
