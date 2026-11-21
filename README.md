# gbnh-native

Native build for GregBoyNH. This repository builds the Windows x86_64 C API
library that will be used by the Minecraft mod. JNI bindings and the emulator
session API are not implemented yet.

The mGBA revision is pinned to 0.10.5 in `cmake/mgba.cmake`. CMake is pinned
to 3.31.12 in `mise.toml`; CI does not use the CMake version from the Docker
image.

## GitHub Actions build

The build runs on Ubuntu using the official `mgba/windows:w64-20261111` Docker
image, pinned by its SHA-256 manifest digest in the workflow. The official
`/opt/win64/CMakeToolchain.txt` supplies the MinGW-w64 cross compiler and static
runtime link flags. The Linux CMake installed by mise is mounted read-only into
the container. No Docker installation is needed on a player's computer.

The build enables `LIBMGBA_ONLY` and the GBA core, statically linking mGBA into
`gbnh_mgba.dll`. The bundle includes the mGBA license, upstream third-party
license notices, source information, a build manifest, and `dependencies.txt`.
It does not include the desktop frontend or the mGBA SDK.

Before publication, CI verifies the PE architecture and rejects imports of
non-system DLLs, including dynamically linked MinGW runtimes. A separate Windows
job loads the packaged DLL with its search path restricted to the bundle and
Windows System32, then calls the version function and initializes/destroys the
GBA core three times. This check does not load a ROM or verify gameplay, JNI,
audio, rendering, or link emulation.

The artifact names no longer contain `ucrt`: the official Docker toolchain is
different from the previous MSYS2 UCRT64 build.

## Local development on Windows

MSYS2 UCRT64 remains an option for local development; it is not the CI release
toolchain. Run `mise trust` and `mise install` in this directory. Install the
compiler and Ninja from the MSYS2 UCRT64 terminal:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-ninja
```

Build from that terminal in the repository directory, with mise on PATH:

```bash
CMAKE_EXE="$(cygpath -u "$(mise where cmake)")/bin/cmake.exe"
"$CMAKE_EXE" --version
"$CMAKE_EXE" -G Ninja -S . -B build/local -DCMAKE_BUILD_TYPE=Release
"$CMAKE_EXE" --build build/local --target gbnh_native
```

Local MSYS2 builds can have different runtime dependencies. Use the verified
Docker CI bundle for distribution.

## CI builds and dev publication

- Every push to any branch builds a dev artifact named
  `gbnh-native-windows-x86_64-dev-<short-commit>`. A push containing several
  commits builds the branch tip once.
- Tag pushes retain their existing trigger and artifact-only flow, with the
  artifact now named `gbnh-native-windows-x86_64`.
- Manual runs remain available. Branch runs use dev metadata; tag runs use
  release metadata.

Each artifact contains a manifest with its channel, version, full commit SHA,
Git ref, Docker image digest, compiler version, and CMake version. Per-run
artifacts remain available on the Actions run page even if Windows verification
fails; such a run does not publish a dev release.

Successful branch builds and Windows checks update one rolling GitHub prerelease
tagged `dev`. It is created only if absent; later builds keep the same release
and replace its `gbnh-native-windows-x86_64.zip` download. On migration, the old
`gbnh-native-windows-x86_64-ucrt.zip` asset is removed from that release. The
`dev` tag moves to the corresponding commit, and the release notes link to that
build. Tag-triggered builds never update the dev prerelease.

Dev publication is serialized. A recorded run number prevents an older build
from replacing a newer published build. Failed builds or Windows checks do not
publish.
