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

# Test program libipc_test_borrower.exec: executed by ipc/shm/arena_lend/jemalloc/test/shm_pool_collection_test.

# Test program libipc_test_jemalloc_shm_session_server.exec: executed by
# ipc/shm/arena_lend/jemalloc/test/shm_pool_collection_test.  (TODO: Is that a copy/paste error or accurate?)

# Make those binaries in very similar ways; call this for each.
function(handle_binary name) # Load SRCS and CAPNP_SCHEMAS before calling.
  if(CAPNP_SCHEMAS)
    set(CAPNPC_SRC_PREFIX ${isal_root})
    capnp_generate_cpp(capnp_generated_srcs capnp_generated_hdrs_ignored ${CAPNP_SCHEMAS})
  endif()

  add_executable(${name} ${SRCS} ${capnp_generated_srcs})

  # Test header files are *not* exported for user to include; rather they exist only to be cross-included by
  # each other (namely the support files are).  For that to work add the source dir(s) into the include-path.
  target_include_directories(${name} PRIVATE
                             ${GTEST_INCLUDE_DIR}
                             ${FLOW_LIKE_META_ROOT_ipc_core}/src
                             ${FLOW_LIKE_META_ROOT_ipc_transport_structured}/src
                             ${FLOW_LIKE_META_ROOT_ipc_session}/src
                             ${FLOW_LIKE_META_ROOT_ipc_shm}/src
                             ${isal_root}
                             # So that generated-above .capnp.h are found by SRCS' .cpps' `#include`s.:
                             ${CMAKE_CURRENT_BINARY_DIR})

  # Do stuff we've resolved to do on all our targets.
  common_set_target_properties(${name})

  # Link good ol' libipc_shm_arena_lend; and gtest.
  target_link_libraries(${name} PRIVATE ipc_shm_arena_lend ${GTEST_LIB})

  # Might as well export if they `make install` or equivalent.
  install(TARGETS ${name}
          RUNTIME DESTINATION bin)
endfunction()

set(isal_root ${FLOW_LIKE_META_ROOT_ipc_shm_arena_lend}/src)

set(SRCS
    ${FLOW_LIKE_META_ROOT_flow}/src/flow/test/test_common_util.cpp
    ${FLOW_LIKE_META_ROOT_flow}/src/flow/test/test_file_util.cpp
    ${FLOW_LIKE_META_ROOT_ipc_core}/src/ipc/test/test_common_util.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/shm_session_test.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server_executor.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server_launcher.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/test/borrower_collection_test.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/test/borrower_shm_pool_collection_repository_test.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/test/lender_collection_test.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/test/shm_session_data_test.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/test/test_shm_session_util.cpp
    ${isal_root}/ipc/shm/arena_lend/detail/test/shm_pool_offset_ptr_data_test.cpp
    ${isal_root}/ipc/shm/arena_lend/jemalloc/test/ipc_arena_test.cpp
    ${isal_root}/ipc/shm/arena_lend/jemalloc/test/jemalloc_pages_test.cpp
    ${isal_root}/ipc/shm/arena_lend/jemalloc/test/memory_manager_test.cpp
    ${isal_root}/ipc/shm/arena_lend/jemalloc/test/shm_pool_collection_test.cpp
    ${isal_root}/ipc/shm/arena_lend/jemalloc/test/test_util.cpp
    ${isal_root}/ipc/shm/arena_lend/test/borrower_shm_pool_collection_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/borrower_shm_pool_repository_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/divisible_shm_pool_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/memory_manager_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/owner_shm_pool_collection_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/owner_shm_pool_listener_for_repository_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/shm_pool_collection_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/shm_pool_holder_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/shm_pool_offset_ptr_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/shm_pool_repository_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/shm_pool_test.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_borrower.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_owner_shm_pool_collection.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_shm_object.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_shm_pool_collection.cpp
    test_main.cpp)
set(CAPNP_SCHEMAS
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_message.capnp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_session_metadata.capnp)
handle_binary(libipc_unit_test.exec)

set(SRCS
    ${FLOW_LIKE_META_ROOT_flow}/src/flow/test/test_common_util.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_borrower.cpp
    test_borrower_main.cpp)
unset(CAPNP_SCHEMAS)
handle_binary(libipc_test_borrower.exec)

set(SRCS
    ${FLOW_LIKE_META_ROOT_flow}/src/flow/test/test_common_util.cpp
    ${FLOW_LIKE_META_ROOT_flow}/src/flow/test/test_file_util.cpp
    ${FLOW_LIKE_META_ROOT_ipc_core}/src/ipc/test/test_common_util.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server_executor.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_shm_object.cpp
    test_jemalloc_shm_session_server_main.cpp)
set(CAPNP_SCHEMAS
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_message.capnp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_session_metadata.capnp)
handle_binary(libipc_test_jemalloc_shm_session_server.exec)
