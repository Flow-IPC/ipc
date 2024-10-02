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

cmake_minimum_required(VERSION 3.26.3) # See FlowLikeCodeGenerate.cmake for details.
# See that guy; it'll explain inside.  It mandates the following procedure and documents details.

set(PROJ "ipc")

message(CHECK_START "(Project [${PROJ}] root CMake script executing.)")
list(APPEND CMAKE_MESSAGE_INDENT "- ")

set(PROJ_CAMEL "Ipc")
set(PROJ_HUMAN "Flow-IPC")

# The children below rely on this being set and its value to detect they're part of the meta-project,
# as opposed to being loaded separately, and then to load needed things off this place.
# So in *nix setting this indicates to each kid below that they're a part of one big `make`.
# (Whereas otherwise each kid is on its own and assumes all its dependencies have had `make` and `make install`
# done, and the exported stuff is somewhere the `make` will know to search.)
set(FLOW_LIKE_META_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# The following might be a little tough to realize if looking through multiple layers of CMake scripts, so
# for your convenience let's summarize how we do the whole thing.
#   - For each project X of the sub-projects (listed just below in $IPC_META_PROJECTS), in dependency order:
#     - Run its X/CMakeLists.txt.  It will:
#       - include(FlowLikeCodeGenerate).  This makes code generation targets (unless turned off via a particular CFG_):
#         - Build library (from its X/src/CMakeLists.txt).
#         - Build tests (from its X/test/{basic|suite}/CMakeLists.txt); can be enabled/disabled via CFG_ x 2.
#       - include(FlowLikeDocGenerate).  This makes doc generation targets (unless turned off via a particular CFG_):
#         - (As of this writing only X = `flow` has its own documentation.  The rest of the docs are monolithic;
#           see a few lines down.  So only `flow` does that; the rest do not include(FlowLikeDocGenerate).
#   - For our own project, which of course depends on ~all of the above: We are already in our CMakeLists.txt;
#     you are reading it.  So after the above add_subdirectory() x (# of $IPC_META_PROJECTS), we do:
#       - include(FlowLikeCodeGenerate).  Same as for each guy above.
#         - Build tests (from its X/test/{basic|suite}/CMakeLists.txt); can be enabled/disabled via CFG_ x 2.
#         - (As of this writing no library to build; though it'd be easy to add it if it made sense.)
#       - include(FlowLikeDocGenerate).  Same as for each guy above.
#         - (This is where we generate the targets for monolithic documentation for all of ipc_* together.)
#
# Thus, assuming *nix, after going through each bullet above in order, the following is possible:
#   - `make`: Makes all the libraries and all the tests (modulo CFG_* enabling/disabling parts).
#   - `make flow_doc_full flow_doc_public`: Makes `flow` docs.
#   - `make ipc_doc_full ipc_doc_public`: Makes `ipc_*` monolithic docs.
#   - `make install`: Exports the results of `make` (code, not docs) into $CMAKE_INSTALL_PREFIX/{lib|bin|include|...}.
#   - There are also a couple of trivial and boring scripts named stage_generated_docs.sh which will package up
#     the flow and ipc docs into 2 tarballs suitable for HTTP(S) hosting; and place them in a certain location nearby.
#
# So now let's do the bullet points listed above.  First, the sub-projects.

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
#
# Now, in dependency order:
set(IPC_META_PROJECTS flow ipc_core ipc_transport_structured ipc_session ipc_shm ipc_shm_arena_lend)

foreach(ipc_meta_project ${IPC_META_PROJECTS})
  block()
    message(CHECK_START "(Determining absolute/un-symlinked path for sub-project [${ipc_meta_project}].)")
    list(APPEND CMAKE_MESSAGE_INDENT "- ")

    # Regardless of anything try to resolve ./${ipc_meta_project} (whether it's simply a dir or a symlink).
    set(ipc_meta_project_dir ${CMAKE_CURRENT_SOURCE_DIR}/${ipc_meta_project})
    if(EXISTS ${ipc_meta_project_dir})
      file(REAL_PATH ${ipc_meta_project_dir} ipc_meta_project_dir)
      message(VERBOSE "[./${ipc_meta_project}] exists => [${ipc_meta_project_dir}].  "
                        "You may override it via cache setting.")
    else()
      message(VERBOSE "[./${ipc_meta_project}] = [${ipc_meta_project_dir}] does not exist (as a dir or symlink); "
                        "will need to be supplied via cache setting.")
      set(ipc_meta_project_dir "<unknown>")
    endif()

    set(FLOW_LIKE_META_ROOT_${ipc_meta_project} ${ipc_meta_project_dir} CACHE STRING
        "Location of sub-project [${ipc_meta_project}]; defaults to resolved [./${ipc_meta_project}] if exists.")

    if(NOT (EXISTS ${FLOW_LIKE_META_ROOT_${ipc_meta_project}}))
      message(FATAL_ERROR "[${FLOW_LIKE_META_ROOT_${ipc_meta_project}}] does not exist.  Therefore: "
                            "Unable to determine FLOW_LIKE_META_ROOT_${ipc_meta_project}; either symlink "
                            "[./${ipc_meta_project}], place sub-project there directly, or set "
                            "cache setting explicitly.  You may need to now clear cache (or blow away build) "
                            "for the symlink or direct-placement methods to work.")
    endif()

    list(POP_BACK CMAKE_MESSAGE_INDENT)
    message(CHECK_PASS "(Path = [${FLOW_LIKE_META_ROOT_${ipc_meta_project}}]; it exists.)")
  endblock()
endforeach()

include("${FLOW_LIKE_META_ROOT_flow}/tools/cmake/FlowLikeProject.cmake") # Determine $PROJ_VERSION, at least.
project(${PROJ_CAMEL} VERSION ${PROJ_VERSION} DESCRIPTION ${PROJ_HUMAN} LANGUAGES CXX)

# Got through that; now actually do the add_subdirectory()s.  This needs to below the project() call just above.
# (Also, output/error handling is crisper this way than doing add_subdirectory() within the above loop.)
foreach(ipc_meta_project ${IPC_META_PROJECTS})
  # Finally!  Note in the build output the name is normalized to simply ${ipc_meta_project} (e.g., "flow").
  add_subdirectory(${FLOW_LIKE_META_ROOT_${ipc_meta_project}} ${ipc_meta_project})
endforeach()

# Going back to the summary comment, we're now done with the sub-projects' code generation and, where applicable,
# per-project doc generation.  Now finish up by doing our own stuff.

# The code generation, as noted, as of this writing = tests.  (Conceivably, if we had src/ stuff to build, then
# that would also get built just as for each of the IPC_META_PROJECTs.  We'd still just need the following line;
# no changes.)

include("${FLOW_LIKE_META_ROOT_flow}/tools/cmake/FlowLikeCodeGenerate.cmake")

# As noted in the summary FlowLikeCodeGenerate.cmake does not do doc generation.
# FlowLikeDocGenerate.cmake does that; and it's up to each project root CMake script to include() it.
# So let's do it, much like flow/CMakeLists.txt (which handles its own docs) does.

# See this guy; it'll explain inside.  It mandates the following procedure and documents details.
set(DOC_GEN_CMAKE_SCRIPT "${FLOW_LIKE_META_ROOT_flow}/tools/cmake/FlowLikeDocGenerate.cmake")

# It may also be instructive to contrast with `flow`'s CMakeLists.txt.
#   - Flow has its own self-contained doc generation, though it uses the same FlowLikeDocGenerate utility.
#   - But Flow-IPC has monolithic documentation.
#     - Therefore you see several ipc_*/src/ipc source trees being merged together here.
#   - And Flow-IPC has a monolithic additional set of *.dox.txt files which add a guided Manual.
#     - Therefore you see src/doc/manual, which lives directly inside this meta-project as opposed to any kid.

set(DOC_INPUT
    ${FLOW_LIKE_META_ROOT_ipc_core}/src/ipc
    ${FLOW_LIKE_META_ROOT_ipc_transport_structured}/src/ipc
    ${FLOW_LIKE_META_ROOT_ipc_session}/src/ipc
    ${FLOW_LIKE_META_ROOT_ipc_shm}/src/ipc
    ${FLOW_LIKE_META_ROOT_ipc_shm_arena_lend}/src/ipc
    src/doc/manual)

# For now I (ygoldfel) am excluding echan's SHM-jemalloc stuff which *has* been integrated into Flow-IPC
# at large; I just have not gone over it and polished and eliminated Doxy-warnings and what-not.  echan may have
# partially; anyway it'll be done soon.
# TODO: That.
set(DOC_EXCLUDE_PATTERNS
    */ipc/session/standalone/shm/arena_lend/* */ipc/shm/arena_lend/*)

# Lastly the guided Manual has some images which live here.
set(DOC_IMAGE_PATH src/doc/manual/assets/img)

include(${DOC_GEN_CMAKE_SCRIPT})

# Last note: Indeed there's the Flow-IPC generated docs; and the separate Flow generated docs.
# By doing add_subdirectory(flow) we added the `make` (or equivalent) targets flow_doc_*.
# By doing the stuff immediately above we added the targets ipc_doc_*.
# Yay!

list(POP_BACK CMAKE_MESSAGE_INDENT)
message(CHECK_PASS "(Done, success.)")
