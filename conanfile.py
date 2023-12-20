from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain

class IpcRecipe(ConanFile):
    name = "ipc"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"

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
        # Boost libs, jemalloc lib, capnp/kj libs will build how they would've built anyway.
        "build_type_cflags_override": "ANY",
        "doc": [True, False],
    }

    default_options = {
        "build": True,
        "build_no_lto": False,
        "build_type_cflags_override": "",
        "doc": False,
    }

    def configure(self):
        if self.options.build:
            self.options["jemalloc"].enable_cxx = False
            self.options["jemalloc"].prefix = "je_"

    def generate(self):
        deps = CMakeDeps(self)
        if self.options.doc:
            # TODO: This magic version number is repeated several times.  Code reuse.
            deps.build_context_activated = ["doxygen/1.9.4"]
        deps.generate()

        toolchain = CMakeToolchain(self)
        if self.options.build:
            toolchain.variables["CFG_ENABLE_TEST_SUITE"] = "ON"
            toolchain.variables["JEMALLOC_PREFIX"] = self.options["jemalloc"].prefix
            if self.options.build_no_lto:
                toolchain.variables["CFG_NO_LTO"] = "ON"
        if self.options.doc:
            toolchain.variables["CFG_ENABLE_DOC_GEN"] = "ON"
            toolchain.variables["CFG_SKIP_CODE_GEN"] = "ON"
        if self.options.build_type_cflags_override:
            suffix = str(self.settings.build_type).upper()
            toolchain.variables["CMAKE_CXX_FLAGS_" + suffix] = self.options.build_type_cflags_override
            toolchain.variables["CMAKE_C_FLAGS_" + suffix] = self.options.build_type_cflags_override

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
            self.requires("capnproto/1.0.1")
            self.requires("flow/1.0")
            self.requires("gtest/1.14.0")
            self.requires("jemalloc/5.2.1")

    def build_requirements(self):
        self.tool_requires("cmake/3.26.3")
        if self.options.doc:
            self.tool_requires("doxygen/1.9.4")

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def layout(self):
        cmake_layout(self)
