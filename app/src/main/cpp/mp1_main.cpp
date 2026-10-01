#include <android/log.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#ifdef __ANDROID__
#include "SDL_main.h"
#endif
#include "SDL2/SDL.h"

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
extern "C" void mp1_android_pad_state(unsigned short* buttons, float* x, float* y);

namespace {
constexpr std::uint64_t kMarioPartyUsXxh3 = 0x19f905a0cbc9fe8fULL;
constexpr char kTag[] = "MP1Recomp";
constexpr int kOutputRate = 48000;

SDL_Window* g_window = nullptr;
SDL_AudioDeviceID g_audio_device = 0;
SDL_AudioStream* g_audio_stream = nullptr;
int g_source_rate = kOutputRate;

void log_error(const char* message) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "%s", message);
}

void message_box(const char* message) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "%s", message);
    if (g_window != nullptr) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Mario Party 1 Recomp", message, g_window);
    }
}

bool rebuild_audio_stream() {
    if (g_audio_stream != nullptr) {
        SDL_FreeAudioStream(g_audio_stream);
        g_audio_stream = nullptr;
    }
    g_audio_stream = SDL_NewAudioStream(
        AUDIO_S16SYS, 2, g_source_rate,
        AUDIO_S16SYS, 2, kOutputRate
    );
    if (g_audio_stream == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL_NewAudioStream failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

bool init_audio() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL audio init failed: %s", SDL_GetError());
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq = kOutputRate;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 0x200;

    g_audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (g_audio_device == 0) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    if (!rebuild_audio_stream()) {
        return false;
    }

    SDL_PauseAudioDevice(g_audio_device, 0);
    return true;
}

void queue_samples(int16_t* audio_data, size_t sample_count) {
    if (g_audio_device == 0 || g_audio_stream == nullptr || audio_data == nullptr || sample_count == 0) {
        return;
    }

    const int byte_count = static_cast<int>(sample_count * sizeof(int16_t));
    if (SDL_AudioStreamPut(g_audio_stream, audio_data, byte_count) != 0) {
        return;
    }

    int available = SDL_AudioStreamAvailable(g_audio_stream);
    if (available <= 0) {
        return;
    }

    static std::vector<uint8_t> converted;
    converted.resize(static_cast<size_t>(available));
    const int received = SDL_AudioStreamGet(g_audio_stream, converted.data(), available);
    if (received > 0) {
        SDL_QueueAudio(g_audio_device, converted.data(), static_cast<Uint32>(received));
    }
}

size_t get_frames_remaining() {
    if (g_audio_device == 0) {
        return 0;
    }
    size_t bytes = SDL_GetQueuedAudioSize(g_audio_device);
    if (g_audio_stream != nullptr) {
        const int pending = SDL_AudioStreamAvailable(g_audio_stream);
        if (pending > 0) {
            bytes += static_cast<size_t>(pending);
        }
    }
    return bytes / (2 * sizeof(int16_t));
}

void set_frequency(uint32_t frequency) {
    if (frequency == 0 || static_cast<int>(frequency) == g_source_rate) {
        return;
    }
    g_source_rate = static_cast<int>(frequency);
    rebuild_audio_stream();
}

ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_AUDIO) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL_Init failed: %s", SDL_GetError());
    }
    return nullptr;
}

ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    const uint32_t flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP;
    g_window = SDL_CreateWindow(
        "Mario Party 1 Recomp",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        720,
        flags
    );
    if (g_window == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL_CreateWindow failed: %s", SDL_GetError());
    }
    return g_window;
}

void update_gfx(void*) {
    recompinput::handle_events();
}

std::unique_ptr<ultramodern::renderer::RendererContext> create_render_context(
    uint8_t* rdram,
    ultramodern::renderer::WindowHandle window_handle,
    bool developer_mode
) {
    return recompui::renderer::create_render_context(
        rdram,
        window_handle,
        ultramodern::renderer::PresentationMode::PresentEarly,
        developer_mode
    );
}

void poll_input() {
    recompinput::poll_inputs();
}

bool get_input(int controller_num, uint16_t* buttons, float* x, float* y) {
    const bool physical = recompinput::profiles::get_n64_input(controller_num, buttons, x, y);
    if (controller_num == 0) {
        mp1_android_pad_state(buttons, x, y);
        return true;
    }
    return physical;
}

ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num) {
    if (controller_num == 0) {
        return {
            .connected_device = ultramodern::input::Device::Controller,
            .connected_pak = ultramodern::input::Pak::RumblePak,
        };
    }
    return {
        .connected_device = ultramodern::input::Device::None,
        .connected_pak = ultramodern::input::Pak::None,
    };
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
} // namespace

int main(int argc, char** argv) {
    if (argc < 3 || argv[1] == nullptr || argv[2] == nullptr) {
        log_error("Expected config directory and ROM path from SDLActivity.");
        return 2;
    }

    const std::filesystem::path config_path = argv[1];
    const std::filesystem::path rom_path = argv[2];

    if (!std::filesystem::is_regular_file(rom_path)) {
        log_error("Imported Mario Party ROM is missing.");
        return 3;
    }

    std::error_code ec;
    std::filesystem::create_directories(config_path, ec);
    recomp::register_config_path(config_path);
    recomp::register_game(mario_party_us);

    recompinput::profiles::initialize_input_bindings();
    recompinput::players::set_single_player_mode(true);
    recompinput::profiles::load_controls_config(config_path / "controls.json");

    std::u8string game_id = u8"mp1_us";
    const auto validation = recomp::select_rom(rom_path, game_id);
    if (validation != recomp::RomValidationError::Good) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Runtime ROM validation failed: %d", static_cast<int>(validation));
        return 4;
    }

    if (!init_audio()) {
        return 5;
    }

    recomp::rsp::callbacks_t rsp_callbacks{
        .get_rsp_microcode = mp1_get_rsp_microcode,
    };

    ultramodern::renderer::callbacks_t renderer_callbacks{
        .create_render_context = create_render_context,
    };

    ultramodern::audio_callbacks_t audio_callbacks{
        .queue_samples = queue_samples,
        .get_frames_remaining = get_frames_remaining,
        .set_frequency = set_frequency,
    };

    ultramodern::input::callbacks_t input_callbacks{
        .poll_input = poll_input,
        .get_input = get_input,
        .set_rumble = recompinput::set_rumble,
        .get_connected_device_info = get_connected_device_info,
    };

    ultramodern::gfx_callbacks_t gfx_callbacks{
        .create_gfx = create_gfx,
        .create_window = create_window,
        .update_gfx = update_gfx,
    };

    ultramodern::error_handling::callbacks_t error_callbacks{
        .message_box = message_box,
    };

    recomp::Configuration cfg{
        .project_version = recomp::Version{0, 1, 0},
        .window_handle = {},
        .rsp_callbacks = rsp_callbacks,
        .renderer_callbacks = renderer_callbacks,
        .audio_callbacks = audio_callbacks,
        .input_callbacks = input_callbacks,
        .gfx_callbacks = gfx_callbacks,
        .events_callbacks = {},
        .error_handling_callbacks = error_callbacks,
        .threads_callbacks = {},
        .message_queue_control = {
            .requeue_timer = false,
        },
    };

    __android_log_print(ANDROID_LOG_INFO, kTag, "Starting Mario Party native runtime");
    recomp::start_game(game_id, {});
    recomp::start(cfg);

    if (g_audio_stream != nullptr) {
        SDL_FreeAudioStream(g_audio_stream);
        g_audio_stream = nullptr;
    }
    if (g_audio_device != 0) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }

    return 0;
}
