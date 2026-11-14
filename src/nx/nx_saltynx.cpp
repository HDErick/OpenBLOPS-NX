#include "nx_saltynx.h"

#include <switch.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>

namespace
{
constexpr u32 kFpsMagic = 0x465053;
constexpr size_t kPageSize = 0x1000;
constexpr u64 kAllocationSize = 310; // Includes the NX-FPS performance counters.
constexpr unsigned kSampleCount = 10;
constexpr u64 kPublishIntervalNs = 50'000'000; // Status Monitor checks the heartbeat within 100 ms.
constexpr u64 kRetryIntervalNs = 1'000'000'000;

#ifdef NX_DXVK
constexpr u8 kGraphicsApi = 3; // Vulkan
#else
constexpr u8 kGraphicsApi = 2; // OpenGL
#endif

struct ResolutionCalls
{
    u16 width, height, calls;
};

// The first 174 bytes of the SaltyNX NX-FPS shared block.
struct __attribute__((packed)) FpsBlock
{
    u32 magic;
    u8 fps;
    float average;
    bool active;
    u8 fpsLocker[4];
    u8 api;
    u32 ticks[kSampleCount];
    u8 bufferAndDisplaySettings[5];
    ResolutionCalls render[8], viewport[8];
    u8 refreshSettings[4];
    float readSpeed;
    u8 fpsLockedDocked;
    u64 frameNumber;
    s8 expectedSetBuffers;
};

static_assert(sizeof(FpsBlock) == 174);
static_assert(offsetof(FpsBlock, active) == 9);
static_assert(offsetof(FpsBlock, api) == 14);
static_assert(offsetof(FpsBlock, ticks) == 15);
static_assert(offsetof(FpsBlock, render) == 60);
static_assert(offsetof(FpsBlock, viewport) == 108);
static_assert(offsetof(FpsBlock, frameNumber) == 165);

Thread s_publisher{};
UEvent s_stopEvent{};
std::atomic<bool> s_running{false};

std::atomic<u64> s_lastPresent{0}, s_frameCount{0};
std::atomic<u32> s_resolution{0};
std::array<std::atomic<u32>, kSampleCount> s_frameTicks{};

// Only the publisher accesses the page after startup. The render thread
// records frame data in the atomics and never waits on SaltyNX IPC.
SharedMemory s_shared{};
u8 *s_page = nullptr;
volatile FpsBlock *s_block = nullptr;

volatile FpsBlock *findBlock()
{
    if (!s_page)
        return nullptr;

    for (size_t offset = 0; offset + sizeof(FpsBlock) <= kPageSize; offset += sizeof(u32))
    {
        auto *block = reinterpret_cast<volatile FpsBlock *>(s_page + offset);
        if (block->magic == kFpsMagic)
            return block;
    }
    return nullptr;
}

bool raiseSessionLimit()
{
    // Forwarders can have a single session, already used by sm.
    if (!envIsSyscallHinted(0x30) || !envIsSyscallHinted(0x31) || !envIsSyscallHinted(0x7e))
        return false;

    u64 rawHandle = INVALID_HANDLE;
    if (R_FAILED(svcGetInfo(&rawHandle, InfoType_ResourceLimit, INVALID_HANDLE, 0))
        || rawHandle == INVALID_HANDLE)
        return false;

    const Handle limit = Handle(rawHandle);
    s64 current = 0;
    s64 maximum = 0;
    const bool raised = R_SUCCEEDED(svcGetResourceLimitLimitValue(&maximum, limit, LimitableResource_Sessions))
        && R_SUCCEEDED(svcGetResourceLimitCurrentValue(&current, limit, LimitableResource_Sessions))
        && current == maximum
        && R_SUCCEEDED(svcSetResourceLimitLimitValue(limit, LimitableResource_Sessions, maximum + 1));
    svcCloseHandle(limit);
    return raised;
}

void mapPage(Service *service)
{
    Handle memory = INVALID_HANDLE;
    const Result result = serviceDispatch(service, 7, .in_send_pid = true,
        .out_handle_attrs = {SfOutHandleAttr_HipcCopy}, .out_handles = &memory);
    if (R_FAILED(result) || memory == INVALID_HANDLE)
        return;

    shmemLoadRemote(&s_shared, memory, kPageSize, Perm_Rw);
    if (R_SUCCEEDED(shmemMap(&s_shared)))
        s_page = static_cast<u8 *>(shmemGetAddr(&s_shared));
    else
        shmemClose(&s_shared);
}

void attach(bool startup)
{
    // Reuse the block on relaunch so repeated NRO launches don't fill the page.
    s_block = findBlock();
    if (s_block)
        return;

    Handle handle = INVALID_HANDLE;
    Result result = svcConnectToNamedPort(&handle, "SaltySD");
    bool restoreSm = false;
    if (R_VALUE(result) == KERNELRESULT(OutOfResource))
    {
        const bool raised = raiseSessionLimit();
        // Borrow the SM session only before the game starts opening services on other threads.
        restoreSm = !raised && startup;
        if (restoreSm)
            smExit();
        if (raised || restoreSm)
            result = svcConnectToNamedPort(&handle, "SaltySD");
    }

    if (R_SUCCEEDED(result))
    {
        Service service{};
        service.session = handle;
        if (!s_page)
            mapPage(&service);
        s_block = findBlock();
        if (s_page && !s_block)
        {
            u64 offset = 0;
            if (R_SUCCEEDED(serviceDispatchInOut(&service, 6, kAllocationSize, offset, .in_send_pid = true))
                && offset <= kPageSize - kAllocationSize && offset % sizeof(u32) == 0)
            {
                std::memset(s_page + offset, 0, kAllocationSize);
                s_block = reinterpret_cast<volatile FpsBlock *>(s_page + offset);
                s_block->expectedSetBuffers = -1;
                s_block->magic = kFpsMagic;
            }
        }

        // End the service's single session so overlays can connect.
        const u64 zero = 0;
        serviceDispatchIn(&service, 0, zero, .in_send_pid = true);
        svcCloseHandle(handle);
    }
    if (restoreSm)
        smInitialize();
}

void publish(bool running)
{
    if (!s_block || s_block->magic != kFpsMagic)
        return;

    const u64 frames = s_frameCount.load(std::memory_order_acquire);
    const u64 lastPresent = s_lastPresent.load(std::memory_order_relaxed);
    const u64 frequency = armGetSystemTickFreq();
    std::array<u32, kSampleCount> ticks{};
    u64 totalTicks = 0;
    unsigned sampleCount = 0;
    if (running && lastPresent && armGetSystemTick() - lastPresent < frequency)
    {
        for (unsigned i = 0; i < ticks.size(); ++i)
        {
            ticks[i] = s_frameTicks[i].load(std::memory_order_relaxed);
            totalTicks += ticks[i];
            sampleCount += ticks[i] != 0;
        }
    }

    const float average = sampleCount ? float(double(frequency) * sampleCount / totalTicks) : 0.0f;
    const u8 fps = u8(std::min(average + 0.5f, 255.0f));
    s_block->fps = fps;
    s_block->average = average;
    s_block->api = kGraphicsApi;
    s_block->frameNumber = frames;

    // Some overlays compute FPS directly from all ten slots.
    const u32 fallback = sampleCount ? u32(totalTicks / sampleCount) : UINT32_MAX;
    for (unsigned i = 0; i < ticks.size(); ++i)
        s_block->ticks[i] = ticks[i] ? ticks[i] : fallback;

    const u32 resolution = running ? s_resolution.load(std::memory_order_relaxed) : 0;
    const u16 width = u16(resolution);
    const u16 height = u16(resolution >> 16);
    const u16 calls = resolution ? std::max<unsigned>(fps, 1) : 0;
    s_block->render[0].width = width;
    s_block->render[0].height = height;
    s_block->render[0].calls = calls;
    s_block->viewport[0].width = width;
    s_block->viewport[0].height = height;
    s_block->viewport[0].calls = calls;
    for (unsigned i = 1; i < 8; ++i)
    {
        s_block->render[i].calls = 0;
        s_block->viewport[i].calls = 0;
    }

    // Status Monitor clears this flag to check that the game is still publishing.
    s_block->active = running && frames != 0;
}

void publisherMain(void *)
{
    svcSetThreadCoreMask(CUR_THREAD_HANDLE, -1, 0x7);
    Result waitResult;
    do
    {
        if (!s_block || s_block->magic != kFpsMagic)
            attach(false);
        publish(true);
        waitResult = waitSingle(waiterForUEvent(&s_stopEvent), s_block ? kPublishIntervalNs : kRetryIntervalNs);
    } while (R_VALUE(waitResult) == KERNELRESULT(TimedOut));
    publish(false);
}
}

void NX_SaltyNXStart()
{
    if (s_running.load(std::memory_order_relaxed))
        return;

    s_lastPresent.store(0, std::memory_order_relaxed);
    s_frameCount.store(0, std::memory_order_relaxed);
    s_resolution.store(0, std::memory_order_relaxed);
    for (auto &ticks : s_frameTicks)
        ticks.store(0, std::memory_order_relaxed);

    attach(true);
    publish(false);
    ueventCreate(&s_stopEvent, true);
    if (R_SUCCEEDED(threadCreate(&s_publisher, publisherMain, nullptr, nullptr, 0x8000, 0x3b, -2)))
    {
        if (R_SUCCEEDED(threadStart(&s_publisher)))
        {
            s_running.store(true, std::memory_order_relaxed);
            return;
        }
        threadClose(&s_publisher);
    }
    if (s_page)
        shmemClose(&s_shared);
    s_page = nullptr;
    s_block = nullptr;
}

void NX_SaltyNXStop()
{
    if (!s_running.exchange(false, std::memory_order_relaxed))
        return;

    ueventSignal(&s_stopEvent);
    threadWaitForExit(&s_publisher);
    threadClose(&s_publisher);
    if (s_page)
        shmemClose(&s_shared);
    s_page = nullptr;
    s_block = nullptr;
}

void NX_SaltyNXFrame(unsigned width, unsigned height)
{
    if (!s_running.load(std::memory_order_relaxed))
        return;

    // The engine serializes presents even when render ownership changes threads.
    const u64 now = armGetSystemTick();
    const u64 previous = s_lastPresent.load(std::memory_order_relaxed);
    const u64 frames = s_frameCount.load(std::memory_order_relaxed);
    if (previous && now > previous)
    {
        const u64 elapsed = now - previous;
        if (elapsed < armGetSystemTickFreq())
            s_frameTicks[frames % kSampleCount].store(u32(elapsed), std::memory_order_relaxed);
        else
            for (auto &ticks : s_frameTicks)
                ticks.store(0, std::memory_order_relaxed);
    }
    if (width && height)
    {
        const u32 resolution = std::min(width, 65535u) | (std::min(height, 65535u) << 16);
        s_resolution.store(resolution, std::memory_order_relaxed);
    }
    s_lastPresent.store(now, std::memory_order_relaxed);
    s_frameCount.store(frames + 1, std::memory_order_release);
}
