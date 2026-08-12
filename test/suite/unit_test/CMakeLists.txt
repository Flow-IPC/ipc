# Flow-IPC
# Copyright 2023 Akamai Technologies, Inc.
#
# Licensed under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in
# compliance with the License.  You may obtain a copy
# of the License at
#
#   https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in
# writing, software distributed under the License is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing
# permissions and limitations under the License.

include("${FLOW_LIKE_META_ROOT_flow}/tools/cmake/FlowLikeLoadGoogleTest.cmake")
# See instructions in that file re. mechanics and conventions of GoogleTest-using programs;
# we shall declare a few now.  GTEST_LIB and GTEST_INCLUDE_DIR are set for us.

# This test requires some little silly capnp schemas.
find_package(CapnProto CONFIG REQUIRED)

# Test program: unit_test.

# Test program libipc_test_borrower.exec: executed by ipc/shm/arena_lend/jemalloc/test/ipc_arena_test.

# Test program libipc_test_jemalloc_shm_session_server.exec: executed by
# ipc/shm/arena_lend/jemalloc/test/ipc_arena_test.  (TODO: Is that a copy/paste error or accurate?)

# Make those binaries in very similar ways; call this for each.
# Before calling: Load SRCS; also load CAPNP_SCHEMA_GROUPS (list of group names like "isal struc")
# and for each group G load CAPNP_SCHEMAS_<G> (list of .capnp files).  Each group's schemas share
# a common CAPNPC_SRC_PREFIX = ${<G>_root}.  (E.g., group "isal" uses ${isal_root}; "struc" uses ${struc_root}.)
# CAPNP_SCHEMA_GROUPS may be empty/unset, in which case no schemas are compiled.
function(handle_binary name_root)
  set(name "${name_root}.exec")

  # For each schema group, run capnp_generate_cpp() with the appropriate prefix; accumulate results.
  set(all_capnp_generated_srcs "")
  set(all_capnp_generated_hdrs "")
  foreach(group ${CAPNP_SCHEMA_GROUPS})
    if(CAPNP_SCHEMAS_${group})
      set(CAPNPC_SRC_PREFIX ${${group}_root})
      unset(grp_srcs)
      unset(grp_hdrs)
      capnp_generate_cpp(grp_srcs grp_hdrs ${CAPNP_SCHEMAS_${group}})
      list(APPEND all_capnp_generated_srcs ${grp_srcs})
      list(APPEND all_capnp_generated_hdrs ${grp_hdrs})
    endif()
  endforeach()

  if(all_capnp_generated_srcs)
    # Create a custom target that depends on the generated files.
    add_custom_target(${name_root}_schema_generation
                      DEPENDS ${all_capnp_generated_srcs} ${all_capnp_generated_hdrs})
  endif()

  add_executable(${name} ${SRCS} ${all_capnp_generated_srcs})

  if(all_capnp_generated_srcs)
    # Add explicit dependency on schema generation; otherwise things tend to go weird with a parallelized build.
    add_dependencies(${name} ${name_root}_schema_generation)
  endif()

  # Test header files are *not* exported for user to include; rather they exist only to be cross-included by
  # each other (namely the support files are).  For that to work add the source dir(s) into the include-path.
  target_include_directories(${name} PRIVATE
                             ${GTEST_INCLUDE_DIR}
                             ${FLOW_LIKE_META_ROOT_ipc_core}/src
                             ${FLOW_LIKE_META_ROOT_ipc_transport_structured}/src
                             ${FLOW_LIKE_META_ROOT_ipc_session}/src
                             ${FLOW_LIKE_META_ROOT_ipc_shm}/src
                             ${isal_root}
                             # So that generated-above .capnp.h are found by SRCS' .cpps' `#include`s:
                             ${CMAKE_CURRENT_BINARY_DIR})

  # Do stuff we've resolved to do on all our targets.
  common_set_target_properties(${name})

  # Link good ol' libipc_shm_arena_lend; and gtest.
  target_link_libraries(${name} PRIVATE ipc_shm_arena_lend ${GTEST_LIB})

  # Might as well export if they `make install` or equivalent.
  install(TARGETS ${name}
          RUNTIME DESTINATION bin)
endfunction()

set(session_root ${FLOW_LIKE_META_ROOT_ipc_session}/src)
set(isal_root ${FLOW_LIKE_META_ROOT_ipc_shm_arena_lend}/src)
set(core_root ${FLOW_LIKE_META_ROOT_ipc_core}/src)
set(shm_root ${FLOW_LIKE_META_ROOT_ipc_shm}/src)
set(struc_root ${FLOW_LIKE_META_ROOT_ipc_transport_structured}/src)

set(SRCS
    ${FLOW_LIKE_META_ROOT_flow}/src/flow/test/test_common_util.cpp
    ${FLOW_LIKE_META_ROOT_flow}/src/flow/test/test_file_util.cpp
    ${FLOW_LIKE_META_ROOT_ipc_core}/src/ipc/test/test_common_util.cpp
    ${core_root}/ipc/transport/test/blob_transport_stats_test.cpp
    ${shm_root}/ipc/shm/classic/test/pool_arena_stats_test.cpp
    ${struc_root}/ipc/transport/struc/test/channel_test.cpp
    ${struc_root}/ipc/transport/struc/test/channel_test_mq_bipc.cpp
    ${struc_root}/ipc/transport/struc/test/channel_test_mq_posix.cpp
    ${struc_root}/ipc/transport/struc/test/channel_stats_test.cpp
    ${struc_root}/ipc/transport/struc/test/channel_stats_test_shm_classic.cpp
    ${struc_root}/ipc/transport/struc/test/channel_stats_test_shm_jemalloc.cpp
    ${struc_root}/ipc/transport/struc/test/msg_test.cpp
    ${struc_root}/ipc/transport/struc/test/serializer_stats_test.cpp
    ${struc_root}/ipc/transport/struc/test/serializer_stats_test_shm_classic.cpp
    ${struc_root}/ipc/transport/struc/test/serializer_stats_test_shm_jemalloc.cpp
    ${session_root}/ipc/session/test/session_connect_test.cpp
    ${session_root}/ipc/session/test/session_connect_test_shm_classic.cpp
    ${session_root}/ipc/session/test/session_connect_test_shm_jemalloc.cpp
    ${session_root}/ipc/session/test/session_persistent_cleanup_test.cpp
    ${session_root}/ipc/session/test/session_persistent_cleanup_test_shm_classic.cpp
    ${session_root}/ipc/session/test/session_persistent_cleanup_test_shm_jemalloc.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/shm_session_test.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server_executor.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server_launcher.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/test/borrower_shm_pool_collection_repository_test.cpp
    ${isal_root}/ipc/shm/arena_lend/detail/test/lend_tracker_pool_test.cpp
    ${isal_root}/ipc/shm/arena_lend/detail/test/shm_pool_offset_ptr_data_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/shm_pool_offset_ptr_test.cpp
    ${isal_root}/ipc/shm/arena_lend/jemalloc/test/jemalloc_pages_test.cpp
    ${isal_root}/ipc/shm/arena_lend/jemalloc/test/ipc_arena_stats_test.cpp
    ${isal_root}/ipc/shm/arena_lend/jemalloc/test/ipc_arena_test.cpp
    ${isal_root}/ipc/shm/arena_lend/jemalloc/test/memory_manager_test.cpp
    ${isal_root}/ipc/shm/arena_lend/jemalloc/test/thread_cache_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/borrower_shm_pool_collection_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/divisible_shm_pool_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/memory_manager_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/owner_shm_pool_collection_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/shm_pool_collection_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/owner_shm_pool_repository_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/shm_pool_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_borrower.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_owner_shm_pool_collection.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_shm_object.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_shm_pool_collection.cpp
    ${isal_root}/ipc/shm/test/lend_borrow_test.cpp
    test_main.cpp)
set(CAPNP_SCHEMA_GROUPS isal struc)
set(CAPNP_SCHEMAS_isal
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_message.capnp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_session_metadata.capnp)
set(CAPNP_SCHEMAS_struc
    ${struc_root}/ipc/transport/struc/test/test_schema.capnp)
handle_binary(libipc_unit_test)

set(SRCS
    ${FLOW_LIKE_META_ROOT_flow}/src/flow/test/test_common_util.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_borrower.cpp
    test_borrower_main.cpp)
unset(CAPNP_SCHEMA_GROUPS)
handle_binary(libipc_test_borrower)

set(SRCS
    ${FLOW_LIKE_META_ROOT_flow}/src/flow/test/test_common_util.cpp
    ${FLOW_LIKE_META_ROOT_flow}/src/flow/test/test_file_util.cpp
    ${FLOW_LIKE_META_ROOT_ipc_core}/src/ipc/test/test_common_util.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server_executor.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_shm_object.cpp
    test_jemalloc_shm_session_server_main.cpp)
set(CAPNP_SCHEMA_GROUPS isal)
set(CAPNP_SCHEMAS_isal
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_message.capnp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_session_metadata.capnp)
handle_binary(libipc_test_jemalloc_shm_session_server)
