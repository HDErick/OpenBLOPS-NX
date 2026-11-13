#include "directory_engine.h"
#if defined(OPENBLOPS_NO_STEAM_AUTH) && defined(KISAK_SP)
#include "directory_protocol.h"
#include <client/splitscreen.h>
#include <client_mp/cl_main_mp.h>
#include <gfx_d3d/r_material.h>
#include <qcommon/cmd.h>
#include <qcommon/common.h>
#include <server_mp/sv_init_mp.h>
#include <ui/ui_main.h>
#include <ui/ui_server.h>
#include <ui/ui_shared.h>
#include <ui/ui_utils.h>
#include <ui_mp/ui_main_mp.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <vector>

namespace
{
constexpr const char *browserName = "server_browser_unranked";
constexpr const char *hostName = "openblops_zombie_host";
constexpr const char *passwordName = "join_password_popmenu";
bool choosingMap = false, configuringHost = false, startingHost = false;
std::string selectedMap = "zombie_theater", runningHostMap;
char mapCaption[128] = "", hostNotice[192] = "";
char browserStatus[256] = "", browserCounts[96] = "";

bool Named(const char *a, const char *b) { return a && b && !I_stricmp(a, b); }
bool Label(const itemDef_s *item, const char *key)
{
    if (!Item_IsTextDefType(item) || !item->typeData.textDef) return false;
    const auto *text = item->typeData.textDef;
    if (Named(text->text, key)) return true;
    if (!text->textExpData) return false;
    const auto &exp = text->textExpData->textExp;
    for (int i = 0; exp.rpn && i < exp.numRpn; ++i)
        if (exp.rpn[i].type == 0 && exp.rpn[i].data.constant.dataType == VAL_STRING
            && Named(exp.rpn[i].data.constant.internals.string, key)) return true;
    return false;
}

struct Item
{
    itemDef_s item{};
    textDef_s text{};
    focusItemDef_s focus{};
    listBoxDef_s list{};
    editFieldDef_s edit{};
    multiDef_s multi{};
    GenericEventHandler events[4]{};
    GenericEventScript scripts[4]{};
};
struct Menu
{
    menuDef_t menu{};
    std::list<Item> owned;
    std::vector<itemDef_s *> items;
    GenericEventHandler events[3]{};
    GenericEventScript open{};
};
// Own all mutable windows, text caches, edit cursors and list selection. Only
// immutable SP fonts/materials are borrowed; never load ui_mp over the SP zones.
struct MenuSet { Menu browser, host, password; };
std::list<MenuSet> sets;
struct PatchedMenu { menuDef_t *menu; std::list<Item> owned; std::vector<itemDef_s *> items; };
std::list<PatchedMenu> patched;

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
Item &Copy(std::list<Item> &storage, const itemDef_s *source, menuDef_t *parent)
{
    storage.emplace_back(); auto &part = storage.back();
    part.item = *source;
    part.item.parent = parent;
    part.item.animInfo = nullptr; part.item.rectExpData = nullptr;
    part.item.window.dynamicFlags[0] = 4;
    if (Item_IsTextDefType(source) && source->typeData.textDef)
    {
        part.text = *source->typeData.textDef;
        part.item.typeData.textDef = &part.text;
        part.text.textRect[0] = {};
        if (source->dataType != 1 && part.text.textTypeData.focusItemDef)
        {
            part.focus = *part.text.textTypeData.focusItemDef;
            part.text.textTypeData.focusItemDef = &part.focus;
        }
    }
    return part;
}
Item &Control(Menu &menu, const itemDef_s *source, const char *name, const char *label,
    float x, float y, float width, float height, int type = 3)
{
    auto &part = Copy(menu.owned, source, &menu.menu);
    auto &item = part.item;
    Ungate(item);
    item.window.name = name; item.window.group = nullptr;
    item.window.rect = item.window.rectClient = {x, y, width, height, 1, 1};
    item.window.style = item.window.border = item.window.ownerDraw = 0;
    item.window.background = nullptr;
    item.ui3dWindowId = -1; item.type = item.dataType = type;
    item.dvar = nullptr; item.onEvent = nullptr;
    item.typeData.textDef = &part.text;
    part.text.text = label; part.text.textExpData = nullptr;
    part.text.textTypeData.focusItemDef = &part.focus;
    part.text.alignment = 0; part.text.textAlignMode = 0;
    part.text.textalignx = 0; part.text.textaligny = 18;
    part.text.textscale = 0.3f;
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
void Field(Menu &menu, const itemDef_s *source, const char *name, const char *label,
    const char *dvar, float y, int maxChars)
{
    Control(menu, source, "field_label", label, 48, y, 184, 28, 1);
    auto &part = Control(menu, source, name, nullptr, 242, y, 350, 28, 5);
    part.item.dvar = dvar;
    part.item.window.border = 1; part.item.window.borderSize = 1;
    for (int i = 0; i < 4; ++i) part.item.window.borderColor[i] = .5f;
    part.focus.focusTypeData.editField = &part.edit;
    part.edit.maxChars = maxChars; part.edit.maxPaintChars = 38;
    part.text.textalignx = 4;
}
void Panel(Menu &menu, const menuDef_t *source, const char *name, const char *caption)
{
    menu.menu = *source;
    menu.menu.window.name = name;
    menu.menu.window.rect = menu.menu.window.rectClient = {0, 0, 640, 480, 4, 4};
    menu.menu.initialRectInfo = menu.menu.window.rect;
    menu.menu.window.border = 0;
    menu.menu.window.style = 0;
    menu.menu.window.dynamicFlags[0] = 0;
    menu.menu.ui3dWindowId = -1; menu.menu.fullScreen = 1;
    menu.menu.cursorItem[0] = -1; menu.menu.onKey = nullptr;
    menu.menu.visibleExp = {}; menu.menu.rectXExp = {}; menu.menu.rectYExp = {};
    menu.menu.showBits = 0; menu.menu.hideBits = ~0ULL;
    menu.menu.openFadingTime = menu.menu.closeFadingTime = 0;
    menu.menu.openSlideDirection = menu.menu.closeSlideDirection = 0;
    menu.menu.fadeTimeCounter = menu.menu.slideTimeCounter = -1;
    menu.open.action = "focusFirst ;";
    const char *events[] = {"onOpen", "onESC", "onClose"};
    for (int i = 0; i < 3; ++i)
    {
        menu.events[i].name = events[i];
        if (i < 2) menu.events[i].next = &menu.events[i + 1];
    }
    menu.events[0].eventScript = &menu.open;
    menu.menu.onEvent = menu.events;
    // Fill the viewport with the co-op background. The mods menu's other
    // layers were sized around its own header, footer and 4:3 center panel.
    auto &background = menu.owned.emplace_back().item;
    background.parent = &menu.menu;
    background.window.name = "directory_background";
    background.window.rect = background.window.rectClient = {0, 0, 640, 480, 4, 4};
    background.window.style = 1;
    background.window.staticFlags = 0x100000;
    background.window.dynamicFlags[0] = 4;
    background.window.backColor[3] = 1.0f;
    background.hideBits = ~0ULL;
    background.ui3dWindowId = -1;
    for (int i = 0; i < source->itemCount; ++i)
    {
        const auto *item = source->items[i];
        if (item->type == 0 && item->window.background
            && Named(Material_GetName(item->window.background), "menu_background_coop_blurred"))
        {
            background.window.background = item->window.background;
            background.window.backColor[0] = background.window.backColor[1] = background.window.backColor[2] = 1.0f;
            break;
        }
    }
    menu.items.push_back(&background);
    const itemDef_s *text = nullptr;
    for (int i = 0; i < source->itemCount; ++i)
        if (source->items[i]->type == 3 && source->items[i]->typeData.textDef)
            text = source->items[i];
    auto &title = Control(menu, text, "directory_title", caption, 32, 24, 576, 32, 1);
    title.text.textscale = .45f; title.text.textaligny = 28;
}
void Finish(Menu &menu)
{
    menu.menu.items = menu.items.data();
    menu.menu.itemCount = int(menu.items.size());
}
bool ValidMap(const char *map)
{
    return map && bo1dir::Token(map, 31) && !std::strncmp(map, "zombie_", 7);
}
void ResetLocalSplit()
{
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
    CL_SplitScreenResetSession();
#endif
}
void CancelHostSelection()
{
    Dvar_SetBoolByName("bo1_directory_publish", false);
    configuringHost = choosingMap = startingHost = false;
    runningHostMap.clear();
}
void OpenHost(UiContext *dc)
{
    CancelHostSelection();
    ResetLocalSplit();
    configuringHost = true; hostNotice[0] = 0;
    if (ValidMap(Dvar_GetString("ui_mapname"))) selectedMap = Dvar_GetString("ui_mapname");
    std::snprintf(mapCaption, sizeof(mapCaption), "MAP: %s", UI_GetMapDisplayName(selectedMap.c_str()));
    Menus_OpenByName(0, dc, hostName);
}
void Back(UiContext *dc, const char *name)
{
    if (Named(name, browserName)) { Directory_CancelJoin(); Directory_CancelRefresh(); }
    if (Named(name, hostName)) { configuringHost = choosingMap = false; }
    Menus_CloseByName(0, dc, name);
}

void PatchEntry(menuDef_t *menu)
{
    const bool root = Named(menu->window.name, "main_online");
    if (!root && !Named(menu->window.name, "menu_xboxlive")) return;
    for (auto &copy : patched)
        if (copy.menu == menu && menu->items == copy.items.data()) return;
    patched.push_back({menu, {}, {}}); auto &copy = patched.back();
    const itemDef_s *enabledStyle[2][2]{};
    for (int i = 0; i < menu->itemCount; ++i)
    {
        auto *source = menu->items[i];
        const bool online = root && Named(source->window.name, "online_button1");
        const bool browser = !root && Label(source, "@MPUI_FIND_MATCH_CAPS");
        const bool host = !root && Label(source, "@MPUI_CUSTOM_MATCH_CAPS");
        if (!online && !browser && !host) { copy.items.push_back(source); continue; }
        if (!root)
        {
            // Keep both stock visibility variants and the separate focus text.
            // Directory actions work in either variant, so use the enabled
            // colors for both. The stock focus scripts show the row backing.
            const bool button = source->type == 3;
            auto *&style = enabledStyle[host][button];
            if (!style) style = source;
            auto &part = Copy(copy.owned, source, menu);
            std::memcpy(part.item.window.foreColor, style->window.foreColor, sizeof(part.item.window.foreColor));
            if (button)
                part.item.window.name = browser ? "directory_zombie_browser" : "directory_zombie_host";
            part.text.text = browser ? "SERVER BROWSER" : "HOST MATCH";
            part.text.textExpData = nullptr;
            copy.items.push_back(&part.item);
            continue;
        }
        auto &part = Copy(copy.owned, source, menu);
        part.text.text = "ONLINE";
        part.text.textExpData = nullptr;
        copy.items.push_back(&part.item);
    }
    menu->items = copy.items.data(); menu->itemCount = int(copy.items.size());
}
} // namespace

bool Directory_ZombiesPrepareMenus(UiContext *dc)
{
    if (!dc) return false;
    if (Menus_FindByName(dc, browserName) && Menus_FindByName(dc, hostName)
        && Menus_FindByName(dc, passwordName)) return true;
    auto *source = Menus_FindByName(dc, "mods_menu");
    if (!source || dc->menuCount + 3 > int(sizeof(dc->Menus) / sizeof(dc->Menus[0]))) return false;
    const itemDef_s *text = nullptr, *list = nullptr;
    for (int i = 0; i < source->itemCount; ++i)
    {
        auto *item = source->items[i];
        if (item->type == 3 && item->typeData.textDef) text = item;
        if (item->type == 4 && item->typeData.textDef) list = item;
    }
    if (!text || !list) return false;
    _Dvar_RegisterString("g_password", "", 0, "Server password");
    _Dvar_RegisterString("password", "", 2, "Password for joining a server");
    _Dvar_RegisterInt("bo1_zombie_maxplayers", 4, 1, 4, 1, "Maximum Zombies co-op players");
    sets.emplace_back(); auto &set = sets.back();

    Panel(set.browser, source, browserName, "ZOMBIES SERVER BROWSER");
    Control(set.browser, text, "directory_counts", browserCounts, 32, 65, 576, 24, 1);
    // Native feeder 2 supplies sorting, selection and the validated directory
    // rows, exactly as in MP. SP uses text for password/type columns.
    Control(set.browser, text, "header_password", "PASSWORD", 32, 96, 76, 24, 1);
    auto &serverHeader = Control(set.browser, text, "header_server", "SERVER", 114, 96, 220, 24);
    serverHeader.scripts[0].action = "uiScript ServerSort 3 ;";
    auto &mapHeader = Control(set.browser, text, "header_map", "MAP", 338, 96, 172, 24);
    mapHeader.scripts[0].action = "uiScript ServerSort 4 ;";
    auto &playersHeader = Control(set.browser, text, "header_players", "PLAYERS", 514, 96, 90, 24);
    playersHeader.scripts[0].action = "uiScript ServerSort 5 ;";
    auto &servers = Control(set.browser, list, "browser", nullptr, 32, 124, 576, 242, 4);
    servers.list = *list->typeData.textDef->textTypeData.focusItemDef->focusTypeData.listBox;
    servers.focus.focusTypeData.listBox = &servers.list;
    servers.list.special = 2; servers.list.numColumns = 6;
    servers.list.elementHeight = 22; servers.list.elementWidth = 576;
    servers.list.cursorPos[0] = servers.list.startPos[0] = servers.list.endPos[0] = 0;
    servers.list.rows = nullptr; servers.list.rowCount = servers.list.maxRows = 0;
    servers.list.notselectable = 0; servers.list.noScrollBars = 0;
    const float x[] = {0, 0, 0, 82, 306, 482}, width[] = {0, 0, 78, 220, 172, 90};
    for (int i = 0; i < 6; ++i)
    {
        servers.list.columnInfo[i] = {};
        servers.list.columnInfo[i].maxChars = i == 3 ? 48 : 32;
        servers.list.columnInfo[i].rect = {x[i], 0, width[i], 22, 0, 0};
    }
    Event(servers, 3, "doubleClick", "uiScript JoinServer ;");
    Control(set.browser, text, "directory_status", browserStatus, 32, 374, 576, 24, 1);
    Control(set.browser, text, "directory_refresh", "REFRESH", 32, 434, 150, 28);
    Control(set.browser, text, "directory_join", "JOIN SERVER", 224, 434, 170, 28);
    Control(set.browser, text, "directory_back", "BACK", 500, 434, 108, 28);
    set.browser.open.action = "setDvar ui_netSource 0 ; setDvar ui_serverBrowserMenu server_browser_unranked ; "
        "setDvar ui_browserGameMode 0 ; setDvar ui_browserMap 0 ; setDvar ui_joinGametype 0 ; "
        "setDvar ui_browserShowEmpty 1 ; setDvar ui_browserShowFull 1 ; setDvar ui_browserShowPassword -1 ; "
        "uiScript PopulateServers ; setFocus browser ;";
    Finish(set.browser);

    Panel(set.host, source, hostName, "HOST MATCH - ZOMBIES");
    Control(set.host, text, "directory_map", mapCaption, 48, 92, 544, 28, 1);
    Control(set.host, text, "directory_change_map", "CHANGE MAP", 48, 130, 544, 28);
    Field(set.host, text, "directory_server_name", "SERVER NAME", "bo1_directory_name", 186, 48);
    Field(set.host, text, "directory_host_password", "PASSWORD", "g_password", 226, 32);
    Control(set.host, text, "limit_label", "MAX PLAYERS", 48, 266, 184, 28, 1);
    auto &limit = Control(set.host, text, "directory_maxplayers", nullptr, 242, 266, 350, 28, 10);
    limit.item.dvar = "bo1_zombie_maxplayers"; limit.focus.focusTypeData.multi = &limit.multi;
    limit.multi.count = 4;
    static const char *values[] = {"1", "2", "3", "4"};
    for (int i = 0; i < 4; ++i) { limit.multi.dvarList[i] = values[i]; limit.multi.dvarValue[i] = float(i + 1); }
    Control(set.host, text, "directory_host_notice", hostNotice, 48, 322, 544, 56, 1);
    Control(set.host, text, "directory_start", "START MATCH", 48, 434, 200, 28);
    Control(set.host, text, "directory_back", "BACK", 500, 434, 108, 28);
    Finish(set.host);

    Panel(set.password, source, passwordName, "SERVER PASSWORD");
    set.password.menu.fullScreen = 0;
    Field(set.password, text, "directory_join_password", "PASSWORD", "ui_directory_joinPassword", 200, 32);
    Event(set.password.owned.back(), 3, "onAccept", "uiScript JoinServer ;");
    Control(set.password, text, "directory_password_accept", "JOIN", 242, 270, 150, 28);
    Control(set.password, text, "directory_password_cancel", "CANCEL", 442, 270, 150, 28);
    set.password.open.action = "setFocus directory_join_password ;";
    Finish(set.password);
    for (auto *menu : {&set.browser.menu, &set.host.menu, &set.password.menu}) dc->Menus[dc->menuCount++] = menu;
    return true;
}

void Directory_MenuOpening(int localClientNum, UiContext *dc, menuDef_t *menu)
{
    if (localClientNum || !menu) return;
    PatchEntry(menu);
    if (Named(menu->window.name, "main_online") || Named(menu->window.name, "menu_xboxlive"))
        Directory_ZombiesPrepareMenus(dc);
    if (Named(menu->window.name, "main_online") && !startingHost)
    {
        configuringHost = choosingMap = false;
        Dvar_SetBoolByName("bo1_directory_publish", false);
    }
}

bool Directory_HandleMenuEvent(int localClientNum, UiContext *dc, itemDef_s *item, const char *event)
{
    if (localClientNum || !item || !item->parent) return false;
    const char *menu = item->parent->window.name;
    const bool action = Named(event, "action");
    // Native lifecycle events use a temporary item with only parent/onEvent
    // initialized. Its window and control fields must not be inspected.
    const char *name = action ? item->window.name : nullptr;
    if (Named(menu, passwordName))
    {
        if ((action && Named(name, "directory_password_accept"))
            || (Named(event, "onAccept") && Named(item->dvar, "ui_directory_joinPassword")))
            return Directory_AcceptJoinPassword();
        if (Named(event, "onClose")) Directory_CancelJoinPassword();
        if (Named(event, "onESC") || (action && Named(name, "directory_password_cancel")))
        {
            Directory_CancelJoinPassword(); Menus_CloseByName(0, dc, passwordName); return true;
        }
    }
    if (Named(menu, "levels_zombie") && choosingMap && Named(event, "onESC"))
    {
        choosingMap = false;
        Menus_CloseAll(0, dc); configuringHost = true;
        Menus_OpenByName(0, dc, hostName); return true;
    }
    if ((Named(menu, browserName) || Named(menu, hostName))
        && (Named(event, "onESC") || (action && Named(name, "directory_back"))))
    {
        Back(dc, menu); return true;
    }
    if (!action) return false;
    if (Named(menu, "main_online")) CancelHostSelection();
    if (Named(menu, "main_online") && Named(name, "online_button1"))
    {
        ResetLocalSplit();
        Dvar_SetBoolByName("bo1_directory_publish", false);
        Dvar_SetBoolByName("onlinegame", false); Dvar_SetBoolByName("systemlink", false);
        Dvar_SetStringByName("ui_gametype", "zom");
        Item_RunScript(0, dc, item, "play uin_navigation_click ; setDvar menu_parent_3d_tv_name main_online ; setDvar next_menu_name menu_xboxlive ; open blackout_3d_tv ;");
        return true;
    }
    if (Named(name, "directory_zombie_browser"))
    {
        ResetLocalSplit(); CancelHostSelection();
        Dvar_SetBoolByName("bo1_directory_enabled", true);
        if (Directory_ZombiesPrepareMenus(dc)) Menus_OpenByName(0, dc, browserName);
        return true;
    }
    if (Named(name, "directory_zombie_host")) { OpenHost(dc); return true; }
    if (Named(menu, browserName))
    {
        if (Named(name, "directory_refresh")) { UI_StartServerRefresh(0, dc->contextIndex, 1); return true; }
        if (Named(name, "directory_join")) { UI_RunMenuScript_JoinServer(0, dc->contextIndex); return true; }
    }
    if (Named(menu, hostName))
    {
        if (Named(name, "directory_start")) return Directory_StartHostMatch();
        if (Named(name, "directory_change_map"))
        {
            choosingMap = true;
            Dvar_SetBoolByName("onlinegame", false); Dvar_SetBoolByName("systemlink", false);
            Item_RunScript(0, dc, item, "setDvar menu_parent_3d_tv_name main_online ; setDvar next_menu_name levels_zombie ; open blackout_3d_tv ;");
            return true;
        }
    }
    return false;
}

bool Directory_StartZombieMapNotify(int localClientNum, const char *notify)
{
    if (localClientNum || !choosingMap || !Named(notify, "startlevel 0")) return false;
    const std::string map = Dvar_GetString("ui_load_level");
    choosingMap = false;
    if (ValidMap(map.c_str()))
    {
        selectedMap = map;
        std::snprintf(mapCaption, sizeof(mapCaption), "MAP: %s", UI_GetMapDisplayName(selectedMap.c_str()));
        hostNotice[0] = 0;
    }
    else
        I_strncpyz(hostNotice, "Choose a Zombies co-op map. Dead Ops Arcade uses solo mode.", sizeof(hostNotice));
    // Complete native picker callbacks first; none may re-enable solo mode
    // after the co-op launch has selected its four-client memory allocation.
    auto *dc = &UI_GetInfo(0)->uiDC;
    Menus_CloseAll(0, dc); configuringHost = true;
    Menus_OpenByName(0, dc, hostName);
    return true;
}
void Directory_ApplyHostPlayerLimit(const char *, bool)
{
    const int capacity = std::clamp(Dvar_GetInt("bo1_zombie_maxplayers"), 1, 4);
    Dvar_SetIntByName("sv_maxclients", capacity);
    Dvar_SetIntByName("party_maxplayers", capacity);
}
bool Directory_StartHostMatch()
{
    if (!configuringHost || !ValidMap(selectedMap.c_str())) return false;
    const std::string map = selectedMap, name = Dvar_GetString("bo1_directory_name"), password = Dvar_GetString("g_password");
    startingHost = true;
    Menus_CloseAll(0, &UI_GetInfo(0)->uiDC);
    ResetLocalSplit();
    Dvar_SetStringByName("bo1_directory_name", const_cast<char *>(name.c_str()));
    Dvar_SetStringByName("sv_hostname", const_cast<char *>(name.empty() ? "OpenBLOPS Zombies" : name.c_str()));
    Dvar_SetStringByName("g_password", const_cast<char *>(password.c_str()));
    Dvar_SetBoolByName("onlinegame", false); Dvar_SetBoolByName("systemlink", true);
    for (const char *flag : {"splitscreen", "xblive_privatematch", "xblive_rankedmatch", "xblive_wagermatch", "xblive_basictraining", "xblive_theater", "playlist_enabled", "arcademode"})
        Dvar_SetBoolByName(flag, false);
    Directory_ApplyHostPlayerLimit("zom", true);
    Dvar_SetStringByName("g_gametype", "zom");
    Dvar_SetBoolByName("bo1_directory_enabled", true);
    Dvar_SetBoolByName("bo1_directory_publish", true);
    runningHostMap = map; configuringHost = false;
    Cbuf_AddText(0, va("setclientbeingusedandprimaryandactive\nwait; wait; map %s\n", map.c_str()));
    Com_Printf(14, "Directory host: starting public Zombies on %s\n", map.c_str());
    return true;
}
void Directory_FrontendFrame()
{
    if (startingHost && Com_IsMenuLevel(nullptr)) return;
    if (startingHost && runningHostMap == Dvar_GetString("mapname")) startingHost = false;
    if (!runningHostMap.empty() && !startingHost
        && (!com_sv_running->current.enabled || Com_IsMenuLevel(nullptr)))
    {
        Dvar_SetBoolByName("bo1_directory_publish", false); runningHostMap.clear();
    }
    std::snprintf(browserStatus, sizeof(browserStatus), "%s", Directory_Status());
    int players = 0;
    for (int i = 0; i < cls.numunrankedservers; ++i) players += cls.unrankedServers[i].clients;
    std::snprintf(browserCounts, sizeof(browserCounts), "%d players, %d servers", players, cls.numunrankedservers);
}
#endif
