#include <switch.h>
#include <d3d9.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>
#include <new>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include "nx_renderer_build.h"
#include "../nx_install.h"
#include "horizon_threads.h"
#include "d3d9_macro_capture.inc"

// Preserve the engine's Win32 tag names for C++ linkage.
struct HWND__;
struct HMONITOR__;
struct tagRECT { LONG left, top, right, bottom; };
struct tagPOINT { LONG x, y; };
namespace kisak_d3d9 {
using HWND = ::HWND__*;
using HMONITOR = ::HMONITOR__*;
using RECT = ::tagRECT;
using POINT = ::tagPOINT;
}
#define NX_DXVK_BRIDGE
#include "../compat/d3d9.h"

volatile bool g_nxLoadingHint = false;
bool NxProf_Enabled();
int NxProf_GpuScopeCount();
const char* NxProf_GpuScopeName(int scope);
extern "C" void NX_DxvkReportCounters(void* nativeDevice);

namespace kisak_d3d9 {
static_assert(sizeof(RECT)==sizeof(::RECT) && alignof(RECT)==alignof(::RECT));
static_assert(sizeof(POINT)==sizeof(::POINT) && alignof(POINT)==alignof(::POINT));
namespace {
std::mutex registryMutex;
std::unordered_map<void*, IUnknown9Like*> registry;
std::unordered_set<const void*> textures;
::IDirect3DDevice9* profileDevice = nullptr; // Owns an independent native ref.
std::atomic<unsigned> errorCount {0};
std::atomic<bool> stopping {false};
thread_local uint64_t drawCalls = 0;

template<typename T> T* unwrap(const IUnknown9Like* handle) {
    return handle ? static_cast<T*>(handle->nxNative) : nullptr;
}

void reportFailure(const char* operation, HRESULT result) {
    if (FAILED(result) && errorCount.fetch_add(1, std::memory_order_relaxed) < 128)
        std::fprintf(stderr, "[nx-dxvk] %s failed: 0x%08x\n", operation, unsigned(result));
}

template<typename Native, typename Handle>
HRESULT wrap(Native* native, Handle** output) {
    // Adopt the reference returned by the native call.
    std::lock_guard<std::mutex> lock(registryMutex);
    auto found = registry.find(native);
    if (found != registry.end()) {
        ++found->second->nxReferences;
        *output = static_cast<Handle*>(found->second);
        return S_OK;
    }
    Handle* handle = new(std::nothrow) Handle;
    if (!handle) { native->Release(); return E_OUTOFMEMORY; }
    handle->nxNative = native;
    handle->nxDestroy = [](IUnknown9Like* p) { delete static_cast<Handle*>(p); };
    if constexpr (std::is_same_v<Handle, IDirect3D9>) handle->nxKind = 2;
    else if constexpr (std::is_same_v<Handle, IDirect3DDevice9>) handle->nxKind = 1;
    try {
        registry.emplace(native, handle);
        if constexpr (std::is_base_of_v<IDirect3DBaseTexture9, Handle>) textures.emplace(handle);
    } catch (const std::bad_alloc&) {
        registry.erase(native); delete handle; native->Release(); return E_OUTOFMEMORY;
    }
    *output = handle;
    return S_OK;
}

// Keep pending queries until their results are available.
constexpr unsigned ProfileFrames = 4, ProfilePairs = 48, ProfileScopes = 16;
struct GpuPair { ::IDirect3DQuery9 *begin=nullptr, *end=nullptr; int scope=-1; bool ended=false; };
struct GpuFrame {
    GpuPair pairs[ProfilePairs];
    ::IDirect3DQuery9 *frequency=nullptr;
    unsigned count=0;
    bool pending=false;
};
GpuFrame gpuFrames[ProfileFrames];
int gpuCurrent=0;
bool gpuUnsupported=false;
double gpuMs[ProfileScopes] {};
unsigned gpuFramesRead=0, gpuFramesSkipped=0;

void releaseGpuQueries() {
    for (auto& frame : gpuFrames) {
        for (auto& pair : frame.pairs) {
            if (pair.begin) pair.begin->Release();
            if (pair.end) pair.end->Release();
            pair = {};
        }
        if (frame.frequency) frame.frequency->Release();
        frame = {};
    }
    gpuCurrent = 0;
}

void endGpuFrame() {
    if (!profileDevice || gpuUnsupported) return;
    if (gpuCurrent >= 0) {
        auto& frame = gpuFrames[gpuCurrent];
        if (frame.count) {
            if (!frame.frequency)
                profileDevice->CreateQuery(::D3DQUERYTYPE_TIMESTAMPFREQ, &frame.frequency);
            if (frame.frequency && SUCCEEDED(frame.frequency->Issue(D3DISSUE_END))) frame.pending = true;
            else { gpuUnsupported = true; return; }
        }
    }
    gpuCurrent = -1;
    for (unsigned i=0; i<ProfileFrames; ++i) {
        auto& frame = gpuFrames[i];
        if (frame.pending) {
            UINT64 frequency=0;
            bool ready = frame.frequency->GetData(&frequency, sizeof(frequency), 0) == S_OK && frequency;
            UINT64 starts[ProfilePairs] {}, ends[ProfilePairs] {};
            for (unsigned j=0; ready && j<frame.count; ++j) {
                auto& pair = frame.pairs[j];
                if (!pair.ended) continue;
                ready = pair.begin->GetData(&starts[j], sizeof(UINT64), 0) == S_OK
                     && pair.end->GetData(&ends[j], sizeof(UINT64), 0) == S_OK;
            }
            if (ready) {
                for (unsigned j=0; j<frame.count; ++j)
                    if (frame.pairs[j].ended && ends[j] >= starts[j])
                        gpuMs[frame.pairs[j].scope] += double(ends[j]-starts[j]) * 1000.0 / frequency;
                ++gpuFramesRead;
                frame.pending=false;
                frame.count=0;
            }
        }
        if (!frame.pending && gpuCurrent < 0) gpuCurrent=int(i);
    }
    if (gpuCurrent < 0) ++gpuFramesSkipped;
}

void onPresent(HRESULT result, uint64_t start) {
    if (!NxProf_Enabled()) {
        drawCalls=0;
        return;
    }
    endGpuFrame();
    static uint64_t frames=0, last=0, presentTicks=0, totalDraws=0, windowFrames=0;
    static unsigned failedReports=0;
    const uint64_t now=armGetSystemTick();
    if (!last) last=start;
    presentTicks += now-start;
    totalDraws += drawCalls; drawCalls=0;
    ++windowFrames;
    if (++frames % 600 == 0 || (FAILED(result) && failedReports++ < 8)) {
        const double tickMs=1000.0/armGetSystemTickFreq();
        std::printf("[nx-dxvk] present %llu: interval %.2f ms, present CPU %.2f ms, draws %.1f, hr=%08x, loading=%d\n",
            (unsigned long long)frames, (now-last)*tickMs/windowFrames, presentTicks*tickMs/windowFrames,
            double(totalDraws)/windowFrames, unsigned(result), int(g_nxLoadingHint));
        if (gpuFramesRead) {
            std::printf("[nx-dxvk] GPU ms (%u completed frames, %u skipped):", gpuFramesRead, gpuFramesSkipped);
            for (int scope=0; scope<NxProf_GpuScopeCount() && scope<int(ProfileScopes); ++scope)
                if (gpuMs[scope]) std::printf(" | %s %.2f", NxProf_GpuScopeName(scope), gpuMs[scope]/gpuFramesRead);
            std::printf("\n");
        }
        std::memset(gpuMs, 0, sizeof(gpuMs)); gpuFramesRead=gpuFramesSkipped=0;
        if (profileDevice) NX_DxvkReportCounters(profileDevice);
        last=now; presentTicks=totalDraws=windowFrames=0;
    }
}

void onDeviceCreated(::IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pp) {
    releaseGpuQueries();
    if (profileDevice) profileDevice->Release();
    profileDevice=device; profileDevice->AddRef();
    std::printf("[nx-dxvk] native D3D9 device ready: %ux%u, format %u, interval %08x\n",
        pp->BackBufferWidth, pp->BackBufferHeight, unsigned(pp->BackBufferFormat), pp->PresentationInterval);
}
} // namespace

ULONG IUnknown9Like::AddRef() {
    std::lock_guard<std::mutex> lock(registryMutex);
    unwrap<::IUnknown>(this)->AddRef();
    return ++nxReferences;
}
ULONG IUnknown9Like::Release() {
    std::lock_guard<std::mutex> lock(registryMutex);
    auto* native=unwrap<::IUnknown>(this);
    const unsigned refs=--nxReferences;
    if (!refs) { registry.erase(nxNative); textures.erase(this); nxDestroy(this); }
    native->Release();
    return refs;
}

IDirect3D9* Direct3DCreate9(UINT sdkVersion) {
    std::printf("[nx-dxvk] %s, native ARM64, NVK 26.2.3, VK_NN_vi_surface\n", NX_RENDERER_LABEL);
    ::IDirect3D9* native=nullptr;
    try { native=::Direct3DCreate9(sdkVersion); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "[nx-dxvk] initialization exception: %s\n", error.what());
        return nullptr;
    }
    IDirect3D9* handle=nullptr;
    if (!native) { std::fprintf(stderr, "[nx-dxvk] Direct3DCreate9 failed; see Vulkan feature/initialization errors above\n"); return nullptr; }
    reportFailure("wrap Direct3D9", wrap(native, &handle));
    return handle;
}
int D3DPERF_BeginEvent(D3DCOLOR color, const wchar_t* name) { return ::D3DPERF_BeginEvent(color,name); }
int D3DPERF_EndEvent() { return ::D3DPERF_EndEvent(); }
void D3DPERF_SetMarker(D3DCOLOR color, const wchar_t* name) { ::D3DPERF_SetMarker(color,name); }

#include "d3d9_forwarders.inc"
} // namespace kisak_d3d9

extern "C" void NX_DxvkHoldDeviceLock(void* nativeDevice, bool hold);
extern "C" void NX_D3D_HoldDeviceLock(bool hold) {
    NX_DxvkHoldDeviceLock(kisak_d3d9::profileDevice, hold);
}

extern "C" bool NX_D3D_IsTexture(const void* pointer) {
    std::lock_guard<std::mutex> lock(kisak_d3d9::registryMutex);
    return kisak_d3d9::textures.count(pointer) != 0;
}

extern "C" void NX_DxvkConfigure(const char* executable) {
    NX_DxvkInitThreadAffinity();
    mkdir(NX_GAME_DIR "/shadercache", 0777);
    mkdir(NX_GAME_DIR "/shadercache/" NX_RENDERER_CACHE, 0777);
    mkdir(NX_GAME_DIR "/shadercache/nvk-26.2.3", 0777);
    setenv("NX_DXVK_EXE_PATH", executable, 1);
    // stderr uses the game's asynchronous SD log.
    setenv("DXVK_LOG_PATH", "none", 1);
    setenv("DXVK_SHADER_CACHE_PATH", NX_GAME_DIR "/shadercache/" NX_RENDERER_CACHE, 1);
    setenv("DXVK_STATE_CACHE_PATH", NX_GAME_DIR "/shadercache/" NX_RENDERER_CACHE, 1);
    setenv("MESA_SHADER_CACHE_DIR", NX_GAME_DIR "/shadercache/nvk-26.2.3", 1);
    setenv("MESA_SHADER_CACHE_MAX_SIZE", "256M", 0);
    setenv("DXVK_CONFIG_FILE", NX_GAME_DIR "/" NX_RENDERER_CONFIG, 1);
    // Defaults for installations without a config file.
    struct stat configStat {};
    if (stat(NX_GAME_DIR "/" NX_RENDERER_CONFIG, &configStat) != 0)
        setenv("DXVK_CONFIG", "dxvk.numCompilerThreads=2;d3d9.maxFrameLatency=1;d3d9.textureMemory=128"
            ";dxvk.numShaderCompilerThreads=2;dxvk.shaderCompilationMethod=async;d3d9.deviceLocalConstantBuffers=False"
            ";d3d9.floatEmulation=False;d3d9.strictPow=False"
            , 0);
    std::printf("[nx-dxvk] SD logging active; config %s; persistent shader caches enabled\n",
        NX_GAME_DIR "/" NX_RENDERER_CONFIG);
}

void NxProf_GpuBegin(int scope) {
    if (!NxProf_Enabled()) return;
    using namespace kisak_d3d9;
    if (!profileDevice || gpuUnsupported || gpuCurrent < 0 || scope<0 || scope>=int(ProfileScopes)) return;
    auto& frame=gpuFrames[gpuCurrent];
    if (frame.count == ProfilePairs) return;
    auto& pair=frame.pairs[frame.count];
    if (!pair.begin) profileDevice->CreateQuery(::D3DQUERYTYPE_TIMESTAMP, &pair.begin);
    if (!pair.end) profileDevice->CreateQuery(::D3DQUERYTYPE_TIMESTAMP, &pair.end);
    if (!pair.begin || !pair.end) { gpuUnsupported=true; std::printf("[nx-dxvk] GPU timestamps unavailable\n"); return; }
    pair.scope=scope; pair.ended=false;
    if (SUCCEEDED(pair.begin->Issue(D3DISSUE_END))) ++frame.count;
}
void NxProf_GpuEnd(int scope) {
    if (!NxProf_Enabled()) return;
    using namespace kisak_d3d9;
    if (!profileDevice || gpuUnsupported || gpuCurrent<0) return;
    auto& frame=gpuFrames[gpuCurrent];
    for (int i=int(frame.count)-1; i>=0; --i) {
        auto& pair=frame.pairs[i];
        if (pair.scope==scope && !pair.ended) { pair.ended=SUCCEEDED(pair.end->Issue(D3DISSUE_END)); break; }
    }
}

extern "C" void NX_DxvkServiceRenderThread() {
    if (kisak_d3d9::stopping.load(std::memory_order_acquire)) threadExit();
}
extern "C" void NX_DxvkRequestStop() { kisak_d3d9::stopping.store(true, std::memory_order_release); }
extern "C" bool NX_DxvkShutdown() {
    using namespace kisak_d3d9;
    // Engine threads must exit before the final device release.
    std::printf("[nx-dxvk] releasing queries and native resources\n");
    releaseGpuQueries();
    std::printf("[nx-dxvk] profiling queries released; collecting engine handles\n");
    std::vector<IUnknown9Like*> handles;
    {
        std::lock_guard<std::mutex> lock(registryMutex);
        for (auto& entry : registry) handles.push_back(entry.second);
        registry.clear(); textures.clear();
    }
    for (unsigned kind=0; kind<=2; ++kind) {
        std::printf("[nx-dxvk] shutdown: releasing %s\n", kind==0 ? "resources" : kind==1 ? "device handles" : "factory handles");
        for (auto* handle : handles) if (handle->nxKind==kind) {
            auto* native=unwrap<::IUnknown>(handle);
            for (unsigned ref=0; ref<handle->nxReferences; ++ref) native->Release();
        }
    }
    for (auto* handle : handles) handle->nxDestroy(handle);
    std::printf("[nx-dxvk] shutdown: final device reference (GPU drain and worker joins)\n");
    if (profileDevice) { profileDevice->Release(); profileDevice=nullptr; }
    std::printf("[nx-dxvk] device and worker threads released (%zu engine handles)\n", handles.size());
    return true;
}
