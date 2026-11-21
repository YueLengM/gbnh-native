#include "gbnh_native.h"

#ifdef GBNH_HAS_MGBA
#include <mgba/core/core.h>
#endif

const char* gbnh_native_version(void) {
    return "gbnh-native-0.1.0";
}

int gbnh_native_mgba_available(void) {
#ifdef GBNH_HAS_MGBA
    struct mCore* core = mCoreCreate(mPLATFORM_GBA);
    if (!core) {
        return 0;
    }
    core->deinit(core);
    return 1;
#else
    return 0;
#endif
}
