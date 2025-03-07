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

set(CAPNPC_IMPORT_DIRS ${FLOW_LIKE_META_ROOT_ipc_transport_structured}/src)
capnp_generate_cpp(capnp_generated_srcs capnp_generated_hdrs "schema.capnp")

# Create a custom target that depends on the generated files.
add_custom_target(perf_demo_schema_generation DEPENDS ${capnp_generated_srcs} ${capnp_generated_hdrs})

function(handle_binary name_sh jem_else_classic)
  if(jem_else_classic)
    set(name_pfx "shm_jemalloc")
    set(def_val "1")
  else()
    set(name_pfx "shm_classic")
    set(def_val "0")
  endif()
  set(name "perf_demo_${name_sh}_${name_pfx}.exec") # Must match common.cpp constant values.
  add_executable(${name} common.cpp "main_${name_sh}.cpp" ${capnp_generated_srcs})

  # Add explicit dependency on schema generation; otherwise things tend to go weird with a parallelized build.
  add_dependencies(${name} perf_demo_schema_generation)

  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_definitions(${name} PRIVATE "JEM_ELSE_CLASSIC=${def_val}")

  common_set_target_properties(${name})

  # Link good ol' libipc_shm_arena_lend.
  target_link_libraries(${name} PRIVATE ipc_shm_arena_lend)

  install(TARGETS ${name}
          RUNTIME DESTINATION bin)

  message(STATUS "Recommended: [cd ${CMAKE_INSTALL_PREFIX}/bin && "
                   "./${name}].")
  message(STATUS "Run srv program first in 1 terminal, then cli in another, as same user, from that dir.")
  message(STATUS "This variant internally uses [${name_pfx}] zero-copy backing mechanism.")
endfunction()

# Identical programs; except one pair uses SHM-jemalloc as internal SHM-provider; the other uses SHM-classic.
handle_binary(srv TRUE)
handle_binary(cli TRUE)
handle_binary(srv FALSE)
handle_binary(cli FALSE)
