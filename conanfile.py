from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout, CMakeDeps, CMakeToolchain

class IpcRecipe(ConanFile):
    name = "ipc"
    version = "1.0"
    settings = "os", "compiler", "build_type", "arch"

    options = {
        "build": [True, False], 
        "doc": [True, False],
        "instal_path": "ANY"
    }
    
    default_options = {
        "build": True, 
        "doc": False,
        "install_path": "."
    }

    def configure(self):
        if self.options.build:
            self.options["jemalloc"].enable_cxx = False 
            self.options["jemalloc"].prefix = "je_"
    
    def generate(self):
        deps = CMakeDeps(self)
        if self.options.doc:
            deps.build_context_activated = ["doxygen/1.9.4"]
        deps.generate()
       
        toolchain = CMakeToolchain(self)
        if self.options.build:
            toolchain.variables["CFG_ENABLE_TEST_SUITE"] = "ON"
            toolchain.variables["CMAKE_VERBOSE_MAKEFILE"] = True
            toolchain.variables["JEMALLOC_PREFIX"] = self.options["jemalloc"].prefix
            toolchain.variables["CMAKE_PREFIX_PATH"] = self.options.install_path
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

    def package(self):
        cmake = CMake(self)
        cmake.install()
            
    def layout(self):
        cmake_layout(self)
