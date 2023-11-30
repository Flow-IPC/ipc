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

# We'll need Google's unit-test framework, so let's find that.
block()
  set(lib_name "gtest")
  # Search for static lib specifically.  TODO: Is this needed really?  Being paranoid here?
  if(UNIX)
    set(lib_name "lib${lib_name}.a")
  else()
    message(FATAL_ERROR "Unsupported OS; please modify script to search for static lib Windows-style.")
  endif()
  find_library(GTEST_LIB NAMES ${lib_name})
  if(NOT GTEST_LIB)
    message(FATAL_ERROR
              "Could not find Google unit-test gtest library ([${lib_name}]) location via CMake uber-searcher "
              "find_library().  Do not fret: Please build gtest if not yet done and try again; "
              "if it is still not found then please supply the full path to the library file (including "
              "the file itself) as the CMake knob (cache setting) GTEST_LIB.")
  endif()
  message(VERBOSE "CMake uber-searcher find_library() located Google unit-test gtest lib (see below); "
                    "though you can override this via cache setting.")

  # Search for its include-root.
  find_path(GTEST_INCLUDE_DIR NAMES gtest/gtest.h)
  if(NOT GTEST_INCLUDE_DIR)
    message(FATAL_ERROR
              "Could not find Google unit-test gtest/*.h include-path via CMake uber-searcher "
              "find_path(gtest/gtest.h).  Do not fret: Please build gtest if not yet done and try again; "
              "if it is still not found then please supply the full path to gtest/gtest.h (excluding the "
              "directory and file themselves) as the CMake knob (cache setting) GTEST_INCLUDE_DIR.")
  endif()

  # Note: These days Google's unit-test framework, at least when built with CMake, exports
  # the stuff needed to just do find_package(GTest).  We could use that above, as we do for Cap'n Proto
  # and others.  However I (ygoldfel) have it on good authority that people build the notoriously easy-to-build
  # GTest in a myriad ways, and this find_package() support may not be produced.  So we go with the
  # naughty more-manual way above.
endblock()

message(STATUS "Google unit-test gtest lib location: [${GTEST_LIB}].")

# This test requires some little silly capnp schemas.
find_package(CapnProto CONFIG REQUIRED)

# Test program: unit_test.
# Unit tests are built on Google's unit-test framework.  The conventions we use are roughly as follows.
#   - The unit test files shall be interspersed together with the code of the various libraries libipc_*,
#     albeit always in directories named test/.
#     - In addition the needed program driver(s) (i.e., guys with main()), in the root namespace, shall *not* be
#       so interspersed but rather will live separately under unit_test/ (here).
#   - Let `class X` in namespace N (so N::X) be located in D/X.hpp (at least), where D is a directory usually
#     corresponding to namespace N (more or less).
#   - Its unit test might be then called X_test and shall reside in namespace N::test::X_test.
#     Its source code shall be in D/test/X_test.cpp.
#   - Any supporting code outside of that file can be anywhere under D/test, in files named `test_*.hpp` and
#     `test_*.cpp`; a convention might be to start with `test_X.?pp`, though even more support files may also
#     be present.
#     - For support files like this potentially applicable to ~all unit tests, they shall live in ipc::test
#       and therefore ipc/test/.
#
# The relevant translation units (.cpp, .capnp -> .c++) shall be listed directly below, in normal CMake fashion,
# under the relevant unit-test executables.  We do not make an intermediate libtest* or anything like that;
# the relevant object files are compiled directly from their relevant source files.  (This is pretty common
# when using the GOogle unit-test framework.)  Do note, again, that those relevant source files are directly
# under the sub-projects' source trees: ipc_*/src/.../test/....
#
# This executable, libipc_unit_test, is the compendium of unit tests to execute.
# In addition there are more executables which it will itself run as needed as part of certain specific unit tests.
# These are built below.

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
                             ${IPC_META_ROOT_ipc_core}/src
                             ${IPC_META_ROOT_ipc_transport_structured}/src
                             ${IPC_META_ROOT_ipc_session}/src
                             ${IPC_META_ROOT_ipc_shm}/src
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

set(isal_root ${IPC_META_ROOT_ipc_shm_arena_lend}/src)

set(SRCS
    ${IPC_META_ROOT_ipc_core}/src/ipc/test/test_common_util.cpp
    ${IPC_META_ROOT_ipc_core}/src/ipc/test/test_config.cpp
    ${IPC_META_ROOT_ipc_core}/src/ipc/test/test_file_util.cpp
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
    ${IPC_META_ROOT_ipc_core}/src/ipc/test/test_common_util.cpp
    ${IPC_META_ROOT_ipc_core}/src/ipc/test/test_config.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_borrower.cpp
    test_borrower_main.cpp)
unset(CAPNP_SCHEMAS)
handle_binary(libipc_test_borrower.exec)

set(SRCS
    ${IPC_META_ROOT_ipc_core}/src/ipc/test/test_common_util.cpp
    ${IPC_META_ROOT_ipc_core}/src/ipc/test/test_file_util.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server_executor.cpp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server.cpp
    ${isal_root}/ipc/shm/arena_lend/test/test_shm_object.cpp
    test_jemalloc_shm_session_server_main.cpp)
set(CAPNP_SCHEMAS
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_message.capnp
    ${isal_root}/ipc/session/standalone/shm/arena_lend/jemalloc/test/test_session_metadata.capnp)
handle_binary(libipc_test_jemalloc_shm_session_server.exec)
