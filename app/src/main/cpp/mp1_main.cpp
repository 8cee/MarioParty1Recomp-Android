#include <android/log.h>
#include <algorithm>
#include <array>
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
#include "SDL2/SDL_vulkan.h"

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

std::vector<recomp::GameEntry> supported_games;
SDL_Window* window = nullptr;
RspUcodeFunc* mp1_get_rsp_microcode(const OSTask* task);
void mp1_register_overlays();
extern "C" void mp1_android_pad_state(unsigned short* buttons, float* x, float* y);
extern "C" void mp1_diag_set_directory(const char* directory);
extern "C" void mp1_diag(const char* stage, const char* detail);
extern "C" void mp1_diag_error(const char* stage, const char* detail);

namespace {
constexpr std::uint64_t kMarioPartyUsXxh3 = 0x19f905a0cbc9fe8fULL;
constexpr char kTag[] = "MP1Recomp";
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

void log_error(const char* message) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "%s", message);
}

void message_box(const char* message) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "%s", message);
    if (window != nullptr) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Mario Party 1 Recomp", message, window);
    }
}

bool update_audio_converter() {
    const int ret = SDL_BuildAudioCVT(
        &g_audio_convert,
        AUDIO_F32,
        static_cast<Uint8>(kInputChannels),
        static_cast<int>(g_source_rate),
        AUDIO_F32,
        static_cast<Uint8>(g_output_channels),
        static_cast<int>(g_output_rate)
    );
    if (ret < 0) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL_BuildAudioCVT failed: %s", SDL_GetError());
        return false;
    }

    g_discarded_output_frames =
        kDuplicatedInputFrames * g_output_rate / g_source_rate;
    return true;
}

bool init_audio() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL audio init failed: %s", SDL_GetError());
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq = static_cast<int>(g_output_rate);
    desired.format = AUDIO_F32;
    desired.channels = static_cast<Uint8>(g_output_channels);
    desired.samples = 0x100;

    g_audio_device = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (g_audio_device == 0) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    if (!update_audio_converter()) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
        return false;
    }

    SDL_PauseAudioDevice(g_audio_device, 0);
    return true;
}

void queue_samples(int16_t* audio_data, size_t sample_count) {
    if (g_audio_device == 0 || audio_data == nullptr || sample_count == 0) {
        return;
    }

    static std::vector<float> swap_buffer;
    static std::array<float, kDuplicatedInputFrames * kInputChannels> duplicated_samples{};

    const size_t resampled_sample_count =
        sample_count + kDuplicatedInputFrames * kInputChannels;
    const size_t max_sample_count =
        std::max(resampled_sample_count,
                 resampled_sample_count * static_cast<size_t>(std::max(1, g_audio_convert.len_mult)));
    if (swap_buffer.size() < max_sample_count) {
        swap_buffer.resize(max_sample_count);
    }

    for (size_t i = 0; i < duplicated_samples.size(); ++i) {
        swap_buffer[i] = duplicated_samples[i];
    }

    // N64 audio arrives with the channel order affected by the runtime's
    // endian-address translation. Match the proven DK64/BM64/Banjo path.
    for (size_t i = 0; i + 1 < sample_count; i += kInputChannels) {
        swap_buffer[i + 0 + duplicated_samples.size()] =
            audio_data[i + 1] * (1.0f / 32768.0f);
        swap_buffer[i + 1 + duplicated_samples.size()] =
            audio_data[i + 0] * (1.0f / 32768.0f);
    }

    if (sample_count >= duplicated_samples.size()) {
        for (size_t i = 0; i < duplicated_samples.size(); ++i) {
            duplicated_samples[i] = swap_buffer[i + sample_count];
        }
    }

    g_audio_convert.buf = reinterpret_cast<Uint8*>(swap_buffer.data());
    g_audio_convert.len = static_cast<int>(
        (sample_count + duplicated_samples.size()) * sizeof(float)
    );

    if (SDL_ConvertAudio(&g_audio_convert) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL_ConvertAudio failed: %s", SDL_GetError());
        return;
    }

    const uint64_t queued_us =
        (static_cast<uint64_t>(SDL_GetQueuedAudioSize(g_audio_device)) /
         (g_output_channels * sizeof(float))) *
        1000000ULL / std::max<uint32_t>(1, g_output_rate);

    uint32_t bytes_to_queue = static_cast<uint32_t>(g_audio_convert.len_cvt);
    const uint32_t trim_bytes =
        g_output_channels * g_discarded_output_frames * sizeof(float);
    if (bytes_to_queue > trim_bytes) {
        bytes_to_queue -= trim_bytes;
    }

    float* samples_to_queue =
        swap_buffer.data() + (g_output_channels * g_discarded_output_frames / 2);

    // Keep queued latency bounded, matching the working recomp ports.
    const uint32_t skip_factor = static_cast<uint32_t>(queued_us / 100000ULL);
    if (skip_factor != 0 && skip_factor < 8) {
        const uint32_t skip_ratio = 1u << skip_factor;
        bytes_to_queue /= skip_ratio;
        const size_t frames =
            bytes_to_queue / (g_output_channels * sizeof(float));
        for (size_t i = 0; i < frames; ++i) {
            samples_to_queue[2 * i + 0] = samples_to_queue[2 * skip_ratio * i + 0];
            samples_to_queue[2 * i + 1] = samples_to_queue[2 * skip_ratio * i + 1];
        }
    }

    SDL_QueueAudio(g_audio_device, samples_to_queue, bytes_to_queue);
}

size_t get_frames_remaining() {
    if (g_audio_device == 0) {
        return 0;
    }

    uint64_t buffered_bytes = SDL_GetQueuedAudioSize(g_audio_device);
    buffered_bytes =
        buffered_bytes * kInputChannels * g_source_rate /
        std::max<uint32_t>(1, g_output_rate) /
        std::max<uint32_t>(1, g_output_channels);

    const uint32_t frames_per_vi = g_source_rate / 60;
    const uint64_t one_vi_bytes = kBytesPerFrame * frames_per_vi;
    if (buffered_bytes > one_vi_bytes) {
        buffered_bytes -= one_vi_bytes;
    } else {
        buffered_bytes = 0;
    }

    return static_cast<size_t>(buffered_bytes / kBytesPerFrame);
}

void set_frequency(uint32_t frequency) {
    if (frequency == 0 || frequency == g_source_rate) {
        return;
    }
    g_source_rate = frequency;
    update_audio_converter();
}

ultramodern::gfx_callbacks_t::gfx_data_t create_gfx() {
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

    mp1_diag("gfx", "SDL_Init begin");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_AUDIO) != 0) {
        const char* error = SDL_GetError();
        mp1_diag_error("gfx", error != nullptr ? error : "SDL_Init failed");
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL_Init failed: %s", error != nullptr ? error : "unknown");
        return nullptr;
    }
    mp1_diag("gfx", "SDL_Init succeeded");
    return nullptr;
}

ultramodern::renderer::WindowHandle create_window(ultramodern::gfx_callbacks_t::gfx_data_t) {
    const uint32_t flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP;
    mp1_diag("gfx", "SDL_CreateWindow begin");
    window = SDL_CreateWindow(
        "Mario Party 1 Recomp",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        720,
        flags
    );
    if (window == nullptr) {
        const char* error = SDL_GetError();
        mp1_diag_error("gfx", error != nullptr ? error : "SDL_CreateWindow failed");
        __android_log_print(ANDROID_LOG_ERROR, kTag, "SDL_CreateWindow failed: %s", error != nullptr ? error : "unknown");
        return {};
    }
    mp1_diag("gfx", "SDL_CreateWindow succeeded");
    // Preflight the Vulkan loader and surface extension query before entering
    // RT64, so an Android driver/SDL failure has a precise log entry.
    mp1_diag("vulkan", "SDL_Vulkan_LoadLibrary begin");
    if (SDL_Vulkan_LoadLibrary(nullptr) != 0) {
        mp1_diag_error("vulkan", SDL_GetError());
    } else {
        mp1_diag("vulkan", "SDL_Vulkan_LoadLibrary succeeded");
        unsigned int extension_count = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(window, &extension_count, nullptr)) {
            mp1_diag_error("vulkan", SDL_GetError());
        } else {
            char detail[96];
            std::snprintf(detail, sizeof(detail),
                "surface instance extensions available: %u", extension_count);
            mp1_diag("vulkan", detail);
        }
        SDL_Vulkan_UnloadLibrary();
    }
    return window;
}

void update_gfx(void*) {
    recompinput::handle_events();
}

std::unique_ptr<ultramodern::renderer::RendererContext> create_render_context(
    uint8_t* rdram,
    ultramodern::renderer::WindowHandle window_handle,
    bool developer_mode
) {
    mp1_diag("renderer", "RT64 render context creation begin");
    if (window_handle == ultramodern::renderer::WindowHandle{}) {
        mp1_diag_error("renderer", "refusing RT64 initialization with an invalid window handle");
        return nullptr;
    }
    auto context = recompui::renderer::create_render_context(
        rdram,
        window_handle,
        ultramodern::renderer::PresentationMode::PresentEarly,
        developer_mode
    );
    if (context == nullptr) {
        mp1_diag_error("renderer", "RT64 render context creation returned null");
    } else {
        mp1_diag("renderer", "RT64 render context creation succeeded");
    }
    return context;
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

    mp1_diag_set_directory(config_path.string().c_str());
    mp1_diag("startup", "native runtime entered");

    if (!std::filesystem::is_regular_file(rom_path)) {
        mp1_diag_error("startup", "imported Mario Party ROM is missing");
        log_error("Imported Mario Party ROM is missing.");
        return 3;
    }

    std::error_code ec;
    std::filesystem::create_directories(config_path, ec);
    recomp::register_config_path(config_path);
    recomp::register_game(mario_party_us);
    mp1_register_overlays();
    supported_games.clear();
    supported_games.push_back(mario_party_us);

    recompinput::profiles::initialize_input_bindings();
    recompinput::players::set_single_player_mode(true);
    recompinput::profiles::load_controls_config(config_path / "controls.json");

    std::u8string game_id = u8"mp1_us";
    const auto validation = recomp::select_rom(rom_path, game_id);
    if (validation != recomp::RomValidationError::Good) {
        char validation_message[96];
        std::snprintf(validation_message, sizeof(validation_message),
            "runtime ROM validation failed: %d", static_cast<int>(validation));
        mp1_diag_error("rom", validation_message);
        __android_log_print(ANDROID_LOG_ERROR, kTag, "%s", validation_message);
        return 4;
    }

    if (!init_audio()) {
        mp1_diag_error("audio", "SDL audio initialization failed");
        return 5;
    }
    mp1_diag("audio", "SDL audio initialized");

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

    // Match the proven DK64/Banjo/BM64 startup contract: recomp::start()
    // owns frontend/runtime initialization and the transition into the selected
    // game. Starting guest execution before this point can race renderer/window
    // initialization on Android.
    mp1_diag("runtime", "selecting imported Mario Party ROM");
    recomp::start_game(game_id, {});
    mp1_diag("runtime", "entering recomp runtime");
    __android_log_print(ANDROID_LOG_INFO, kTag, "Entering Mario Party recomp runtime");
    recomp::start(cfg);
    mp1_diag("runtime", "recomp runtime returned to Android main");

    if (g_audio_device != 0) {
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }

    return 0;
}
