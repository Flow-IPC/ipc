cmake_minimum_required(VERSION 3.20)
project(Snap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SNAP_ENABLE_ZEROCOPY  "Enable MSG_ZEROCOPY send paths (Linux >= 4.14)" OFF)
option(SNAP_ENABLE_HUGEPAGES "Enable transparent hugepage-backed SHM and pool" OFF)
option(SNAP_BUILD_EXAMPLES   "Build all example programs" ON)
option(SNAP_BUILD_TESTS      "Build all test/bench programs" ON)

add_compile_options(
    -O3
    -march=native
    -mtune=native
    -fno-exceptions
    -fno-rtti
    -ffast-math
    -funroll-loops
    -fno-plt
    -Wall
    -Wextra
    -Wpedantic
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-flto=auto -fuse-linker-plugin -DNDEBUG)
    add_link_options(-flto=auto)
endif()

find_package(OpenSSL REQUIRED)

add_library(snap INTERFACE)
target_include_directories(snap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${OPENSSL_INCLUDE_DIR})
target_link_libraries(snap INTERFACE ${OPENSSL_LIBRARIES} pthread rt numa)

if(SNAP_ENABLE_ZEROCOPY)
    target_compile_definitions(snap INTERFACE SNAP_ENABLE_ZEROCOPY)
endif()
if(SNAP_ENABLE_HUGEPAGES)
    target_compile_definitions(snap INTERFACE SNAP_ENABLE_HUGEPAGES)
endif()

find_library(NUMA_LIB numa)
if(NUMA_LIB)
    set(SNAP_LINK_LIBS snap pthread rt ${NUMA_LIB})
    message(STATUS "libnuma found: ${NUMA_LIB}")
else()
    set(SNAP_LINK_LIBS snap pthread rt)
    message(STATUS "libnuma not found, NUMA features disabled")
endif()


if(SNAP_BUILD_EXAMPLES)
    # Core Examples
    add_executable(snap_01_inproc examples/01_inproc.cpp)
    add_executable(snap_02_shm    examples/02_shm.cpp)
    add_executable(snap_03_udp    examples/03_udp.cpp)
    add_executable(snap_04_tcp    examples/04_tcp.cpp)
    add_executable(snap_05_pubsub examples/05_pubsub.cpp)
    add_executable(snap_06_pipeline examples/06_pipeline.cpp)
    add_executable(snap_07_multicast examples/07_multicast.cpp)
    add_executable(snap_08_pool     examples/08_pool.cpp)
    add_executable(snap_09_batch    examples/09_batch.cpp)

    # Web Protocol Examples (New v3.0)
    add_executable(snap_10_http     examples/10_http_server.cpp)
    add_executable(snap_12_ws       examples/12_ws_chat_server.cpp)

    target_link_libraries(snap_01_inproc PRIVATE snap ${SNAP_LINK_LIBS})
    target_link_libraries(snap_02_shm    PRIVATE snap ${SNAP_LINK_LIBS})
    target_link_libraries(snap_03_udp    PRIVATE snap ${SNAP_LINK_LIBS})
    target_link_libraries(snap_04_tcp    PRIVATE snap ${SNAP_LINK_LIBS})
    target_link_libraries(snap_05_pubsub PRIVATE snap ${SNAP_LINK_LIBS})
    target_link_libraries(snap_06_pipeline PRIVATE snap ${SNAP_LINK_LIBS})
    target_link_libraries(snap_07_multicast PRIVATE snap ${SNAP_LINK_LIBS})
    target_link_libraries(snap_08_pool     PRIVATE snap ${SNAP_LINK_LIBS})
    target_link_libraries(snap_09_batch    PRIVATE snap ${SNAP_LINK_LIBS})
    target_link_libraries(snap_10_http     PRIVATE snap ${SNAP_LINK_LIBS})
    target_link_libraries(snap_12_ws       PRIVATE snap ${SNAP_LINK_LIBS})
endif()

if(SNAP_BUILD_TESTS)
    add_executable(snap_bench         test/bench.cpp)
    add_executable(snap_spsc_test     test/spsc_test.cpp)
    add_executable(snap_mpmc_test     test/mpmc_test.cpp)
    add_executable(snap_pool_test     test/pool_test.cpp)
    add_executable(snap_dispatch_test test/dispatch_test.cpp)

    foreach(target snap_bench snap_spsc_test snap_mpmc_test snap_pool_test snap_dispatch_test)
        target_link_libraries(${target} PRIVATE ${SNAP_LINK_LIBS})
    endforeach()
endif()

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/snap
    FILES_MATCHING PATTERN "*.hpp"
)