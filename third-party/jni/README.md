# OpenJDK JNI headers

Unmodified OpenJDK JNI headers copied from the installed JetBrains Runtime SDK:
`JBRSDK-17.0.14+1-1367.22-nomod` (Java 17.0.14).

- `include/jni.h`
- `include/win32/jni_md.h` (stored under `win32/`)
- `legal/java.base/LICENSE`

Source: https://github.com/JetBrains/JetBrainsRuntime/tree/jbr17
Upstream: https://github.com/openjdk/jdk17u

These headers carry GPLv2 with the Classpath exception, as indicated in the file headers.
The Windows platform header is intentional: the official Linux Docker image cross-compiles a Windows DLL.
There is no JVM/linker dependency; the JVM supplies the JNI function table at runtime.
The bridge returns `JNI_VERSION_1_6` and only uses JNI functions available in Java 8.
CI compiles its smoke test for Java 8 and runs it with Java 8.
