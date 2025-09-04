CMAKE_MINIMUM_REQUIRED(VERSION 3.25)
PROJECT(async-function-execution)

SET(CMAKE_CXX_STANDARD 20)

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
    ADD_DEFINITIONS(-DDEBUG)
ENDIF()

# GTest static librari is not compiled as pic
SET(CMAKE_POSITION_INDEPENDENT_CODE OFF)

FIND_PACKAGE(GTest REQUIRED)
FILE(GLOB_RECURSE source_test_files "${CMAKE_CURRENT_LIST_DIR}/source/*")
ADD_EXECUTABLE(async_function_execution_tests ${source_test_files} ${CMAKE_CURRENT_LIST_DIR}/mainTests.cpp)
TARGET_INCLUDE_DIRECTORIES(async_function_execution_tests PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include/")
TARGET_LINK_LIBRARIES(async_function_execution_tests PUBLIC ${GTEST_LIBRARIES} async-function-execution-static)

TARGET_COMPILE_OPTIONS(async_function_execution_tests PRIVATE -Wall -Wextra -Wpedantic)

INCLUDE(GoogleTest)
GTEST_DISCOVER_TESTS(async_function_execution_tests)