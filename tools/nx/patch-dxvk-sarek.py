"""Idempotent Horizon changes for Sarek 1.13.0 (37f397e142b977a343e920dbc4c7bf7ed2c63a81)."""
from pathlib import Path
import sys
source=Path(sys.argv[1])
assert "version : 'v1.13.0'" in (source/'meson.build').read_text()
def replace(path,before,after):
    file=source/path
    content=file.read_text()
    if (after and after in content) or (not after and before not in content):
        return
    assert content.count(before)==1, f'Unexpected pinned source in {path}: {before[:80]}'
    file.write_text(content.replace(before,after))

replace('src/vulkan/vulkan_loader.cpp', 'namespace dxvk::vk {',
    'extern "C" PFN_vkVoidFunction nvk_loaderless_GetInstanceProcAddr(VkInstance, const char*);\n\nnamespace dxvk::vk {')
replace('src/vulkan/vulkan_loader.cpp', '''  : m_library(LoadLibraryA("vulkan-1"))
  , m_getInstanceProcAddr(reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(m_library, "vkGetInstanceProcAddr"))) {''',
    '''  : m_library(nullptr), m_getInstanceProcAddr(nvk_loaderless_GetInstanceProcAddr) {''')
replace('src/vulkan/vulkan_presenter.cpp', '''    HINSTANCE instance = reinterpret_cast<HINSTANCE>(
      GetWindowLongPtr(m_window, GWLP_HINSTANCE));

    VkWin32SurfaceCreateInfoKHR info;
    info.sType      = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.pNext      = nullptr;
    info.flags      = 0;
    info.hinstance  = instance;
    info.hwnd       = m_window;

    VkResult status = m_vki->vkCreateWin32SurfaceKHR(
      m_vki->instance(), &info, nullptr, &m_surface);''',
    '''    VkResult status = NX_SarekCreateSurface(m_vki->instance(), &m_surface);''')
replace('src/dxvk/platform/dxvk_win32_exts.cpp','"Win32 WSI"','"Horizon WSI"')
replace('src/dxvk/platform/dxvk_win32_exts.cpp','VK_KHR_WIN32_SURFACE_EXTENSION_NAME','VK_NN_VI_SURFACE_EXTENSION_NAME')
replace('src/dxvk/dxvk_instance.cpp', '''    m_extProviders.push_back(&VrInstance::s_instance);
    m_extProviders.push_back(&DxvkXrProvider::s_instance);''',
    '')
replace('src/d3d11/d3d11_include.h', '#ifndef _MSC_VER', '#if !defined(_MSC_VER) && !defined(NX_DXVK_SAREK)')
replace('src/d3d9/d3d9_adapter.cpp', '''    DISPLAY_DEVICEA device = { };
    device.cb = sizeof(device);

    if (!::EnumDisplayDevicesA(nullptr, m_displayIndex, &device, 0)) {
      Logger::err("D3D9Adapter::GetAdapterIdentifier: Failed to query display info");
      return D3DERR_INVALIDCALL;
    }''', '')
replace('src/d3d9/d3d9_adapter.cpp', 'device.DeviceName', '"Horizon"')
replace('src/d3d9/d3d9_adapter.cpp', 'const char* driver = GetDriverDLL(DxvkGpuVendor(vendorId));',
    'const char* driver = "NVK (Horizon)";')
# libnx paths are UTF-8.
for path in ('src/util/config/config.cpp', 'src/d3d9/d3d9_shader.cpp', 'src/d3d9/d3d9_fixed_function.cpp'):
    file = source/path
    text = file.read_text()
    if 'str::tows(' in text:
        file.write_text(text.replace('str::tows(', 'str::topath('))
replace('src/dxvk/dxvk_state_cache.h', 'std::wstring getCacheFileName()', 'std::string getCacheFileName()')
replace('src/dxvk/dxvk_state_cache.cpp', 'std::wstring DxvkStateCache::getCacheFileName()', 'std::string DxvkStateCache::getCacheFileName()')
replace('src/dxvk/dxvk_state_cache.cpp', 'return str::tows(path.c_str());', 'return str::topath(path.c_str());')
replace('src/dxvk/dxvk_pipemanager.h', 'void stopWorkerThreads() const;', 'void stopWorkerThreads();')
replace('src/dxvk/dxvk_pipemanager.cpp',
    '  DxvkPipelineManager::~DxvkPipelineManager() = default;',
    '''  DxvkPipelineManager::~DxvkPipelineManager() {
    stopWorkerThreads();
  }''')
replace('src/dxvk/dxvk_pipemanager.cpp',
    '  void DxvkPipelineManager::stopWorkerThreads() const {',
    '''  void DxvkPipelineManager::stopWorkerThreads() {
    m_async = nullptr;
    m_dyasync = nullptr;''')
replace('src/dxvk/dxvk_asynccompiler.h', '    ~DxvkAsyncCompiler();',
    '''    ~DxvkAsyncCompiler();

    bool isCompilingShaders() const {
      return m_pending.load(std::memory_order_relaxed) != 0;
    }''')
replace('src/dxvk/dxvk_asynccompiler.h', '    std::atomic<bool>           m_compilerStop{false};',
    '''    std::atomic<uint32_t>       m_pending{0};
    std::atomic<bool>           m_compilerStop{false};''')
replace('src/dxvk/dxvk_asynccompiler.cpp',
    '      m_compilerQueue.emplace(PipelineEntry{ pipeline, state, renderPass });',
    '''      m_compilerQueue.emplace(PipelineEntry{ pipeline, state, renderPass });
      m_pending.fetch_add(1, std::memory_order_relaxed);''')
replace('src/dxvk/dxvk_asynccompiler.cpp',
    '''            entry.pipeline->writePipelineStateToCache(entry.state, entry.renderPass->format());
        }
      }''',
    '''            entry.pipeline->writePipelineStateToCache(entry.state, entry.renderPass->format());
        }
        m_pending.fetch_sub(1, std::memory_order_relaxed);
      }''')
replace('src/dxvk/dxvk_pipemanager.cpp',
    '''    return m_stateCache != nullptr
        && m_stateCache->isCompilingShaders();''',
    '''    return (m_stateCache != nullptr && m_stateCache->isCompilingShaders())
        || (m_async != nullptr && m_async->isCompilingShaders());''')
for path in ('src/dxvk/dxvk_fence.h', 'src/dxvk/dxvk_memory.h'):
    replace(path, '#ifdef _WIN32', '#if defined(_WIN32) || defined(__SWITCH__)')
env_file = 'src/util/util_env.cpp'
linux_exe_path = '#elif defined(__linux__)\n    std::array<char, PATH_MAX> exePath'
old_switch_exe_path = (
    '#elif defined(__SWITCH__)\n    auto path = getEnvVar("NX_DXVK_EXE_PATH");\n'
    '    return path.empty() ? "sdmc:/switch/kisakblack/KisakBlack.nro" : path;\n'
)
switch_exe_path = '#elif defined(__SWITCH__)\n    return getEnvVar("NX_DXVK_EXE_PATH");\n'
if old_switch_exe_path in (source / env_file).read_text():
    replace(env_file, old_switch_exe_path, switch_exe_path)
else:
    replace(env_file, linux_exe_path, switch_exe_path + linux_exe_path)
replace('src/util/util_env.cpp', 'namespace dxvk::env {',
    'extern "C" void NX_DxvkSetThreadName(const char*);\n\nnamespace dxvk::env {')
replace('src/util/util_env.cpp', '    ::pthread_setname_np(pthread_self(), posixName.data());',
    '    NX_DxvkSetThreadName(name.c_str());')
# The device's allocation callbacks (src/nx/dxvk/horizon_alloc.cpp): NVK allocates
# an object per descriptor set, about one a draw, under newlib's one heap lock by
# default. The device is destroyed with the callbacks it was created with.
file=source/'src/dxvk/dxvk_adapter.cpp'
text=file.read_text()
if 'NX_DxvkDeviceAllocator' not in text:
    assert text.count('vkCreateDevice(m_handle, &info, nullptr, &device)')==2
    file.write_text(text.replace('vkCreateDevice(m_handle, &info, nullptr, &device)',
        'vkCreateDevice(m_handle, &info, NX_DxvkDeviceAllocator(), &device)'))
replace('src/vulkan/vulkan_loader.cpp', 'this->vkDestroyDevice(m_device, nullptr);',
    'this->vkDestroyDevice(m_device, NX_DxvkDeviceAllocator());')
replace('src/util/thread.h','      ::pthread_setschedparam(this->native_handle(), policy, &param);',
    '      (void)policy; (void)param;')
replace('src/util/thread.h','case ThreadPriority::Lowest: policy = SCHED_IDLE;',
    'case ThreadPriority::Lowest: policy = SCHED_OTHER;')
file=source/'src/d3d9/d3d9_swapchain.cpp'
text=file.read_text()
if 'struct D3D9WindowData {' in text:
    start=text.index('  struct D3D9WindowData {')
    end=text.index('  static uint16_t MapGammaControlPoint',start)
    original=text[start:end]
    replace(str(file.relative_to(source)),original,'''  static void SetActivateProcessed(HWND,bool) {}
  static void ResetWindowProc(HWND) {}
  static void HookWindowProc(HWND,D3D9SwapChainEx*) {}
  class D3D9WindowMessageFilter { public: explicit D3D9WindowMessageFilter(HWND,bool=true) {} };

''')
replace('src/d3d9/d3d9_device.cpp', '''    const bool hwCursor = (inputWidth  == HardwareCursorWidth &&
                           inputHeight == HardwareCursorHeight)
                          || params.Windowed;''', '    const bool hwCursor = false;')
replace('src/d3d9/d3d9_cursor.cpp',
    '  void D3D9Cursor::ResetHardwareCursor() {\n    ::DestroyCursor(m_hCursor);\n    m_hCursor = nullptr;',
    '  void D3D9Cursor::ResetHardwareCursor() {\n    m_hCursor = nullptr;')
replace('src/d3d9/d3d9_cursor.h','        ::DestroyCursor(m_hCursor);','        ResetHardwareCursor();')
# Reject GetDC before it locks the texture.
file=source/'src/d3d9/d3d9_surface.cpp'
text=file.read_text()
if '    *phDC = nullptr;' not in text:
    start=text.index('  HRESULT STDMETHODCALLTYPE D3D9Surface::GetDC(HDC *phDC) {')
    end=text.index('  HRESULT STDMETHODCALLTYPE D3D9Surface::ReleaseDC',start)
    replace(str(file.relative_to(source)),text[start:end],'''  HRESULT STDMETHODCALLTYPE D3D9Surface::GetDC(HDC *phDC) {
    if (!phDC) return D3DERR_INVALIDCALL;
    *phDC = nullptr;
    return D3DERR_NOTAVAILABLE;
  }

''')
file=source/'src/d3d9/d3d9_cursor.cpp'
text=file.read_text()
if '    if (m_sCursor.Bitmap != nullptr)' in text:
    start=text.index('    if (m_sCursor.Bitmap != nullptr)',text.index('void D3D9Cursor::SetHardwareCursor'))
    end=text.index('\n  }',start)
    replace(str(file.relative_to(source)),text[start:end],'')
replace('src/dxvk/dxvk_pipemanager.cpp', '          DxvkShaderCompileMethod::Dyasync));',
    '          DxvkShaderCompileMethod::Async));')
print('DXVK Sarek Horizon patches ready')
