#include <cstdio>
#include <cinttypes>

#include "librecomp/rsp.hpp"
#include "ultramodern/ultra64.h"

extern RspUcodeFunc aspMain;

RspUcodeFunc* mp1_get_rsp_microcode(const OSTask* task) {
    switch (task->t.type) {
        case M_AUDTASK:
            return aspMain;
        default:
            std::fprintf(stderr, "MP1: unknown RSP task type %" PRIu32 "\n", task->t.type);
            return nullptr;
    }
}
