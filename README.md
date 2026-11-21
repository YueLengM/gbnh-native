# gbnh-native

Native build for GregBoyNH. This repository will produce the JNI library used by
the Minecraft mod and bundles the mGBA core with its Windows runtime files.

The mGBA revision is pinned in `cmake/mgba.cmake`. Use the MSYS2 UCRT64
terminal on Windows:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The first milestone builds a small native smoke-test library. JNI bindings and
the emulator session API will be added after the toolchain is verified.
