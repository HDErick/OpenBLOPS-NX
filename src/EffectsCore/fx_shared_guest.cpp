#include "fx_shared_guest.h"
#include "fx_system.h"
#include "fx_update.h"
#include "fx_draw.h"
#include "fx_dvars.h"
#include "fx_sort.h"
#include "fx_archive.h"
#include "fx_unique_handle.h"
#include "fx_sprite.h"
#include "fx_beam.h"
#include "fx_postlight.h"
#include <client/client.h>
#include <cgame_mp/cg_local_mp.h>
#include <cgame_mp/cg_main_mp.h>
#include <cgame_mp/cg_ents_mp.h>
#include <cgame/cg_weapons.h>
#include <cgame/cg_local.h>
#include <bgame/bg_misc.h>
#include <gfx_d3d/r_drawsurf.h>
#include <clientscript/scr_const.h>
#include <cstring>

// The simulation remains host-owned. This flag is thread-local because the
// primary effect workers and this synchronous lower-view draw use different
// threads. Private muzzle ownership is modified only after their worker wait.
static thread_local bool s_drawingGuestFx;
struct GuestMuzzleHandle { unsigned short owner = 0xFFFF; unsigned int unique = 0; };
static GuestMuzzleHandle s_guestMuzzles[64];
static unsigned int s_nextGuestMuzzle;
static bool s_haveGuestFireSequence;
static unsigned short s_guestFireSequence;

bool FX_SplitGuestIsDrawing()
{
    return s_drawingGuestFx;
}

bool FX_SplitGuestEffectVisible(const FxSystem *system, const FxEffect *effect)
{
    if (system->localClientNum != 0)
        return true;
    // The upper gun's bolted flash belongs only to the upper view.
    if (s_drawingGuestFx && effect->boltAndSortOrder.dobjHandle == 1536)
        return false;
    for (const GuestMuzzleHandle &entry : s_guestMuzzles)
    {
        if (entry.unique && effect->owner == entry.owner)
        {
            const FxEffectContainer *root = UniqueHandleToEffect(const_cast<FxSystem *>(system), entry.unique);
            if (root && root->effect.uniqueHandle == entry.unique)
                return s_drawingGuestFx;
        }
    }
    return true;
}

void FX_SplitGuestReset()
{
    // Shared shutdown may already have freed the pools. Kill only our live
    // generation-checked roots when the host FX system is still initialized.
    FxSystemContainer *container = FX_GetSystem(0);
    for (GuestMuzzleHandle &entry : s_guestMuzzles)
    {
        if (container && container->system.isInitialized && entry.unique)
            if (FxEffectContainer *effect = UniqueHandleToEffect(&container->system, entry.unique))
                FX_KillEffect(&container->system, effect);
        entry = GuestMuzzleHandle{};
    }
    s_nextGuestMuzzle = 0;
    s_haveGuestFireSequence = false;
}

void FX_SplitGuestMuzzleFlash(const refdef_s *view)
{
    const playerState_s &ps = clients[1].snap.ps;
    const unsigned short sequence = static_cast<unsigned short>(ps.predictableEventSequence);
    if (!s_haveGuestFireSequence)
    {
        s_haveGuestFireSequence = true;
        s_guestFireSequence = sequence;
        return;
    }
    unsigned short count = static_cast<unsigned short>(sequence - s_guestFireSequence);
    if (count > 4) count = 4;
    s_guestFireSequence = sequence;
    if (!count)
        return;
    cg_s *game = CG_GetLocalClientGlobals(1);
    const ViewModelInfo *model = CG_GetLocalClientViewModelInfo(1);
    const int weapon = BG_GetViewmodelWeaponIndex(&game->predictedPlayerState);
    if (!model->viewModelDObj || weapon <= 0 || weapon >= BG_GetNumWeapons())
        return;
    const WeaponDef *definition = BG_GetWeaponDef(weapon);
    if (!definition->viewFlashEffect)
        return;
    float axis[3][3], origin[3];
    if (!CG_DObjGetWorldTagMatrix(&game->viewModelPose, model->viewModelDObj,
        scr_const.tag_flash, axis, origin))
        return;
    FxSystem *system = &FX_GetSystem(0)->system;
    for (unsigned short offset = count; offset; --offset)
    {
        const int event = ps.predictableEvents[(sequence - offset) & 3];
        if (event != EV_FIRE_WEAPON && event != EV_FIRE_WEAPON_LASTSHOT
            && event != EV_FIRE_WEAPON_LEFT && event != EV_FIRE_WEAPON_LASTSHOT_LEFT)
            continue;
        const unsigned int handle = FX_PlayOrientedEffect(system, definition->viewFlashEffect,
            view->time, origin, axis);
        FxEffectContainer *effect = UniqueHandleToEffect(system, handle);
        if (effect)
        {
            GuestMuzzleHandle &entry = s_guestMuzzles[s_nextGuestMuzzle++ % 64];
            if (entry.unique)
                if (FxEffectContainer *old = UniqueHandleToEffect(system, entry.unique))
                    FX_KillEffect(system, old);
            entry.owner = effect->effect.owner;
            entry.unique = handle;
        }
    }
}

void FX_DrawSharedGuest(const refdef_s *view)
{
    FxSystemContainer *container = FX_GetSystem(0);
    if (!container || !container->system.isInitialized || !fx_enable->current.enabled
        || !fx_draw->current.enabled)
        return;
    FxSystem *system = &container->system;
    const FxCamera savedCamera = system->camera;
    const FxSpriteInfo savedSprite = system->sprite;
    const unsigned int savedCloudCount = system->gfxCloudCount;
    FX_SetupCamera(&system->camera, view->vieworg, view->viewaxis,
        view->tanHalfFovX, view->tanHalfFovY, view->zFar);
    system->camera.isValid = 1;
    s_drawingGuestFx = true;
    FX_SortEffects(system);
    FxReflectParams reflect;
    FX_GetNullReflection(&reflect);
    // Fresh camera-facing geometry is appended to the frame arena; never reset
    // mesh counters or overwrite vertices referenced by the upper draw lists.
    FX_DrawNonSpriteElems(system, &reflect, 0);
    FxGenerateVertsCmd command{};
    FX_FillGenerateVertsCmd(0, &command);
    std::memcpy(command.vieworg, view->vieworg, sizeof(command.vieworg));
    std::memcpy(command.viewaxis, view->viewaxis, sizeof(command.viewaxis));
    R_BeginCodeMeshVerts();
    FX_SpriteGenerateVerts(&command);
    FX_Beam_GenerateVerts(&command);
    FX_PostLight_GenerateVerts(command.postLightInfo, system);
    FX_DrawSpriteElems(system, system->msecDraw, &reflect, 0);
    R_EndCodeMeshVerts();
    // Do not call FX_EndUpdate, FX_Update*, GlassCl_GenerateVerts, or
    // FX_ToggleVisBlockerFrame here: those advance shared simulation/state.
    s_drawingGuestFx = false;
    system->camera = savedCamera;
    system->sprite = savedSprite;
    system->gfxCloudCount = savedCloudCount;
}
