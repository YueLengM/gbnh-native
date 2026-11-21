# gbnh-native

Native build for GregBoyNH. This repository will produce the JNI library used by
the Minecraft mod and bundles the mGBA core with its Windows runtime files.

The mGBA revision is pinned in `cmake/mgba.cmake`. CMake is pinned to 3.31.12
in `mise.toml`; CMake 4 is not used by the build workflow.

On Windows, run `mise trust` and `mise install` in this directory. Install the
compiler and Ninja from the MSYS2 UCRT64 terminal:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-ninja
```

Build from that UCRT64 terminal in the repository directory. mise must be
available on PATH. Invoke its pinned CMake explicitly to avoid accidentally
using another version supplied by MSYS2 or the system:

```bash
CMAKE_EXE="$(cygpath -u "$(mise where cmake)")/bin/cmake.exe"
"$CMAKE_EXE" --version
"$CMAKE_EXE" -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release
"$CMAKE_EXE" --build build
```

GitHub Actions installs CMake through mise, verifies version 3.31.12, and uses
that executable for configure, build, and install. MSYS2 supplies MinGW-w64 GCC
and Ninja only.

The first milestone builds a small native smoke-test library. JNI bindings and
the emulator session API will be added after the toolchain is verified.
