#include <cstdint>
#include "recomp.h"

extern "C" void osEPiWriteIo_recomp(uint8_t*, recomp_context* ctx) {
    // Cartridge build: PI register writes used by unavailable peripherals are
    // treated as successful no-ops.
    ctx->r2 = 0;
}

extern "C" void osLeoDiskInit_recomp(uint8_t*, recomp_context* ctx) {
    // Mario Party retail cartridge does not use the 64DD path.
    ctx->r2 = 0;
}

extern "C" void __osPopThread_recomp(uint8_t*, recomp_context* ctx) {
    // Only referenced by the 64DD initialization path in this build.
    ctx->r2 = 0;
}

extern "C" void __osEnqueueThread_recomp(uint8_t*, recomp_context*) {
    // Only referenced by the 64DD initialization path in this build.
}

extern "C" void osPfsIsPlug_recomp(uint8_t* rdram, recomp_context* ctx) {
    // No Controller Pak is exposed by the Android port. Report a successful
    // query with an empty port bitmask; controller input itself is separate.
    if (ctx->r5 != 0) {
        MEM_B(0, ctx->r5) = 0;
    }
    ctx->r2 = 0;
}
