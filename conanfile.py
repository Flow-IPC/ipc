from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain

class IpcRecipe(ConanFile):
    name = "ipc"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "build": [True, False], 
        "doc": [True, False]
    }
    
    default_options = {
        "build": True, 
        "doc": False
    }

    def generate(self):
        deps = CMakeDeps(self)
        if self.options.doc:
            deps.build_context_activated = ["doxygen/1.9.4"]
        deps.generate()
       
        toolchain = CMakeToolchain(self)
        if self.options.build:
            toolchain.variables["CFG_ENABLE_TEST_SUITE"] = "ON"
            toolchain.variables["JEMALLOC_PREFIX"] = "je_"
            toolchain.variables["CMAKE_VERBOSE_MAKEFILE"] = True
        toolchain.generate()
    
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
    
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
            
    def layout(self):
        cmake_layout(self)
