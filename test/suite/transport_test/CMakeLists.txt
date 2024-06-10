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

# This test requires some little silly capnp schemas.
find_package(CapnProto CONFIG REQUIRED)

# Test program: transport_test.
# It's a compendium of tests and test tools for testing ipc::transport (including ipc::transport::struc)
# and ipc::session.   SHM modes are tested extensively.
#
# Please see ./README.txt.

set(NAME_ROOT transport_test)
set(NAME "${NAME_ROOT}.exec")

# Please see explanation in ipc_session's src/CMakeLists.txt for why we do the following.
set(CAPNPC_IMPORT_DIRS ${FLOW_LIKE_META_ROOT_ipc_transport_structured}/src
                       ${FLOW_LIKE_META_ROOT_ipc_shm}/src)

capnp_generate_cpp(capnp_generated_srcs capnp_generated_hdrs_ignored "ex.capnp")

add_executable(${NAME} script_interpreter.cpp ex_guy.cpp transport_test.cpp ${capnp_generated_srcs})

# So that schema.capnp.h is found by main.cpp's #include.
target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Do stuff we've resolved to do on all our targets.
common_set_target_properties(${NAME})

# Link good ol' libipc_shm_arena_lend.
target_link_libraries(${NAME} PRIVATE ipc_shm_arena_lend)

# Export (to separate directory, as there are multiple useful files) if they `make install` or equivalent.
install(TARGETS ${NAME}
        RUNTIME DESTINATION bin/${NAME_ROOT})
# Our input scripts for transport_test.exec SCRIPTED mode.  (One can also input one's own or even use it
# interactively from stdin.  These provide good coverage however.)
install(FILES README.txt cli-script.txt srv-script.txt DESTINATION bin/${NAME_ROOT})
