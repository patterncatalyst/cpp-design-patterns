from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain


class CppDesignPatternsConan(ConanFile):
    """Root Conan recipe for Design Patterns with C++.

    One pinned dependency for the whole repo: GoogleTest. Every example
    links against it through CTest via gtest_discover_tests, per the
    r01 build plan's locked toolchain table.
    """

    name = "cpp-design-patterns"
    version = "0.1.0"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires("gtest/1.17.0")

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        # `conan install` runs once per preset (build/cpp23, build/cpp20 —
        # see scripts/build.sh), each with its own --output-folder. Conan's
        # default behavior also merges a same-named "conan-release" entry
        # into the repo-root CMakeUserPresets.json on every install, which
        # collides across presets ("Duplicate preset: conan-release").
        # CMakePresets.json already points each preset straight at its own
        # build/<preset>/generators/conan_toolchain.cmake, so that merge
        # step is unnecessary here; disable it.
        tc.user_presets_path = None
        tc.generate()

    def layout(self):
        # scripts/build.sh always passes an explicit --output-folder
        # (build/cpp23 or build/cpp20, one per CMakePresets.json configure
        # preset), so pinning the generators sub-folder here is enough to
        # make every preset's `toolchainFile` land at a predictable,
        # preset-scoped path: build/<preset>/generators/conan_toolchain.cmake.
        self.folders.generators = "generators"
