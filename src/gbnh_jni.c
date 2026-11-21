#include "gbnh_native.h"
#include <mgba/flags.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/savedata.h>
#include <mgba-util/vfs.h>
#include <jni.h>
#include "com_yuelengm_gbnh_emulation_NativeBindings.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef COLOR_16_BIT
#error GBNH JNI requires the default 32-bit mGBA framebuffer
#endif

#define WIDTH 240
#define HEIGHT 160
#define AUDIO_FRAMES 2048
#define MAX_SESSIONS 16
#define MAX_STATE_SIZE (4 * 1024 * 1024)
#define JNI_METHOD(name) Java_com_yuelengm_gbnh_emulation_NativeBindings_##name
#define STATE_FLAGS (SAVESTATE_SAVEDATA | SAVESTATE_RTC)

/* All entry points are serialized by NativeBindings.class. Handles are IDs, never pointers. */
struct Session {
    jlong id;
    struct mCore* core;
    color_t pixels[WIDTH * HEIGHT];
    jint argb[WIDTH * HEIGHT];
    short audio[AUDIO_FRAMES * 2];
    struct Session* next;
};

static struct Session* sessions;
static jlong next_id = 1;
static unsigned session_count;

static void fail(JNIEnv* env, const char* type, const char* message) {
    jclass exception = (*env)->FindClass(env, type);
    if (exception) {
        (*env)->ThrowNew(env, exception, message);
        (*env)->DeleteLocalRef(env, exception);
    }
}

static struct Session* find_session(JNIEnv* env, jlong id) {
    struct Session* session;
    for (session = sessions; session; session = session->next) {
        if (session->id == id) return session;
    }
    fail(env, "java/lang/IllegalStateException", "GBA session is closed or its handle is invalid");
    return NULL;
}

static void free_session(struct Session* session) {
    if (session->core) {
        mCoreConfigDeinit(&session->core->config);
        session->core->deinit(session->core);
    }
    free(session);
}

static struct VFile* copy_bytes(JNIEnv* env, jbyteArray bytes, jsize minimum, jsize maximum) {
    if (!bytes) {
        fail(env, "java/lang/NullPointerException", "Native input array is null");
        return NULL;
    }
    jsize size = (*env)->GetArrayLength(env, bytes);
    if (size < minimum || size > maximum) {
        fail(env, "java/lang/IllegalArgumentException", "Native input array has an invalid size");
        return NULL;
    }
    struct VFile* vf = VFileMemChunk(NULL, size);
    if (!vf) {
        fail(env, "java/lang/OutOfMemoryError", "Cannot allocate native input");
        return NULL;
    }
    if (size) {
        void* data = vf->map(vf, size, MAP_WRITE);
        if (!data) {
            vf->close(vf);
            fail(env, "java/lang/OutOfMemoryError", "Cannot map native input");
            return NULL;
        }
        (*env)->GetByteArrayRegion(env, bytes, 0, size, data);
        vf->unmap(vf, data, size);
        if ((*env)->ExceptionCheck(env)) {
            vf->close(vf);
            return NULL;
        }
    }
    return vf;
}

static void clear_audio(struct Session* session) {
    blip_clear(session->core->getAudioChannel(session->core, 0));
    blip_clear(session->core->getAudioChannel(session->core, 1));
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void) vm;
    (void) reserved;
    return JNI_VERSION_1_6;
}

JNIEXPORT jint JNICALL JNI_METHOD(abiVersion)(JNIEnv* env, jclass cls) {
    (void) env; (void) cls;
    return 1;
}

JNIEXPORT jstring JNICALL JNI_METHOD(version)(JNIEnv* env, jclass cls) {
    (void) cls;
    return (*env)->NewStringUTF(env, gbnh_native_version());
}

JNIEXPORT jlong JNICALL JNI_METHOD(create)(JNIEnv* env, jclass cls, jbyteArray rom, jbyteArray save, jint sample_rate) {
    (void) cls;
    if (sample_rate < 8000 || sample_rate > 48000) {
        fail(env, "java/lang/IllegalArgumentException", "Audio rate must be between 8000 and 48000 Hz");
        return 0;
    }
    if (session_count >= MAX_SESSIONS || next_id == LLONG_MAX) {
        fail(env, "java/lang/IllegalStateException", "Native GBA session limit reached");
        return 0;
    }
    struct VFile* rom_file = copy_bytes(env, rom, 0xC0, 32 * 1024 * 1024);
    if (!rom_file) return 0;
    struct VFile* save_file = save ? copy_bytes(env, save, 0, 1024 * 1024) : VFileMemChunk(NULL, 0);
    if (!save_file) {
        rom_file->close(rom_file);
        if (!(*env)->ExceptionCheck(env)) fail(env, "java/lang/OutOfMemoryError", "Cannot allocate GBA save");
        return 0;
    }
    struct Session* session = calloc(1, sizeof(*session));
    struct mCore* core = mCoreCreate(mPLATFORM_GBA);
    if (!session || !core) {
        free(session);
        free(core);
        rom_file->close(rom_file);
        save_file->close(save_file);
        fail(env, "java/lang/OutOfMemoryError", "Cannot allocate GBA core");
        return 0;
    }
    mCoreInitConfig(core, NULL);
    if (!core->init(core)) {
        mCoreConfigDeinit(&core->config);
        free(core);
        free(session);
        rom_file->close(rom_file);
        save_file->close(save_file);
        fail(env, "java/lang/IllegalStateException", "Cannot initialize GBA core");
        return 0;
    }
    session->core = core;
    // Do not read the standalone mGBA user configuration or open user files.
    mCoreConfigSetDefaultIntValue(&core->config, "useBios", 0);
    mCoreConfigSetDefaultIntValue(&core->config, "skipBios", 1);
    mCoreConfigSetDefaultIntValue(&core->config, "volume", 256);
    mCoreLoadForeignConfig(core, &core->config);
    core->setVideoBuffer(core, session->pixels, WIDTH);
    core->setAudioBufferSize(core, AUDIO_FRAMES);
    blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), sample_rate);
    blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), sample_rate);
    if (!core->isROM(rom_file) || !core->loadROM(core, rom_file)) {
        rom_file->close(rom_file);
        save_file->close(save_file);
        free_session(session);
        fail(env, "java/lang/IllegalArgumentException", "The supplied data is not a supported GBA ROM");
        return 0;
    }
    // Successful loadROM/loadSave transfer VFile ownership to mGBA.
    if (!core->loadSave(core, save_file)) {
        save_file->close(save_file);
        free_session(session);
        fail(env, "java/lang/IllegalStateException", "Cannot attach GBA battery save");
        return 0;
    }
    core->reset(core);
    session->id = next_id++;
    session->next = sessions;
    sessions = session;
    ++session_count;
    return session->id;
}

JNIEXPORT void JNICALL JNI_METHOD(destroy)(JNIEnv* env, jclass cls, jlong id) {
    (void) cls;
    struct Session** cursor = &sessions;
    while (*cursor && (*cursor)->id != id) cursor = &(*cursor)->next;
    if (!*cursor) {
        fail(env, "java/lang/IllegalStateException", "GBA session is already closed or invalid");
        return;
    }
    struct Session* session = *cursor;
    *cursor = session->next;
    --session_count;
    free_session(session);
}

JNIEXPORT jint JNICALL JNI_METHOD(runFrame)(JNIEnv* env, jclass cls, jlong id, jint keys, jintArray pixels, jshortArray audio) {
    (void) cls;
    struct Session* session = find_session(env, id);
    if (!session) return 0;
    if (!pixels || !audio || (*env)->GetArrayLength(env, pixels) < WIDTH * HEIGHT ||
        (*env)->GetArrayLength(env, audio) < AUDIO_FRAMES * 2 || (keys & ~0x3ff)) {
        fail(env, "java/lang/IllegalArgumentException", "Invalid GBA frame output buffers or input keys");
        return 0;
    }
    struct mCore* core = session->core;
    core->setKeys(core, keys);
    // Never hold a Java array pinned while running emulated code.
    core->runFrame(core);
    for (unsigned i = 0; i < WIDTH * HEIGHT; ++i) {
        uint32_t p = session->pixels[i];
        session->argb[i] = (jint) (0xff000000U | ((p & 0xff) << 16) | (p & 0xff00) | ((p >> 16) & 0xff));
    }
    blip_t* left = core->getAudioChannel(core, 0);
    blip_t* right = core->getAudioChannel(core, 1);
    int samples = blip_samples_avail(left);
    int right_samples = blip_samples_avail(right);
    if (right_samples < samples) samples = right_samples;
    if (samples > AUDIO_FRAMES) {
        clear_audio(session);
        fail(env, "java/lang/IllegalStateException", "GBA audio output exceeded its frame buffer");
        return 0;
    }
    blip_read_samples(left, session->audio, samples, 1);
    blip_read_samples(right, session->audio + 1, samples, 1);
    (*env)->SetIntArrayRegion(env, pixels, 0, WIDTH * HEIGHT, session->argb);
    if (!(*env)->ExceptionCheck(env)) (*env)->SetShortArrayRegion(env, audio, 0, samples * 2, session->audio);
    return samples;
}

JNIEXPORT void JNICALL JNI_METHOD(reset)(JNIEnv* env, jclass cls, jlong id) {
    (void) cls;
    struct Session* session = find_session(env, id);
    if (!session) return;
    session->core->setKeys(session->core, 0);
    session->core->reset(session->core);
    clear_audio(session);
    memset(session->pixels, 0, sizeof(session->pixels));
}

JNIEXPORT jbyteArray JNICALL JNI_METHOD(saveRam)(JNIEnv* env, jclass cls, jlong id) {
    (void) cls;
    struct Session* session = find_session(env, id);
    if (!session) return NULL;
    // Export the full memory-backed save file, including mGBA's RTC trailer.
    // savedataClone exports only SRAM/flash/EEPROM bytes and would drop that trailer.
    struct GBA* gba = session->core->board;
    struct GBASavedata* savedata = &gba->memory.savedata;
    GBASavedataUnmask(savedata);
    if (GBASavedataSize(savedata)) GBASavedataRTCWrite(savedata);
    struct VFile* vf = savedata->vf;
    ssize_t size = vf ? vf->size(vf) : 0;
    if (size < 0 || size > 1024 * 1024) {
        fail(env, "java/lang/IllegalStateException", "Invalid GBA battery save size");
        return NULL;
    }
    void* data = size ? vf->map(vf, size, MAP_READ) : NULL;
    if (size && !data) {
        fail(env, "java/lang/OutOfMemoryError", "Cannot map GBA battery save");
        return NULL;
    }
    jbyteArray bytes = (*env)->NewByteArray(env, (jsize) size);
    if (bytes && size) (*env)->SetByteArrayRegion(env, bytes, 0, (jsize) size, data);
    if (data) vf->unmap(vf, data, size);
    return bytes;
}

JNIEXPORT jbyteArray JNICALL JNI_METHOD(saveState)(JNIEnv* env, jclass cls, jlong id) {
    (void) cls;
    struct Session* session = find_session(env, id);
    if (!session) return NULL;
    struct VFile* vf = VFileMemChunk(NULL, 0);
    if (!vf) {
        fail(env, "java/lang/OutOfMemoryError", "Cannot allocate GBA snapshot");
        return NULL;
    }
    jbyteArray bytes = NULL;
    if (!mCoreSaveStateNamed(session->core, vf, STATE_FLAGS)) {
        fail(env, "java/lang/IllegalStateException", "Cannot serialize GBA snapshot");
    } else {
        ssize_t size = vf->size(vf);
        if (size <= 0 || size > MAX_STATE_SIZE) {
            fail(env, "java/lang/IllegalStateException", "Invalid serialized GBA snapshot size");
        } else {
            void* data = vf->map(vf, size, MAP_READ);
            if (data) {
                bytes = (*env)->NewByteArray(env, (jsize) size);
                if (bytes) (*env)->SetByteArrayRegion(env, bytes, 0, (jsize) size, data);
                vf->unmap(vf, data, size);
            } else {
                fail(env, "java/lang/OutOfMemoryError", "Cannot map GBA snapshot");
            }
        }
    }
    vf->close(vf);
    return bytes;
}

JNIEXPORT void JNICALL JNI_METHOD(loadState)(JNIEnv* env, jclass cls, jlong id, jbyteArray bytes) {
    (void) cls;
    struct Session* session = find_session(env, id);
    if (!session) return;
    struct VFile* vf = copy_bytes(env, bytes, 1, MAX_STATE_SIZE);
    if (!vf) return;
    bool success = mCoreLoadStateNamed(session->core, vf, STATE_FLAGS);
    vf->close(vf);
    if (!success) {
        fail(env, "java/lang/IllegalArgumentException", "GBA snapshot is invalid or belongs to a different ROM/core version");
        return;
    }
    clear_audio(session);
    memset(session->pixels, 0, sizeof(session->pixels));
}
