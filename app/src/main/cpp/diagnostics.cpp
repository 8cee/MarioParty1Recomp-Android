#include <android/log.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>

namespace {
std::mutex g_diag_mutex;
std::filesystem::path g_diag_path;

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

void append_file(const char* level, const char* stage, const char* detail) {
    std::lock_guard<std::mutex> lock(g_diag_mutex);
    if (g_diag_path.empty()) return;

    std::FILE* f = std::fopen(g_diag_path.string().c_str(), "a");
    if (f == nullptr) return;
    std::fprintf(
        f,
        "%lld [%s] [%s] %s\n",
        now_ms(),
        level ? level : "INFO",
        stage ? stage : "runtime",
        detail ? detail : ""
    );
    std::fflush(f);
    std::fclose(f);
}
}

extern "C" void mp1_diag_set_directory(const char* directory) {
    if (directory == nullptr || *directory == '\0') return;

    std::lock_guard<std::mutex> lock(g_diag_mutex);
    std::error_code ec;
    std::filesystem::path dir(directory);
    std::filesystem::create_directories(dir, ec);
    g_diag_path = dir / "mp1-diagnostic.log";

    // Start each launch with a visible separator while retaining previous
    // crash history until the user clears app data.
    std::FILE* f = std::fopen(g_diag_path.string().c_str(), "a");
    if (f != nullptr) {
        std::fprintf(f, "\n%lld [INFO] [launch] ===== Mario Party 1 Recomp =====\n", now_ms());
        std::fflush(f);
        std::fclose(f);
    }
}

extern "C" void mp1_diag(const char* stage, const char* detail) {
    __android_log_print(
        ANDROID_LOG_INFO,
        "MP1Recomp",
        "[%s] %s",
        stage ? stage : "runtime",
        detail ? detail : ""
    );
    append_file("INFO", stage, detail);
}

extern "C" void mp1_diag_error(const char* stage, const char* detail) {
    __android_log_print(
        ANDROID_LOG_ERROR,
        "MP1Recomp",
        "[%s] %s",
        stage ? stage : "runtime",
        detail ? detail : ""
    );
    append_file("ERROR", stage, detail);
}
