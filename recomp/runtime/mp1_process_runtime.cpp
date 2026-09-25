#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "recomp.h"
#include "funcs.h"
#include <ultramodern/ultramodern.hpp>

namespace {

constexpr uint32_t kProcessTop = 0x800E23CC;
constexpr uint32_t kProcessCurrent = 0x800E23D0;

constexpr uint32_t kOffNext = 0x00;
constexpr uint32_t kOffOldestChild = 0x08;
constexpr uint32_t kOffHeap = 0x18;
constexpr uint32_t kOffExecMode = 0x1C;
constexpr uint32_t kOffStat = 0x1E;
constexpr uint32_t kOffSleepTime = 0x24;
constexpr uint32_t kOffJumpSp = 0x2C;
constexpr uint32_t kOffJumpFunc = 0x30;

constexpr uint16_t kExecDefault = 0;
constexpr uint16_t kExecSleeping = 1;
constexpr uint16_t kExecWatch = 2;
constexpr uint16_t kExecDead = 3;

struct ProcessTransfer {
    int code;
};

struct HostProcess {
    uint32_t address = 0;
    uint8_t* rdram = nullptr;
    recomp_context context{};
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    bool initialized = false;
    bool runnable = false;
    bool yielded = false;
    bool done = false;
    bool kill_requested = false;
};

std::mutex g_processes_mutex;
std::unordered_map<uint32_t, std::unique_ptr<HostProcess>> g_processes;
thread_local HostProcess* g_current_host_process = nullptr;

void notify_scheduler(HostProcess& process) {
    process.cv.notify_all();
}

void wait_until_scheduled(HostProcess& process) {
    std::unique_lock lock(process.mutex);
    process.cv.wait(lock, [&] { return process.runnable || process.kill_requested; });
    if (process.kill_requested) {
        throw ProcessTransfer{3};
    }
    process.runnable = false;
    process.yielded = false;
}

void yield_current_process() {
    HostProcess* process = g_current_host_process;
    if (process == nullptr) {
        return;
    }

    {
        std::lock_guard lock(process->mutex);
        process->yielded = true;
    }
    notify_scheduler(*process);
    wait_until_scheduled(*process);
}

void mark_process_done(HostProcess& process) {
    {
        std::lock_guard lock(process.mutex);
        process.done = true;
        process.yielded = true;
    }
    notify_scheduler(process);
}

void run_process(HostProcess* process, uint32_t function_vram, uint32_t stack_pointer) {
    g_current_host_process = process;

    {
        std::lock_guard lock(process->mutex);
        process->context = {};
        process->context.r29 = stack_pointer;
        process->initialized = true;
    }
    notify_scheduler(*process);

    try {
        wait_until_scheduled(*process);

        recomp_func_t* entry = get_function(static_cast<int32_t>(function_vram));
        if (entry == nullptr) {
            throw ProcessTransfer{2};
        }

        entry(process->rdram, &process->context);

        // Game processes normally end through HuPrcEnd. If one returns
        // naturally, route it through the same cleanup path.
        process->context.r4 = process->address;
        try {
            HuPrcEnd(process->rdram, &process->context);
        } catch (const ProcessTransfer&) {
        }
    } catch (const ProcessTransfer& transfer) {
        if (transfer.code == 3) {
            // A process killed while suspended must execute the normal game
            // cleanup path (children, destructor, unlink, process count).
            process->context.r4 = process->address;
            try {
                HuPrcEnd(process->rdram, &process->context);
            } catch (const ProcessTransfer&) {
            }
        }
    }

    mark_process_done(*process);
    g_current_host_process = nullptr;
}

HostProcess* get_or_create_process(uint8_t* rdram, uint32_t address) {
    std::lock_guard lock(g_processes_mutex);
    auto found = g_processes.find(address);
    if (found != g_processes.end()) {
        return found->second.get();
    }

    auto process = std::make_unique<HostProcess>();
    process->address = address;
    process->rdram = rdram;
    HostProcess* result = process.get();

    const uint32_t stack_pointer = MEM_W(kOffJumpSp, address);
    const uint32_t function_vram = MEM_W(kOffJumpFunc, address);
    process->thread = std::thread(run_process, result, function_vram, stack_pointer);

    {
        std::unique_lock process_lock(process->mutex);
        process->cv.wait(process_lock, [&] { return process->initialized; });
    }

    g_processes.emplace(address, std::move(process));
    return result;
}

bool resume_process(HostProcess& process) {
    {
        std::lock_guard lock(process.mutex);
        process.runnable = true;
        process.yielded = false;
    }
    notify_scheduler(process);

    std::unique_lock lock(process.mutex);
    process.cv.wait(lock, [&] { return process.yielded || process.done; });
    return process.done;
}

void request_process_kill(HostProcess& process) {
    {
        std::lock_guard lock(process.mutex);
        process.kill_requested = true;
        process.runnable = true;
    }
    notify_scheduler(process);

    std::unique_lock lock(process.mutex);
    process.cv.wait(lock, [&] { return process.done; });
}

void reap_process(uint32_t address, uint32_t heap, uint8_t* rdram, recomp_context* scheduler_ctx) {
    std::unique_ptr<HostProcess> process;
    {
        std::lock_guard lock(g_processes_mutex);
        auto found = g_processes.find(address);
        if (found != g_processes.end()) {
            process = std::move(found->second);
            g_processes.erase(found);
        }
    }

    if (process && process->thread.joinable()) {
        process->thread.join();
    }

    if (heap != 0) {
        scheduler_ctx->r4 = heap;
        HuMemDirectFree(rdram, scheduler_ctx);
    }
}

} // namespace

// N64ModernRuntime currently lists these libultra functions as runtime-provided
// but does not implement every one needed by Mario Party 1.

extern "C" void __osEnqueueThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::thread_queue_insert(rdram, static_cast<uint32_t>(ctx->r4), static_cast<uint32_t>(ctx->r5));
}

extern "C" void __osPopThread_recomp(uint8_t* rdram, recomp_context* ctx) {
    ctx->r2 = ultramodern::thread_queue_pop(rdram, static_cast<uint32_t>(ctx->r4));
}

extern "C" void osEPiWriteIo_recomp(uint8_t*, recomp_context* ctx) {
    // Mario Party uses this for PI/device register writes. There is no
    // writable cartridge device in the native runtime; acknowledge the write.
    ctx->r2 = 0;
}

extern "C" void osLeoDiskInit_recomp(uint8_t*, recomp_context* ctx) {
    // The supported target is the retail cartridge release, not 64DD.
    ctx->r2 = 0;
}

extern "C" void osPfsIsPlug_recomp(uint8_t* rdram, recomp_context* ctx) {
    // Controller Pak storage is not exposed yet. Report no inserted paks.
    if (ctx->r5 != 0) {
        MEM_B(0, ctx->r5) = 0;
    }
    ctx->r2 = 0;
}

extern "C" void setjmp_recomp(uint8_t*, recomp_context* ctx) {
    // Mario Party's process scheduler uses setjmp only as the yield point.
    // Host threads preserve the native C stack across a yield, so the initial
    // setjmp path always returns zero.
    ctx->r2 = 0;
}

extern "C" void longjmp_recomp(uint8_t*, recomp_context* ctx) {
    // Calls to the global process jump buffer are the game's cooperative
    // scheduler transfer. Preserve the recompiled C stack by suspending the
    // host process thread instead of attempting a C setjmp/longjmp across it.
    if (g_current_host_process != nullptr) {
        const int transfer = static_cast<int32_t>(ctx->r5);
        if (transfer == 1) {
            yield_current_process();
            return;
        }
        if (transfer == 2) {
            throw ProcessTransfer{2};
        }
    }

    // Any other longjmp path is unexpected in the supported retail runtime.
    throw ProcessTransfer{2};
}

extern "C" void HuPrcCall(uint8_t* rdram, recomp_context* ctx) {
    const int32_t elapsed = static_cast<int32_t>(ctx->r4);
    uint32_t process_addr = MEM_W(0, kProcessTop);

    while (process_addr != 0) {
        MEM_W(0, kProcessCurrent) = process_addr;

        uint32_t next = MEM_W(kOffNext, process_addr);
        const uint32_t heap = MEM_W(kOffHeap, process_addr);
        const uint16_t stat = MEM_HU(kOffStat, process_addr);
        uint16_t mode = MEM_HU(kOffExecMode, process_addr);

        if ((stat & 1) != 0 && mode != kExecDead) {
            process_addr = next;
            continue;
        }

        if (mode == kExecSleeping) {
            int32_t sleep = static_cast<int32_t>(MEM_W(kOffSleepTime, process_addr));
            if (sleep > 0) {
                sleep -= elapsed;
                MEM_W(kOffSleepTime, process_addr) = sleep;
                if (sleep > 0) {
                    process_addr = next;
                    continue;
                }
            }
            MEM_W(kOffSleepTime, process_addr) = 0;
            MEM_H(kOffExecMode, process_addr) = kExecDefault;
            mode = kExecDefault;
        } else if (mode == kExecWatch) {
            if (MEM_W(kOffOldestChild, process_addr) != 0) {
                process_addr = next;
                continue;
            }
            MEM_H(kOffExecMode, process_addr) = kExecDefault;
            mode = kExecDefault;
        }

        HostProcess* host = get_or_create_process(rdram, process_addr);

        bool done = false;
        if (mode == kExecDead) {
            request_process_kill(*host);
            done = true;
        } else {
            done = resume_process(*host);
        }

        // Read next after execution because a process can create/link children
        // or unlink itself while it is running.
        next = MEM_W(kOffNext, process_addr);

        if (done) {
            reap_process(process_addr, heap, rdram, ctx);
        }

        process_addr = next;
    }

    MEM_W(0, kProcessCurrent) = 0;
}
