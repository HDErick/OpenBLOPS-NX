#include "nx_clock.h"
#include "nx_load_boost.h"

#include <switch.h>

#include <cstdio>

namespace
{
constexpr u32 NX_HANDHELD_DEFAULT_HZ = 307200000;   // what the system sets undocked

bool s_useClkrst, s_usePcv;
ClkrstSession s_gpuSession;
u32 s_originalHz;        // the handheld clock before the first change, 0 before it
u64 s_lastCheckTick;
u32 s_lastReportedHz;
int s_lastRequestedMhz = -1;

bool nxClockGet(u32 *hz)
{
    if ( s_useClkrst )
        return R_SUCCEEDED(clkrstGetClockRate(&s_gpuSession, hz));
    if ( s_usePcv )
        return R_SUCCEEDED(pcvGetClockRate(PcvModule_GPU, hz));
    return false;
}

bool nxClockSet(u32 hz)
{
    if ( s_useClkrst )
        return R_SUCCEEDED(clkrstSetClockRate(&s_gpuSession, hz));
    if ( s_usePcv )
        return R_SUCCEEDED(pcvSetClockRate(PcvModule_GPU, hz));
    return false;
}

u32 nxClockHandheldHz(int mhz)
{
    if ( mhz <= 0 )
        return 0;
    if ( mhz <= 307 )
        return 307200000;
    if ( mhz <= 384 )
        return 384000000;
    return 460800000;
}
}

void NX_ClockInit()
{
    if ( hosversionAtLeast(8, 0, 0) )
    {
        if ( R_SUCCEEDED(clkrstInitialize()) )
        {
            if ( R_SUCCEEDED(clkrstOpenSession(&s_gpuSession, PcvModuleId_GPU, 3)) )
                s_useClkrst = true;
            else
                clkrstExit();
        }
    }
    else if ( R_SUCCEEDED(pcvInitialize()) )
    {
        s_usePcv = true;
    }
    printf("[nx-clock] GPU clock control %s\n",
           s_useClkrst ? "through clkrst" : s_usePcv ? "through pcv" : "unavailable");
}

static void nxClockUpdate(int handheldMhz)
{
    if (appletGetFocusState() != AppletFocusState_InFocus)
        return;
    if ( !s_useClkrst && !s_usePcv )
        return;
    const u64 now = armGetSystemTick();
    if ( s_lastCheckTick && s_lastRequestedMhz == handheldMhz
        && armTicksToNs(now - s_lastCheckTick) < 2000000000ull )
        return;
    s_lastCheckTick = now;
    s_lastRequestedMhz = handheldMhz;
    if ( appletGetOperationMode() != AppletOperationMode_Handheld ) {
        s_originalHz = s_lastReportedHz = 0;
        return;
    }
    u32 current = 0;
    if ( !nxClockGet(&current) )
        return;
    if (s_originalHz && current != s_lastReportedHz)
        s_originalHz = s_lastReportedHz = 0;
    const u32 requested = nxClockHandheldHz(handheldMhz);
    const u32 want = requested ? requested : s_originalHz;
    if (!want || current == want)
        return;
    // Keep external clock overrides; otherwise adjust only our own setting.
    if (!s_originalHz && current != NX_HANDHELD_DEFAULT_HZ)
        return;
    if ( nxClockSet(want) )
    {
        if (!s_originalHz) s_originalHz = current;
        printf("[nx-clock] handheld GPU clock %.1f -> %.1f MHz\n", current / 1e6, want / 1e6);
        s_lastReportedHz = want;
        if (!requested) s_originalHz = s_lastReportedHz = 0;
    }
}

void NX_ClockUpdate(int handheldMhz)
{
    NX_LoadBoostRunGpuClockUpdate(nxClockUpdate, handheldMhz);
}

void NX_ClockExit()
{
    // Put back only our own change: a clock another tool set since stays.
    u32 current = 0;
    if ( s_originalHz && appletGetOperationMode() == AppletOperationMode_Handheld && nxClockGet(&current)
        && current == s_lastReportedHz )
        nxClockSet(s_originalHz);
    if ( s_useClkrst )
    {
        clkrstCloseSession(&s_gpuSession);
        clkrstExit();
    }
    if ( s_usePcv )
        pcvExit();
    s_useClkrst = s_usePcv = false;
    s_originalHz = s_lastReportedHz = 0;
    s_lastCheckTick = 0;
    s_lastRequestedMhz = -1;
}
