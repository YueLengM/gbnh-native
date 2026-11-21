package com.yuelengm.gbnh.emulation;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Paths;
import java.util.Arrays;

/** Also compiled independently by gbnh-native CI against its copy of NativeBindings. No Minecraft dependencies. */
public final class NativeSmokeTest {

    private NativeSmokeTest() {}

    public static void main(String[] args) {
        if (args.length != 1) throw new IllegalArgumentException("Expected the absolute DLL path");
        System.load(
            Paths.get(args[0])
                .toAbsolutePath()
                .toString());
        run();
    }

    public static void run() {
        check(NativeBindings.abiVersion() == NativeBindings.ABI_VERSION, "JNI ABI");
        check(
            NativeBindings.version()
                .startsWith("gbnh-native-"),
            "native version");
        expect(IllegalArgumentException.class, () -> NativeBindings.create(new byte[4], null, 44100));
        expect(IllegalArgumentException.class, () -> NativeBindings.create(new byte[1024], null, 44100));
        expect(IllegalStateException.class, () -> NativeBindings.reset(Long.MAX_VALUE));
        byte[] rom = testRom();
        int[] pixels = new int[240 * 160];
        short[] audio = new short[4096];
        byte[] seed = new byte[32768];
        Arrays.fill(seed, (byte) 0x6d);
        long first = NativeBindings.create(rom, null, 44100);
        long second = 0;
        try {
            second = NativeBindings.create(rom, seed, 44100);
            final long handle = first;
            expect(IllegalArgumentException.class, () -> NativeBindings.runFrame(handle, 0, new int[1], audio));
            boolean audible = false;
            for (int frame = 0; frame < 12; frame++) {
                int samples = NativeBindings.runFrame(first, 0, pixels, audio);
                check(samples > 0 && samples <= 2048, "audio sample count");
                for (int sample = 0; sample < samples * 2; sample++) audible |= audio[sample] != 0;
            }
            check(audible, "generated PSG tone");
            check(pixels[120 + 80 * 240] == 0xffff0000, "BGR-to-ARGB red framebuffer");
            byte[] battery = NativeBindings.saveRam(first);
            check(battery.length == 32768 && battery[0] == 0x5a && battery[1] == 1, "SRAM writes and released A key");
            NativeBindings.runFrame(first, 1, pixels, audio);
            check(NativeBindings.saveRam(first)[1] == 0, "pressed A key reaches ROM");
            byte[] snapshot = NativeBindings.saveState(first);
            check(snapshot.length > 1024, "snapshot serialization");
            NativeBindings.runFrame(first, 0, pixels, audio);
            check(NativeBindings.saveRam(first)[1] == 1, "SRAM changed after snapshot");
            NativeBindings.loadState(first, snapshot);
            check(NativeBindings.saveRam(first)[1] == 0, "snapshot restores SRAM");
            expect(IllegalArgumentException.class, () -> NativeBindings.loadState(handle, new byte[16]));
            NativeBindings.runFrame(second, 0, pixels, audio);
            check(NativeBindings.saveRam(second)[16] == 0x6d, "seed save restored");
            check((NativeBindings.saveRam(first)[16] & 255) == 255, "sessions do not share SRAM");
            long restored = NativeBindings.create(rom, battery, 44100);
            try {
                NativeBindings.runFrame(restored, 0, pixels, audio);
                check(Arrays.equals(battery, NativeBindings.saveRam(restored)), "battery save round trip");
            } finally {
                NativeBindings.destroy(restored);
            }
            NativeBindings.reset(first);
            NativeBindings.runFrame(first, 0, pixels, audio);
        } finally {
            if (second != 0) NativeBindings.destroy(second);
            NativeBindings.destroy(first);
        }
        final long closed = first;
        expect(IllegalStateException.class, () -> NativeBindings.reset(closed));
        expect(IllegalStateException.class, () -> NativeBindings.destroy(closed));
        // Use an RTC-enabled cartridge header to exercise the pinned core's hardware override.
        // The program remains the same original generated homebrew.
        byte[] rtcRom = testRom();
        System.arraycopy("AXVE".getBytes(StandardCharsets.US_ASCII), 0, rtcRom, 0xac, 4);
        byte[] rtcSave = new byte[131072 + 16];
        Arrays.fill(rtcSave, (byte) 0xff);
        byte[] rtc = { 0x26, 0x11, 0x21, 0x06, 0x12, 0x34, 0x56, 0x40, 0, 0, 0, 0, 0, 0, 0, 0 };
        ByteBuffer.wrap(rtc)
            .order(ByteOrder.LITTLE_ENDIAN)
            .putLong(8, System.currentTimeMillis() / 1000);
        System.arraycopy(rtc, 0, rtcSave, 131072, rtc.length);
        long clock = NativeBindings.create(rtcRom, rtcSave, 44100);
        try {
            check(Arrays.equals(rtcSave, NativeBindings.saveRam(clock)), "battery save retains mGBA RTC trailer");
        } finally {
            NativeBindings.destroy(clock);
        }
        System.out.println(
            "JNI smoke passed: frame/color, keys, PCM tone, SRAM/RTC, snapshots, multiple cores and invalid handles.");
    }

    /** Generates an original ARM homebrew: red screen, square wave, and active-low A key written to SRAM. */
    static byte[] testRom() {
        ArmRom rom = new ArmRom();
        rom.data.putInt(0, 0xea00002e); // Branch from the entry point to 0xC0.
        byte[] title = "GBNH JNITEST".getBytes(StandardCharsets.US_ASCII);
        System.arraycopy(title, 0, rom.data.array(), 0xa0, title.length);
        rom.data.put(0xb2, (byte) 0x96);
        int checksum = 0x19;
        for (int index = 0xa0; index <= 0xbc; index++) checksum += rom.data.get(index) & 255;
        rom.data.put(0xbd, (byte) -checksum);
        rom.write16(0x04000000, 0x0403); // Mode 3, BG2 enabled.
        rom.write16(0x04000084, 0x0080); // Sound master enable.
        rom.write16(0x04000080, 0x1177); // PSG channel 1 to left and right.
        rom.write16(0x04000082, 0x0002); // PSG 100% volume.
        rom.write16(0x04000060, 0x0000);
        rom.write16(0x04000062, 0xf080);
        rom.write16(0x04000064, 0x86d6); // Trigger a sustained square wave.
        rom.literal(0, 0x06000000);
        rom.literal(1, 31); // GBA native red.
        rom.literal(2, 240 * 160);
        int draw = rom.position;
        rom.emit(0xe0c010b2); // STRH r1, [r0], #2
        rom.emit(0xe2522001); // SUBS r2, r2, #1
        rom.branch(1, draw); // BNE
        rom.literal(0, 0x0e000000);
        rom.literal(1, 0x5a);
        rom.emit(0xe5c01000); // STRB r1, [r0]
        rom.literal(2, 0x04000130);
        int input = rom.position;
        rom.emit(0xe1d230b0); // LDRH r3, [r2]
        rom.emit(0xe2033001); // AND r3, r3, #1
        rom.emit(0xe5c03001); // STRB r3, [r0, #1]
        rom.branch(14, input);
        return rom.data.array();
    }

    static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    static void expect(Class<? extends Throwable> type, Runnable action) {
        try {
            action.run();
        } catch (Throwable exception) {
            if (type.isInstance(exception)) return;
            throw new AssertionError("Expected " + type.getName(), exception);
        }
        throw new AssertionError("Expected " + type.getName());
    }

    private static final class ArmRom {

        final ByteBuffer data = ByteBuffer.allocate(32768)
            .order(ByteOrder.LITTLE_ENDIAN);
        int position = 0xc0;
        int pool = 0x400;

        void emit(int instruction) {
            data.putInt(position, instruction);
            position += 4;
        }

        void literal(int register, int value) {
            data.putInt(pool, value);
            emit(0xe59f0000 | (register << 12) | (pool - position - 8));
            pool += 4;
        }

        void write16(int address, int value) {
            literal(4, address);
            literal(5, value);
            emit(0xe1c450b0);
        }

        void branch(int condition, int target) {
            emit((condition << 28) | 0x0a000000 | (((target - position - 8) / 4) & 0x00ffffff));
        }
    }
}
