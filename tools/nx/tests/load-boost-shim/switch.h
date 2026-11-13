#pragma once
#include <cstdint>

using u64 = uint64_t;
using Result = uint32_t;
#define R_FAILED(result) ((result) != 0)

enum ApmCpuBoostMode { ApmCpuBoostMode_Normal, ApmCpuBoostMode_FastLoad };
enum AppletFocusState { AppletFocusState_InFocus = 1, AppletFocusState_OutOfFocus, AppletFocusState_Background };
enum AppletHookType { AppletHookType_OnFocusState, AppletHookType_OnResume, AppletHookType_OnPerformanceMode };
using AppletHookFn = void (*)(AppletHookType, void *);
struct AppletHookCookie {};

bool hosversionAtLeast(unsigned major, unsigned minor, unsigned micro);
AppletFocusState appletGetFocusState();
Result appletSetCpuBoostMode(ApmCpuBoostMode mode);
void appletHook(AppletHookCookie *, AppletHookFn callback, void *);
void appletUnhook(AppletHookCookie *);
u64 armGetSystemTick();
u64 armTicksToNs(u64 ticks);
