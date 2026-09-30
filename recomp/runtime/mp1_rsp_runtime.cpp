#include <cstdint>
struct recomp_context;
extern "C" void aspMain(uint8_t*, recomp_context*);

// Mario Party uses the standard audio RSP task. Kept isolated so the Android
// runtime can share the same dispatch shape as the working BM64 port.
extern "C" void* mp1_audio_rsp_entry() {
    return reinterpret_cast<void*>(&aspMain);
}
