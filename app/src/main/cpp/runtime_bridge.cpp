#include <jni.h>
#include <android/log.h>
#include <fstream>
#include <string>

#define LOG_TAG "MP1Runtime"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#include <atomic>

static std::atomic<unsigned int> g_buttons{0};
static std::atomic<float> g_axis_x{0.0f};
static std::atomic<float> g_axis_y{0.0f};
static std::atomic<bool> g_game_started{false};\n
struct Mp1AndroidInputState {
    unsigned int buttons;
    float stick_x;
    float stick_y;
};

extern "C" Mp1AndroidInputState mp1_android_poll_input() {
    return Mp1AndroidInputState{
        g_buttons.load(std::memory_order_relaxed),
        g_axis_x.load(std::memory_order_relaxed),
        g_axis_y.load(std::memory_order_relaxed)
    };
}

extern "C" void mp1_android_reset_input() {
    g_buttons.store(0, std::memory_order_relaxed);
    g_axis_x.store(0.0f, std::memory_order_relaxed);
    g_axis_y.store(0.0f, std::memory_order_relaxed);
}

// Runtime-facing helpers deliberately contain no Android UI dependencies.
// Generated/recompiled Mario Party code can poll these from the emulation thread.
extern "C" unsigned int mp1_android_buttons() {
    return g_buttons.load(std::memory_order_relaxed);
}

extern "C" float mp1_android_stick_x() {
    return g_axis_x.load(std::memory_order_relaxed);
}

extern "C" float mp1_android_stick_y() {
    return g_axis_y.load(std::memory_order_relaxed);
}


extern "C"
JNIEXPORT jstring JNICALL
Java_com_eightcee_marioparty1recomp_RuntimeBridge_initialize(
        JNIEnv* env, jobject, jstring romPath) {
    const char* path = env->GetStringUTFChars(romPath, nullptr);
    std::ifstream rom(path, std::ios::binary | std::ios::ate);

    std::string result;
    if (!rom.good()) {
        result = "Native runtime error: unable to open ROM";
    } else {
        const auto size = static_cast<long long>(rom.tellg());
        g_game_started.store(true);
        LOGI("Native runtime initialized. ROM=%s size=%lld", path, size);
        result = "Native runtime initialized.\nROM loaded: " + std::to_string(size) +
                 " bytes\nNext stage: connect recompiled Mario Party code and renderer.";
    }

    env->ReleaseStringUTFChars(romPath, path);
    return env->NewStringUTF(result.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_eightcee_marioparty1recomp_VirtualPadView_nativeInit(JNIEnv*, jobject) {
    LOGI("Virtual pad initialized");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_eightcee_marioparty1recomp_VirtualPadView_nativeButton(
        JNIEnv*, jobject, jint id, jboolean pressed) {
    if (id < 0 || id >= 31) return;
    const unsigned int mask = 1u << static_cast<unsigned int>(id);
    unsigned int current = g_buttons.load();
    unsigned int next;
    do {
        next = pressed ? (current | mask) : (current & ~mask);
    } while (!g_buttons.compare_exchange_weak(current, next));
    LOGI("Virtual pad button id=%d pressed=%d state=0x%08x", id, pressed ? 1 : 0, next);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_eightcee_marioparty1recomp_VirtualPadView_nativeAxis(
        JNIEnv*, jobject, jfloat x, jfloat y) {
    g_axis_x.store(x);
    g_axis_y.store(y);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_eightcee_marioparty1recomp_VirtualPadView_nativeIsGameStarted(JNIEnv*, jobject) {
    return g_game_started.load() ? JNI_TRUE : JNI_FALSE;
}


extern "C"
JNIEXPORT void JNICALL
Java_com_eightcee_marioparty1recomp_RuntimeBridge_setButton(
        JNIEnv*, jobject, jint id, jboolean pressed) {
    if (id < 0 || id >= 31) return;
    const unsigned int mask = 1u << static_cast<unsigned int>(id);
    unsigned int current = g_buttons.load();
    unsigned int next;
    do {
        next = pressed ? (current | mask) : (current & ~mask);
    } while (!g_buttons.compare_exchange_weak(current, next));
}

extern "C"
JNIEXPORT void JNICALL
Java_com_eightcee_marioparty1recomp_RuntimeBridge_setAxis(
        JNIEnv*, jobject, jfloat x, jfloat y) {
    g_axis_x.store(x);
    g_axis_y.store(y);
}
