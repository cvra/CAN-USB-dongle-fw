cmake_minimum_required(VERSION 2.6)

project(slcan_adapter)
set(CMAKE_BUILD_TYPE Debug)

include_directories(.)

add_definitions(-DTEST)

add_executable(
    tests
    ../src/slcan.c
    ./slcan_test.cpp
    ../src/timestamp/timestamp.c
    ../src/timestamp/timestamp_test.cpp
    )

target_link_libraries(
    tests
    CppUTest
    CppUTestExt
    )
