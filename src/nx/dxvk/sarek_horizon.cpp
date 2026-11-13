#include "sarek_horizon.h"
#include "util/util_monitor.h"
#include "util/util_shared_res.h"
#include <vulkan/vulkan.h>

extern "C" PFN_vkVoidFunction nvk_loaderless_GetInstanceProcAddr(VkInstance,const char*);
extern "C" VkResult NX_SarekCreateSurface(VkInstance instance,VkSurfaceKHR *surface) {
    auto create=reinterpret_cast<PFN_vkCreateViSurfaceNN>(nvk_loaderless_GetInstanceProcAddr(instance,"vkCreateViSurfaceNN"));
    if (!create) return VK_ERROR_EXTENSION_NOT_PRESENT;
    VkViSurfaceCreateInfoNN info{VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN};
    info.window=nwindowGetDefault();
    return create(instance,&info,nullptr,surface);
}
namespace dxvk {
HMONITOR GetDefaultMonitor() { return reinterpret_cast<HMONITOR>(1); }
BOOL SetMonitorDisplayMode(HMONITOR monitor,DEVMODEW*) { return monitor!=nullptr; }
BOOL RestoreMonitorDisplayMode() { return TRUE; }
BOOL GetMonitorDisplayMode(HMONITOR monitor,DWORD index,DEVMODEW *result) {
    if (!monitor || !result) return FALSE;
    u32 w=1280,h=720;
    if (index==ENUM_CURRENT_SETTINGS || index==ENUM_REGISTRY_SETTINGS)
        nwindowGetDimensions(nwindowGetDefault(),&w,&h);
    else if (index==0) { w=960; h=540; }
    else if (index!=1) return FALSE;
    *result={}; result->dmSize=sizeof(*result);
    result->dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL|DM_DISPLAYFREQUENCY;
    result->dmPelsWidth=w; result->dmPelsHeight=h; result->dmBitsPerPel=32; result->dmDisplayFrequency=60;
    return TRUE;
}
void GetWindowClientSize(HWND,UINT *width,UINT *height) {
    u32 w=1280,h=720; nwindowGetDimensions(nwindowGetDefault(),&w,&h);
    if (width) *width=w; if (height) *height=h;
}
void GetMonitorClientSize(HMONITOR,UINT *width,UINT *height) { GetWindowClientSize(nullptr,width,height); }
void GetMonitorRect(HMONITOR,RECT *rect) { GetWindowRect(nullptr,rect); }
HANDLE openKmtHandle(HANDLE) { return INVALID_HANDLE_VALUE; }
bool setSharedMetadata(HANDLE,void*,uint32_t) { return false; }
bool getSharedMetadata(HANDLE,void*,uint32_t,uint32_t*) { return false; }
}
