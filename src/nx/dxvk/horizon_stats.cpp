#include "d3d9/d3d9_device.h"
#include <cstdio>

static thread_local dxvk::D3D9DeviceLock* s_heldDeviceLock = nullptr;

extern "C" void NX_DxvkHoldDeviceLock(void* nativeDevice, bool hold) {
    if (hold && !s_heldDeviceLock && nativeDevice) {
        auto* d3d9=static_cast<dxvk::D3D9DeviceEx*>(static_cast<IDirect3DDevice9*>(nativeDevice));
        s_heldDeviceLock=new dxvk::D3D9DeviceLock(d3d9->LockDevice());
    } else if (!hold && s_heldDeviceLock) {
        delete s_heldDeviceLock;
        s_heldDeviceLock=nullptr;
    }
}

extern "C" void NX_DxvkReportCounters(void* nativeDevice) {
    auto* d3d9=static_cast<dxvk::D3D9DeviceEx*>(static_cast<IDirect3DDevice9*>(nativeDevice));
    auto device=d3d9->GetDXVKDevice();
    auto stats=device->getStatCounters();
    using C=dxvk::DxvkStatCounter;
    std::printf("[nx-dxvk] counters: graphics pipelines=%llu compiler busy=%llu submits=%llu GPU waits=%llu CS waits=%llu\n",
        (unsigned long long)stats.getCtr(C::PipeCountGraphics),
        (unsigned long long)stats.getCtr(C::PipeCompilerBusy),
        (unsigned long long)stats.getCtr(C::QueueSubmitCount),
        (unsigned long long)stats.getCtr(C::GpuSyncCount),
        (unsigned long long)stats.getCtr(C::CsSyncCount));
    auto memory=device->adapter()->memoryProperties();
    auto heapInfo=device->adapter()->getMemoryHeapInfo();
    for (uint32_t heap=0; heap<memory.memoryHeapCount; ++heap) {
        auto usage=device->getMemoryStats(heap);
        std::printf("[nx-dxvk] heap %u: allocated=%llu MiB used=%llu MiB driver usage=%llu MiB budget=%llu MiB total=%llu MiB\n", heap,
            (unsigned long long)(usage.memoryAllocated>>20), (unsigned long long)(usage.memoryUsed>>20),
            (unsigned long long)(heapInfo.heaps[heap].memoryAllocated>>20),
            (unsigned long long)(heapInfo.heaps[heap].memoryBudget>>20),
            (unsigned long long)(memory.memoryHeaps[heap].size>>20));
    }
}
