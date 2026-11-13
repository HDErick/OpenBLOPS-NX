#pragma once
#include <switch.h>
#include <windows.h>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <thread>
#include <malloc.h>
#include <vulkan/vulkan_core.h>

using HCURSOR = HANDLE;
using LPCSTR = const char *;
struct DEVMODEW {
    WORD dmSize;
    DWORD dmFields, dmBitsPerPel, dmPelsWidth, dmPelsHeight;
    DWORD dmDisplayFlags, dmDisplayFrequency, dmDisplayOrientation;
};
constexpr DWORD DM_PELSWIDTH=0x80000, DM_PELSHEIGHT=0x100000, DM_BITSPERPEL=0x40000;
constexpr DWORD DM_DISPLAYFREQUENCY=0x400000, DM_DISPLAYFLAGS=0x200000, DM_DISPLAYORIENTATION=0x80;
constexpr DWORD DMDO_DEFAULT=0, DMDO_90=1, DMDO_180=2, DMDO_270=3, DM_INTERLACED=2;
constexpr LONG GWL_STYLE=-16, GWL_EXSTYLE=-20;
constexpr DWORD WS_VISIBLE=0x10000000, WS_CAPTION=0xc00000, WS_THICKFRAME=0x40000;
constexpr DWORD WS_SYSMENU=0x80000, WS_MINIMIZEBOX=0x20000, WS_MAXIMIZEBOX=0x10000;
constexpr DWORD WS_EX_TOPMOST=8, WS_EX_WINDOWEDGE=0x100, WS_EX_CLIENTEDGE=0x200;
constexpr DWORD WS_OVERLAPPEDWINDOW=WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX;
constexpr DWORD WS_EX_OVERLAPPEDWINDOW=WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE;
constexpr DWORD SWP_NOACTIVATE=0x10, SWP_NOZORDER=4, SWP_FRAMECHANGED=0x20, SWP_SHOWWINDOW=0x40;
#define HWND_TOPMOST reinterpret_cast<HWND>(intptr_t(-1))

inline HMODULE LoadLibraryA(LPCSTR) { return nullptr; }
inline HMODULE GetModuleHandleA(LPCSTR) { return nullptr; }
inline HMODULE GetModuleHandleW(LPCWSTR) { return nullptr; }
inline HMODULE GetModuleHandle(LPCSTR) { return nullptr; }
inline void *GetProcAddress(HMODULE,LPCSTR) { return nullptr; }
inline void FreeLibrary(HMODULE) {}
inline BOOL CloseHandle(HANDLE) { return FALSE; }
inline BOOL IsWindow(HWND w) { return w == nwindowGetDefault() && nwindowIsValid(nwindowGetDefault()); }
inline BOOL GetWindowRect(HWND,RECT *r) {
    u32 w=1280,h=720; nwindowGetDimensions(nwindowGetDefault(),&w,&h);
    *r={0,0,LONG(w),LONG(h)}; return TRUE;
}
inline BOOL ClientToScreen(HWND,POINT*) { return TRUE; }
inline LONG GetWindowLongW(HWND,int index) { return index==GWL_STYLE ? WS_VISIBLE : 0; }
inline LONG SetWindowLongW(HWND,int,LONG value) { return value; }
inline BOOL SetWindowPos(HWND w,HWND,int,int,int,int,UINT) { return IsWindow(w); }
inline POINT nxSarekCursor{};
inline BOOL GetCursorPos(POINT *p) { *p=nxSarekCursor; return TRUE; }
inline BOOL SetCursorPos(int x,int y) { nxSarekCursor={x,y}; return TRUE; }
inline HCURSOR SetCursor(HCURSOR) { return nullptr; }
inline void Sleep(DWORD ms) { svcSleepThread(uint64_t(ms)*1000000); }
inline void *_aligned_malloc(size_t size,size_t alignment) { return memalign(alignment,size); }
inline void _aligned_free(void *p) { free(p); }

extern "C" VkResult NX_SarekCreateSurface(VkInstance, VkSurfaceKHR*);
// The Vulkan device's allocation callbacks (horizon_alloc.cpp), given at
// vkCreateDevice and vkDestroyDevice (tools/nx/patch-dxvk-sarek.py).
extern "C" const VkAllocationCallbacks *NX_DxvkDeviceAllocator(void);
