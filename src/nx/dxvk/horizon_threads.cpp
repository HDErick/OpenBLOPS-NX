#include <switch.h>
#include <pthread.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <new>
#include "horizon_threads.h"

// Shader compilation needs larger stacks than libnx's 128 KiB default.
extern "C" int __real_pthread_create(pthread_t*, const pthread_attr_t*, void*(*)(void*), void*);
namespace {
struct WorkerCores {
    u64 mask = 0;
    bool core3 = false;

    WorkerCores() {
        u64 priorities = 0;
        const Result coresResult = svcGetInfo(&mask, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0);
        const Result priorityResult = svcGetInfo(&priorities, InfoType_PriorityMask, CUR_PROCESS_HANDLE, 0);
        if (R_FAILED(coresResult)) mask = 0;
        core3 = R_SUCCEEDED(coresResult) && R_SUCCEEDED(priorityResult)
            && (mask & 8) && (mask & 7) && (priorities & (u64(1) << 63));
    }
};

const WorkerCores& workerCores() {
    static const WorkerCores cores;
    return cores;
}

struct Start { void*(*entry)(void*); void* argument; };
void* rendererThreadEntry(void* opaque) {
    const Start start=*static_cast<Start*>(opaque);
    delete static_cast<Start*>(opaque);
    Result result=svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2c);
    if (R_FAILED(result)) std::fprintf(stderr,"[nx-dxvk] worker priority failed: %08x\n",unsigned(result));
    // Any of the three application cores. libnx starts a pthread on the process's
    // default core (0) with only that core in its mask, which put DXVK's and Mesa's
    // threads on the main thread's core when core 3 was not available.
    const auto& cores = workerCores();
    if ((cores.mask & 7) == 7)
        svcSetThreadCoreMask(CUR_THREAD_HANDLE, -1, 7);
    return start.entry(start.argument);
}
}

extern "C" void NX_DxvkInitThreadAffinity() {
    (void)workerCores();
}

extern "C" void NX_DxvkSetThreadName(const char* name) {
    const int priority = !std::strcmp(name, "dxvk-writer") || std::strstr(name, "cache") ? 0x3b
        : !std::strcmp(name, "dxvk-pcompiler") || !std::strcmp(name, "dxvk-dyasync")
            || std::strstr(name, "shader") ? 0x2e : 0x2c;
    Result result = svcSetThreadPriority(CUR_THREAD_HANDLE, priority);
    if (R_FAILED(result)) std::fprintf(stderr, "[nx-dxvk] %s priority failed: %08x\n", name, unsigned(result));
    // DXVK's command-stream thread (every D3D9 call becomes Vulkan there) starts on
    // core 1, away from the main thread on core 0, and may still move to any of the
    // three. Its priority is one step above the engine's threads (0x2c): Horizon
    // does not time-slice threads of equal priority, so on a shared core it waited
    // for a busy engine thread to block before it could take the next commands.
    if (!std::strcmp(name, "dxvk-cs")) {
        if ((workerCores().mask & 7) == 7)
            svcSetThreadCoreMask(CUR_THREAD_HANDLE, 1, 7);
        svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2b);
    }
    if (std::strcmp(name, "dxvk-writer") || !workerCores().core3) return;

    s32 previousPriority = 0;
    result = svcGetThreadPriority(&previousPriority, CUR_THREAD_HANDLE);
    if (R_SUCCEEDED(result)) result = svcSetThreadPriority(CUR_THREAD_HANDLE, 63);
    if (R_FAILED(result)) return;
    result = svcSetThreadCoreMask(CUR_THREAD_HANDLE, 3, 8);
    if (R_FAILED(result))
        svcSetThreadPriority(CUR_THREAD_HANDLE, previousPriority);
}

extern "C" int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attributes,
                                      void*(*entry)(void*), void* argument) {
    pthread_attr_t attr;
    int error=0;
    if (attributes) attr=*attributes;
    else if ((error=pthread_attr_init(&attr))) return error;
    void* suppliedStack=nullptr;
    size_t stackSize=0;
    error=pthread_attr_getstack(&attr,&suppliedStack,&stackSize);
    if (!error && !suppliedStack && stackSize<2*1024*1024)
        error=pthread_attr_setstacksize(&attr,2*1024*1024);
    Start* start=nullptr;
    if (!error) {
        start=new(std::nothrow) Start{entry,argument};
        if (!start) error=ENOMEM;
    }
    if (!error) error=__real_pthread_create(thread,&attr,rendererThreadEntry,start);
    if (error) delete start;
    if (!attributes) pthread_attr_destroy(&attr);
    return error;
}
