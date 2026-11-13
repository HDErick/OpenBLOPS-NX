#include "ui_splitscreen.h"
#include "ui_shared.h"
#include "ui_utils.h"
#include "ui_feeders.h"
#include "ui_main.h"
#include <ui_mp/ui_main_mp.h>
#include <client/splitscreen.h>
#include <qcommon/common.h>
#include <qcommon/cmd.h>
#include <cstring>
#include <list>
#include <vector>

namespace
{
    bool zombieSelection;
    const char *const guestButton = "kisak_split_screen";
    struct Button
    {
        itemDef_s item{};
        textDef_s text{};
        focusItemDef_s focus{};
        GenericEventScript focusScript{}, leaveScript{};
        GenericEventHandler focusEvent{}, leaveEvent{}, actionEvent{};
    };
    struct ItemCopy
    {
        itemDef_s item{};
        textDef_s text{};
        focusItemDef_s focus{};
        UIAnimInfo animation{};
        std::vector<animParamsDef_t> states;
        std::vector<animParamsDef_t *> statePointers;
    };
    // Asset definitions/scripts are read-only; windows, text caches and animation
    // clocks are private. Stable storage also survives base/patch asset swaps.
    struct FrontRow
    {
        Button button;
        std::list<ItemCopy> shifted;
        std::list<ItemCopy> layers;
        std::vector<itemDef_s *> rowItems;
        std::vector<itemDef_s *> items;
    };
    std::list<FrontRow> frontRows;
#ifndef KISAK_SP
    struct LobbyCopy
    {
        menuDef_t menu{};
        menuDef_t *source = nullptr;
        std::list<ItemCopy> copies;
        std::vector<itemDef_s *> items;
    };
    std::list<LobbyCopy> lobbyCopies;
    LobbyCopy *splitLobby;
#endif

    bool Named(const char *a, const char *b) { return a && !I_stricmp(a, b); }

    bool StartSelectedMap(UiContext *dc, const char *map)
    {
        if (!CL_SplitScreenCanStartMap(map)) return false;
        char selected[64];
        std::strcpy(selected, map);
#ifndef KISAK_SP
        char gametype[64];
        const char *choice = Dvar_GetString("ui_gametype");
        if (std::strlen(choice) >= sizeof(gametype)) return false;
        std::strcpy(gametype, choice);
#endif
        // Closing the picker can focus main_online again; its native onFocus
        // sets onlinegame=1. Finish those callbacks before setting host modes.
        Menus_CloseAll(0, dc);
#ifndef KISAK_SP
        Dvar_SetStringByName("ui_gametype", gametype);
#endif
        return CL_SplitScreenStartMap(selected);
    }

#ifndef KISAK_SP
    bool HasAction(const itemDef_s *item, const char *token)
    {
        for (const auto *event = item->onEvent; event; event = event->next)
            if (Named(event->name, "action"))
                for (const auto *script = event->eventScript; script; script = script->next)
                    if (script->action && std::strstr(script->action, token)) return true;
        return false;
    }
#endif

    void CopyItem(ItemCopy &copy, const itemDef_s *source, menuDef_t *parent, float dy)
    {
        copy.item = *source;
        auto &item = copy.item;
        item.parent = parent;
        if (Item_IsTextDefType(source) && source->typeData.textDef)
        {
            copy.text = *source->typeData.textDef;
            item.typeData.textDef = &copy.text;
            copy.text.textRect[0] = {};
            if (source->dataType == 3 && copy.text.textTypeData.focusItemDef)
            {
                copy.focus = *copy.text.textTypeData.focusItemDef;
                copy.text.textTypeData.focusItemDef = &copy.focus;
            }
        }
        if (source->animInfo)
        {
            copy.animation = *source->animInfo;
            item.animInfo = &copy.animation;
            const int count = copy.animation.animStateCount;
            copy.states.resize(count);
            copy.statePointers.resize(count);
            for (int i = 0; i < count; ++i)
            {
                copy.states[i] = *source->animInfo->animStates[i];
                copy.states[i].rectClient.y += dy;
                copy.statePointers[i] = &copy.states[i];
            }
            copy.animation.animStates = copy.statePointers.data();
            copy.animation.currentAnimState.rectClient.y += dy;
            copy.animation.nextAnimState.rectClient.y += dy;
        }
        if (dy)
        {
            item.window.rect.y += dy;
            item.window.rectClient.y += dy;
            // These stock main-menu rows use fixed row spacing. Keep a later
            // animation/expression update from restoring the old row position.
            item.rectExpData = nullptr;
        }
    }

#ifndef KISAK_SP
    void RestorePrivateLobby(UiContext *dc)
    {
        if (!splitLobby) return;
        for (int i = 0; i < dc->menuCount; ++i)
            if (dc->Menus[i] == &splitLobby->menu)
            {
                Menus_Close(0, dc, &splitLobby->menu);
                dc->Menus[i] = splitLobby->source;
            }
        splitLobby = nullptr;
    }

    bool OpenSplitLobby(UiContext *dc)
    {
        RestorePrivateLobby(dc);
        int slot = -1;
        for (int i = 0; i < dc->menuCount; ++i)
            if (Named(dc->Menus[i]->window.name, "menu_xboxlive_privatelobby")) { slot = i; break; }
        if (slot < 0) return false;
        menuDef_t *source = dc->Menus[slot];
        Menus_CloseAll(0, dc);
        CL_SplitScreenResetSession();
        lobbyCopies.emplace_back();
        LobbyCopy &copy = lobbyCopies.back();
        copy.source = source;
        copy.menu = *source;
        copy.menu.window.dynamicFlags[0] = 0;
        copy.menu.cursorItem[0] = -1;
        copy.menu.fadeTimeCounter = copy.menu.slideTimeCounter = -1;
        for (int i = 0; i < source->itemCount; ++i)
        {
            copy.copies.emplace_back();
            CopyItem(copy.copies.back(), source->items[i], &copy.menu, 0);
            auto *item = &copy.copies.back().item;
            item->window.dynamicFlags[0] &= ~2; // no inherited keyboard focus
            copy.items.push_back(item);
        }
        copy.menu.items = copy.items.data();
        copy.menu.itemCount = static_cast<int>(copy.items.size());
        splitLobby = &copy;
        // Keep the stock name within this UI context: its child map/mode/rule
        // menus and ui_animate scripts all target it by name. The asset pool
        // and regular private lobby retain the original object and item state.
        dc->Menus[slot] = &copy.menu;
        Dvar_SetBoolByName("systemlink", false);
        Dvar_SetBoolByName("onlinegame", true);
        Dvar_SetBoolByName("xblive_privatematch", true);
        Dvar_SetBoolByName("xblive_rankedmatch", false);
        Dvar_SetBoolByName("xblive_basictraining", false);
        if (!CL_SplitScreenCanStartMap(Dvar_GetString("ui_mapname")))
            Dvar_SetStringByName("ui_mapname", const_cast<char *>("mp_mountain"));
        if (!Dvar_GetString("ui_gametype")[0])
            Dvar_SetStringByName("ui_gametype", const_cast<char *>("tdm"));
        return Menus_OpenByName(0, dc, "menu_xboxlive_privatelobby") != 0;
    }

    void AddMainMenuRow(menuDef_t *menu)
    {
        if (!Named(menu->window.name, "main_text")) return;
        for (int i = 0; i < menu->itemCount; ++i)
            if (Named(menu->items[i]->window.name, guestButton)) return;
        const itemDef_s *play = nullptr;
        for (int i = 0; i < menu->itemCount; ++i)
            if (menu->items[i]->type == 3 && menu->items[i]->typeData.textDef
                && HasAction(menu->items[i], "open live_flyout")) { play = menu->items[i]; break; }
        if (!play) return;
        frontRows.emplace_back();
        FrontRow &row = frontRows.back();
        rectDef_s rect = play->window.rectClient;
        const float step = rect.h;
        rect.y += step;
        // The enabled Play row is backing -> focus box -> white button ->
        // black focused label. Its later grey copies are online-disabled states,
        // which do not apply to this local option. Preserve native draw order,
        // materials, font/alignment and all colors instead of painting new text.
        int at = 0;
        while (at < menu->itemCount && menu->items[at] != play) ++at;
        auto sameRow = [&](const itemDef_s *item) {
            const auto &r = item->window.rectClient;
            const auto &p = play->window.rectClient;
            return r.x == p.x && r.y == p.y && r.h == p.h && r.w >= p.w - 1 && r.w <= p.w + 1;
        };
        int first = at;
        if (at >= 2 && sameRow(menu->items[at - 1]) && menu->items[at - 1]->type == 0
            && sameRow(menu->items[at - 2]) && menu->items[at - 2]->type == 2) first = at - 2;
        int last = at;
        if (at + 1 < menu->itemCount && sameRow(menu->items[at + 1]) && menu->items[at + 1]->type == 1) last = at + 1;
        for (int i = first; i <= last; ++i)
        {
            const auto *source = menu->items[i];
            row.layers.emplace_back();
            auto &layer = row.layers.back();
            CopyItem(layer, source, menu, step);
            auto *item = &layer.item;
            if (source == play)
            {
                row.button.item = layer.item;
                row.button.text = layer.text;
                row.button.focus = layer.focus;
                item = &row.button.item;
                item->typeData.textDef = &row.button.text;
                row.button.text.textTypeData.focusItemDef = &row.button.focus;
                item->window.name = guestButton;
                row.button.actionEvent.name = "action";
                row.button.actionEvent.next = &row.button.focusEvent;
                row.button.focusScript.action = "play uin_navigation_over ; setLocalVarInt ui_highlight 0 ; "
                    "setLocalVarString ui_choicegroup main_text ;";
                row.button.focusEvent.name = "onFocus";
                row.button.focusEvent.eventScript = &row.button.focusScript;
                row.button.focusEvent.next = &row.button.leaveEvent;
                row.button.leaveEvent.name = "leaveFocus";
                item->onEvent = &row.button.actionEvent;
            }
            else
            {
                item->window.name = (source->type == 0 || source->type == 1)
                    ? "kisak_split_screen_focus" : "kisak_split_screen_backing";
                item->onEvent = nullptr;
            }
            item->dvar = item->dvarTest = item->enableDvar = nullptr;
            item->dvarFlags = 0;
            item->visibleExp = {};
            item->forecolorAExp = {};
            item->showBits = 0;
            item->hideBits = ~0ULL;
            // Retain native material tint/alpha (0x10000), clearing only
            // inherited focus while making this local option visible.
            item->window.dynamicFlags[0] = (item->window.dynamicFlags[0] & ~2) | 4;
            if (Item_IsTextDefType(item) && item->typeData.textDef)
            {
                item->typeData.textDef->text = "SPLIT-SCREEN";
                item->typeData.textDef->textExpData = nullptr;
            }
            row.rowItems.push_back(item);
        }
        bool inserted = false;
        for (int i = 0; i < menu->itemCount; ++i)
        {
            auto *item = menu->items[i];
            const rectDef_s &r = item->window.rectClient;
            const bool lowerRow = r.x == rect.x && r.w >= rect.w - 1 && r.w <= rect.w + 1 && r.h == rect.h
                && r.y >= rect.y && r.y < 190;
            if (lowerRow && !inserted)
            {
                row.items.insert(row.items.end(), row.rowItems.begin(), row.rowItems.end());
                inserted = true;
            }
            if (lowerRow)
            {
                row.shifted.emplace_back();
                CopyItem(row.shifted.back(), item, menu, step);
                item = &row.shifted.back().item;
            }
            row.items.push_back(item);
        }
        if (!inserted) row.items.insert(row.items.end(), row.rowItems.begin(), row.rowItems.end());
        menu->items = row.items.data();
        menu->itemCount = static_cast<int>(row.items.size());
    }

#endif
}

bool UI_SplitScreenIsCustomMenu(const menuDef_t *menu)
{
#ifdef KISAK_SP
    return false;
#else
    for (const auto &copy : lobbyCopies)
        if (menu == &copy.menu) return true;
    return false;
#endif
}

void UI_SplitScreenAddMenus(int localClientNum, UiContext *dc, menuDef_t *menu)
{
    if (localClientNum || !menu || !menu->window.name) return;
#ifdef KISAK_SP
    if (!Named(menu->window.name, "main_online")) return;
    for (int i = 0; i < menu->itemCount; ++i)
        if (Named(menu->items[i]->window.name, guestButton)) return;
    // Copy Solo's backing and both language variants into the third row.
    const itemDef_s *source = nullptr;
    for (int i = 0; i < menu->itemCount; ++i)
        if (menu->items[i]->type == 3 && Named(menu->items[i]->window.name, "online_button2")
            && menu->items[i]->typeData.textDef) { source = menu->items[i]; break; }
    if (!source) return;
    frontRows.emplace_back();
    auto &row = frontRows.back();
    row.items.assign(menu->items, menu->items + menu->itemCount);
    auto &events = row.button;
    events.focusScript.action = "play uin_navigation_over ; setLocalVarInt ui_highlight 3 ; setLocalVarString ui_choicegroup main ;";
    events.leaveScript.action = "setLocalVarInt ui_highlight 0 ;";
    events.actionEvent.name = "action";
    events.actionEvent.next = &events.focusEvent;
    events.focusEvent.name = "onFocus";
    events.focusEvent.eventScript = &events.focusScript;
    events.focusEvent.next = &events.leaveEvent;
    events.leaveEvent.name = "leaveFocus";
    events.leaveEvent.eventScript = &events.leaveScript;
    const auto &solo = source->window.rectClient;
    for (int i = 0; i < menu->itemCount; ++i)
    {
        const auto *layer = menu->items[i];
        const auto &rect = layer->window.rectClient;
        const bool button = layer->type == 3 && Named(layer->window.name, "online_button2") && layer->typeData.textDef;
        if ((!button && layer->type != 0) || rect.x != solo.x || rect.y != solo.y || rect.w != solo.w || rect.h != solo.h)
            continue;
        auto &copy = row.layers.emplace_back();
        CopyItem(copy, layer, menu, solo.h);
        auto &item = copy.item;
        item.window.dynamicFlags[0] = (item.window.dynamicFlags[0] & ~2) | 4;
        if (button)
        {
            item.window.name = guestButton;
            item.onEvent = &events.actionEvent;
            copy.text.text = "SPLIT-SCREEN";
            copy.text.textExpData = nullptr;
        }
        else
        {
            item.window.name = "kisak_split_screen_backing";
            item.visibleExp = {}; // The copied buttons' focus drives this backing.
        }
        row.rowItems.push_back(&item);
        row.items.push_back(&item);
    }
    menu->items = row.items.data();
    menu->itemCount = static_cast<int>(row.items.size());
#else
    AddMainMenuRow(menu);
#endif
}

void UI_SplitScreenMenuOpening(int localClientNum, UiContext *dc, menuDef_t *menu)
{
    UI_SplitScreenAddMenus(localClientNum, dc, menu);
    if (localClientNum || !menu) return;
    if (Named(menu->window.name, "main_online") || Named(menu->window.name, "main_text"))
    {
#ifndef KISAK_SP
        // A parent/background menu may be reopened while the lobby or one of
        // its pickers is still visible. Only a real return after closing the
        // copied lobby should discard the selected match type.
        if (splitLobby)
            for (int i = 0; i < dc->menuCount; ++i)
                if (dc->Menus[i] == &splitLobby->menu && Window_IsVisible(0, &splitLobby->menu.window)) return;
        RestorePrivateLobby(dc);
#endif
        zombieSelection = false;
        CL_SplitScreenFrontendEntered();
    }
#ifndef KISAK_SP
    else if (Named(menu->window.name, "menu_xboxlive_privatelobby")
        && (!splitLobby || menu != &splitLobby->menu))
        CL_SplitScreenResetSession();
#endif
}

bool UI_SplitScreenHandleEvent(int localClientNum, UiContext *dc, itemDef_s *item, const char *event)
{
    if (localClientNum || !item || !Named(event, "action")) return false;
#ifdef KISAK_SP
    if (item->parent && Named(item->parent->window.name, "main_online"))
    {
        if (Named(item->window.name, guestButton))
        {
            zombieSelection = true;
            CL_SplitScreenResetSession();
            Item_RunScript(0, dc, item, "play uin_navigation_click ; setDvar menu_parent_3d_tv_name main_online ; "
                "setDvar next_menu_name levels_zombie ; open blackout_3d_tv ;");
            return true;
        }
        zombieSelection = false;
        CL_SplitScreenResetSession();
    }
#else
    for (auto &row : frontRows)
        if (item == &row.button.item)
        {
            OpenSplitLobby(dc);
            return true;
        }
    if (item->parent && Named(item->parent->window.name, "main_text"))
    {
        RestorePrivateLobby(dc);
        CL_SplitScreenResetSession();
    }
#endif
    return false;
}

bool UI_SplitScreenStartZombieMap(int localClientNum, const char *notify)
{
#ifdef KISAK_SP
    if (!localClientNum && zombieSelection && Named(notify, "startlevel 0"))
    {
        // Reuse all native map choices, but start the real co-op host rather
        // than the frontend script's single-player startlevel path.
        if (StartSelectedMap(&UI_GetInfo(0)->uiDC, Dvar_GetString("ui_load_level")))
            zombieSelection = false;
        return true;
    }
#endif
    return false;
}

// Native UI visibility already checked the window/dvar bits. Local row layers
// replace only the copied row's online availability/highlight expressions.
bool UI_SplitScreenIsHiddenLayer(int contextIndex, const itemDef_s *item)
{
    // Leave only the button text visible while unfocused, including hiding
    // the copied row's translucent backing.
    if (item && (Named(item->window.name, "kisak_split_screen_focus")
        || Named(item->window.name, "kisak_split_screen_backing")))
        for (const auto &row : frontRows)
            for (const auto *layer : row.rowItems)
                if (layer == item)
                {
                    for (const auto *button : row.rowItems)
                        if (Named(button->window.name, guestButton) && Window_HasFocus(contextIndex, &button->window))
                            return false;
                    return true;
                }
    return false;
}

bool UI_SplitScreenStartMpMatch()
{
#ifndef KISAK_SP
    if (splitLobby)
    {
        CL_SplitGuestLog("Split-Screen native Start Match selected");
        StartSelectedMap(&UI_GetInfo(0)->uiDC, Dvar_GetString("ui_mapname"));
        return true;
    }
    // An ordinary private-match start also cancels a failed/abandoned queued
    // split-screen load; the last menu choice is authoritative.
    CL_SplitScreenResetSession();
#endif
    return false;
}
