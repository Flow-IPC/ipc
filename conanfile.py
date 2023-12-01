from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeDeps

class IpcConanFile(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = (
        "CMakeToolchain"
    )

    requires = (
        "flow/1.0",
        "gtest/1.14.0",
        "jemalloc/5.2.1"
    )
    
    tool_requires = (
        "cmake/3.26.3", 
        "doxygen/1.9.4"
    )
    
    def layout(self):
        cmake_layout(self)

    def generate(self):
        cmake = CMakeDeps(self)
        cmake.build_context_activated = ["doxygen/1.9.4"]
        cmake.generate()
