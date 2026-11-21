# JNI smoke test

`tests/java/com/yuelengm/gbnh/emulation/NativeBindings.java` mirrors the binding
class in the main `gbnh` mod. `NativeSmokeTest.java` mirrors the native integration
test from the mod's `src/nativeTest/java` source set. Keep these copies in sync
when changing ABI 1. Incompatible method/signature changes require an ABI bump.
The committed header in `include/` is generated from that binding class with
`javac -h`; the main mod also generates it under `build/generated/jni` during compilation.

The test is a standalone Java 8 program. It generates its own ARM homebrew ROM
and has no Minecraft, commercial cartridge or Nintendo BIOS dependencies. CI
runs it on Windows with `-Xcheck:jni` after the DLL import and C API checks.

```powershell
$sources = @(Get-ChildItem tests/java -Recurse -Filter *.java | ForEach-Object FullName)
javac -source 8 -target 8 -d build/jni-test $sources
java -Xcheck:jni -cp build/jni-test com.yuelengm.gbnh.emulation.NativeSmokeTest E:/path/gbnh_mgba.dll
```
