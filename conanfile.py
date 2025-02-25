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

from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain

def load_version_from_file(version_path):  # Now takes version_path as an argument
    with open(version_path, "r") as version_file:
        # Read the entire file content and strip whitespace (matches what FlowLikeProject.cmake does).
        version = version_file.read().strip()
    return version

class IpcRecipe(ConanFile):
    name = "ipc"
    version = load_version_from_file("./VERSION")
    settings = "os", "compiler", "build_type", "arch"

    DOXYGEN_VERSION = "1.9.4"

    options = {
        "build": [True, False],
        "build_no_lto": [True, False],

        # Replaces the core C/C++ compiler flags (not linker flags) for the chosen settings.build_type.
        # Note that these appear *after* any C[XX]FLAGS and tools.build.c[xx]flags
        # on the compiler command line, so it would not be
        # sufficient to instead add the desired flags to tools.build* or *FLAGS, as if a setting in
        # the core CMake-chosen flags conflicts with one of those, the core one wins due to being last on command
        # line.  Long story short, this is for the core flags, typically: -O<something> [-g] [-DNDEBUG].
        # So default for, e.g., RelWithDebInfo in Linux = -O2 -g -DNDEBUG; and one could set
        # this option to "-O3 -g -DNDEBUG" to increase the optimization level.
        #
        # This affects `ipc` CMake only; meaning flow, ipc_*, ipc objects will have this overridden; while
        # Boost libs, jemalloc lib, capnp/kj, gtest libs will build how they would've built anyway.
        "build_type_cflags_override": ["ANY"],

        # 0 => default (let build script decide, as of this writing 17 meaning C++17) or a #, probably `20` as of
        # this writing.
        "build_cxx_std": ["ANY"],
        "doc": [True, False],
    }

    default_options = {
        "build": True,
        "build_no_lto": False,
        "build_type_cflags_override": "",
        "build_cxx_std": 0,
        "doc": False,
    }

    def configure(self):
        if self.options.build:
            self.options["jemalloc"].enable_cxx = False
            self.options["jemalloc"].prefix = "je_"

    def generate(self):
        deps = CMakeDeps(self)
        if self.options.doc:
            deps.build_context_activated = [f"doxygen/{self.DOXYGEN_VERSION}"]
        deps.generate()

        toolchain = CMakeToolchain(self)
        if self.options.build:
            toolchain.variables["CFG_ENABLE_TEST_SUITE"] = "ON"

            # We're not doing anything wrong here; we tell jemalloc itself to be built with this
            # API-name prefix via options.jemalloc.prefix, and then we tell Flow-IPC CMake script(s) what that was via
            # FLOW_IPC_JEMALLOC_PREFIX CMake variable (as if via `-DFLOW_IPC_JEMALLOC_PREFIX=je_` to `cmake`).  But:
            # Flow-IPC CMake script(s) can figure this out by itself; if FLOW_IPC_JEMALLOC_PREFIX is not given, then
            # it finds jemalloc-config binary, which a normal jemalloc install would put into (install-prefix)/bin,
            # and uses it to print the prefix.  However commenting out the next line does not work for some reason:
            # an error results saying jemalloc-config cannot be found, and that we should either provide path
            # to that binary via yet another CMake cache setting; or simply supply the prefix as
            # FLOW_IPC_JEMALLOC_PREFIX.
            # So this approach is fine; just it would be nice if the Conan magic worked in a more understandable way;
            # if Flow-IPC CMake script(s) can find libjemalloc.a and headers, why can't it
            # find_program(jemalloc-config)?  This slightly suggests something is "off" possibly*.
            # Still, the bottom line is it works, so this fallback is fine too.  One could say it'd be nice to
            # test Flow-IPC CMake script(s) smartness in the way that would be more likely used by the user;
            # but one could say that is splitting hairs too.
            #
            # (*) Update: Indeed something is "off": the Conan recipe for jemalloc specifically in package() chooses
            # to install the header(s) and libraries but not jemalloc-config (which would be used in this context)
            # or jemalloc.pc (which is used elsewhere in a CMake build script with its preferred
            # pkg_check_modules() technique).  TODO: So the true fix for both issues is to fix (or somehow wrangle)
            # jemalloc Conan recipe to install jemalloc fully/normally.  Meanwhile here we work around it this way,
            # while elsewhere we use a fallback to pkg_check_modules() which is why we need not, here, try to
            # determine the lib/header locations and supply them manually as well.  Once resolved delete this comment
            # and the next line.  (Elsewhere = ipc_shm_arena_lend/src/CMakeLists.txt; see the analogous TODO there.)
            toolchain.variables["FLOW_IPC_JEMALLOC_PREFIX"] = self.dependencies["jemalloc"].options.prefix

            if self.options.build_no_lto:
                toolchain.variables["CFG_NO_LTO"] = "ON"
            if self.options.build_type_cflags_override:
                suffix = str(self.settings.build_type).upper()
                toolchain.variables["CMAKE_CXX_FLAGS_" + suffix] = self.options.build_type_cflags_override
                toolchain.variables["CMAKE_C_FLAGS_" + suffix] = self.options.build_type_cflags_override
            if self.options.build_cxx_std != 0:
                toolchain.variables["CMAKE_CXX_STANDARD"] = self.options.build_cxx_std
        else:
            toolchain.variables["CFG_SKIP_CODE_GEN"] = "ON"
        if self.options.doc:
            toolchain.variables["CFG_ENABLE_DOC_GEN"] = "ON"

        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()

        # Cannot use cmake.build(...) because not possible to pass make arguments like --keep-going.
        if self.options.build:
            self.run("cmake --build . -- --keep-going VERBOSE=1")
        if self.options.doc:
            # Note: `flow_doc_public flow_doc_full` could also be added here and work; however
            # we leave that to `flow` and its own Conan setup.
            self.run("cmake --build . -- ipc_doc_public ipc_doc_full --keep-going VERBOSE=1")

    def requirements(self):
        if self.options.build:
            flow_version = load_version_from_file("./flow/VERSION")
            self.requires("capnproto/1.0.2")
            self.requires(f"flow/{flow_version}")
            self.requires("jemalloc/5.2.1")

    def build_requirements(self):
        self.tool_requires("cmake/3.26.3")
        if self.options.doc:
            self.tool_requires(f"doxygen/{self.DOXYGEN_VERSION}")

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def layout(self):
        cmake_layout(self)
