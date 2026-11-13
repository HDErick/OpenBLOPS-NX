#include "splitscreen.h"
#include <client/client.h>
#include <qcommon/cmd.h>
#include <client_mp/cl_cgame_mp.h>
#include <ui_mp/ui_main_mp.h>
#include <client/cl_keys.h>
#include <ui/ui_main.h>
#include <ui/keycodes.h>
#include <win32/win_gamepad.h>
#include <win32/win_input.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifndef KISAK_SP
// Loaded PC menu assets have one mutable UI context. Lend it to P2 while
// its menu is open; profile, clientscript and world ownership stay with P1.
struct SplitGuestMenuState
{
    bool owner = false, dispatching = false;
    char mainMenu[256] = "class";
    int pending = -1, serverId = 0, port = -1;
    int blockedFrame = -1, direction = 0, repeatTime = 0;
    unsigned int previous = 0, suppressed = 0;
};
static SplitGuestMenuState s_guestMenu;
static bool s_guestMenuDrawing = false;
bool CL_SplitGuestDrawingMenu() { return s_guestMenuDrawing; }
const char *CL_SplitGuestScriptMainMenu() { return s_guestMenu.mainMenu; }

void CL_SplitGuestDrawMenu()
{
    if (!s_guestMenu.owner) return;
    const ScreenPlacement saved = scrPlaceView[0];
    const ScreenPlacement savedFull = scrPlaceFull;
    scrPlaceView[0] = scrPlaceView[1];
    scrPlaceFull = scrPlaceView[1];
    s_guestMenuDrawing = true;
    UI_UpdateTime(0, cls.realtime);
    UI_Refresh(0);
    s_guestMenuDrawing = false;
    scrPlaceFull = savedFull;
    scrPlaceView[0] = saved;
}


bool CL_SplitGuestOwnsMenu() { return s_guestMenu.owner; }
bool CL_SplitGuestMenuAcceptsUiInput() { return !s_guestMenu.owner || s_guestMenu.dispatching; }
bool CL_SplitGuestMenuBlocksInput()
{
    return s_guestMenu.owner || s_guestMenu.blockedFrame == cls.realtime;
}

void CL_SplitGuestResetMenu()
{
    s_guestMenu.pending = -1;
    // Keep ownership through onClose so a stale response cannot reach P1.
    if (s_guestMenu.owner)
    {
        s_guestMenu.serverId = 0;
        UI_SetActiveMenu(0, UIMENU_NONE);
    }
    s_guestMenu = SplitGuestMenuState{};
}

bool CL_SplitGuestMenuResponse(const char *menu, const char *response)
{
    if (!s_guestMenu.owner)
        return false;
    // Only P2's dispatched input may select/cancel its menu. Deferred onClose
    // scripts from P1's outgoing menu must not send "back" on P2's connection.
    if (!s_guestMenu.dispatching)
        return true;
    // Consume invalid/stale replies instead of falling through to P1.
    if (!CL_IsSplitGuest(1) || !clients || !clients[1].serverId
        || s_guestMenu.serverId != clients[1].serverId || !menu || !response)
        return true;
    int index = 0;
    while (index < 32 && I_stricmp(CL_GetConfigString(2548 + index), menu))
        ++index;
    if (index == 32 || std::strlen(response) > 256 || std::strpbrk(response, "\r\n;\""))
        return true;
    char command[384], event[448];
    std::snprintf(command, sizeof(command), "mr %d %d %s", clients[1].serverId, index, response);
    CL_AddReliableCommand(1, command);
    std::snprintf(event, sizeof(event), "P2 native menu %s -> %s", menu, response);
    CL_SplitGuestLog(event);
    return true;
}

void CL_SplitGuestHandleMenuCommand(const char *text)
{
    if (text[0] == 'v' && (text[1] == ' ' || text[1] == '\t'))
    {
        Cmd_TokenizeStringNoEval(text);
        for (int i = 1; i + 1 < Cmd_Argc(); i += 2)
            if (!I_stricmp(Cmd_Argv(i), "g_scriptMainMenu"))
                I_strncpyz(s_guestMenu.mainMenu, Cmd_Argv(i + 1), sizeof(s_guestMenu.mainMenu));
        Cmd_EndTokenizedString();
        return;
    }

    if ((text[0] == 'u' || text[0] == 'L') && (!text[1] || text[1] == ' ' || text[1] == '\t'))
    {
        s_guestMenu.pending = -1;
        if (s_guestMenu.owner)
        {
            if (text[0] == 'u') UI_ClosePopupScriptMenu(0, false);
            else UI_CloseInGameMenu(0);
        }
        return;
    }
    if (text[0] != 't' || (text[1] != ' ' && text[1] != '\t'))
        return;
    char *end = nullptr;
    const long index = std::strtol(text + 2, &end, 10);
    if (end == text + 2 || index < 0 || index >= 32
        || (*end && *end != ' ' && *end != '\t') || !clients[1].serverId)
        return;
    const char *menu = CL_GetConfigString(2548 + static_cast<int>(index));
    if (I_stricmp(menu, "team_marinesopfor") && I_stricmp(menu, "class")
        && I_stricmp(menu, "class_marines") && I_stricmp(menu, "class_opfor")
        && I_stricmp(menu, "changeclass") && I_stricmp(menu, "changeclass_offline")
        && I_stricmp(menu, "changeclass_barebones") && I_stricmp(menu, "changeclass_custom"))
        return;
    s_guestMenu.pending = static_cast<int>(index);
    s_guestMenu.serverId = clients[1].serverId;
    CL_SplitGuestLog("P2 team/class menu queued; waiting for shared UI");
}

static void SplitGuestMenuKey(int key)
{
    s_guestMenu.dispatching = true;
    UI_KeyEvent(0, key, 1);
    UI_KeyEvent(0, key, 0);
    s_guestMenu.dispatching = false;
}

void CL_SplitGuestMenuFrame()
{
    SplitGuestMenuState &state = s_guestMenu;
    if (!CL_IsSplitGuest(1) || !clients || CL_GetLocalClientConnectionState(1) != CA_ACTIVE
        || CL_GetLocalClientConnectionState(0) != CA_ACTIVE)
    {
        if (state.owner) CL_SplitGuestResetMenu();
        return;
    }
    if (state.serverId && state.serverId != clients[1].serverId)
    {
        CL_SplitGuestResetMenu();
        return;
    }
    const int port = Dvar_GetInt("splitguest_controller");
    if (static_cast<unsigned int>(port) >= GPAD_PORT_COUNT || !GPad_IsActive(port)
        || !Dvar_GetBool("gpad_enabled") || !IN_IsForegroundWindow())
    {
        if (state.owner) CL_SplitGuestResetMenu();
        state.previous = state.suppressed = ~0u;
        return;
    }
    const GamePadButton buttons[] = {GPAD_A, GPAD_B, GPAD_START,
        GPAD_UP, GPAD_DOWN, GPAD_LEFT, GPAD_RIGHT};
    unsigned int held = 0;
    for (unsigned int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
        if (GPad_GetButton(port, buttons[i]) > 0.15) held |= 1u << i;
    if (state.port != port)
    {
        state.port = port;
        state.previous = state.suppressed = held;
    }
    state.suppressed &= held;
    held &= ~state.suppressed;
    const unsigned int pressed = held & ~state.previous;
    state.previous = held;
    if (state.owner)
    {
        state.blockedFrame = cls.realtime;
        if (!Key_IsCatcherActive(0, 16) || !Menu_GetFocused(&UI_GetInfo(0)->uiDC))
        {
            state.owner = false;
            state.previous = state.suppressed = held;
            state.direction = 0;
        }
    }
    if (!state.owner)
    {
        // A queued P2 class prompt waits until P1 finishes its own menu.
        if (Key_IsCatcherActive(0, -1) || Menu_GetFocused(&UI_GetInfo(0)->uiDC)) return;
        if (state.pending < 0 && !(pressed & 4u)) return;
        state.owner = true;
        state.serverId = clients[1].serverId;
        state.blockedFrame = cls.realtime;
        if (state.pending >= 0)
        {
            const char *name = CL_GetConfigString(2548 + state.pending);
            if (!Menus_FindByName(&UI_GetInfo(0)->uiDC, name) || !UI_PopupScriptMenu(0, name, false))
            {
                state.owner = false;
                return;
            }
            state.pending = -1;
        }
        else UI_SetActiveMenu(0, UIMENU_INGAME);
        state.previous = state.suppressed = held;
        CL_SplitGuestLog("P2 owns shared native menu; A select, B back, Start back");
        return;
    }
    if (state.pending >= 0)
    {
        const char *name = CL_GetConfigString(2548 + state.pending);
        if (Menus_FindByName(&UI_GetInfo(0)->uiDC, name) && UI_PopupScriptMenu(0, name, false))
            state.pending = -1;
    }
    // Start always means Back, independent of Nintendo's A/B menu swap.
    if (pressed & 4u) { SplitGuestMenuKey(K_ESCAPE); return; }
    if (pressed & 2u) SplitGuestMenuKey(K_BUTTON_B);
    else if (pressed & 1u) SplitGuestMenuKey(K_BUTTON_A);
    int direction = (held & (1u << 3)) ? K_UPARROW : (held & (1u << 4)) ? K_DOWNARROW
        : (held & (1u << 5)) ? K_LEFTARROW : (held & (1u << 6)) ? K_RIGHTARROW : 0;
    if (!direction)
    {
        const float x = static_cast<float>(GPad_GetStick(port, GPAD_LX));
        const float y = static_cast<float>(GPad_GetStick(port, GPAD_LY));
        direction = y > 0.55f ? K_UPARROW : y < -0.55f ? K_DOWNARROW
            : x < -0.55f ? K_LEFTARROW : x > 0.55f ? K_RIGHTARROW : 0;
    }
    if (direction && (direction != state.direction || cls.realtime >= state.repeatTime))
    {
        SplitGuestMenuKey(direction);
        state.repeatTime = cls.realtime + (direction == state.direction ? 140 : 350);
    }
    state.direction = direction;
}
#else
const char *CL_SplitGuestScriptMainMenu() { return ""; }
bool CL_SplitGuestDrawingMenu() { return false; }
void CL_SplitGuestDrawMenu() {}
bool CL_SplitGuestOwnsMenu() { return false; }
bool CL_SplitGuestMenuAcceptsUiInput() { return true; }
bool CL_SplitGuestMenuBlocksInput() { return false; }
void CL_SplitGuestResetMenu() {}
bool CL_SplitGuestMenuResponse(const char *, const char *) { return false; }
void CL_SplitGuestHandleMenuCommand(const char *) {}
void CL_SplitGuestMenuFrame() {}
#endif
