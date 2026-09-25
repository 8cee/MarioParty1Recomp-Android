#include <jni.h>
#include <android/log.h>
#include <fstream>
#include <string>

#define LOG_TAG "MP1Runtime"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

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
        LOGI("Native runtime initialized. ROM=%s size=%lld", path, size);
        result = "Native runtime initialized.\nROM loaded: " + std::to_string(size) +
                 " bytes\nNext stage: connect recompiled Mario Party code and renderer.";
    }

    env->ReleaseStringUTFChars(romPath, path);
    return env->NewStringUTF(result.c_str());
}
