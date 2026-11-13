#pragma once
#include "../load-boost-shim/switch.h"
using u32 = uint32_t;
#define R_SUCCEEDED(result) ((result) == 0)
struct ClkrstSession {};
enum { PcvModule_GPU, PcvModuleId_GPU };
enum AppletOperationMode { AppletOperationMode_Handheld, AppletOperationMode_Console };
AppletOperationMode appletGetOperationMode();
Result clkrstInitialize();
void clkrstExit();
Result clkrstOpenSession(ClkrstSession *, int, int);
void clkrstCloseSession(ClkrstSession *);
Result clkrstGetClockRate(ClkrstSession *, u32 *);
Result clkrstSetClockRate(ClkrstSession *, u32);
Result pcvInitialize();
void pcvExit();
Result pcvGetClockRate(int, u32 *);
Result pcvSetClockRate(int, u32);
