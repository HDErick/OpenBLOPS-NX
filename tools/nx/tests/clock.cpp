#include <switch.h>
#include "../../../src/nx/nx_clock.h"
#include "../../../src/nx/nx_load_boost.h"
#include <cstdio>
#include <stdexcept>
#include <string>

static u64 now = 1;
static u32 rate = 307200000;
static unsigned firmware = 20, sets, clkrstOpens, pcvOpens, closes;
static Result serviceFailure, setFailure, boostFailure;
static AppletFocusState focus = AppletFocusState_InFocus;
static AppletOperationMode operation = AppletOperationMode_Handheld;
static AppletHookFn hook;
bool hosversionAtLeast(unsigned major, unsigned, unsigned) { return firmware >= major; }
AppletFocusState appletGetFocusState() { return focus; }
AppletOperationMode appletGetOperationMode() { return operation; }
u64 armGetSystemTick() { return now; }
u64 armTicksToNs(u64 ticks) { return ticks; }
void appletHook(AppletHookCookie *, AppletHookFn fn, void *) { hook = fn; }
void appletUnhook(AppletHookCookie *) { hook = nullptr; }
Result appletSetCpuBoostMode(ApmCpuBoostMode mode) {
    if (boostFailure) { auto result = boostFailure; boostFailure = 0; return result; }
    rate = mode == ApmCpuBoostMode_FastLoad ? 76800000 : 307200000;
    return 0;
}
Result clkrstInitialize() { ++clkrstOpens; return serviceFailure; }
void clkrstExit() { ++closes; }
Result clkrstOpenSession(ClkrstSession *, int, int) { return 0; }
void clkrstCloseSession(ClkrstSession *) {}
Result clkrstGetClockRate(ClkrstSession *, u32 *value) { *value = rate; return 0; }
Result clkrstSetClockRate(ClkrstSession *, u32 value) {
    ++sets;
    if (setFailure) { auto result = setFailure; setFailure = 0; return result; }
    rate = value; return 0;
}
Result pcvInitialize() { ++pcvOpens; return serviceFailure; }
void pcvExit() { ++closes; }
Result pcvGetClockRate(int, u32 *value) { *value = rate; return 0; }
Result pcvSetClockRate(int, u32 value) { return clkrstSetClockRate(nullptr, value); }
static void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
static void update(int mhz) { now += 2000000000; NX_ClockUpdate(mhz); }
int main(int argc, char **argv) {
    try {
        require(argc == 2, "missing scenario");
        const std::string scenario = argv[1];
        if (scenario == "legacy") firmware = 7;
        if (scenario == "denied") serviceFailure = 1;
        NX_ClockInit();
        NX_LoadBoostInit();
        update(460);
        require(!sets, "GPU changed during boot boost");
        NX_LoadBoostSet(NxLoadPhase::Boot, false);
        if (scenario == "denied") {
            update(460); require(!sets, "unavailable clock service used"); NX_ClockExit();
            require(!closes, "uninitialized clock service closed");
        } else {
            if (scenario == "set-failure") {
                setFailure = 1; update(460);
                require(rate == 307200000, "failed set changed ownership");
            }
            update(460); require(rate == 460800000, "handheld clock not applied");
            const auto calls = sets;
            for (unsigned i = 0; i < 10000; ++i) NX_ClockUpdate(460);
            require(sets == calls, "clock update not throttled");
            NX_ClockUpdate(384); require(rate == 384000000, "owned clock could not change");
            NX_ClockUpdate(0); require(rate == 307200000, "disable did not restore original");
            update(460);
            rate = 614400000; update(384); NX_ClockExit();
            require(rate == 614400000, "external clock override overwritten");
            NX_ClockInit(); rate = 307200000; update(460);
            operation = AppletOperationMode_Console; rate = 768000000; update(384);
            require(rate == 768000000, "docked clock changed");
            operation = AppletOperationMode_Handheld; rate = 307200000; update(460);
            require(rate == 460800000, "undocking did not restore preference");
            focus = AppletFocusState_OutOfFocus; rate = 307200000; update(460);
            require(rate == 307200000, "clock changed out of focus");
            focus = AppletFocusState_InFocus; update(460);
            NX_LoadBoostSet(NxLoadPhase::Client, true); update(460);
            require(rate == 76800000, "GPU update interfered with load boost");
            boostFailure = 1; NX_LoadBoostSet(NxLoadPhase::Client, false); update(460);
            require(rate == 76800000, "GPU update bypassed pending Normal restore");
            NX_LoadBoostUpdate(); update(460);
            require(rate == 460800000, "GPU preference not restored after loading");
            NX_ClockExit(); require(rate == 307200000, "exit did not restore owned clock");
            require(firmware < 8 ? (pcvOpens == 2 && !clkrstOpens) : (clkrstOpens == 2 && !pcvOpens), "wrong firmware service");
        }
        NX_LoadBoostExit();
        printf("PASS: clock %s\n", scenario.c_str());
    } catch (const std::exception &error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
