from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeDeps

class IpcRecipe(ConanFile):
    name = "ipc"
    
    settings = "os", "compiler", "build_type", "arch"
    
    generators = (
        "CMakeToolchain"
    )

    tool_requires = (
        "cmake/3.26.3", 
    )

    options = {
        "build": [True, False], 
        "doc": [True, False]
    }
    
    default_options = {
        "build": True, 
        "doc": False
    }

    def requirements(self):
        if self.options.build:
            self.requires("flow/1.0")
            self.requires("gtest/1.14.0")
            self.requires("jemalloc/5.2.1")
    
    def build_requirements(self):
        if self.options.doc:
            self.tool_requires("doxygen/1.9.4")
            
    def layout(self):
        cmake_layout(self)

    def generate(self):
        cmake = CMakeDeps(self)
        if self.options.doc:
            cmake.build_context_activated = ["doxygen/1.9.4"]
        cmake.generate()
