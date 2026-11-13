#include <switch.h>
#include <pthread.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../../../src/nx/dxvk/horizon_threads.h"

static u64 allowedCores = 7, allowedPriorities = UINT64_MAX;
static u32 currentMask = 7;
static s32 currentCore = -1, currentPriority = 59;
static bool failCoreInfo, failPriorityInfo, failMove, failPriority, failGetPriority;
static unsigned infoCalls, affinityCalls;

Result svcGetInfo(u64* value, InfoType type, Handle handle, u64 sub) {
    assert(handle == CUR_PROCESS_HANDLE && !sub);
    ++infoCalls;
    if ((type == InfoType_CoreMask && failCoreInfo)
        || (type == InfoType_PriorityMask && failPriorityInfo)) return 1;
    *value = type == InfoType_CoreMask ? allowedCores : allowedPriorities;
    return 0;
}

Result svcGetThreadPriority(s32* value, Handle handle) {
    assert(handle == CUR_THREAD_HANDLE);
    if (failGetPriority) return 2;
    *value = currentPriority;
    return 0;
}

Result svcSetThreadPriority(Handle handle, s32 priority) {
    assert(handle == CUR_THREAD_HANDLE && priority >= 0 && priority < 64);
    if (!(allowedPriorities & (u64(1) << priority)) || (priority == 63 && failPriority)) return 3;
    currentPriority = priority;
    return 0;
}

Result svcSetThreadCoreMask(Handle handle, s32 core, u32 mask) {
    assert(handle == CUR_THREAD_HANDLE);
    ++affinityCalls;
    assert(mask && !(mask & ~allowedCores));
    assert(core == -1 || (mask & (1u << core)));
    if (core == 3 && failMove) return 4;
    currentCore = core;
    currentMask = mask;
    return 0;
}

static void* worker(void* arg) {
    assert(currentPriority == 0x2c);
    assert(currentMask == 7);
    return arg;
}

extern "C" int __real_pthread_create(pthread_t*, const pthread_attr_t* attributes,
    void* (*entry)(void*), void* argument) {
    size_t stack = 0;
    assert(!pthread_attr_getstacksize(attributes, &stack) && stack >= 2 * 1024 * 1024);
    assert(entry(argument) == reinterpret_cast<void*>(123));
    return 0;
}

extern "C" int __wrap_pthread_create(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);

int main(int argc, char** argv) {
    assert(argc == 2);
    const char* scenario = argv[1];
    const bool three = !std::strcmp(scenario, "three");
    if (!three) allowedCores = 15;
    if (!std::strcmp(scenario, "no-priority-63")) allowedPriorities &= ~(u64(1) << 63);
    failCoreInfo = !std::strcmp(scenario, "core-query-failure");
    failPriorityInfo = !std::strcmp(scenario, "priority-query-failure");
    failMove = !std::strcmp(scenario, "affinity-failure");
    failPriority = !std::strcmp(scenario, "priority-failure");
    failGetPriority = !std::strcmp(scenario, "get-priority-failure");
    const bool offload = !three && (allowedPriorities >> 63)
        && !failCoreInfo && !failPriorityInfo && !failMove && !failPriority && !failGetPriority;

    NX_DxvkInitThreadAffinity();
    NX_DxvkInitThreadAffinity();
    assert(infoCalls == 2);
    currentMask = failCoreInfo ? 7 : u32(allowedCores);
    pthread_t thread;
    assert(!__wrap_pthread_create(&thread, nullptr, worker, reinterpret_cast<void*>(123)));
    assert(affinityCalls == (failCoreInfo ? 0u : 1u));

    NX_DxvkSetThreadName("dxvk-cs");
    assert(currentMask == 7 && currentPriority == 0x2b);
    assert(currentCore == (failCoreInfo ? -1 : 1));
    assert(affinityCalls == (failCoreInfo ? 0u : 2u));

    currentMask = 7;
    currentCore = -1;
    NX_DxvkSetThreadName("dxvk-writer");
    assert(currentMask == (offload ? 8u : 7u));
    assert(currentPriority == (offload ? 63 : 0x3b));
    assert(currentCore == (offload ? 3 : -1));
    if (three) assert(affinityCalls == 2);

    unsigned calls = affinityCalls;
    for (const char* name : {"dxvk-cs", "dxvk-submit", "dxvk-queue", "dxvk-pcompiler", "dxvk-dyasync",
        "dxvk-shader", "dxvk-cache", "ordinary-worker"}) {
        currentMask = 7;
        currentCore = -1;
        NX_DxvkSetThreadName(name);
        const bool cs = !std::strcmp(name, "dxvk-cs");
        if (cs && !failCoreInfo) ++calls;
        assert(currentMask == 7 && currentCore == (cs && !failCoreInfo ? 1 : -1) && affinityCalls == calls);
        const s32 expected = cs ? 0x2b : std::strstr(name, "shader") || std::strstr(name, "compiler")
            || std::strstr(name, "dyasync") ? 0x2e
            : std::strstr(name, "writer") || std::strstr(name, "cache") ? 0x3b : 0x2c;
        assert(currentPriority == expected);
    }
    assert(infoCalls == 2);
    std::printf("PASS: %s permissions, worker roles and fallback\n", scenario);
}
