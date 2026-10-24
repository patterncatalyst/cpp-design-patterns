from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class SolidFirstPrinciplesConan(ConanFile):
    """Standalone Conan recipe for examples/03-solid-first-principles.

    Mirrors the repo root's conanfile.py so this example builds on its own,
    without configuring anything else in the repo:
    `cd examples/03-solid-first-principles && conan install . && cmake --preset cpp23 &&
    cmake --build --preset cpp23`. When this example is built as part of
    the repo root's superbuild instead, this file is never invoked: the
    root conanfile.py's own gtest/1.17.0 requirement already satisfies this
    example's find_package(GTest REQUIRED) call.
    """

    name = "cpp-design-patterns-03-solid-first-principles"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("gtest/1.17.0")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        # See the root conanfile.py for why this merge step is disabled:
        # it collides across the cpp23/cpp20 output folders.
        tc.user_presets_path = None
        tc.generate()

    def layout(self):
        self.folders.generators = "generators"
