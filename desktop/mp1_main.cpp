#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <SDL.h>
#include <SDL_syswm.h>

#include "librecomp/game.hpp"
#include "librecomp/rsp.hpp"
#include "ultramodern/ultramodern.hpp"
#include "recompui/renderer.h"
#include "recompinput/input_events.h"
#include "recompinput/input_state.h"
#include "recompinput/players.h"
#include "recompinput/profiles.h"

extern "C" void recomp_entrypoint(uint8_t* rdram, recomp_context* ctx);
gpr get_entrypoint_address();
RspUcodeFunc* mp1_get_rsp_microcode(const OSTask* task);
void mp1_register_overlays();

std::vector<recomp::GameEntry> supported_games;
SDL_Window* window = nullptr;

extern "C" void mp1_diag_set_directory(const char*) {}
extern "C" void mp1_diag(const char* stage, const char* detail) {
    std::fprintf(stdout, "[%s] %s\n", stage ? stage : "runtime", detail ? detail : "");
    std::fflush(stdout);
}
extern "C" void mp1_diag_error(const char* stage, const char* detail) {
    std::fprintf(stderr, "[%s] %s\n", stage ? stage : "runtime", detail ? detail : "");
    std::fflush(stderr);
}

namespace {
constexpr std::uint64_t kMarioPartyUsXxh3 = 0x19f905a0cbc9fe8fULL;
constexpr int kOutputRate = 48000;
SDL_AudioDeviceID g_audio_device = 0;
SDL_AudioCVT g_audio_convert{};
uint32_t g_source_rate = kOutputRate;
uint32_t g_output_rate = kOutputRate;
constexpr uint32_t kInputChannels = 2;
uint32_t g_output_channels = 2;
constexpr uint32_t kDuplicatedInputFrames = 4;
uint32_t g_discarded_output_frames = 0;
constexpr uint32_t kBytesPerFrame = kInputChannels * sizeof(float);

void message_box(const char* message) {
    std::fprintf(stderr, "%s\n", message ? message : "Mario Party runtime error");
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Mario Party 1 Recomp", message, window);
}

bool update_audio_converter() {
    if (SDL_BuildAudioCVT(&g_audio_convert, AUDIO_F32, kInputChannels, (int)g_source_rate,
                          AUDIO_F32, (Uint8)g_output_channels, (int)g_output_rate) < 0) {
        std::fprintf(stderr, "SDL_BuildAudioCVT failed: %s\n", SDL_GetError());
        return false;
    }
    g_discarded_output_frames = kDuplicatedInputFrames * g_output_rate / g_source_rate;
    return true;
}

bool init_audio() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    SDL_AudioSpec desired{};
    desired.freq = (int)g_output_rate;
    desired.format = AUDIO_F32;
    desired.channels = (Uint8)g_output_channels;
    desired.samples = 0x100;
    g_audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (!g_audio_device) return false;
    if (!update_audio_converter()) return false;
    SDL_PauseAudioDevice(g_audio_device, 0);
    return true;
}

void queue_samples(int16_t* audio_data, size_t sample_count) {
    if (!g_audio_device || !audio_data || !sample_count) return;
    static std::vector<float> swap_buffer;
    static std::array<float, kDuplicatedInputFrames * kInputChannels> duplicated_samples{};
    const size_t total = sample_count + duplicated_samples.size();
    const size_t cap = std::max(total, total * (size_t)std::max(1, g_audio_convert.len_mult));
    if (swap_buffer.size() < cap) swap_buffer.resize(cap);
    std::copy(duplicated_samples.begin(), duplicated_samples.end(), swap_buffer.begin());
    for (size_t i = 0; i + 1 < sample_count; i += 2) {
        swap_buffer[duplicated_samples.size() + i] = audio_data[i + 1] / 32768.0f;
        swap_buffer[duplicated_samples.size() + i + 1] = audio_data[i] / 32768.0f;
    }
    if (sample_count >= duplicated_samples.size()) {
        for (size_t i = 0; i < duplicated_samples.size(); ++i)
            duplicated_samples[i] = swap_buffer[sample_count + i];
    }
    g_audio_convert.buf = reinterpret_cast<Uint8*>(swap_buffer.data());
    g_audio_convert.len = (int)(total * sizeof(float));
    if (SDL_ConvertAudio(&g_audio_convert) < 0) return;
    uint32_t bytes = (uint32_t)g_audio_convert.len_cvt;
    const uint32_t trim = g_output_channels * g_discarded_output_frames * sizeof(float);
    if (bytes > trim) bytes -= trim;
    float* start = swap_buffer.data() + (g_output_channels * g_discarded_output_frames / 2);
    SDL_QueueAudio(g_audio_device, start, bytes);
}

size_t get_frames_remaining() {
    if (!g_audio_device) return 0;
    uint64_t bytes = SDL_GetQueuedAudioSize(g_audio_device);
    bytes = bytes * kInputChannels * g_source_rate /
        std::max<uint32_t>(1, g_output_rate) / std::max<uint32_t>(1, g_output_channels);
    const uint64_t one_vi = kBytesPerFrame * (g_source_rate / 60);
    bytes = bytes > one_vi ? bytes - one_vi : 0;
    return (size_t)(bytes / kBytesPerFrame);
}

void set_frequency(uint32_t frequency) {
    if (frequency && frequency != g_source_rate) {
        g_source_rate = frequency;
        update_audio_converter();
    }
}

ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    }
    return nullptr;
}

ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    window = SDL_CreateWindow("Mario Party 1 Recomp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) return {};
    SDL_SysWMinfo wm{};
    SDL_VERSION(&wm.version);
    if (!SDL_GetWindowWMInfo(window, &wm)) return {};
    return { wm.info.win.window, GetCurrentThreadId() };
}

void update_gfx(void*) { recompinput::handle_events(); }

std::unique_ptr<ultramodern::renderer::RendererContext> create_render_context(
    uint8_t* rdram, ultramodern::renderer::WindowHandle handle, bool developer_mode) {
    return recompui::renderer::create_render_context(
        rdram, handle, ultramodern::renderer::PresentationMode::PresentEarly, developer_mode);
}

void poll_input() { recompinput::poll_inputs(); }
bool get_input(int controller_num, uint16_t* buttons, float* x, float* y) {
    return recompinput::profiles::get_n64_input(controller_num, buttons, x, y);
}
ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num) {
    if (controller_num == 0) return {
        .connected_device = ultramodern::input::Device::Controller,
        .connected_pak = ultramodern::input::Pak::RumblePak,
    };
    return { .connected_device = ultramodern::input::Device::None, .connected_pak = ultramodern::input::Pak::None };
}

recomp::GameEntry mario_party_us{
    .rom_hash = kMarioPartyUsXxh3,
    .internal_name = "MarioParty",
    .display_name = "Mario Party",
    .game_id = u8"mp1_us",
    .mod_game_id = "",
    .save_type = recomp::SaveType::Eep4k,
    .is_enabled = true,
    .entrypoint_address = get_entrypoint_address(),
    .entrypoint = recomp_entrypoint,
};
}

int main(int argc, char** argv) {
    const std::filesystem::path base = std::filesystem::current_path();
    const std::filesystem::path rom = argc > 1 ? std::filesystem::path(argv[1]) : base / "marioparty.us.z64";
    const std::filesystem::path config = base / "config";
    if (!std::filesystem::is_regular_file(rom)) {
        message_box("Put the verified Mario Party (USA) ROM beside the EXE as marioparty.us.z64, or drag the ROM onto the EXE.");
        return 2;
    }
    std::filesystem::create_directories(config);
    recomp::register_config_path(config);
    recomp::register_game(mario_party_us);
    mp1_register_overlays();
    supported_games = { mario_party_us };
    recompinput::profiles::initialize_input_bindings();
    recompinput::players::set_single_player_mode(true);
    recompinput::profiles::load_controls_config(config / "controls.json");
    std::u8string game_id = u8"mp1_us";
    if (recomp::select_rom(rom, game_id) != recomp::RomValidationError::Good) {
        message_box("Unsupported ROM. Use the verified Mario Party (USA) ROM.");
        return 3;
    }
    if (!init_audio()) {
        message_box("SDL audio initialization failed.");
        return 4;
    }

    recomp::rsp::callbacks_t rsp_callbacks{ .get_rsp_microcode = mp1_get_rsp_microcode };
    ultramodern::renderer::callbacks_t renderer_callbacks{ .create_render_context = create_render_context };
    ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples = queue_samples, .get_frames_remaining = get_frames_remaining, .set_frequency = set_frequency };
    ultramodern::input::callbacks_t input_callbacks{
        .poll_input = poll_input, .get_input = get_input, .set_rumble = recompinput::set_rumble,
        .get_connected_device_info = get_connected_device_info };
    ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx = create_gfx, .create_window = create_window, .update_gfx = update_gfx };
    ultramodern::error_handling::callbacks_t error_callbacks{ .message_box = message_box };

    recomp::Configuration cfg{
        .project_version = recomp::Version{0,1,0}, .window_handle = {},
        .rsp_callbacks = rsp_callbacks, .renderer_callbacks = renderer_callbacks,
        .audio_callbacks = audio_callbacks, .input_callbacks = input_callbacks,
        .gfx_callbacks = gfx_callbacks, .events_callbacks = {},
        .error_handling_callbacks = error_callbacks, .threads_callbacks = {},
        .message_queue_control = { .requeue_timer = false },
    };

    recomp::start_game(game_id, {});
    recomp::start(cfg);
    if (g_audio_device) SDL_CloseAudioDevice(g_audio_device);
    return 0;
}
