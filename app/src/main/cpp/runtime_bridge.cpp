#include <jni.h>
#include <atomic>
#include <android/log.h>

static std::atomic<int> g_buttons{0};
static std::atomic<float> g_x{0.0f}, g_y{0.0f};

extern "C" void mp1_android_pad_state(unsigned short* buttons, float* x, float* y) {
    *buttons |= static_cast<unsigned short>(g_buttons.load());
    *x = g_x.load(); *y = g_y.load();
}

extern "C" JNIEXPORT void JNICALL Java_com_eightcee_marioparty1recomp_RuntimeBridge_setVirtualPad(JNIEnv*, jobject, jint b, jfloat x, jfloat y) {
    g_buttons.store(b); g_x.store(x); g_y.store(y);
}
extern "C" JNIEXPORT void JNICALL Java_com_eightcee_marioparty1recomp_RuntimeBridge_writeDiagnostic(JNIEnv* env, jobject, jstring s) {
    const char* p=env->GetStringUTFChars(s,nullptr); __android_log_print(ANDROID_LOG_INFO,"MP1Recomp","%s",p); env->ReleaseStringUTFChars(s,p);
}
