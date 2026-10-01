#include <android/log.h>
#include <pthread.h>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "recomp.h"

namespace {
constexpr char kTag[] = "MP1Process";

// Mario Party 1 (US) process globals/functions from the matching decomp ELF.
constexpr uint32_t kProcessTop     = 0x800E23CC;
constexpr uint32_t kProcessCurrent = 0x800E23D0;
constexpr uint32_t kProcessCount   = 0x800E23D4;
constexpr uint32_t kHuPrcEnd       = 0x80063514;
constexpr uint32_t kHuMemDirectFree = 0x8003B6C8;

// Process layout (sizeof 0x90).
constexpr uint32_t kNext       = 0x00;
constexpr uint32_t kPrev       = 0x04; // youngest_child doubles as root-list prev
constexpr uint32_t kHeap       = 0x18;
constexpr uint32_t kExecMode   = 0x1C;
constexpr uint32_t kStat       = 0x1E;
constexpr uint32_t kSleepTime  = 0x24;
constexpr uint32_t kJumpSp     = 0x2C;
constexpr uint32_t kJumpFunc   = 0x30;
constexpr uint32_t kDestructor = 0x88;
constexpr uint32_t kOldestChild = 0x08;

constexpr uint16_t kExecDefault  = 0;
constexpr uint16_t kExecSleeping = 1;
constexpr uint16_t kExecWatch    = 2;
constexpr uint16_t kExecDead     = 3;

inline gpr guest_gpr(uint32_t address) {
    return static_cast<gpr>(static_cast<int64_t>(static_cast<int32_t>(address)));
}

inline uint32_t read32(uint8_t* rdram, uint32_t address) {
    return static_cast<uint32_t>(MEM_W(0, guest_gpr(address)));
}

inline void write32(uint8_t* rdram, uint32_t address, uint32_t value) {
    MEM_W(0, guest_gpr(address)) = static_cast<int32_t>(value);
}

inline uint16_t read16(uint8_t* rdram, uint32_t address) {
    return MEM_HU(0, guest_gpr(address));
}

inline void write16(uint8_t* rdram, uint32_t address, uint16_t value) {
    MEM_H(0, guest_gpr(address)) = static_cast<int16_t>(value);
}

struct ProcessHost {
    uint32_t process = 0;
    uint8_t* rdram = nullptr;
    recomp_context ctx{};
    std::thread thread;

    std::mutex mutex;
    std::condition_variable cv;
    bool permit = false;
    bool yielded = false;
    bool terminated = false;
    bool force_end = false;
    uint32_t heap_to_free = 0;
};

std::mutex g_hosts_mutex;
std::unordered_map<uint32_t, std::unique_ptr<ProcessHost>> g_hosts;
thread_local ProcessHost* g_current_host = nullptr;

[[noreturn]] void terminate_host(ProcessHost* host) {
    {
        std::unique_lock lock(host->mutex);
        host->terminated = true;
        host->yielded = false;
        host->cv.notify_all();
        lock.unlock();
    }
    pthread_exit(nullptr);
    __builtin_unreachable();
}

void call_guest(ProcessHost* host, uint32_t vram) {
    recomp_func_t* fn = get_function(static_cast<int32_t>(vram));
    if (fn == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "Missing guest function at %08x", vram);
        terminate_host(host);
    }
    fn(host->rdram, &host->ctx);
}

[[noreturn]] void run_process_end(ProcessHost* host) {
    host->force_end = false;
    call_guest(host, kHuPrcEnd);
    __android_log_print(ANDROID_LOG_ERROR, kTag, "HuPrcEnd unexpectedly returned for process %08x", host->process);
    terminate_host(host);
}

void wait_for_next_slice(ProcessHost* host) {
    std::unique_lock lock(host->mutex);
    host->yielded = true;
    host->cv.notify_all();
    host->cv.wait(lock, [&] { return host->permit || host->force_end; });
    const bool end_now = host->force_end;
    host->permit = false;
    host->yielded = false;
    lock.unlock();

    if (end_now) {
        run_process_end(host);
    }
}

void process_thread_main(ProcessHost* host) {
    g_current_host = host;

    const uint32_t stack = read32(host->rdram, host->process + kJumpSp);
    const uint32_t entry = read32(host->rdram, host->process + kJumpFunc);

    host->ctx = {};
    host->ctx.r29 = guest_gpr(stack);
    host->ctx.f_odd = &host->ctx.f0.u32h;
    host->ctx.mips3_float_mode = 0;

    {
        std::unique_lock lock(host->mutex);
        host->cv.wait(lock, [&] { return host->permit || host->force_end; });
        const bool end_now = host->force_end;
        host->permit = false;
        lock.unlock();

        if (end_now) {
            run_process_end(host);
        }
    }

    __android_log_print(
        ANDROID_LOG_DEBUG, kTag,
        "Starting process %08x entry=%08x sp=%08x",
        host->process, entry, stack
    );

    call_guest(host, entry);

    // A process function returning is equivalent to the game's HuPrcEnd path.
    run_process_end(host);
}

ProcessHost* get_or_create_host(uint8_t* rdram, uint32_t process) {
    std::lock_guard lock(g_hosts_mutex);
    auto it = g_hosts.find(process);
    if (it != g_hosts.end()) {
        return it->second.get();
    }

    auto host = std::make_unique<ProcessHost>();
    host->process = process;
    host->rdram = rdram;
    ProcessHost* raw = host.get();
    g_hosts.emplace(process, std::move(host));
    raw->thread = std::thread(process_thread_main, raw);
    return raw;
}

void free_guest_heap(uint8_t* rdram, uint32_t heap) {
    if (heap == 0) {
        return;
    }

    recomp_func_t* fn = get_function(static_cast<int32_t>(kHuMemDirectFree));
    if (fn == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "HuMemDirectFree not found");
        return;
    }

    recomp_context ctx{};
    ctx.r4 = guest_gpr(heap);
    ctx.f_odd = &ctx.f0.u32h;
    fn(rdram, &ctx);
}

void reap_host(uint8_t* rdram, uint32_t process, ProcessHost* host) {
    uint32_t heap = 0;
    {
        std::lock_guard lock(host->mutex);
        heap = host->heap_to_free;
    }

    if (host->thread.joinable()) {
        host->thread.join();
    }
    free_guest_heap(rdram, heap);

    std::lock_guard lock(g_hosts_mutex);
    auto it = g_hosts.find(process);
    if (it != g_hosts.end() && it->second.get() == host) {
        g_hosts.erase(it);
    }
}

void run_slice(uint8_t* rdram, uint32_t process, bool force_end) {
    ProcessHost* host = get_or_create_host(rdram, process);

    {
        std::unique_lock lock(host->mutex);
        if (force_end) {
            host->force_end = true;
        } else {
            host->permit = true;
        }
        host->yielded = false;
        host->cv.notify_all();
        host->cv.wait(lock, [&] { return host->yielded || host->terminated; });

        if (!host->terminated) {
            return;
        }
    }

    reap_host(rdram, process, host);
}

void unlink_process(uint8_t* rdram, uint32_t process) {
    const uint32_t next = read32(rdram, process + kNext);
    const uint32_t prev = read32(rdram, process + kPrev);

    if (next != 0) {
        write32(rdram, next + kPrev, prev);
    }

    if (prev != 0) {
        write32(rdram, prev + kNext, next);
    } else {
        write32(rdram, kProcessTop, next);
    }
}
} // namespace

extern "C" void HuPrcCall(uint8_t* rdram, recomp_context* ctx) {
    const int32_t tick = static_cast<int32_t>(ctx->r4);

    uint32_t process = read32(rdram, kProcessTop);
    while (process != 0) {
        // Save next before running; HuPrcEnd/HuPrcTerminate may unlink current.
        const uint32_t next = read32(rdram, process + kNext);
        write32(rdram, kProcessCurrent, process);

        const uint16_t stat = read16(rdram, process + kStat);
        uint16_t mode = read16(rdram, process + kExecMode);

        if ((stat & 1) && mode != kExecDead) {
            process = next;
            continue;
        }

        switch (mode) {
            case kExecSleeping: {
                int32_t sleep = static_cast<int32_t>(read32(rdram, process + kSleepTime));
                if (sleep > 0) {
                    sleep -= tick;
                    if (sleep <= 0) {
                        sleep = 0;
                        write16(rdram, process + kExecMode, kExecDefault);
                    }
                    write32(rdram, process + kSleepTime, static_cast<uint32_t>(sleep));
                }
                // Original scheduler waits until the following HuPrcCall after wake.
                break;
            }

            case kExecWatch:
                if (read32(rdram, process + kOldestChild) == 0) {
                    write16(rdram, process + kExecMode, kExecDefault);
                    run_slice(rdram, process, false);
                }
                break;

            case kExecDead:
                run_slice(rdram, process, true);
                break;

            case kExecDefault:
                run_slice(rdram, process, false);
                break;

            default:
                __android_log_print(ANDROID_LOG_WARN, kTag, "Unknown process mode %u at %08x", mode, process);
                break;
        }

        process = next;
    }

    write32(rdram, kProcessCurrent, 0);
}

extern "C" void HuPrcSleep(uint8_t* rdram, recomp_context* ctx) {
    ProcessHost* host = g_current_host;
    if (host == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "HuPrcSleep called outside process host thread");
        return;
    }

    const int32_t time = static_cast<int32_t>(ctx->r4);
    if (time != 0 && read16(rdram, host->process + kExecMode) != kExecDead) {
        write16(rdram, host->process + kExecMode, kExecSleeping);
        write32(rdram, host->process + kSleepTime, static_cast<uint32_t>(time));
    }

    wait_for_next_slice(host);
}

extern "C" void HuPrcChildWatch(uint8_t* rdram, recomp_context*) {
    ProcessHost* host = g_current_host;
    if (host == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "HuPrcChildWatch called outside process host thread");
        return;
    }

    if (read32(rdram, host->process + kOldestChild) != 0) {
        write16(rdram, host->process + kExecMode, kExecWatch);
        wait_for_next_slice(host);
    }
}

extern "C" void HuPrcTerminate(uint8_t* rdram, recomp_context* ctx) {
    ProcessHost* host = g_current_host;
    if (host == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "HuPrcTerminate called outside process host thread");
        return;
    }

    uint32_t process = static_cast<uint32_t>(ctx->r4);
    if (process == 0) {
        process = host->process;
    }

    const uint32_t destructor = read32(rdram, process + kDestructor);
    if (destructor != 0) {
        // The original HuPrcTerminate runs the destructor in the terminating
        // process context before unlinking/freeing its process heap.
        call_guest(host, destructor);
    }

    const uint32_t heap = read32(rdram, process + kHeap);
    unlink_process(rdram, process);

    int32_t count = static_cast<int32_t>(read32(rdram, kProcessCount));
    if (count > 0) {
        write32(rdram, kProcessCount, static_cast<uint32_t>(count - 1));
    }

    {
        std::lock_guard lock(host->mutex);
        host->heap_to_free = heap;
    }

    terminate_host(host);
}

// These symbols can remain referenced by the renamed generated originals.
// The active MP1 process paths above never use guest setjmp/longjmp.
extern "C" void setjmp_recomp(uint8_t*, recomp_context* ctx) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "Unexpected setjmp_recomp call");
    ctx->r2 = 0;
}

extern "C" void longjmp_recomp(uint8_t*, recomp_context*) {
    __android_log_print(ANDROID_LOG_ERROR, kTag, "Unexpected longjmp_recomp call");
}
