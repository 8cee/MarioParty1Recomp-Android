#include <android/log.h>
#include <cstdio>
#include <string>

extern "C" void mp1_diag(const char* stage, const char* detail) {
    __android_log_print(ANDROID_LOG_INFO, "MP1Recomp", "[%s] %s", stage ? stage : "runtime", detail ? detail : "");
}

extern "C" void mp1_diag_error(const char* stage, const char* detail) {
    __android_log_print(ANDROID_LOG_ERROR, "MP1Recomp", "[%s] %s", stage ? stage : "runtime", detail ? detail : "");
}
