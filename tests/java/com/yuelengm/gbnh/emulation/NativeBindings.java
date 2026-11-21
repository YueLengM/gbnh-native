package com.yuelengm.gbnh.emulation;

/** JNI ABI 1. Keep this class and its native method names unchanged when packaging. */
final class NativeBindings {

    static final int ABI_VERSION = 1;

    private NativeBindings() {}

    static synchronized native int abiVersion();

    static synchronized native String version();

    static synchronized native long create(byte[] rom, byte[] save, int sampleRate);

    static synchronized native void destroy(long handle);

    static synchronized native int runFrame(long handle, int keys, int[] argb, short[] stereo);

    static synchronized native void reset(long handle);

    static synchronized native byte[] saveRam(long handle);

    static synchronized native byte[] saveState(long handle);

    static synchronized native void loadState(long handle, byte[] state);
}
