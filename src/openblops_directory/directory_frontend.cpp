#include "directory_engine.h"
#if defined(OPENBLOPS_NO_STEAM_AUTH) && defined(KISAK_MP)
#include "directory_protocol.h"
#include <client/splitscreen.h>
#include <cstring>
#include <list>
#include <qcommon/cmd.h>
#include <qcommon/common.h>
#include <server_mp/sv_init_mp.h>
#include <string>
#include <ui/ui_main.h>
#include <ui/ui_shared.h>
#include <ui/ui_utils.h>
#include <ui/ui_splitscreen.h>
#include <vector>
namespace
{
bool Named(const char *a, const char *b)
{
    return a && b && !I_stricmp(a, b);
}
GenericEventScript *Event(const itemDef_s *item, const char *name)
{
    for (auto *event = item ? item->onEvent : nullptr; event; event = event->next)
        if (Named(event->name, name))
            return event->eventScript;
    return nullptr;
}
bool HasAction(const itemDef_s *item, const char *text)
{
    for (auto *script = Event(item, "action"); script; script = script->next)
        if (script->action && std::strstr(script->action, text))
            return true;
    return false;
}
// Stock menu labels are usually localization/case-conversion expressions,
// including a separate text item shown only while the button is highlighted.
bool HasLabel(const textDef_s *text, const char *key)
{
    if (!text)
        return false;
    if (Named(text->text, key))
        return true;
    if (!text->textExpData)
        return false;
    const auto &expression = text->textExpData->textExp;
    for (int i = 0; expression.rpn && i < expression.numRpn; ++i)
    {
        const auto &part = expression.rpn[i];
        if (part.type == 0 && part.data.constant.dataType == VAL_STRING &&
            Named(part.data.constant.internals.string, key))
            return true;
    }
    return false;
}
struct ItemCopy
{
    itemDef_s item{};
    textDef_s text{};
    focusItemDef_s focus{};
};
struct MenuCopy
{
    menuDef_t *menu = nullptr;
    std::list<ItemCopy> copies;
    std::vector<itemDef_s *> items;
    itemDef_s *hostButton = nullptr;
    GenericEventScript *privateAction = nullptr;
};
std::list<MenuCopy> menus;
struct ServerSettingItem
{
    itemDef_s item{};
    textDef_s text{};
    focusItemDef_s focus{};
    editFieldDef_s edit{};
};
struct ServerSettingsCopy
{
    menuDef_t *menu = nullptr;
    std::list<ServerSettingItem> copies;
    std::vector<itemDef_s *> items;
};
std::list<ServerSettingsCopy> serverSettings, passwordPopups;

void PatchPasswordPopup(menuDef_t *menu)
{
    if (!menu || !Named(menu->window.name, "join_password_popmenu")) return;
    for (auto &copy : passwordPopups)
        if (copy.menu == menu && menu->items == copy.items.data())
        {
            copy.copies.front().item.dvar = Directory_HasPasswordPrompt() ? "ui_directory_joinPassword" : "password";
            return;
        }
    for (int i = 0; i < menu->itemCount; ++i)
    {
        auto *source = menu->items[i];
        if (!Named(source->dvar, "password") || !source->typeData.textDef) continue;
        passwordPopups.emplace_back(); auto &copy = passwordPopups.back(); copy.menu = menu;
        copy.copies.emplace_back(); auto &part = copy.copies.back();
        part.item = *source; part.text = *source->typeData.textDef;
        part.item.typeData.textDef = &part.text;
        auto *focus = part.text.textTypeData.focusItemDef;
        if (focus && focus->focusTypeData.editField)
        {
            part.focus = *focus; part.edit = *focus->focusTypeData.editField;
            part.focus.focusTypeData.editField = &part.edit;
            part.text.textTypeData.focusItemDef = &part.focus;
        }
        // Typing stays in an unarchived draft, outside the userinfo sent to peers.
        part.item.dvar = Directory_HasPasswordPrompt() ? "ui_directory_joinPassword" : "password";
        copy.items.assign(menu->items, menu->items + menu->itemCount);
        copy.items[i] = &part.item; menu->items = copy.items.data();
        return;
    }
}

// Retain the shipped panel, title, help and Back action for the settings
// supported by the community listen host. Text input and focus remain
// native; edit/cursor state is privately owned instead of changing zone assets.
void PatchServerSettings(menuDef_t *menu)
{
    if (!menu || !Named(menu->window.name, "server_settings")) return;
    _Dvar_RegisterString("bo1_directory_name", "OpenBLOPS match", 1,
        "Public match label (not a player roster)");
    _Dvar_RegisterString("g_password", "", 0, "Password");
    _Dvar_RegisterInt("bo1_host_maxplayers", 0, 0, 18, 1u, "Max players override (zero uses mode defaults)");
    static multiDef_s limits = {};
    static const char *labels[] = { "DEFAULT", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "10", "11", "12", "13", "14", "15", "16", "17", "18" };
    limits.count = 19;
    for (int i = 0; i < limits.count; ++i) { limits.dvarList[i] = labels[i]; limits.dvarValue[i] = float(i); }
    for (const auto &copy : serverSettings)
        if (copy.menu == menu && menu->items == copy.items.data()) return;
    itemDef_s *name = nullptr, *password = nullptr, *maxPlayers = nullptr;
    for (int i = 0; i < menu->itemCount; ++i)
    {
        auto *item = menu->items[i];
        if (Named(item->dvar, "sv_hostname")) name = item;
        if (Named(item->dvar, "g_password")) password = item;
        if (Named(item->dvar, "sv_maxclients")) maxPlayers = item;
    }
    if (!name || !password || !maxPlayers || !name->typeData.textDef || !password->typeData.textDef) return;
    serverSettings.emplace_back();
    auto &copy = serverSettings.back(); copy.menu = menu;
    for (int i = 0; i < menu->itemCount; ++i)
    {
        auto *source = menu->items[i];
        const bool nameRow = source->window.rect.y == name->window.rect.y
            && source->window.rect.x == name->window.rect.x && source->window.rect.h == name->window.rect.h;
        const bool passwordRow = source->window.rect.y == password->window.rect.y
            && source->window.rect.x == password->window.rect.x && source->window.rect.h == password->window.rect.h;
        const bool maxRow = source->window.rect.y == maxPlayers->window.rect.y
            && source->window.rect.x == maxPlayers->window.rect.x && source->window.rect.h == maxPlayers->window.rect.h;
        // Unused stock controls (including VAC) remain hidden.
        if (!nameRow && !passwordRow && !maxRow && source->window.rect.h == name->window.rect.h
            && source->window.rect.x == name->window.rect.x) continue;
        if (!nameRow && !passwordRow && !maxRow) { copy.items.push_back(source); continue; }
        copy.copies.emplace_back(); auto &part = copy.copies.back();
        part.item = *source;
        auto &item = part.item; item.parent = menu;
        const float dy = maxRow ? name->window.rect.y + 76.0f - maxPlayers->window.rect.y
            : passwordRow ? name->window.rect.y + 38.0f - password->window.rect.y : 0.0f;
        item.window.rect.y += dy; item.window.rectClient.y += dy;
        item.rectExpData = nullptr; item.animInfo = nullptr;
        if (Item_IsTextDefType(source) && source->typeData.textDef)
        {
            part.text = *source->typeData.textDef; item.typeData.textDef = &part.text;
            part.text.textRect[0] = {};
            if (source == name || source == password)
            {
                auto *focus = source->typeData.textDef->textTypeData.focusItemDef;
                if (focus && focus->focusTypeData.editField)
                {
                    part.focus = *focus; part.edit = *focus->focusTypeData.editField;
                    part.focus.focusTypeData.editField = &part.edit;
                    part.text.textTypeData.focusItemDef = &part.focus;
                    part.edit.cursorPos[0] = part.edit.paintOffset = 0;
                    if (source == name) part.edit.maxChars = 48;
                }
            }
        }
        if (source == name) item.dvar = "bo1_directory_name";
        if (maxRow)
        {
            // Stock maxplayers is dedicated-only. Use native multi-choice input
            // and show its copied highlight only while this choice has focus.
            const bool highlight = source != maxPlayers && source->visibleExp.numRpn != maxPlayers->visibleExp.numRpn;
            item.window.name = source == maxPlayers ? "openblops_host_maxplayers"
                : highlight ? "openblops_host_maxplayers_hl" : "openblops_host_maxplayers_row";
            item.visibleExp = {}; item.showBits = 0; item.hideBits = ~0ULL;
            item.dvarFlags = 0; item.dvarTest = nullptr; item.enableDvar = nullptr;
            if (source == maxPlayers)
            {
                item.type = item.dataType = 10;
                part.focus = *source->typeData.textDef->textTypeData.focusItemDef;
                part.focus.focusTypeData.multi = &limits;
                part.text.textTypeData.focusItemDef = &part.focus;
                part.text.text = nullptr; part.text.textExpData = nullptr;
                item.dvar = "bo1_host_maxplayers";
            }
        }
        copy.items.push_back(&item);
    }
    menu->items = copy.items.data();
    menu->itemCount = int(copy.items.size());
}

enum class HostPhase
{
    Off,
    Configuring,
    Queued,
    Active
};
HostPhase hostPhase = HostPhase::Off;
bool enteringPublicSetup = false;
std::list<ItemCopy> lobbyCaptions;
void PatchLobbyCaption(menuDef_t *menu)
{
    if (!menu || !Named(menu->window.name, "menu_xboxlive_privatelobby")) return;
    const char *caption = hostPhase == HostPhase::Off ? "OFFLINE MATCH" : "HOST MATCH";
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
    if (UI_SplitScreenIsCustomMenu(menu)) caption = "SPLIT-SCREEN";
#endif
    for (int i = 0; i < menu->itemCount; ++i)
    {
        auto *source = menu->items[i];
        if (!Item_IsTextDefType(source) || !source->typeData.textDef) continue;
        if (!Named(source->window.name, "openblops_lobby_caption")
            && !HasLabel(source->typeData.textDef, "@MPUI_CUSTOM_MATCH_LOBBY_CAPS")) continue;
        for (auto &copy : lobbyCaptions)
            if (&copy.item == source && copy.item.parent == menu) { copy.text.text = caption; return; }
        // Each normal/split-screen lobby owns its caption; leave the zone's
        // expression and any independently copied lobby untouched.
        lobbyCaptions.emplace_back(); auto &copy = lobbyCaptions.back();
        copy.item = *source; copy.text = *source->typeData.textDef;
        copy.item.window.name = "openblops_lobby_caption";
        copy.item.parent = menu; copy.item.typeData.textDef = &copy.text;
        copy.text.text = caption; copy.text.textExpData = nullptr; copy.text.textRect[0] = {};
        // MenuCopy owns stable pointer storage for the remaining native rows.
        menus.emplace_back(); auto &owner = menus.back(); owner.menu = menu;
        owner.items.assign(menu->items, menu->items + menu->itemCount);
        owner.items[i] = &copy.item; menu->items = owner.items.data();
        return;
    }
}
std::string hostMap;
void CancelPublic(bool explicitChoice = false)
{
    if (explicitChoice || hostPhase != HostPhase::Off)
        Dvar_SetBoolByName("bo1_directory_publish", false);
    hostPhase = HostPhase::Off;
    hostMap.clear();
}
void PatchMenu(menuDef_t *menu)
{
    if (!menu || (!Named(menu->window.name, "main_text") && !Named(menu->window.name, "live_flyout")))
        return;
    for (const auto &copy : menus)
        if (copy.menu == menu && menu->items == copy.items.data())
            return;
    itemDef_s *privateButton = nullptr;
    if (Named(menu->window.name, "live_flyout"))
        for (int i = 0; i < menu->itemCount; ++i)
            if (HasAction(menu->items[i], "open menu_xboxlive_privatelobby"))
                privateButton = menu->items[i];
    menus.emplace_back();
    auto &copy = menus.back();
    copy.menu = menu;
    copy.privateAction = Event(privateButton, "action");
    for (int i = 0; i < menu->itemCount; ++i)
    {
        itemDef_s *source = menu->items[i];
        const bool text = Item_IsTextDefType(source) && source->typeData.textDef;
        const auto *label = text ? source->typeData.textDef : nullptr;
        const bool operations = HasLabel(label, "@MENU_OPERATIONS_CAPS");
        const bool ranked = Named(menu->window.name, "live_flyout") && privateButton &&
                            (HasLabel(label, "@MENU_RANKED_MATCH_CAPS") || HasAction(source, "quickmatch"));
        const bool offline = Named(menu->window.name, "live_flyout") && privateButton
            && (HasLabel(label, "@MPUI_CUSTOM_MATCH_CAPS") || HasLabel(label, "PRIVATE MATCH") || source == privateButton);
        const bool wager = Named(menu->window.name, "live_flyout")
            && (HasLabel(label, "@MPUI_WAGER_MATCH_CAPS") || HasLabel(label, "WAGER MATCH"));
        if (!operations && !ranked && !offline && !wager)
        {
            copy.items.push_back(source);
            continue;
        }
        copy.copies.emplace_back();
        auto &part = copy.copies.back();
        part.item = *source;
        auto &item = part.item;
        item.parent = menu;
        if (text)
        {
            part.text = *source->typeData.textDef;
            item.typeData.textDef = &part.text;
            part.text.text = wager ? "" : offline ? "OFFLINE MATCH" : ranked ? "HOST MATCH" : "CREATE A CLASS";
            part.text.textExpData = nullptr;
            part.text.textRect[0] = {};
            if (source->dataType == 3 && part.text.textTypeData.focusItemDef)
            {
                part.focus = *part.text.textTypeData.focusItemDef;
                part.text.textTypeData.focusItemDef = &part.focus;
            }
        }
        if (ranked && HasAction(source, "quickmatch"))
        {
            copy.hostButton = &item;
            // Private Match's availability, Ranked's position and focus layers.
            item.dvar = privateButton->dvar;
            item.dvarTest = privateButton->dvarTest;
            item.enableDvar = privateButton->enableDvar;
            item.dvarFlags = privateButton->dvarFlags;
            item.visibleExp = privateButton->visibleExp;
            item.showBits = privateButton->showBits;
            item.hideBits = privateButton->hideBits;
        }
        copy.items.push_back(&item);
    }
    menu->items = copy.items.data();
}
} // namespace
void Directory_MenuOpening(int localClientNum, UiContext *dc, menuDef_t *menu)
{
    (void)dc;
    if (localClientNum || !menu)
        return;
    PatchMenu(menu);
    PatchLobbyCaption(menu);
    PatchServerSettings(menu);
    PatchPasswordPopup(menu);
    if (Named(menu->window.name, "main_text") && !enteringPublicSetup)
    {
        if (hostPhase == HostPhase::Configuring ||
            (hostPhase == HostPhase::Active && !com_sv_running->current.enabled))
            CancelPublic();
    }
}
bool Directory_HandleMenuEvent(int localClientNum, UiContext *dc, itemDef_s *item, const char *event)
{
    if (localClientNum || !item || !item->parent)
        return false;
    if (Named(item->parent->window.name, "join_password_popmenu"))
    {
        if (Named(event, "onClose") || Named(event, "onESC"))
            Directory_CancelJoinPassword();
        if ((Named(event, "action") && HasAction(item, "JoinServer"))
            || (Named(event, "onAccept") && Named(item->dvar, "ui_directory_joinPassword")))
            return Directory_AcceptJoinPassword();
    }
    for (auto &copy : menus)
        if (copy.hostButton == item)
        {
            if (Named(event, "onFocus"))
            {
                if (auto *script = Event(item, event))
                    Item_RunSingleEventScript(localClientNum, dc, item, script);
                Dvar_SetStringByName(
                    "ui_lobby_desc",
                    const_cast<char *>("Set up a match and advertise it in the server browser."));
                return true;
            }
            if (Named(event, "action"))
            {
                if (!copy.privateAction)
                    return true;
                CancelPublic(true);
                hostPhase = HostPhase::Configuring;
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
                CL_SplitScreenResetSession();
#endif
                enteringPublicSetup = true;
                // Execute the shipped Private Match action, including its native
                // conditions/defaults. Suppress only the old quickmatch action.
                Item_RunSingleEventScript(localClientNum, dc, item, copy.privateAction);
                enteringPublicSetup = false;
                Com_Printf(14, "Directory host: public match setup selected; awaiting Start Match\n");
                return true;
            }
            return false;
        }
    if (Named(event, "action"))
    {
        if (Named(item->parent->window.name, "live_flyout"))
            CancelPublic(true);
        else if (Named(item->parent->window.name, "main_text") && hostPhase != HostPhase::Queued)
            CancelPublic();
    }
    return false;
}
void Directory_ApplyHostPlayerLimit(const char *mode, bool publicMatch)
{
    const auto *setting = _Dvar_RegisterInt("bo1_host_maxplayers", 0, 0, 18, 1u,
        "Max players override (zero uses mode defaults)");
    const int override = setting->current.integer;
    const bool tdm = mode && (!I_stricmp(mode, "tdm") || !I_stricmp(mode, "hctdm"));
    if (!publicMatch && !override && !tdm) return;
    int capacity = override ? override : Dvar_GetInt("sv_maxclients");
    if (!override && tdm && capacity < 12) capacity = 12;
    if (publicMatch) capacity = capacity < 1 ? 18 : capacity > 18 ? 18 : capacity;
    Dvar_SetIntByName("sv_maxclients", capacity);
    Dvar_SetIntByName("party_maxplayers", capacity);
}
bool Directory_StartHostMatch()
{
    if (hostPhase != HostPhase::Configuring)
        return false;
    const std::string map = Dvar_GetString("ui_mapname"), mode = Dvar_GetString("ui_gametype");
    if (!bo1dir::Token(map, 31) || map.compare(0, 3, "mp_") || !bo1dir::Token(mode, 15))
    {
        Com_PrintWarning(14, "Directory host: select a valid multiplayer map and mode before starting\n");
        return true;
    }
    const std::string name = Dvar_GetString("bo1_directory_name"), password = Dvar_GetString("g_password");
    hostMap = map;
    hostPhase = HostPhase::Queued;
    // Close callbacks can reapply online frontend flags. Finish them first.
    Menus_CloseAll(0, &UI_GetInfo(0)->uiDC);
    Dvar_SetStringByName("sv_hostname", const_cast<char *>(name.empty() ? "OpenBLOPS match" : name.c_str()));
    Dvar_SetStringByName("g_password", const_cast<char *>(password.c_str()));
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
    CL_SplitScreenResetSession();
#endif
    Dvar_SetBoolByName("onlinegame", false);
    Dvar_SetBoolByName("systemlink", true);
    for (const char *flag : {"xblive_privatematch", "xblive_rankedmatch", "xblive_basictraining",
                             "xblive_wagermatch", "xblive_theater", "xblive_clanmatch", "playlist_enabled"})
        Dvar_SetBoolByName(flag, false);
    Directory_ApplyHostPlayerLimit(mode.c_str(), true);
    Dvar_SetStringByName("g_gametype", const_cast<char *>(mode.c_str()));
    Dvar_SetStringByName("sv_mapRotationCurrent", const_cast<char *>(Dvar_GetString("sv_mapRotation")));
    Dvar_SetBoolByName("bo1_directory_enabled", true);
    Dvar_SetBoolByName("bo1_directory_publish", true);
    Cbuf_AddText(0, va("setclientbeingusedandprimaryandactive\nwait; wait; map %s\n", map.c_str()));
    Com_Printf(14, "Directory host: starting public %s on %s\n", mode.c_str(), map.c_str());
    return true;
}
void Directory_FrontendFrame()
{
    if (hostPhase == HostPhase::Queued && com_sv_running->current.enabled && SV_Loaded() &&
        hostMap == Dvar_GetString("mapname"))
        hostPhase = HostPhase::Active;
    if (hostPhase == HostPhase::Active && (!Dvar_GetString("mapname")[0] || Com_IsMenuLevel(nullptr)))
        CancelPublic();
}
#endif
