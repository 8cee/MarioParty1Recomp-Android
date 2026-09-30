#include <android/log.h>
#include <filesystem>
#include <cstdint>

extern "C" void mp1_android_pad_state(unsigned short*, float*, float*);

static constexpr std::uint64_t kMarioPartyUsXxh3 = 0x19f905a0cbc9fe8fULL;
static constexpr const char* kGameId = "mp1_us";
static constexpr const char* kInternalName = "MarioParty";

extern "C" int mp1_android_boot(const char* config_path, const char* rom_path) {
    if (!config_path || !rom_path || !std::filesystem::is_regular_file(rom_path)) {
        __android_log_print(ANDROID_LOG_ERROR, "MP1Recomp", "ROM path missing");
        return -1;
    }
    __android_log_print(ANDROID_LOG_INFO, "MP1Recomp", "Boot request game=%s internal=%s hash=%llx", kGameId, kInternalName, (unsigned long long)kMarioPartyUsXxh3);
    // N64ModernRuntime registration/select/start wiring is linked here once
    // the pinned runtime submodules and generated recomp entrypoint are present.
    return 0;
}
