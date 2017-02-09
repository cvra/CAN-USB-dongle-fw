cmake_minimum_required(VERSION 2.6)

project(slcan_adapter)
set(CMAKE_BUILD_TYPE Debug)

include_directories(../src)

list( APPEND CMAKE_CXX_FLAGS "-std=c++0x -Wall -Wextra")
list( APPEND CMAKE_C_FLAGS "-Wall -Wextra -Wno-char-subscripts -std=gnu99")

add_executable(
    tests
    ../src/slcan.c
    ../src/timestamp/timestamp.c
    slcan_test.cpp
    timestamp_test.cpp
    )

target_link_libraries(
    tests
    CppUTest
    CppUTestExt
    )

# Run unit tests
add_custom_target(check ./tests -c DEPENDS tests)
