# Copyright (c) 2019 Harikrishnan (harikrishnan.prabakaran@gmail.com) Distributed under the MIT
# License. See accompanying file LICENSE.md or copy at http://opensource.org/licenses/MIT

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_NAME sheap)

project(${PROJECT_NAME} C CXX)
set(PROJECT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(${CMAKE_PROJECT_NAME}_VERSION_MAJOR 0)
set(${CMAKE_PROJECT_NAME}_VERSION_MINOR 0)
set(${CMAKE_PROJECT_NAME}_VERSION_PATCH 1)
set(PROJECT_VERSION
    "${${CMAKE_PROJECT_NAME}_VERSION_MAJOR}.${${CMAKE_PROJECT_NAME}_VERSION_MINOR}.${${CMAKE_PROJECT_NAME}_VERSION_PATCH}"
)

set(LIB "${PROJECT_NAME}")
set(TEST "test_${PROJECT_NAME}")
set(BENCH "bench_${PROJECT_NAME}")

set(SRC_PATH "${PROJECT_PATH}/src")
set(TEST_DIR "${PROJECT_PATH}/test")
set(BENCH_DIR "${SRC_PATH}/benchmark")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

message(STATUS "Building ${CMAKE_BUILD_TYPE} version!")

include(AdditionalBuildTypes.cmake)

find_package(Threads)
find_package(Boost REQUIRED COMPONENTS system thread)

if(NOT MSVC)
    add_compile_options("-Wall" "-pedantic")
    set(LIBRARY_LINK_TYPE SHARED)
endif(NOT MSVC)

if(BUILD_COVERAGE_ANALYSIS)
    include(CodeCoverage.cmake)
    append_coverage_compiler_flags()
    setup_target_for_coverage_lcov(NAME coverage EXECUTABLE ${TEST} DEPENDENCIES ${TEST})
endif(BUILD_COVERAGE_ANALYSIS)

set(SRC "${SRC_PATH}/Sheap.cpp")
add_library(${LIB} ${LIBRARY_LINK_TYPE} ${SRC})
target_include_directories(
    ${LIB}
    PRIVATE include
    INTERFACE include)
target_link_libraries(
    ${LIB}
    PRIVATE ${CMAKE_THREAD_LIBS_INIT} Boost::boost
    INTERFACE Boost::boost)

add_subdirectory(${TEST_DIR})
add_subdirectory(${BENCH_DIR})
