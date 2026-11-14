// The Switch updater's popup (nx/nx_update.h): both executables check for a
// release at boot and, when one with another version is out, offer it over
// the main menu. Until it is installed, SERVER BROWSER and HOST MATCH open this
// popup instead (Directory_UpdateBlocksOnline): online play needs the current
// version. The menu is built in code from a stock popup present in SP and MP.
#include "directory_engine.h"
#if defined(OPENBLOPS_NO_STEAM_AUTH) && defined(KISAK_NX)
#include <nx/nx_update.h>
#include <client/client.h>
#include <qcommon/cmd.h>
#include <qcommon/common.h>
#include <win32/win_shared.h>
#include <ui/ui_main.h>
#include <ui/ui_shared.h>
#include <ui/ui_utils.h>
#include <ui/keycodes.h>
#include <ui_mp/ui_main_mp.h>
#include <universal/dvar.h>
#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <vector>


namespace
{
constexpr const char *menuName = "openblops_update";

bool Named(const char *a, const char *b) { return a && b && !I_stricmp(a, b); }

struct Item
{
    itemDef_s item{};
    textDef_s text{};
    focusItemDef_s focus{};
    GenericEventHandler events[3]{};
    GenericEventScript scripts[3]{};
};
struct Menu
{
    menuDef_t menu{};
    std::list<Item> owned;
    std::vector<itemDef_s *> items;
    GenericEventHandler events[3]{};
    GenericEventScript open{};
};
Menu *s_menu;
bool s_built, s_checkStarted, s_shown, s_dismissed, s_gate, s_quitQueued;
int s_uiSince, s_installedAt;
// After an install the game closes by itself (the new NROs load on the next start).
constexpr int UPDATE_CLOSE_MS = 3000;

// What the popup shows; the items point at these and are refreshed each frame.
char s_title[64], s_versionLine[160], s_releaseLine[160], s_status[200];
char s_notes[5][96];
char s_primary[32], s_secondary[32];
const dvar_s *s_checkDvar;

void Ungate(itemDef_s &item)
{
    item.dvarTest = item.enableDvar = nullptr;
    item.dvarFlags = 0;
    item.visibleExp = {}; item.forecolorAExp = {};
    item.showBits = 0; item.hideBits = ~0ULL;
    item.rectExpData = nullptr; item.animInfo = nullptr;
    item.window.dynamicFlags[0] = 4;
    item.window.staticFlags &= ~0x100000;
}
void Event(Item &part, int index, const char *name, const char *action)
{
    part.events[index].name = name;
    part.events[index].eventScript = &part.scripts[index];
    part.scripts[index].action = action;
    if (index) part.events[index - 1].next = &part.events[index];
    part.item.onEvent = part.events;
}
Item &Control(Menu &menu, const itemDef_s *source, const char *name, const char *label,
    float x, float y, float width, float height, int type, float scale)
{
    menu.owned.emplace_back();
    auto &part = menu.owned.back();
    part.item = *source;
    auto &item = part.item;
    item.parent = &menu.menu;
    Ungate(item);
    item.window.name = name; item.window.group = nullptr;
    item.window.rect = item.window.rectClient = {x, y, width, height, 1, 1};
    item.window.style = item.window.border = item.window.ownerDraw = 0;
    item.window.background = nullptr;
    item.ui3dWindowId = -1; item.type = item.dataType = type;
    item.dvar = nullptr; item.onEvent = nullptr;
    part.text = *source->typeData.textDef;
    item.typeData.textDef = &part.text;
    part.text.textRect[0] = {};
    part.text.text = label; part.text.textExpData = nullptr;
    part.text.textTypeData.focusItemDef = &part.focus;
    part.text.alignment = 0; part.text.textAlignMode = 0;
    part.text.textalignx = 0; part.text.textaligny = 18;
    part.text.textscale = scale;
    for (int c = 0; c < 4; ++c) item.window.foreColor[c] = 1;
    if (type != 1)
    {
        Event(part, 0, "action", "play uin_navigation_click ;");
        Event(part, 1, "onFocus", "play uin_navigation_over ; setcolor forecolor .953 .722 .212 1 ;");
        Event(part, 2, "leaveFocus", "setcolor forecolor 1 1 1 1 ;");
    }
    menu.items.push_back(&item);
    return part;
}

const itemDef_s *TextSource(UiContext *dc)
{
    for (const char *name : {"error_popmenu", "main", "main_text", "pausedmenu"})
        if (const menuDef_t *menu = Menus_FindByName(dc, name))
            for (int i = 0; i < menu->itemCount; ++i)
            {
                const itemDef_s *item = menu->items[i];
                if ((item->type == 1 || item->type == 3) && item->typeData.textDef)
                    return item;
            }
    return nullptr;
}

bool Build(UiContext *dc)
{
    if (s_built) return true;
    const menuDef_t *source = Menus_FindByName(dc, "error_popmenu");
    const itemDef_s *text = TextSource(dc);
    if (!source || !text || dc->menuCount + 1 > int(sizeof(dc->Menus) / sizeof(dc->Menus[0])))
        return false;
    s_menu = new Menu();
    Menu &m = *s_menu;
    m.menu = *source;
    m.menu.window.name = menuName;
    m.menu.window.rect = m.menu.window.rectClient = {0, 0, 640, 480, 4, 4};
    m.menu.initialRectInfo = m.menu.window.rect;
    m.menu.window.border = 0; m.menu.window.style = 0; m.menu.window.dynamicFlags[0] = 0;
    m.menu.ui3dWindowId = -1; m.menu.fullScreen = 0;
    m.menu.cursorItem[0] = -1; m.menu.onKey = nullptr;
    m.menu.visibleExp = {}; m.menu.rectXExp = {}; m.menu.rectYExp = {};
    m.menu.showBits = 0; m.menu.hideBits = ~0ULL;
    m.menu.openFadingTime = m.menu.closeFadingTime = 0;
    m.menu.openSlideDirection = m.menu.closeSlideDirection = 0;
    m.menu.fadeTimeCounter = m.menu.slideTimeCounter = -1;
    m.open.action = "setFocus update_primary ;";
    const char *events[] = {"onOpen", "onESC", "onClose"};
    for (int i = 0; i < 3; ++i)
    {
        m.events[i].name = events[i];
        if (i < 2) m.events[i].next = &m.events[i + 1];
    }
    m.events[0].eventScript = &m.open;
    m.menu.onEvent = m.events;
    // A dimmed full-screen backing over the menu underneath.
    auto &background = m.owned.emplace_back().item;
    background.parent = &m.menu;
    background.window.name = "update_background";
    background.window.rect = background.window.rectClient = {0, 0, 640, 480, 4, 4};
    background.window.style = 1;
    background.window.staticFlags = 0x100000;
    background.window.dynamicFlags[0] = 4;
    background.window.backColor[0] = background.window.backColor[1] = background.window.backColor[2] = 0.02f;
    background.window.backColor[3] = 0.9f;
    background.hideBits = ~0ULL;
    background.ui3dWindowId = -1;
    m.items.push_back(&background);
    Control(m, text, "update_title", s_title, 64, 70, 512, 32, 1, .45f).text.textaligny = 28;
    Control(m, text, "update_version", s_versionLine, 64, 118, 512, 24, 1, .3f);
    Control(m, text, "update_release", s_releaseLine, 64, 146, 512, 24, 1, .3f);
    for (int i = 0; i < 5; ++i)
        Control(m, text, "update_note", s_notes[i], 64, 182 + 22.0f * i, 512, 22, 1, .25f);
    Control(m, text, "update_status", s_status, 64, 304, 512, 24, 1, .3f);
    Control(m, text, "update_primary", s_primary, 64, 380, 220, 28, 3, .3f);
    Control(m, text, "update_secondary", s_secondary, 356, 380, 220, 28, 3, .3f);
    m.menu.items = m.items.data();
    m.menu.itemCount = int(m.items.size());
    dc->Menus[dc->menuCount++] = &m.menu;
    s_built = true;
    return true;
}

// The release notes as a few short lines: Markdown marks dropped, wrapped.
void FillNotes(const char *notes)
{
    for (auto &line : s_notes) line[0] = 0;
    int row = 0;
    std::string text(notes);
    size_t at = 0;
    while (row < 5 && at < text.size())
    {
        size_t end = text.find('\n', at);
        std::string line = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
        at = end == std::string::npos ? text.size() : end + 1;
        size_t start = line.find_first_not_of(" #*->\t");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        std::string clean;
        for (char c : line)
            if ((unsigned char)c >= 0x20 && (unsigned char)c < 0x7f && c != '*' && c != '`' && c != '^') clean += c;
        while (!clean.empty() && row < 5)
        {
            size_t cut = clean.size() <= 80 ? clean.size() : clean.rfind(' ', 80);
            if (cut == std::string::npos || cut == 0) cut = clean.size() < 80 ? clean.size() : 80;
            std::snprintf(s_notes[row++], sizeof(s_notes[0]), "%s", clean.substr(0, cut).c_str());
            clean = cut < clean.size() ? clean.substr(cut + 1) : std::string();
        }
    }
}

void Refresh()
{
    NxUpdateInfo info;
    NX_UpdateGetInfo(info);
    std::snprintf(s_versionLine, sizeof(s_versionLine), "OpenBLOPS-NX %s is available. This copy is version %s.",
                  info.version[0] ? info.version : "?", info.current);
    std::snprintf(s_releaseLine, sizeof(s_releaseLine), "%s", info.title);
    FillNotes(info.notes);
    const double mb = 1.0 / (1024.0 * 1024.0);
    s_secondary[0] = 0;
    switch (info.phase)
    {
    // Button prompts: glyph 2 draws as the Switch's A (confirm) and glyph 1 as
    // its B (back), as nx_switchglyphs labels them.
    case NxUpdatePhase::Downloading:
        std::snprintf(s_title, sizeof(s_title), "DOWNLOADING UPDATE");
        if (info.total)
            std::snprintf(s_status, sizeof(s_status), "Downloading: %.1f of %.1f MB", info.done * mb, info.total * mb);
        else
            std::snprintf(s_status, sizeof(s_status), "Downloading: %.1f MB", info.done * mb);
        std::snprintf(s_primary, sizeof(s_primary), "\x02 CANCEL");
        break;
    case NxUpdatePhase::Extracting:
        std::snprintf(s_title, sizeof(s_title), "INSTALLING UPDATE");
        std::snprintf(s_status, sizeof(s_status), "Installing: %.0f%%  (do not turn off the console)",
                      info.total ? 100.0 * (double)info.done / (double)info.total : 0.0);
        s_primary[0] = 0;
        break;
    case NxUpdatePhase::Installed:
    {
        std::snprintf(s_title, sizeof(s_title), "UPDATE INSTALLED");
        const int left = s_installedAt ? (UPDATE_CLOSE_MS - (Sys_Milliseconds() - s_installedAt) + 999) / 1000 : 3;
        std::snprintf(s_status, sizeof(s_status), "Version %s is installed. The game closes in %d s; start it again to play.",
                      info.version, left > 0 ? left : 0);
        s_primary[0] = 0;
        break;
    }
    case NxUpdatePhase::Failed:
        std::snprintf(s_title, sizeof(s_title), "UPDATE FAILED");
        std::snprintf(s_status, sizeof(s_status), "%s", info.error);
        std::snprintf(s_primary, sizeof(s_primary), "\x02 RETRY");
        std::snprintf(s_secondary, sizeof(s_secondary), s_gate ? "\x01 BACK" : "\x01 LATER");
        break;
    default:
        std::snprintf(s_title, sizeof(s_title), s_gate ? "UPDATE REQUIRED" : "UPDATE AVAILABLE");
        std::snprintf(s_status, sizeof(s_status), "%s", s_gate
            ? "Online play needs the current version. Update now?"
            : "Update now? The game closes when it is done.");
        std::snprintf(s_primary, sizeof(s_primary), "\x02 UPDATE");
        std::snprintf(s_secondary, sizeof(s_secondary), s_gate ? "\x01 BACK" : "\x01 LATER");
        break;
    }
}

// An update is known and not yet running: online play waits for it.
bool Outdated()
{
    NxUpdateInfo info;
    NX_UpdateGetInfo(info);
    switch (info.phase)
    {
    case NxUpdatePhase::Available:
    case NxUpdatePhase::Downloading:
    case NxUpdatePhase::Extracting:
    case NxUpdatePhase::Installed:
    case NxUpdatePhase::Failed:
        return info.version[0] != 0;
    default:
        return false;
    }
}

bool Busy()
{
    NxUpdateInfo info;
    NX_UpdateGetInfo(info);
    return info.phase == NxUpdatePhase::Downloading || info.phase == NxUpdatePhase::Extracting;
}

void Close(UiContext *dc)
{
    if (Busy()) return;   // the download keeps the popup up; CANCEL first
    Menus_CloseByName(0, dc, menuName);
    s_dismissed = true;
    s_gate = false;
}

bool AtMainMenu()
{
    if (!cls.uiStarted || !com_sv_running) return false;
    if (com_sv_running->current.enabled && !Com_IsMenuLevel(nullptr)) return false;   // in a match
    return CL_GetLocalClientConnectionState(0) <= CA_DISCONNECTED || Com_IsMenuLevel(nullptr);
}
} // namespace

void Directory_UpdateFrame()
{
    if (!s_checkDvar)
        s_checkDvar = _Dvar_RegisterBool("nx_update_check", true, 1, "Check GitHub for a new OpenBLOPS-NX release at start");
    NxUpdateInfo info;
    NX_UpdateGetInfo(info);
    // Normally running already: main() starts the check at launch, so it is
    // done by the time the menus are up. This covers a launch that skipped it.
    if (!s_checkStarted && info.phase == NxUpdatePhase::Idle && s_checkDvar->current.enabled)
    {
        s_checkStarted = true;
        NX_UpdateStartCheck();
    }
    if (!cls.uiStarted) return;
    const int now = Sys_Milliseconds();
    if (!s_uiSince) s_uiSince = now;
    auto *ui = UI_GetInfo(0);
    if (!ui) return;
    UiContext *dc = &ui->uiDC;
    // Installed: close the game a few seconds later.
    if (info.phase == NxUpdatePhase::Installed && !s_quitQueued)
    {
        if (!s_installedAt) s_installedAt = now ? now : 1;
        if (now - s_installedAt >= UPDATE_CLOSE_MS)
        {
            s_quitQueued = true;
            Com_Printf(14, "Update installed: closing the game\n");
            Cbuf_AddText(0, "quit\n");
        }
    }
    // Offer it once, over the main menu (not over a loading screen or a match).
    if (!s_shown && info.phase == NxUpdatePhase::Available && AtMainMenu() && now - s_uiSince > 500 && Build(dc))
    {
        s_shown = true;
        Refresh();
        Menus_OpenByName(0, dc, menuName);
    }
    if (s_built && Menus_FindByName(dc, menuName) && Menu_IsVisible(0, dc, &s_menu->menu))
        Refresh();
}

bool Directory_UpdateBlocksOnline()
{
    return Outdated();
}

// The browser or a host menu was asked for while out of date: the popup instead.
const char *Directory_UpdateGateMenu(const char *name)
{
    if (!name || !Outdated()) return name;
    const bool online = Named(name, "server_browser_unranked") || Named(name, "openblops_zombie_host")
        || Named(name, "server_browser_ranked") || Named(name, "server_browser_wager")
        || Named(name, "server_browser_friends") || Named(name, "server_browser_favorites")
        || Named(name, "server_browser_history");
    if (!online) return name;
    auto *ui = UI_GetInfo(0);
    if (!ui || !Build(&ui->uiDC)) return name;
    s_gate = true;
    Refresh();
    Com_Printf(14, "Update required: %s opens the update popup instead\n", name);
    return menuName;
}

bool Directory_UpdateOpenGate()
{
    if (!Outdated()) return false;
    auto *ui = UI_GetInfo(0);
    if (!ui || !Build(&ui->uiDC)) return false;
    s_gate = true;
    Refresh();
    Menus_OpenByName(0, &ui->uiDC, menuName);
    return true;
}

bool Directory_UpdateMenuEvent(int localClientNum, UiContext *dc, itemDef_s *item, const char *event)
{
    if (localClientNum || !item || !item->parent || !Named(item->parent->window.name, menuName)) return false;
    if (Named(event, "onESC")) { Close(dc); return true; }
    if (!Named(event, "action")) return false;
    NxUpdateInfo info;
    NX_UpdateGetInfo(info);
    const char *name = item->window.name;
    if (Named(name, "update_primary"))
    {
        switch (info.phase)
        {
        case NxUpdatePhase::Available:
        case NxUpdatePhase::Failed:
            NX_UpdateStartInstall();
            break;
        case NxUpdatePhase::Downloading:
            NX_UpdateCancel();
            break;
        default:   // Installed: Directory_UpdateFrame closes the game itself
            break;
        }
        return true;
    }
    if (Named(name, "update_secondary"))
    {
        if (s_secondary[0]) Close(dc);
        return true;
    }
    return true;
}

bool Directory_UpdateControllerKey(int localClientNum, UiContext *dc, menuDef_t *menu, int key)
{
    if (localClientNum || !menu || !Named(menu->window.name, menuName)) return false;
    if (key == K_BUTTON_A)
    {
        const int cursor = Menu_GetCursorItem(dc->contextIndex, menu);
        if (cursor >= 0 && cursor < menu->itemCount)
            Directory_UpdateMenuEvent(localClientNum, dc, menu->items[cursor], "action");
        return true;
    }
    const bool back = key == K_LEFTARROW || key == K_DPAD_LEFT || key == K_APAD_LEFT
        || key == K_UPARROW || key == K_DPAD_UP || key == K_APAD_UP;
    const bool next = key == K_RIGHTARROW || key == K_DPAD_RIGHT || key == K_APAD_RIGHT
        || key == K_DOWNARROW || key == K_DPAD_DOWN || key == K_APAD_DOWN;
    if (back) { Menu_SetPrevCursorItem(localClientNum, dc, menu, 1); return true; }
    if (next) { Menu_SetNextCursorItem(localClientNum, dc, menu, 0, 1); return true; }
    return false;
}
#endif
