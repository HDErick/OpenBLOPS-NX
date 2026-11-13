#include <switch.h>
#include "../../../src/nx/nx_load_boost.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static AppletFocusState focus = AppletFocusState_InFocus;
static AppletHookFn callback;
static std::vector<ApmCpuBoostMode> requests;
static ApmCpuBoostMode mode = ApmCpuBoostMode_Normal;
static Result nextResult;
static unsigned firmware = 20;
static u64 now;

bool hosversionAtLeast(unsigned major, unsigned, unsigned) { return firmware >= major; }
AppletFocusState appletGetFocusState() { return focus; }
Result appletSetCpuBoostMode(ApmCpuBoostMode requested)
{
    requests.push_back(requested);
    const Result result = nextResult;
    nextResult = 0;
    if (!result) mode = requested;
    return result;
}
void appletHook(AppletHookCookie *, AppletHookFn fn, void *) { callback = fn; }
void appletUnhook(AppletHookCookie *) { callback = nullptr; }
u64 armGetSystemTick() { return now; }
u64 armTicksToNs(u64 ticks) { return ticks; }

static void check(bool condition, const char *message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static void expect(bool boosted, size_t calls)
{
    check(NX_LoadBoostActive() == boosted, "boost ownership");
    check((mode == ApmCpuBoostMode_FastLoad) == boosted, "requested system mode");
    check(requests.size() == calls, "unexpected or missing clock request");
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    const std::string scenario = argv[1];
    if (scenario == "unsupported") firmware = 6;
    if (scenario == "denied") nextResult = 0x1234;
    NX_LoadBoostInit();

    if (scenario == "unsupported" || scenario == "denied")
    {
        NX_LoadBoostSet(NxLoadPhase::Boot, false);
        NX_LoadBoostSet(NxLoadPhase::Server, true);
        NX_LoadBoostSet(NxLoadPhase::Server, false);
        NX_LoadBoostUpdate();
        NX_LoadBoostExit();
        expect(false, scenario == "unsupported" ? 0 : 1);
    }
    else if (scenario == "lifecycle")
    {
        expect(true, 1);
        NX_LoadBoostSet(NxLoadPhase::Server, true);
        NX_LoadBoostSet(NxLoadPhase::Boot, false);
        NX_LoadBoostSet(NxLoadPhase::Client, true);
        NX_LoadBoostSet(NxLoadPhase::Client, false);
        expect(true, 1);
        NX_LoadBoostSet(NxLoadPhase::Client, true);
        NX_LoadBoostSet(NxLoadPhase::Server, false);
        expect(true, 1);
        NX_LoadBoostSet(NxLoadPhase::Client, false);
        expect(false, 2);
        for (unsigned frame = 0; frame < 10000; ++frame) NX_LoadBoostUpdate();
        expect(false, 2);
        NX_LoadBoostSet(NxLoadPhase::Server, true);
        expect(true, 3);
        NX_LoadBoostReset("load error");
        expect(false, 4);
        NX_LoadBoostSet(NxLoadPhase::Client, true);
        NX_LoadBoostSet(NxLoadPhase::Client, false);
        expect(false, 6);
        NX_LoadBoostExit();
        check(!callback, "exit left applet hook installed");
        NX_LoadBoostSet(NxLoadPhase::Server, true);
        expect(false, 6);
    }
    else if (scenario == "focus")
    {
        focus = AppletFocusState_OutOfFocus;
        callback(AppletHookType_OnFocusState, nullptr);
        expect(false, 2);
        focus = AppletFocusState_InFocus;
        callback(AppletHookType_OnFocusState, nullptr);
        expect(true, 3);
        callback(AppletHookType_OnPerformanceMode, nullptr);
        expect(true, 3);
        callback(AppletHookType_OnResume, nullptr);
        expect(true, 4);
        focus = AppletFocusState_Background;
        callback(AppletHookType_OnFocusState, nullptr);
        NX_LoadBoostSet(NxLoadPhase::Boot, false);
        focus = AppletFocusState_InFocus;
        callback(AppletHookType_OnFocusState, nullptr);
        callback(AppletHookType_OnResume, nullptr);
        expect(false, 5);
        NX_LoadBoostExit();
    }
    else if (scenario == "restore-retry")
    {
        nextResult = 0x4321;
        NX_LoadBoostSet(NxLoadPhase::Boot, false);
        expect(true, 2);
        NX_LoadBoostUpdate();
        expect(true, 2);
        now = 500000000;
        NX_LoadBoostUpdate();
        expect(false, 3);
        NX_LoadBoostUpdate();
        expect(false, 3);
        NX_LoadBoostExit();
    }
    else if (scenario == "exit")
    {
        NX_LoadBoostSet(NxLoadPhase::Client, true);
        nextResult = 0x4321;
        NX_LoadBoostExit();
        expect(true, 2);
        NX_LoadBoostSet(NxLoadPhase::Server, true);
        expect(true, 2);
        NX_LoadBoostExit();
        expect(false, 3);
        NX_LoadBoostExit();
        expect(false, 3);
        check(!callback, "exit left applet hook installed");
    }
    else return 2;

    std::printf("PASS: %s\n", scenario.c_str());
}
