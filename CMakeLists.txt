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

cmake_minimum_required(VERSION 3.26.3) # See FlowLikeProjectRoot.cmake for details.
# See that guy; it'll explain inside.  It mandates the following procedure and documents details.

set(PROJ "ipc")
set(PROJ_CAMEL "Ipc")
set(PROJ_VERSION "1.0")
set(PROJ_HUMAN "Flow-IPC")

project(${PROJ_CAMEL} VERSION ${PROJ_VERSION} LANGUAGES CXX)

# The children below rely on this being set and its value to detect they're part of the meta-project,
# as opposed to being loaded separately, and then to load needed things off this place.
# So in *nix setting this indicates to each kid below that they're a part of one big `make`.
# (Whereas otherwise each kid is on its own and assumes the all its dependencies have had `make` and `make install`
# done, and the exported stuff is somewhere the `make` will know to search.)
set(IPC_META_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Load these guys in dependency order (each guy precedes the guy that needs it, matching the required-dependency
# statements present in each guy's own project()y CMakeLists.txt).  Normally we'd just add_subdirectory() each
# one, in said dependency order, right here.  However we'd like a couple extra features:
#   - We want the user to be able to symlink any of the N components to their true location outside of `.`,
#     instead of it simply being here (such as after a tar unpack).
#     - A simple add_subdirectory(X), where X is a symlink to ../(somewhere), works great... except at least the
#       generated Makefile(s) will silently refuse to incrementally build if a source file in ../(somewhere)
#       is touched: it'll just do nothing. So add_subdirectory(REAL_X), where X points to REAL_X, appears to be
#       necessary.
#   - It'd be nice, and in the CMake spirit, to be able to simply specify each guy's location as a CMake cache
#     setting.  (No known use case truly requires this; a symlink should be possible to set up; but the
#     CMake-yness of it feels compelling.)
# So we do both.

# In dependency order.
set(IPC_META_PROJECTS flow ipc_core ipc_transport_structured ipc_session ipc_shm ipc_shm_arena_lend)
foreach(ipc_meta_project ${IPC_META_PROJECTS})
  block()
    # Regardless of anything try to resolve ./${ipc_meta_project} (whether it's simply a dir or a symlink).
    set(ipc_meta_project_dir ${CMAKE_CURRENT_SOURCE_DIR}/${ipc_meta_project})
    if(EXISTS ${ipc_meta_project_dir})
      file(REAL_PATH ${ipc_meta_project_dir} ipc_meta_project_dir)
      message("[./${ipc_meta_project}] exists; resolves to absolute un-symlinked path [${ipc_meta_project_dir}].  "
              "You may override it via cache setting.")
    else()
      message("[./${ipc_meta_project}] = [${ipc_meta_project_dir}] does not exist (as a dir or symlink); "
              "will need to be supplied via cache setting.")
      set(ipc_meta_project_dir "<unknown>")
    endif()
    set(IPC_META_ROOT_${ipc_meta_project} ${ipc_meta_project_dir} CACHE STRING
        "Location of sub-project [${ipc_meta_project}]; defaults to resolved [./${ipc_meta_project}] if exists.")
    message("After considering [./${ipc_meta_project}] and cache setting IPC_META_ROOT_${ipc_meta_project} "
            "determined ultimate location [${IPC_META_ROOT_${ipc_meta_project}}].")
    if(NOT (EXISTS ${IPC_META_ROOT_${ipc_meta_project}}))
      message(FATAL_ERROR "Unable to determine IPC_META_ROOT_${ipc_meta_project}; either symlink "
                          "[./${ipc_meta_project}], place sub-project there directly, or set "
                          "cache setting explicitly.  You may need to now clear cache (or blow away build) "
                          "for the symlink or direct-placement methods to work.")
    endif()

    # Finally!  Note in the build output the name is normalized to simply ${ipc_meta_project} (e.g., "flow").
    add_subdirectory(${IPC_META_ROOT_${ipc_meta_project}} ${ipc_meta_project})
  endblock()
endforeach()

if(NOT CFG_SKIP_CODE_GEN)
  if(CFG_ENABLE_TEST_SUITE)
    # We're about to add_subdirectory(test/suite), wherein there shall be much building too.
    # First a subtlety though: in the above kids we build various C++ and capnp things in a uniform way;
    # and that way is ensured by each kid on their own.  However since they're kids, in CMake that means we don't
    # inherit most things.  (E.g., the C++17 setting will not apply to us.)  To solve this just load the same
    # thingie the kids use to set up the environment.  However set this flag to prevent it from actually going
    # on to build our src/ and test/, etc.  We'll take care of that ourselves.
    set(FLOW_LIKE_PROJECT_ROOT_ENV_ONLY TRUE)
    include("${IPC_META_ROOT_flow}/tools/cmake/FlowLikeProjectRoot.cmake")

    message("This meta-project has a test/suite/ dir; and CFG_ENABLE_TEST_SUITE=ON enabled building/installing "
            "its contents (in addition to any such tests in the individual sub-projects).  "
            "This increases build time but allows serious testing of this meta-project as a whole.")
    add_subdirectory(test/suite)
  else()
    message("This project has a test/suite/ dir; but CFG_ENABLE_TEST_SUITE=OFF disabled building/installing its contents "
            "(as well as any such tests in the individual sub-projects).  "
            "This decreases build time; but the orthogonal CFG_SKIP_BASIC_TESTS=ON can still ensure the basics are okay.")
  endif()
endif()

# See this guy; it'll explain inside.  It mandates the following procedure and documents details.
set(DOC_GEN_CMAKE_SCRIPT "${IPC_META_ROOT_flow}/tools/cmake/FlowLikeDocGenerate.cmake")

# It may also be instructive to contrast with `flow`'s CMakeLists.txt.
#   - Flow has its own self-contained doc generation, though it uses the same FlowLikeDocGenerate utility.
#   - But Flow-IPC has monolithic documentation.
#     - Therefore you see several ipc_*/src/ipc source trees being merged together here.
#   - And Flow-IPC has a monolithic additional set of *.dox.txt files which add a guided Manual.
#     - Therefore you see src/doc/manual, which lives directly inside this meta-project as opposed to any kid.

set(DOC_INPUT
    ${IPC_META_ROOT_ipc_core}/src/ipc
    ${IPC_META_ROOT_ipc_transport_structured}/src/ipc
    ${IPC_META_ROOT_ipc_session}/src/ipc
    ${IPC_META_ROOT_ipc_shm}/src/ipc
    ${IPC_META_ROOT_ipc_shm_arena_lend}/src/ipc
    src/doc/manual)

# For now I (ygoldfel) am excluding echan's SHM-jemalloc stuff which *has* been integrated into Flow-IPC
# at large; I just have not gone over it and polished and eliminated Doxy-warnings and what-not.  echan may have
# partially; anyway it'll be done soon.
# TODO: That.
foreach(doc_input ${DOC_INPUT})
  if(${doc_input} MATCHES ".*/test/.*")
    message(FATAL_ERROR "A Doxygen peculiarity combined with our desire to skip doc-generating from */test/* "
                          "files sadly means your source locations like [${doc_input}] may not contain */test/* "
                          "anywhere in the path prefix... please rename those.  Sorry.")
  endif()
endforeach()
set(DOC_EXCLUDE_PATTERNS
    */test/* */ipc/session/standalone/shm/arena_lend/* */ipc/shm/arena_lend/*)

# Lastly the guided Manual has some images which live here.
set(DOC_IMAGE_PATH src/doc/manual/assets/img)

include(${DOC_GEN_CMAKE_SCRIPT})

# Last note: Indeed there's the Flow-IPC generated docs; and the separate Flow generated docs.
# By doing add_subdirectory(flow) we added the `make` (or equivalent) targets flow_doc_*.
# By doing the stuff immediately above we added the targets ipc_doc_*.
# Yay!
