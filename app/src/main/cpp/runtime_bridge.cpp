#include <jni.h>
#include <atomic>

static std::atomic<int> g_buttons{0};
static std::atomic<float> g_x{0.0f}, g_y{0.0f};

extern "C" void mp1_diag(const char* stage, const char* detail);

extern "C" void mp1_android_pad_state(unsigned short* buttons, float* x, float* y) {
    *buttons |= static_cast<unsigned short>(g_buttons.load());
    const float tx = g_x.load();
    const float ty = g_y.load();
    if ((tx * tx + ty * ty) > 0.0001f) {
        *x = tx;
        *y = ty;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_eightcee_marioparty1recomp_RuntimeBridge_setVirtualPad(
    JNIEnv*, jobject, jint b, jfloat x, jfloat y
) {
    g_buttons.store(b);
    g_x.store(x);
    g_y.store(y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_eightcee_marioparty1recomp_RuntimeBridge_writeDiagnostic(
    JNIEnv* env, jobject, jstring s
) {
    if (s == nullptr) return;
    const char* p = env->GetStringUTFChars(s, nullptr);
    if (p != nullptr) {
        mp1_diag("android", p);
        env->ReleaseStringUTFChars(s, p);
    }
}
