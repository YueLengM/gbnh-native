#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/../.."
cmake=/opt/gbnh-cmake/bin/cmake
build_dir=build/docker-windows-x86_64
bundle_dir="$PWD/dist/native"

# Use the mise-pinned CMake, not the version installed in the Docker image.
version="$("$cmake" --version)"
if [[ "${version%%$'\n'*}" != 'cmake version 3.31.12' ]]; then
  printf 'Expected CMake 3.31.12, got: %s\n' "$version" >&2
  exit 1
fi
printf '%s\n' "$version"
x86_64-w64-mingw32-gcc --version

"$cmake" -S . -B "$build_dir" -G 'Unix Makefiles' \
  -DCMAKE_TOOLCHAIN_FILE=/opt/win64/CMakeToolchain.txt \
  -DCMAKE_BUILD_TYPE=Release
"$cmake" --build "$build_dir" --target gbnh_native --parallel "$(nproc)"
"$cmake" --install "$build_dir" --prefix "$PWD/dist" --component gbnh-native

dll="$bundle_dir/gbnh_mgba.dll"
test -f "$dll"
x86_64-w64-mingw32-objdump -f "$dll" | tee "$build_dir/pe-format.txt"
grep -q 'file format pei-x86-64' "$build_dir/pe-format.txt"
x86_64-w64-mingw32-objdump -p "$dll" > "$build_dir/pe-details.txt"
mapfile -t dependencies < <(awk '/DLL Name:/ { print tolower($NF) }' "$build_dir/pe-details.txt")
if (( ${#dependencies[@]} == 0 )); then
  echo 'No Windows DLL imports were found; cannot verify dependencies.' >&2
  exit 1
fi

# With a statically linked core/runtime, every import must be a Windows component.
for dependency in "${dependencies[@]}"; do
  case "$dependency" in
    advapi32.dll|bcrypt.dll|crypt32.dll|gdi32.dll|imm32.dll|kernel32.dll|msvcrt.dll|ntdll.dll|ole32.dll|oleaut32.dll|rpcrt4.dll|secur32.dll|setupapi.dll|shell32.dll|shlwapi.dll|user32.dll|userenv.dll|version.dll|winmm.dll|ws2_32.dll|ucrtbase.dll|api-ms-win-*.dll|ext-ms-win-*.dll)
      ;;
    *)
      printf 'Unexpected runtime dependency: %s. Do not publish an incomplete bundle.\n' "$dependency" >&2
      exit 1
      ;;
  esac
done
printf '%s\n' "${dependencies[@]}" | sort -u | tee "$bundle_dir/dependencies.txt"

{
  echo 'gbnh-native Windows x86_64 MinGW bundle'
  echo 'mGBA: 0.10.5'
  echo 'mGBA source: https://github.com/mgba-emu/mgba/tree/0.10.5'
  printf 'Channel: %s\n' "${GBNH_BUILD_CHANNEL:?}"
  printf 'Version: %s\n' "${GBNH_BUILD_VERSION:?}"
  printf 'Commit: %s\n' "${GITHUB_SHA:?}"
  printf 'Ref: %s\n' "${GITHUB_REF:?}"
  printf 'Build image: %s\n' "${GBNH_DOCKER_IMAGE:?}"
  printf 'Compiler: %s %s\n' "$(x86_64-w64-mingw32-gcc -dumpmachine)" "$(x86_64-w64-mingw32-gcc -dumpfullversion)"
  echo 'CMake: 3.31.12'
} > "$bundle_dir/manifest.txt"
