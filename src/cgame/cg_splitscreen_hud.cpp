#include <client/splitscreen.h>
#include <client/client.h>
#include <client/screen_placement.h>
#include <cgame_mp/cg_local_mp.h>
#include <cgame_mp/cg_main_mp.h>
#include <cgame_mp/cg_newDraw_mp.h>
#include <cgame/cg_hudelem.h>
#include <cgame/cg_splitscreen_hud.h>
#include <cgame/cg_ammocounter.h>
#include <cgame/cg_playerstate.h>
#include <cgame/cg_draw_indicators.h>
#include <cgame/cg_draw_names.h>
#include <cgame/cg_draw_reticles.h>
#include <bgame/bg_weapons.h>
#include <bgame/bg_weapons_ammo.h>
#include <gfx_d3d/r_rendercmds.h>
#include <ui/ui_main.h>
#include <cstdio>
#include <cstring>

struct SplitGuestHudState
{
    bool valid = false;
    int damageEvent = 0, mode = 0, teleport = 0;
    int weapon = 0, clip = -1, reserve = -1, flags = 0;
    float ads = 0.0f;
};
static SplitGuestHudState s_guestHud;
static unsigned int s_guestHudFrames, s_guestHudPaints, s_guestMenuPaints;
static int s_guestDamageCount, s_guestFeedbackUntil, s_guestHudTime;
const char *CG_SplitGuestHudStatus()
{
    static char status[256];
    std::snprintf(status, sizeof(status),
        "guest_draw frames=%u hud_paints=%u menu_paints=%u cg_draw2D=%d hud_enable=%d damage_event=%d damage_count=%d feedback_until=%d view_time=%d",
        s_guestHudFrames, s_guestHudPaints, s_guestMenuPaints, Dvar_GetBool("cg_draw2D"),
        Dvar_GetBool("hud_enable"), s_guestHud.damageEvent, s_guestDamageCount,
        s_guestFeedbackUntil, s_guestHudTime);
    return status;
}


struct SplitGuestHudWidget
{
    bool text = false;
    int ownerDraw = 0, flags = 0;
    rectDef_s parent{}, rect{};
    float color[4]{};
    Material *material = nullptr;
    Font_s *font = nullptr;
    float scale = 0, textX = 0, textY = 0;
    int align = 0, textStyle = 0, textAlignMode = 0;
};
static SplitGuestHudWidget s_guestWidgets[96];
static int s_guestWidgetCount = 0;

void CG_SplitGuestCaptureHudWidget(int localClientNum, bool text, int ownerDraw,
    const rectDef_s &parent, const rectDef_s &rect, int flags, const float *color,
    Material *material, Font_s *font, float scale, float textX, float textY,
    int align, int textStyle, int textAlignMode)
{
    if (localClientNum != 0 || !CL_IsSplitGuest(1))
        return;
    // Only native gameplay HUD widgets whose draw functions read player state.
    // Do not replay scoreboard, menus, credits or clientscript lifecycle widgets.
    bool include = text ? ownerDraw == 5 || ownerDraw == 20 || ownerDraw == 72
        || ownerDraw == 81 || ownerDraw == 82 || (ownerDraw >= 105 && ownerDraw <= 108)
        || (ownerDraw >= 120 && ownerDraw <= 123) || ownerDraw == 130
        : ownerDraw == 6 || ownerDraw == 103 || ownerDraw == 104
        || ownerDraw == 109 || ownerDraw == 110 || ownerDraw == 112 || (ownerDraw >= 116 && ownerDraw <= 119)
        || ownerDraw == 128 || ownerDraw == 129 || ownerDraw == 200 || ownerDraw == 288;
#ifndef KISAK_SP
    include |= text ? ownerDraw == 144 || ownerDraw == 145
        : ownerDraw == 150 || ownerDraw == 151 || ownerDraw == 152 || ownerDraw == 158
        || ownerDraw == 159 || ownerDraw == 170 || ownerDraw == 317 || ownerDraw == 331;
#endif
    if (!include)
        return;
    int index = 0;
    while (index < s_guestWidgetCount && (s_guestWidgets[index].text != text
        || s_guestWidgets[index].ownerDraw != ownerDraw))
        ++index;
    if (index == s_guestWidgetCount)
    {
        if (s_guestWidgetCount == 96)
            return;
        ++s_guestWidgetCount;
    }
    SplitGuestHudWidget &item = s_guestWidgets[index];
    item.text = text; item.ownerDraw = ownerDraw; item.flags = flags;
    item.parent = parent; item.rect = rect;
    std::memcpy(item.color, color, sizeof(item.color));
    item.material = material; item.font = font; item.scale = scale;
    item.textX = textX; item.textY = textY; item.align = align;
    item.textStyle = textStyle; item.textAlignMode = textAlignMode;
}

static void CG_SplitGuestNativeWidgets()
{
    // Native ammo drawing publishes widths to global layout dvars. Restore the
    // host values after the guest pass so its later UI layout is unaffected.
    const int hostClipWidth = ui_right_ammo_width->current.integer;
    const int hostStockWidth = ui_ammo_stock_width->current.integer;
    for (int i = 0; i < s_guestWidgetCount; ++i)
    {
        const SplitGuestHudWidget &item = s_guestWidgets[i];
        const rectDef_s &r = item.rect;
        float color[4]; std::memcpy(color, item.color, sizeof(color));
        if (item.text)
            CG_OwnerDrawText(1, item.parent, r.x, r.y, r.w, r.h, r.horzAlign, r.vertAlign,
                item.textX, item.textY, item.ownerDraw, item.flags, item.align,
                item.font, item.scale, color, item.material, item.textStyle,
                item.textAlignMode, nullptr, nullptr);
        else
            CG_OwnerDraw(1, item.parent, r.x, r.y, r.w, r.h, r.horzAlign, r.vertAlign,
                item.ownerDraw, item.flags, color, item.material);
    }
    Dvar_SetIntIfChanged((dvar_s *)ui_right_ammo_width, hostClipWidth);
    Dvar_SetIntIfChanged((dvar_s *)ui_ammo_stock_width, hostStockWidth);
}

void CG_SplitGuestResetHud()
{
    s_guestHud = SplitGuestHudState{};
    s_guestHudFrames = s_guestHudPaints = s_guestMenuPaints = 0;
    s_guestDamageCount = s_guestFeedbackUntil = s_guestHudTime = 0;
    s_guestWidgetCount = 0;
    CG_SplitGuestResetMpHud();
#ifdef KISAK_SP
    CG_SplitGuestResetNames();
#endif
    cg_s *game = CG_GetLocalClientGlobals(1);
    if (game)
    {
        game->v_dmg_time = 0;
        game->damageTime = 0;
        CG_ResetLowHealthOverlay(game);
        std::memset(game->viewDamage, 0, sizeof(game->viewDamage));
    }
}

static void CG_SplitGuestUpdateDamage(cg_s *game, const playerState_s *ps)
{
    const bool reset = !s_guestHud.valid || s_guestHud.teleport != (ps->eFlags & 2)
        || (ps->pm_type == 0 && s_guestHud.mode != 0 && s_guestHud.mode != 6 && s_guestHud.mode != 7);
    if (reset)
    {
        game->v_dmg_time = 0;
        CG_ResetLowHealthOverlay(game);
        std::memset(game->viewDamage, 0, sizeof(game->viewDamage));
    }
    else if (ps->damageEvent != s_guestHud.damageEvent && ps->damageCount > 0)
        CG_DamageFeedback(1, ps->damageYaw, ps->damagePitch, ps->damageCount);
    s_guestHud.valid = true;
    s_guestHud.damageEvent = ps->damageEvent;
    s_guestHud.mode = ps->pm_type;
    s_guestHud.teleport = ps->eFlags & 2;
}

bool CG_SplitGuestDrawHud(const refdef_s *view)
{
    ++s_guestHudFrames;
    cg_s *game = CG_GetLocalClientGlobals(1);
    // A viewmodel can be absent (last stand, no weapon, hidden gun). HUD state
    // must not depend on the viewmodel helper having supplied these snapshots.
    static snapshot_s snapshot;
    snapshot.ps = clients[1].snap.ps;
    snapshot.serverTime = clients[1].snap.serverTime;
    game->snap = game->nextSnap = &snapshot;
    game->localClientNum = 1;
    game->clientNum = snapshot.ps.clientNum;
    game->predictedPlayerState = snapshot.ps;
    CL_SplitGuestGetViewAngles(game->predictedPlayerState.viewangles);
    game->time = view->time;
#ifdef KISAK_SP
    game->hudTime = view->time;
#endif
    game->drawHud = true;
    game->refdef = *view;
    CG_SplitGuestUpdateMpHud(&snapshot);
    std::memcpy(game->refdefViewAngles, game->predictedPlayerState.viewangles, sizeof(game->refdefViewAngles));
    game->gunPitch = game->refdefViewAngles[0];
    game->gunYaw = game->refdefViewAngles[1];
    game->playerEntity.bPositionToADS = snapshot.ps.fWeaponPosFrac >= s_guestHud.ads;
    s_guestHud.ads = snapshot.ps.fWeaponPosFrac;
    const unsigned int weapon = BG_GetViewmodelWeaponIndex(&snapshot.ps);
    const int clip = weapon ? BG_GetAmmoInClip(&snapshot.ps, weapon) : -1;
    const int reserve = weapon ? BG_GetTotalAmmoReserve(&snapshot.ps, weapon) : -1;
    if (!s_guestHud.valid || weapon != s_guestHud.weapon || clip != s_guestHud.clip
        || reserve != s_guestHud.reserve)
    {
        game->ammoFadeTime = game->weaponSelectTime = game->time;
        s_guestHud.weapon = weapon; s_guestHud.clip = clip; s_guestHud.reserve = reserve;
    }
    if (snapshot.ps.weapon)
        game->weaponSelect = snapshot.ps.weapon;
    if (!s_guestHud.valid || snapshot.ps.eFlags != s_guestHud.flags)
        game->stanceFadeTime = game->time;
    s_guestHud.flags = snapshot.ps.eFlags;
    CG_SplitGuestUpdateDamage(game, &snapshot.ps);
    s_guestDamageCount = snapshot.ps.damageCount;
    s_guestFeedbackUntil = game->v_dmg_time;
    s_guestHudTime = game->time;
    bool drawHud = Dvar_GetBool("cg_draw2D");
#ifdef KISAK_SP
    drawHud = drawHud && Dvar_GetBool("cg_drawHUD");
#else
    // Both cg_drawHUD and hud_drawHUD are registered only in SP. MP's
    // native HUD functions use hud_enable (CG_AntiBurnInHUD_RegisterDvars).
    drawHud = drawHud && Dvar_GetBool("hud_enable");
#endif
    if (!drawHud && !CL_SplitGuestOwnsMenu())
        return false;
    R_BeginClientCmdList2D();
    R_AddCmdProjectionSet2D();
    R_AddCmdSetScissorValues(true, view->x, view->y, view->width, view->height);
    // Both script HUD layers come from this connection's replicated ps.hud.
    // Do not run a second Menu_PaintAll/CG_Draw2DInternal/clientscript lifecycle.
    if (drawHud)
    {
        ++s_guestHudPaints;
        CG_Draw2dHudElems(1, 0);
        CG_SplitGuestNativeWidgets();
        CG_SplitGuestDrawMpHudText();
        if (snapshot.ps.pm_type != 4 && snapshot.ps.pm_type != 5
            && !(snapshot.ps.otherFlags & 2))
            CG_DrawCrosshair(1);
        CG_Draw2dHudElems(1, 1);
        CG_DrawFlashDamage(game);
        CG_DrawDamageDirectionIndicators(1);
#ifdef KISAK_SP
        CG_DrawReviveIndicators_SP(1);
#endif
    }
    if (CL_SplitGuestOwnsMenu()) ++s_guestMenuPaints;
    CL_SplitGuestDrawMenu();
    R_AddCmdSetScissorValues(false, 0, 0, 0, 0);
    R_AddCmdEndOfList();
    return true;
}
