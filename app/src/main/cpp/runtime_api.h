#pragma once
#include <cstdint>

extern "C" void mp1_android_pad_state(unsigned short* buttons, float* x, float* y);
extern "C" void* mp1_audio_rsp_entry();

namespace mp1 {
inline constexpr std::uint64_t rom_hash = 0x19f905a0cbc9fe8fULL;
inline constexpr const char* game_id = "mp1_us";
inline constexpr const char* internal_name = "MarioParty";
}
