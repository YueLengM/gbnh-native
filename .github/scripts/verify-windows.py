"""Check the packaged C API using only bundle-local and Windows system DLLs."""

import ctypes
import os
from pathlib import Path
import sys


def main():
    if os.name != "nt":
        raise SystemExit("This check must run on Windows.")
    if len(sys.argv) != 2:
        raise SystemExit("Usage: verify-windows.py <path-to-gbnh_mgba.dll>")

    dll_path = Path(sys.argv[1]).resolve(strict=True)
    # Exclude PATH, the working directory, and installed development toolchains.
    load_flags = 0x00000100 | 0x00000800  # DLL_LOAD_DIR | SYSTEM32
    library = ctypes.CDLL(str(dll_path), winmode=load_flags)
    library.gbnh_native_version.argtypes = []
    library.gbnh_native_version.restype = ctypes.c_char_p
    library.gbnh_native_mgba_available.argtypes = []
    library.gbnh_native_mgba_available.restype = ctypes.c_int

    version = library.gbnh_native_version()
    if not version or not version.startswith(b"gbnh-native-"):
        raise RuntimeError(f"Unexpected native version: {version!r}")
    for _ in range(3):
        if library.gbnh_native_mgba_available() != 1:
            raise RuntimeError("The GBA core could not be initialized.")
    print(f"Loaded {dll_path.name}: {version.decode('ascii')}")
    print("GBA core initialization and cleanup passed (3 iterations).")


if __name__ == "__main__":
    main()
