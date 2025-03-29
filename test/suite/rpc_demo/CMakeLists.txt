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

# This test requires a capnp schema (featuring RPC stuff -- interfaces and such).
find_package(CapnProto CONFIG REQUIRED)

capnp_generate_cpp(capnp_generated_srcs capnp_generated_hdrs_ignored "schema.capnp")

function(handle_binary name_sh jem_else_classic)
  if(jem_else_classic)
    set(name_pfx "shm_jemalloc")
    set(def_val "1")
  else()
    set(name_pfx "shm_classic")
    set(def_val "0")
  endif()
  set(name "rpc_demo_${name_sh}_${name_pfx}.exec") # Must match common.cpp constant values.
  add_executable(${name} common.cpp "main_${name_sh}.cpp" ${capnp_generated_srcs})

  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_definitions(${name} PRIVATE "JEM_ELSE_CLASSIC=${def_val}")

  common_set_target_properties(${name})

  # Link good ol' libipc_shm_arena_lend.
  target_link_libraries(${name} PRIVATE ipc_shm_arena_lend)

  install(TARGETS ${name}
          RUNTIME DESTINATION bin/rpc_demo/${name_sh}) # Must match common.cpp constant values.
endfunction()

# Identical programs; except one pair uses SHM-jemalloc as internal SHM-provider; the other uses SHM-classic.
handle_binary(srv TRUE)
handle_binary(cli TRUE)
handle_binary(srv FALSE)
handle_binary(cli FALSE)
