#include "gbnh_native.h"

#ifdef GBNH_HAS_MGBA
#include <mgba/flags.h>
#include <mgba/core/core.h>
#endif

const char* gbnh_native_version(void) {
    return "gbnh-native-0.2.0-mgba-0.10.5-jni1";
}

int gbnh_native_mgba_available(void) {
#ifdef GBNH_HAS_MGBA
    struct mCore* core = mCoreCreate(mPLATFORM_GBA);
    if (!core) {
        return 0;
    }
    mCoreInitConfig(core, NULL);
    if (!core->init(core)) {
        mCoreConfigDeinit(&core->config);
        free(core);
        return 0;
    }
    mCoreConfigDeinit(&core->config);
    core->deinit(core);
    return 1;
#else
    return 0;
#endif
}
