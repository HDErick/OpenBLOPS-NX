#include "nx_bink_audio.h"
#include <switch.h>

namespace
{
std::mutex mutex;
AppletHookCookie hook;
bool initialized, focused = true;
uint64_t backgroundAt, backgroundTime;

uint64_t systemTime()
{
    return armTicksToNs(armGetSystemTick()) / 1000;
}

void focusChanged(AppletHookType type, void *)
{
    if (type != AppletHookType_OnFocusState && type != AppletHookType_OnResume)
        return;
    std::lock_guard<std::mutex> lock(mutex);
    const bool active = appletGetFocusState() == AppletFocusState_InFocus;
    if (active == focused)
        return;
    const uint64_t now = systemTime();
    if (active)
        backgroundTime += now - backgroundAt;
    else
        backgroundAt = now;
    focused = active;
}
}

void NX_BinkInitClock()
{
    std::lock_guard<std::mutex> lock(mutex);
    if (initialized)
        return;
    initialized = true;
    focused = appletGetFocusState() == AppletFocusState_InFocus;
    backgroundAt = systemTime();
    appletHook(&hook, focusChanged, nullptr);
}

bool NX_BinkHasFocus()
{
    std::lock_guard<std::mutex> lock(mutex);
    return focused;
}

uint64_t NX_BinkTimeUs()
{
    std::lock_guard<std::mutex> lock(mutex);
    return (focused ? systemTime() : backgroundAt) - backgroundTime;
}
