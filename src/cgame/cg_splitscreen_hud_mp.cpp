#include "cg_splitscreen_hud.h"
#include <client/splitscreen.h>
#ifndef KISAK_SP
#include <client/client.h>
#include <client_mp/cl_main_mp.h>
#include <cgame_mp/cg_local_mp.h>
#include <cgame_mp/cg_main_mp.h>
#include <cgame_mp/cg_compassfriendlies_mp.h>
#include <ui/ui_utils.h>
#include <universal/com_expressions_eval.h>
#include <cstring>

// Replay only the native HUD's text draw, using private item/text copies.
// This covers score/round/time expressions without running a second menu,
// event script, UI initialization or clientscript lifecycle.
static itemDef_s *s_guestText[128];
static int s_guestTextCount;
extern UiContext cgDC[];
void CG_SplitGuestCaptureHudText(int localClientNum, UiContext *dc, itemDef_s *item)
{
    if (localClientNum || dc != &cgDC[0] || !CL_IsSplitGuest(1) || !item || item->type != 1)
        return;
    const textDef_s *text = Item_GetTextDef(item);
    if (!text || (text->itemFlags & 2)) return;
    for (int i = 0; i < s_guestTextCount; ++i)
        if (s_guestText[i] == item) return;
    if (s_guestTextCount < 128) s_guestText[s_guestTextCount++] = item;
}

void CG_SplitGuestResetMpHud()
{
    s_guestTextCount = 0;
    CG_ResetCompassData(1, 0);
}

void CG_SplitGuestUpdateMpHud(snapshot_s *snapshot)
{
    cg_s *game = CG_GetLocalClientGlobals(1);
    const cg_s *world = CG_GetLocalClientGlobals(0);
    // Map calibration and score/team metadata belong to the shared loaded map.
    // Keep private animation/DObj and camera state out of these copies.
    game->matchUIVisibilityFlags = world->matchUIVisibilityFlags;
    game->numScores = world->numScores;
    game->scoreLimit = world->scoreLimit;
    std::memcpy(game->teamScores, world->teamScores, sizeof(game->teamScores));
    std::memcpy(game->teamPings, world->teamPings, sizeof(game->teamPings));
    std::memcpy(game->teamPlayers, world->teamPlayers, sizeof(game->teamPlayers));
    std::memcpy(game->scoreOrder, world->scoreOrder, sizeof(game->scoreOrder));
    std::memcpy(game->bombTimer, world->bombTimer, sizeof(game->bombTimer));
    game->compassNorthYaw = world->compassNorthYaw;
    std::memcpy(game->compassNorth, world->compassNorth, sizeof(game->compassNorth));
    game->compassMapMaterial = world->compassMapMaterial;
    std::memcpy(game->compassMapUpperLeft, world->compassMapUpperLeft, sizeof(game->compassMapUpperLeft));
    std::memcpy(game->compassMapWorldSize, world->compassMapWorldSize, sizeof(game->compassMapWorldSize));
    game->compassFadeTime = game->time;
    game->offhandFadeTime = game->time;
    for (int i = 0; i < 32; ++i)
    {
        auto &to = game->bgs.clientinfo[i];
        const auto &from = world->bgs.clientinfo[i];
        to.infoValid = from.infoValid;
        to.team = from.team;
        to.oldteam = from.oldteam;
        to.score = from.score;
        to.needsRevive = from.needsRevive;
        std::memcpy(to.perks, from.perks, sizeof(to.perks));
    }
    cgs_t *statics = CG_GetLocalClientStaticGlobals(1);
    const cgs_t *shared = CG_GetLocalClientStaticGlobals(0);
    std::memcpy(statics->gametype, shared->gametype, sizeof(statics->gametype));
    statics->maxclients = shared->maxclients;
    statics->gameEndTime = shared->gameEndTime;
    // Feed the compass only entities actually in this connection's snapshot.
    // Its player state already contains P2's own radar/ping information.
    snapshot->numEntities = clients[1].snap.numEntities;
    if (snapshot->numEntities > static_cast<int>(sizeof(snapshot->entities) / sizeof(snapshot->entities[0])))
        snapshot->numEntities = sizeof(snapshot->entities) / sizeof(snapshot->entities[0]);
    for (int i = 0; i < snapshot->numEntities; ++i)
        snapshot->entities[i] = clients[1].parseEntities[(clients[1].snap.parseEntitiesNum + i) & 2047];
    if (game->compassMapWorldSize[0] > 0 && game->compassMapWorldSize[1] > 0)
        CG_CompassUpdateActors(1);
}

void CG_SplitGuestDrawMpHudText()
{
    const ScreenPlacement saved = scrPlaceView[0];
    scrPlaceView[0] = scrPlaceView[1];
    for (int i = 0; i < s_guestTextCount; ++i)
    {
        itemDef_s item = *s_guestText[i];
        textDef_s text = *Item_GetTextDef(&item);
        item.typeData.textDef = &text;
        if (item.forecolorAExp.filename)
            item.window.foreColor[3] = static_cast<float>(GetExpressionFloat(1, &item, &item.forecolorAExp));
        Item_Text_Paint(1, &cgDC[0], &item);
    }
    scrPlaceView[0] = saved;
}
#else
void CG_SplitGuestCaptureHudText(int, UiContext *, itemDef_s *) {}
void CG_SplitGuestResetMpHud() {}
void CG_SplitGuestUpdateMpHud(snapshot_s *) {}
void CG_SplitGuestDrawMpHudText() {}
#endif
