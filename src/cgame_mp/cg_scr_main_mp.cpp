#include <gfx_d3d/r_scene.h>
#include "cg_scr_main_mp.h"
#include <clientscript/cscr_variable.h>
#include <clientscript/cscr_vm.h>
#include <cgame/cg_camerashake.h>
#include <EffectsCore/fx_system.h>
#include <cgame/cg_scr_main.h>
#include <qcommon/dobj_management.h>
#include <clientscript/cscr_stringlist.h>
#include <cgame/cg_event.h>
#include <demo/demo_playback.h>
#include "cg_main_mp.h"
#include "cg_ents_mp.h"
#include <bgame/bg_perks.h>
#include "cg_vehicles_mp.h"
#include <bgame/bg_dog.h>
#include <bgame/bg_mantle.h>
#include <bgame/bg_weapons_ammo.h>
#include <client/splitscreen.h>
#include <cgame/cg_compass.h>
#include <xanim/dobj_utils.h>
#include <xanim/xmodel.h>
#include <client_mp/cl_cgame_mp.h>
#include <clientscript/scr_const.h>
#include "cg_animscripted_mp.h"
#include <universal/surfaceflags.h>
#include <gfx_d3d/r_fog.h>
#include <gfx_d3d/r_shader_constant_set.h>
#include "cg_servercmds_mp.h"
#ifdef KISAK_SP
// Only the SP-only builtin bodies at the bottom of the TODO(SP-STUB) block need
// these; guarded so the KISAK_MP translation unit is completely unchanged.
#include <universal/dvar.h>
#include <qcommon/cmd.h>
#include <qcommon/com_clients.h>
#include <win32/win_gamerprofile.h>
#include <client_mp/cl_input_mp.h>
#include "cg_predict_mp.h"
#include "cg_view_mp.h"
#include <bgame/bg_misc.h>
#ifdef KISAK_SP
#include <bgame/bg_pmove.h>
#endif
#include <bgame/bg_weapons_def.h>
#include <cgame/cg_spawn.h>
#include <cgame/cg_visionsets.h>
#include <DynEntity/DynEntity_client.h>
#include <DynEntity/DynEntity_load_obj.h>
#include <EffectsCore/fx_marks.h>
#include <qcommon/common.h>
#include <qcommon/cm_trace.h>
#include <stringed/stringed_hooks.h>
#include <universal/com_math_anglevectors.h>
#include <xanim/dobj.h>
#include <cmath>
#include <climits>
#endif

GfxFog cg_clientVolFog;

unsigned __int16 *footTags[4] =
{
    &scr_const.j_palm_ri,
    &scr_const.j_palm_le,
    &scr_const.j_ball_ri,
    &scr_const.j_palm_ri
};

#ifdef KISAK_SP
// ===========================================================================
// TODO(SP-STUB) -- deliberate no-op stubs for retail-SP CLIENT script builtins.
// The client-side twin of the TODO(SP-STUB) block in g_scr_main_mp.cpp; read
// that block's header for the full rationale. Short version:
//
// WHY THEY EXIST: builtin names are resolved at COMPILE time. An unresolved
// name is a fatal CompileError, so ONE missing builtin stops the entire SP
// client script set from loading -- and clientscripts/frontend.csc pulls in a
// 33-file closure (via #include and clientscripts\x:: references) that must all
// compile before the frontend map can run. These are placeholders, NOT
// implementations: every one does nothing but warn once, naming itself.
//
// HOW THIS LIST WAS DERIVED (mechanically, not by eye): SP's four client
// builtin tables were read straight out of BlackOps.exe --
//   client_functions          0x00B71DE0, 154 rows
//   client_project_functions  0x00B84988,  54 rows
//   client_methods            0x00A4E7B8, 114 rows
//   client_project_methods    0x00A606E8,  36 rows
// -- then intersected with every call token in the frontend closure and
// subtracted from this reconstruction's four tables. Result: 25 names over 100
// call sites, ALL of them in the two *project-specific* tables. The two shared
// tables need nothing: all 154 SP client_functions names are already present
// here, and client_methods differs by only two SP-only rows
// (setphysicsgravity 0x006507a0, clearphysicsgravity 0x0050c1f0) that the
// closure never calls -- deliberately not added, since adding an unused row is
// unevidenced churn. TODO(SP): add them if a script is ever seen to need them.
//
// RETURN SHAPES ARE NOT GUESSED. Every row that returns a value had its retail
// SP handler decompiled to see which Scr_Add* it calls, and its call sites read
// to see how the value is consumed. Undefined is fatal in three positions
// (`x.size`, `x[i]`, and Scr_CastBool), so the shape matters; the per-row
// comments record both facts. Rows with no value returned are rows whose retail
// handler pushes nothing AND whose every call site is in statement position.
//
// type IS 0 ON EVERY ROW, matching all 90 rows read from SP's two project
// tables (every one carried 0). 0 also means "not a developer command", which
// is the permissive choice that cannot reintroduce a CompileError.
//
// UPDATE -- 7 of these rows are no longer stubs. The seven builtins that
// actually fired on the frontend boot were reconstructed from their retail SP
// handlers (decompiled + disassembled in Ghidra this session) and now have real
// bodies: getlocalclienthealth, getlocalclientmaxhealth, isextracam,
// setextracamfov, stopextracam, setclientdvar, forcegamemodemappings. Their
// definitions are grouped at the END of this block, under "REAL SP BODIES", and
// their table rows below carry an "SP-IMPL" comment instead of "TODO(SP-STUB)".
// Everything else in this block is still a warn-once no-op.
// ===========================================================================

static void CScr_SPStub_ReportOnce(const char *name, const char *spHandler, const char *retDesc, bool *pReported)
{
    if ( *pReported )
        return;
    *pReported = true;
    Com_PrintWarning(
        24,
        "WARNING: TODO(SP-STUB) client script builtin '%s' (retail SP handler %s) was called "
        "but is an unimplemented stub: it does no work at all and evaluates to %s. "
        "This warning prints once per builtin name.\n",
        name,
        spHandler,
        retDesc);
}

// Pushes nothing -> evaluates to VAR_UNDEFINED. Safe ONLY where retail also
// pushes nothing, or where every call site is a statement / IsDefined-guarded.
#define SP_CSTUB_FUNCTION(symbol, gscName, spHandler)                              \
    static void __cdecl symbol()                                                   \
    {                                                                              \
        static bool s_reported = false;                                            \
        CScr_SPStub_ReportOnce(gscName, spHandler, "undefined", &s_reported);       \
    }

#define SP_CSTUB_METHOD(symbol, gscName, spHandler)                                \
    static void __cdecl symbol(scr_entref_t)                                       \
    {                                                                              \
        static bool s_reported = false;                                            \
        CScr_SPStub_ReportOnce(gscName, spHandler, "undefined", &s_reported);       \
    }

#define SP_CSTUB_FUNCTION_INT(symbol, gscName, spHandler, value)                   \
    static void __cdecl symbol()                                                   \
    {                                                                              \
        static bool s_reported = false;                                            \
        CScr_SPStub_ReportOnce(gscName, spHandler, "the fixed integer " #value,     \
                               &s_reported);                                       \
        Scr_AddInt((value), SCRIPTINSTANCE_CLIENT);                                \
    }

#define SP_CSTUB_METHOD_INT(symbol, gscName, spHandler, value)                     \
    static void __cdecl symbol(scr_entref_t)                                       \
    {                                                                              \
        static bool s_reported = false;                                            \
        CScr_SPStub_ReportOnce(gscName, spHandler, "the fixed integer " #value,     \
                               &s_reported);                                       \
        Scr_AddInt((value), SCRIPTINSTANCE_CLIENT);                                \
    }

#define SP_CSTUB_FUNCTION_FLOAT(symbol, gscName, spHandler, value)                 \
    static void __cdecl symbol()                                                   \
    {                                                                              \
        static bool s_reported = false;                                            \
        CScr_SPStub_ReportOnce(gscName, spHandler, "the fixed float " #value,       \
                               &s_reported);                                       \
        Scr_AddFloat((value), SCRIPTINSTANCE_CLIENT);                              \
    }

#define SP_CSTUB_METHOD_ZEROVEC(symbol, gscName, spHandler)                        \
    static void __cdecl symbol(scr_entref_t)                                       \
    {                                                                              \
        static bool s_reported = false;                                            \
        float zero[3] = { 0.0f, 0.0f, 0.0f };                                      \
        CScr_SPStub_ReportOnce(gscName, spHandler, "the zero vector (0,0,0)",       \
                               &s_reported);                                       \
        Scr_AddVector(zero, SCRIPTINSTANCE_CLIENT);                                \
    }

// --- FUNCTIONS: 12 names, appended to client_project_functions[] below -------
// (SP table client_project_functions @ 0x00B84988; row index given per name.)

static void CScr_PlayFXOnDynEnt_SP()
{
    const int fxId = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    const scr_entref_t entref = Scr_GetEntityRef(1, SCRIPTINSTANCE_CLIENT);
    if (entref.classnum != 4)
    {
        Scr_ParamError(1, "PlayFXOnDynEnt param isn't a DYN entity.", SCRIPTINSTANCE_CLIENT);
        return;
    }
    if (fxId <= 0 || fxId >= 196)
        Scr_Error(SCRIPTINSTANCE_CLIENT, va("CScr_PlayFX: invalid effect id %d", fxId), 0);
    cgs_t *cgs = CG_GetLocalClientStaticGlobals(0);
    Scr_AddInt(DynEntCl_PlayBoltedFX(cgs->fxs[fxId], entref.entnum), SCRIPTINSTANCE_CLIENT);
}

static void CScr_GetWaterHeight_SP()
{
    float pos[3];
    Scr_GetVector(0, pos, SCRIPTINSTANCE_CLIENT);
    const float height = CM_GetWaterHeight(pos, 200.0f, -200.0f);
    Scr_AddFloat(height, SCRIPTINSTANCE_CLIENT);
}

static int CScr_VisionTransitionMsec_SP(unsigned int index)
{
    const float milliseconds = (float)(Scr_GetFloat(index, SCRIPTINSTANCE_CLIENT) * 1000.0f);
    const double rounded = std::nearbyint((double)milliseconds + 9.313225746154785e-10);
    return rounded >= -2147483648.0 && rounded < 2147483648.0 ? (int)rounded : INT_MIN;
}

static void CScr_VisionSetUnderwater_SP()
{
    int duration = 1000;
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if (count != 2 && count != 3)
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "visionsetunderwater() called with wrong params.\n", false);
        return;
    }
    if (count == 3)
        duration = CScr_VisionTransitionMsec_SP(2);
    const int localClientNum = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    if (localClientNum < 0 || localClientNum >= 1)
        Scr_Error(SCRIPTINSTANCE_CLIENT, va("Trying to get a local client index for a client '%d' that is not a local client.", localClientNum), false);
    const char *name = Scr_GetString(1, SCRIPTINSTANCE_CLIENT);
    if (CG_VisionSetStartLerp_To(localClientNum, VISIONSETMODE_UNDERWATER, VISIONSETLERP_TO_SMOOTH, name, duration))
        I_strncpyz(CG_GetLocalClientGlobals(localClientNum)->visionNameUnderwater, name, 64);
}

static void CScr_VisionSetDamage_SP()
{
    const int localClientNum = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    if (localClientNum < 0 || localClientNum >= 1)
        Scr_Error(SCRIPTINSTANCE_CLIENT, va("Trying to get a local client index for a client '%d' that is not a local client.", localClientNum), false);
    cg_s *cg = CG_GetLocalClientGlobals(localClientNum);
    const int enabled = Scr_GetInt(1, SCRIPTINSTANCE_CLIENT);
    const char *name = Scr_GetString(2, SCRIPTINSTANCE_CLIENT);
    int duration = 1000;
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if (count != 3 && count != 4)
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "VisionSetDamage() called with wrong params.\n", false);
        return;
    }
    if (count == 4)
        duration = CScr_VisionTransitionMsec_SP(3);
    const char *target;
    if (enabled)
    {
        cg->damageVisionEndTime = INT_MAX;
        I_strncpyz(cg->visionNameLastStand, name, 64);
        target = cg->visionNameLowHealth;
    }
    else
    {
        const unsigned int endBits = (unsigned int)cg->time + (unsigned int)duration;
        static_assert(sizeof(endBits) == sizeof(cg->damageVisionEndTime), "32-bit damage time");
        memcpy(&cg->damageVisionEndTime, &endBits, sizeof(endBits));
        I_strncpyz(cg->visionNameLastStand, cg->visionNameNaked, 64);
        target = cg->visionNameNaked;
    }
    CG_VisionSetStartLerp_To(cg->localClientNum, VISIONSETMODE_DEATH, VISIONSETLERP_TO_SMOOTH, target, duration);
}

SP_CSTUB_FUNCTION(CScr_SPStubFn_updatedvarsfromprofile, "updatedvarsfromprofile", "0x00894c80")
    // NO RETURN VALUE: 1 statement site, _utility:1489. SP row 1.
// REAL BODY: retail 0x00894140 is the client twin of Scr_TriggerFX -- it stamps
// nextState.time2 (+0x1dc) on an ET_FX centity with the given time or cg time.
static int CScr_FXTimeMsec_SP(float seconds)
{
    const float milliseconds = seconds * 1000.0f;
    const double rounded = std::nearbyint((double)milliseconds + 9.313225746154785e-10);
    return rounded >= -2147483648.0 && rounded < 2147483648.0 ? (int)rounded : INT_MIN;
}

static void CScr_TriggerFX_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) < 1 || Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) > 2)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "Incorrect number of parameters", 0);
    const scr_entref_t entref = Scr_GetEntityRef(0, SCRIPTINSTANCE_CLIENT);
    centity_s *cent = CG_GetEntity(entref.client, entref.entnum);
    if (cent->nextState.eType != ET_FX)
        Scr_ParamError(0, "entity wasn't created with 'newFx'", SCRIPTINSTANCE_CLIENT);
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 2)
        cent->nextState.time2 = CScr_FXTimeMsec_SP(Scr_GetFloat(1, SCRIPTINSTANCE_CLIENT));
    else
        cent->nextState.time2 = CG_GetLocalClientGlobals(0)->time;
}

static centity_s *CScr_SpawnFXEntity_SP(int localClientNum, int fxId, const float *angles, const float *position)
{
    float axis[3][3];
    AnglesToAxis(angles, axis);
    centity_s *cent = CG_Spawn(localClientNum);
    if (cent)
    {
        cent->nextState.un1.scale = fxId;
        cent->nextState.eType = ET_FX;
        Vec3Copy(position, cent->pose.origin);
        Vec3Copy(angles, cent->nextState.lerp.apos.trBase);
    }
    return cent;
}
    // NO RETURN VALUE: 1 live statement site, _fx:251 (two more are commented out). SP row 18.
static void __cdecl CScr_SetWaterFog_SP()
{
    const unsigned int count = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if (count != 7 && count != 17)
        Scr_Error(SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters\nUSAGE: SetWaterFog(<startDist>, <halfwayDist>, <halfwayHeight>, <baseHeight>, <red>, <green>, <blue>)\n", false);

    const float start = Scr_GetFloat(0, SCRIPTINSTANCE_CLIENT);
    const float halfwayDist = Scr_GetFloat(1, SCRIPTINSTANCE_CLIENT);
    if (halfwayDist <= 0.0f)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "SetWaterFog: halfwayDist must be greater than 0", false);
    const float halfwayHeight = Scr_GetFloat(2, SCRIPTINSTANCE_CLIENT);
    if (halfwayHeight < 0.0f)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "SetWaterFog: halfwayHeight must be greater or equal to 0", false);
    const float baseHeight = Scr_GetFloat(3, SCRIPTINSTANCE_CLIENT);
    const float density = 1.0f / halfwayDist;
    const float heightDensity = halfwayHeight >= 1.0f ? 1.0f / halfwayHeight : 0.0f;
    const float red = Scr_GetFloat(4, SCRIPTINSTANCE_CLIENT);
    const float green = Scr_GetFloat(5, SCRIPTINSTANCE_CLIENT);
    const float blue = Scr_GetFloat(6, SCRIPTINSTANCE_CLIENT);

    WaterFogDef *fog = &CG_GetLocalClientGlobals(0)->refdef.waterFog;
    fog->startTime = 0;
    fog->finishTime = 0;
    fog->color[0] = red;
    fog->color[1] = green;
    fog->color[2] = blue;
    fog->color[3] = 1.0f;
    fog->fogStart = start;
    fog->density = density;
    fog->heightDensity = heightDensity;
    fog->baseHeight = baseHeight;
    if (count == 7)
    {
        Com_Printf(1, "setWaterFog: Old syntax used. Please update script.\n");
        fog->sunFogStartAng = 0.0f;
        fog->sunFogEndAng = 0.0f;
        fog->sunFogColor[0] = 1.0f;
        fog->sunFogColor[1] = 1.0f;
        fog->sunFogColor[2] = 1.0f;
        fog->sunFogColor[3] = 1.0f;
        fog->sunFogDir[0] = 1.0f;
        fog->sunFogDir[1] = 0.0f;
        fog->sunFogDir[2] = 0.0f;
        return;
    }

    fog->color[3] = Scr_GetFloat(7, SCRIPTINSTANCE_CLIENT);
    const float maxOpacity = Scr_GetFloat(16, SCRIPTINSTANCE_CLIENT);
    const float sunBlue = Scr_GetFloat(10, SCRIPTINSTANCE_CLIENT);
    const float sunGreen = Scr_GetFloat(9, SCRIPTINSTANCE_CLIENT);
    fog->sunFogColor[0] = Scr_GetFloat(8, SCRIPTINSTANCE_CLIENT);
    fog->sunFogColor[1] = sunGreen;
    fog->sunFogColor[2] = sunBlue;
    fog->sunFogColor[3] = maxOpacity;
    const float sunZ = Scr_GetFloat(13, SCRIPTINSTANCE_CLIENT);
    const float sunY = Scr_GetFloat(12, SCRIPTINSTANCE_CLIENT);
    fog->sunFogDir[0] = Scr_GetFloat(11, SCRIPTINSTANCE_CLIENT);
    fog->sunFogDir[1] = sunY;
    fog->sunFogDir[2] = sunZ;
    fog->sunFogStartAng = Scr_GetFloat(14, SCRIPTINSTANCE_CLIENT);
    fog->sunFogEndAng = Scr_GetFloat(15, SCRIPTINSTANCE_CLIENT);
}

// --- METHODS: 13 names, appended to client_project_methods[] below ----------
// (SP table client_project_methods @ 0x00A606E8; row index given per name.)

static void CScr_GetTagForwardVector_SP(scr_entref_t entref);
static void __cdecl CScr_Swimming_SP(scr_entref_t)
{
    cg_s *cg = CG_GetLocalClientGlobals(0);
    const float waterHeight = CM_GetWaterHeight(cg->predictedPlayerState.origin, 200.0f, -200.0f);
    Scr_AddInt(PM_GetWaterLevel(&cg->predictedPlayerState, waterHeight) >= 3, SCRIPTINSTANCE_CLIENT);
}
// REAL BODY: retail 0x008964d0 resolves the entref (real or fake centity) and
// pushes pose.origin (+0x24). The comment block below predates this.
static void __cdecl CScr_SPStubMeth_getorigin(scr_entref_t entref)
{
    centity_s *cent = CG_GetEntity(entref.client, entref.entnum);
    Scr_AddVector(cent->pose.origin, SCRIPTINSTANCE_CLIENT);
}
    // RETURNS ZERO VECTOR: retail handler 0x008964d0 reads the entity's origin (+0x24..+0x2c)
    //   and pushes Scr_AddVector. Consumed by arithmetic at _utility:1808
    //   `Spawn(..., self GetOrigin() + ( 0, 0, -1000 ), "script_model")`, so it must be a
    //   vector. (0,0,0) is a wrong ANSWER for a real entity -- flagged -- but is the only
    //   shape-correct value available to a stub. SP row 3.
static void __cdecl CScr_GetLinkedEnt_SP(scr_entref_t entref)
{
    const centity_s *cent = CG_GetEntity(entref.client, entref.entnum);
    const int parentNum = CG_GetEntityLinkParent_SP(entref.client, cent);
    if (parentNum != ENTITYNUM_NONE)
    {
        const centity_s *parent = CG_GetEntity(entref.client, parentNum);
        Scr_AddEntityNum(parent->nextState.number, 0, SCRIPTINSTANCE_CLIENT, entref.client);
    }
}
static void __cdecl CScr_GetEye_SP(scr_entref_t)
{
    const cg_s *cg = CG_GetLocalClientGlobals(0);
    float eye[3];
    Vec3Copy(cg->refdef.vieworg, eye);
    const float waterHeight = static_cast<float>(CM_GetWaterHeight(eye, 200.0f, -200.0f));
    const float clip = cg_cameraWaterClip->current.value;
    const float delta = eye[2] - waterHeight;
    if (clip > I_fabs(delta))
        eye[2] = delta > 0.0f ? clip + waterHeight : waterHeight - clip;
    Scr_AddVector(eye, SCRIPTINSTANCE_CLIENT);
}
static void CScr_GetPlayerAngles_SP(scr_entref_t)
{
    Scr_AddVector(CG_GetLocalClientGlobals(0)->refdefViewAngles, SCRIPTINSTANCE_CLIENT);
}
static void CScr_LinkToCamera_SP(scr_entref_t entref)
{
    cg_s *cgameGlob = CG_GetLocalClientGlobals(0);
    int control = FULL_CTRL;
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 1)
        control = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    if (cgameGlob->cameraLinkedEntitiesCount < 4)
    {
        cgameGlob->cameraLinkedEntities[cgameGlob->cameraLinkedEntitiesCount] = entref.entnum;
        cgameGlob->cameraLinkedEntitiesType[cgameGlob->cameraLinkedEntitiesCount] = static_cast<link_type_e>(control);
        ++cgameGlob->cameraLinkedEntitiesCount;
    }
    else
    {
        Com_PrintWarning(14, "WARNING: Max camera linked entities reached. You can link up to %d entities to the camera.\n", 4);
    }
}
static void CScr_UnlinkFromCamera_SP(scr_entref_t entref)
{
    cg_s *cgameGlob = CG_GetLocalClientGlobals(0);
    for (int index = 0; index < cgameGlob->cameraLinkedEntitiesCount; ++index)
    {
        if (cgameGlob->cameraLinkedEntities[index] == entref.entnum)
        {
            --cgameGlob->cameraLinkedEntitiesCount;
            cgameGlob->cameraLinkedEntities[index] = cgameGlob->cameraLinkedEntities[cgameGlob->cameraLinkedEntitiesCount];
            --index;
        }
    }
}
static void CScr_UnlinkAllFromCamera_SP(scr_entref_t entref)
{
    CG_GetLocalClientGlobals(0)->cameraLinkedEntitiesCount = 0;
}
static void CScr_GetNormalizedMovement_SP(scr_entref_t entref)
{
    const usercmd_s *cmd = &cg_pmove[entref.client].cmd;
    float movement[3] = {
        (float)(signed char)cmd->forwardmove * (1.0f / 127.0f),
        (float)(signed char)cmd->rightmove * (1.0f / 127.0f),
        0.0f
    };
    Scr_AddVector(movement, SCRIPTINSTANCE_CLIENT);
}

static void CScr_GetNormalizedCameraMovement_SP(scr_entref_t entref)
{
    const usercmd_s *cmd = &cg_pmove[entref.client].cmd;
    float movement[3] = {
        (float)(signed char)cmd->pitchmove * (-1.0f / 127.0f),
        (float)(signed char)cmd->yawmove * (-1.0f / 127.0f),
        0.0f
    };
    Scr_AddVector(movement, SCRIPTINSTANCE_CLIENT);
}

// ===========================================================================
// REAL SP BODIES -- the seven names above that actually fired on the frontend
// boot and are no longer stubs.
//
// EVIDENCE STANDARD: every body below was reconstructed from its retail SP
// handler in Ghidra program /BlackOps.exe (image base 0x00400000), decompiled
// AND disassembled this session; the per-body comments cite the exact
// instruction addresses the behaviour was read from. Nothing here is guessed.
//
// TWO SHARED FACTS, established once and used by several bodies:
//
// (1) 0x005336b0 == CG_GetPredictedPlayerState. Its whole body is
//     `return cgameGlob + 0x8a364;`. That base is proven to be the predicted
//     player state, not assumed: 0x004b7d30 passes cg+0x8a364 straight into
//     CG_GetPlayerWeapon(const playerState_s *, int) (0x004b7d5a), and reads
//     cg+0x8a448 -- exactly base+0xE4 -- as a bitfield tested with bit 18,
//     which is playerState_s::eFlags2 & 0x40000 (CG_RenderPlayerFromMissilePOV).
//     0xE4 is precisely eFlags2's offset in THIS tree's playerState_s
//     (bg_local.h:596), so the SP and reconstruction layouts agree here.
//     The Ghidra plate on 0x005336b0 now records the corrected adjudication and
//     these same two body-level facts. Its missing localClientNum use is the
//     expected SP MAX_LOCAL_CLIENTS == 1 folding.
//
// (2) cg+0xa46cc / cg+0xa46d0 == cg_s::cameraData.extraCamEntNum /
//     cg_s::cameraData.extraCamFov -- pinned by exact offset arithmetic, not by
//     name similarity. 0x00454210 (SP's CG_CalcExtraCamViewValues) writes the
//     extra cam entity's origin to cg+0x8c120..0x8c128 and its angles to
//     cg+0xa4620..0xa4628, so cg+0xa4620 is refdefViewAngles[3]. In cg_local_mp.h
//     `Camera cameraData` immediately follows `float refdefViewAngles[3]`
//     (:550-551), so cameraData starts at 0xa462C; within Camera (sizeof 0xAC),
//     extraCamEntNum is at +0xA0 and extraCamFov at +0xA4 -- landing on 0xa46CC
//     and 0xa46D0 exactly, both fields, first try.
//     This is deliberately NOT cg_s::extraCamEntity: that MP field is SP's
//     cg+0xce4d8, a different slot, used together with a weapon-def flag in
//     0x00568830 / 0x004b7d30 -- the same equipment/camera-spike test this tree
//     spells `extraCamEntity == 1023` in cg_ammocounter.cpp:914. Writing that
//     field here would corrupt the ammo-counter logic.
//     1023 (0x3FF) is the "no extra cam" sentinel in both trees.
//
// The SP camera owners now consume both fields in cg_camera.cpp and initialise
// extraCamEntNum to 1023 in CG_MapRestart, matching the retail paths above.
// ===========================================================================

// getlocalclienthealth -- retail SP handler 0x00894d80, client_project_functions
// row 3. Disassembly:
//   00894d83  PUSH 0 / CALL Scr_GetInt, then the standard
//             "Trying to get a local client index for a client '%d' that is not
//             a local client." range check -- i.e. CScr_GetLocalClientNum(0).
//   00894db1  CALL 0x005336b0            (CG_GetPredictedPlayerState; see fact 1)
//   00894db6  MOV EAX,dword ptr [EAX + 0x1c4]
//   00894dbf  CALL Scr_AddInt
// +0x1c4 is playerState_s::stats[0] in this tree: renderOptions_s is 4 bytes
// (ent.h:219) which makes every following field line up so that `stats` lands on
// 0x1C4. stats[0] is health here -- g_client_fields.cpp:469 compares
// `ps.stats[0] > sess.maxHealth` and clamps it.
// Manual local adapter name, not a recovered C++ symbol. Retail 0x00894c30,
// client-project record 0x00b84988. The PUSH 1 left on the stack after the
// controller lookup supplies PROFILE_WRITE_IF_CHANGED to the profile update.
static void CScr_UpdateGamerProfile_SP()
{
    const int localClientNum = CScr_GetLocalClientNum(0);
    const int controllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
    GamerProfile_UpdateProfileFromDvars(controllerIndex, PROFILE_WRITE_IF_CHANGED);
}

static void __cdecl CScr_GetLocalClientHealth()
{
    int localClientNum; // [esp+0h] [ebp-4h]

    localClientNum = CScr_GetLocalClientNum(0);
    Scr_AddInt(CG_GetPredictedPlayerState(localClientNum)->stats[0], SCRIPTINSTANCE_CLIENT);
}

// getlocalclientmaxhealth -- retail SP handler 0x00894dd0, row 4. Instruction
// for instruction identical to 0x00894d80 above except the field: +0x1cc rather
// than +0x1c4, i.e. stats[2] rather than stats[0]. stats[2] is maxHealth here --
// g_client_mp.cpp:591 assigns `client->ps.stats[2] = client->sess.maxHealth`.
static void __cdecl CScr_GetLocalClientMaxHealth()
{
    int localClientNum; // [esp+0h] [ebp-4h]

    localClientNum = CScr_GetLocalClientNum(0);
    Scr_AddInt(CG_GetPredictedPlayerState(localClientNum)->stats[2], SCRIPTINSTANCE_CLIENT);
}

// isextracam -- retail SP handler 0x00895b60, client_project_methods row 26.
// Despite the name it is a SETTER and pushes nothing (both RET paths, 0x00895be1
// and 0x00895bee, return with no Scr_Add*). Disassembly:
//   00895b62  Scr_GetNumParam != 1 -> Scr_Error("isextracam() called with wrong
//             params.\n")
//   00895b80  MOVZX EAX,word ptr [ESP+8] ; ESP is still at entry here, and
//             scr_entref_t is 6 bytes {entnum@+0, classnum@+2, client@+4}, so
//             [ESP+4]=entnum and [ESP+8]=client. The value is then fed to
//             Scr_GetInt + the local-client range check, i.e. retail literally
//             writes CScr_GetLocalClientNum(entref.client). Reproduced verbatim;
//             it is equivalent to (0) because SP has MAX_LOCAL_CLIENTS == 1 so
//             entref.client is always 0, which is also the script's own
//             localClientNum argument index.
//   00895bb1  PUSH ESI shifts ESP by 4, so the later [ESP+8] (00895be2) and
//             [ESP+0x14] (00895bd2) both resolve back to entry+4 == entnum.
//             That is the value stored, on BOTH paths.
//   00895bb8  CMP [ESI+0xa46cc],0x3ff -- if an extra cam is already set, retail
//             raises "There can be only one extra camera active in the level."
//             and then assigns anyway (00895bda). CG_MapRestart seeds the same
//             1023 sentinel, so the retail diagnostic is safe to preserve.
static void __cdecl CScr_IsExtraCam(scr_entref_t entref)
{
    cg_s *cgameGlob; // [esp+4h] [ebp-8h]
    int localClientNum; // [esp+8h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 1 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "isextracam() called with wrong params.\n", 0);
    localClientNum = CScr_GetLocalClientNum(entref.client);
    cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    if ( cgameGlob->cameraData.extraCamEntNum != 1023 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "There can be only one extra camera active in the level.", 0);
    cgameGlob->cameraData.extraCamEntNum = entref.entnum;
}

// setextracamfov -- retail SP handler 0x00895c70, client_project_functions row
// 49. Pushes nothing. Disassembly:
//   00895c73  Scr_GetNumParam != 2 -> Scr_Error("CScr_SetExtraCamFov() called
//             with wrong params.\n")
//   00895c95  CALL Scr_GetFloat(1, SCRIPTINSTANCE_CLIENT)  (pushes 1,1)
//   00895ca2  Scr_GetInt(0) + range check == CScr_GetLocalClientNum(0)
//   00895cd0  CMP [EAX+0xa46cc],0x3ff -- if no extra cam is active, Scr_Error
//             ("CScr_SetExtraCamFov(), There is no extra cam active in the
//             level.\n") and return without storing; otherwise
//   00895ce1  MOVSS [EAX+0xa46d0],XMM0
// The Scr_Error here IS kept faithful (unlike isextracam's): frontend always
// calls `camera isExtraCam( 0 );` before `SetExtraCamFov( 0, ... );`, so with
// isextracam implemented above this path is not reachable from correct script.
static void __cdecl CScr_SetExtraCamFov()
{
    cg_s *cgameGlob; // [esp+4h] [ebp-Ch]
    int localClientNum; // [esp+8h] [ebp-8h]
    float fov; // [esp+Ch] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 2 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "CScr_SetExtraCamFov() called with wrong params.\n", 0);
    fov = (float)Scr_GetFloat(1u, SCRIPTINSTANCE_CLIENT);
    localClientNum = CScr_GetLocalClientNum(0);
    cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    if ( cgameGlob->cameraData.extraCamEntNum == 1023 )
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "CScr_SetExtraCamFov(), There is no extra cam active in the level.\n",
            0);
        return;
    }
    cgameGlob->cameraData.extraCamFov = fov;
}

// stopextracam -- retail SP handler 0x00895bf0, client_project_functions row 48.
// Pushes nothing. Disassembly:
//   00895bf2  Scr_GetNumParam != 1 -> Scr_Error("CScr_StopExtraCam() called with
//             wrong params.\n")
//   00895c14  Scr_GetInt(0) + range check == CScr_GetLocalClientNum(0)
//             (note: an explicit PUSH 0 here, unlike isextracam's entref.client)
//   00895c47  CMP [EAX+0xa46cc],ECX  with ECX = 0x3ff; when already cleared it
//             does nothing at all.
//   00895c57  MOV [EAX+0xa46cc],ECX          ; entNum  = 1023
//   00895c5d  MOVSS [EAX+0xa46d0],XMM0 from  ; fov     = 65.0f
//             the constant at 0x009ac544 == 0x42820000 == 65.0f.
static void __cdecl CScr_StopExtraCam()
{
    cg_s *cgameGlob; // [esp+4h] [ebp-8h]
    int localClientNum; // [esp+8h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 1 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "CScr_StopExtraCam() called with wrong params.\n", 0);
    localClientNum = CScr_GetLocalClientNum(0);
    cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    if ( cgameGlob->cameraData.extraCamEntNum != 1023 )
    {
        cgameGlob->cameraData.extraCamEntNum = 1023;
        cgameGlob->cameraData.extraCamFov = 65.0f;
    }
}

// setshaderconstant -- retail SP handler 0x00896380, client_project_methods row 25.
// mapshaderconstant  -- retail SP handler 0x008961a0, client_project_methods row 24.
// Together these feed the frontend TV-monitor material (clientscripts/frontend.csc; e.g.
// :124 `self mapshaderconstant( localClientNum, 0, "scriptVector0" );` and
// :129 `self setshaderconstant( localClientNum, 1, gridx, gridy, 0, 0 );`). Both decompiled
// AND disassembled this session in Ghidra (/BlackOps.exe, image base 0x00400000).
//
// setshaderconstant (0x00896380), decompile:
//   if (entref.classnum == 0) CG_GetLocalClientGlobals(entref.client);   // return value UNUSED
//   else Scr_Error(CLIENT, "not an entity", 0);                          // -- see note below
//   Scr_GetNumParam(CLIENT) != 6 -> Scr_Error(CLIENT,
//       "USAGE: ent setshaderconstant( <localClientNum>, <index>, <x>, <y>, <z>, <w>)\n", 0)
//   localClientNum = Scr_GetInt(0,CLIENT), range-checked == 0 with the exact
//       "Trying to get a local client index for a client '%d' ..." message --
//       i.e. CScr_GetLocalClientNum(0) (same helper cg_scr_main.cpp:975 already used
//       25+ times elsewhere in this translation unit).
//   index = Scr_GetInt(1,CLIENT); x,y,z,w = Scr_GetFloat(2..5,CLIENT)
//   entity resolution is retail's OWN entnum/localClientNum arithmetic INLINED, not a call:
//       entnum < 0x400  -> entnum*0x31C + perLocalClientEntityArray[localClientNum]
//       entnum >= 0x400 -> (localClientNum*0x200 + entnum)*800 - 0xC7FFC + fakeEntityArrayBase
//     This is byte-for-byte CG_GetEntity's own body (cg_local_mp.h:1316-1325:
//       `if (entityIndex < 1024) return &cg_entitiesArray[localClientNum][entityIndex];
//        else return &cg_fakeEntitiesArray[512*localClientNum - 1024 + entityIndex].cent;`) --
//     0x400==1024, 0x200==512, and 800 (sizeof retail's fake-entity record) all appear
//     verbatim in get_function_signature's immediate_values dump for 0x00896380:
//     [1024,800,1,2,3,4,5,6,8,40,9,12,236,16,20,796] (236==0xEC, 796==0x31C, both also
//     used below). So CG_GetEntity(localClientNum, entref.entnum) is the exact call to reuse.
//   if (resolved != 0) FUN_006c9430(resolved+0xEC, index, &{x,y,z,w})
//     FUN_006c9430 (0x006c9430) decompiles to EXACTLY this tree's
//     R_SetShaderConstantSetValue(ShaderConstantSet*, uint index, float*v)
//     (Engine/Renderer/gfx_d3d/r_shader_constant_set.cpp): index<7 AND bit `index` of the
//     used-bitmask (offset +0x77) must be set, then value[index][0..3] = v[0..3] -- field
//     offsets 0/0x70/0x77 match ShaderConstantSet::value[7][4]/constantSource[7]/used
//     exactly. So resolved+0xEC is a ShaderConstantSet. This tree already knows that
//     independently: ShaderConstantSet's own header comment says "XREF: cpose_t/r",
//     cpose_t::constantSet is the last field of cpose_t (bg_local.h:185, and cpose_t is
//     centity_s::pose), and `cent->pose.constantSet` / `&cent->cent.pose.constantSet` (for
//     fake entities) is already the established read/write pattern at 20+ call sites
//     (cg_players_mp.cpp, cg_actors_mp.cpp, cg_spawn.cpp:170, cg_snapshot_mp.cpp,
//     cg_weapons.cpp, cg_main_mp.cpp). cg_spawn.cpp:170's
//     `R_InitShaderConstantSet(&cent->cent.pose.constantSet);` already runs for every
//     dynamically-spawned fake entity, so a script_model TV monitor already has a
//     zeroed/unused ShaderConstantSet waiting for mapshaderconstant/setshaderconstant to
//     fill in -- no extra init work needed here.
//   NOT reproduced: the discarded `CG_GetLocalClientGlobals(entref.client)` call on the
//     classnum==0 path has no observable effect in this build (plain array index, no
//     assert baked in) and its result is never used, so it is a pure no-op to omit.
//
// mapshaderconstant (0x008961a0), decompile: identical entref/localClientNum handling,
//   then:
//     argCount = Scr_GetNumParam(CLIENT); if (argCount<3 || argCount>7) Scr_Error(CLIENT,
//         "USAGE: ent mapshaderconstant( <localClientNum>, <index>, <constant name>)\n", 0)
//     index = Scr_GetInt(1,CLIENT)
//     constantName = Scr_GetString (Ghidra 0x00567cb0) (2,CLIENT) -- already documented earlier
//         in this exact file (see setclientdvar below) as "0x00567cb0 == Scr_GetConstString +
//         SL_ConvertToString", i.e. Scr_GetString. CONFIRMED 2026-08-26: the live database now
//         names 0x00567cb0 Scr_GetString and its plate derives exactly that two-step body, so
//         this file's earlier inference is independently corroborated.
//     x,y,z,w default 0.0f, each overridable by Scr_GetFloat(3..6,CLIENT) if argCount allows
//         -- every frontend.csc call site uses the bare 3-arg form, so this is x=y=z=w=0 in
//         practice (e.g. :124 `self mapshaderconstant( localClientNum, 0, "scriptVector0" );`).
//     pSelf = CG_GetEntity(localClientNum, entref.entnum)  -- identical inlined arithmetic to
//         setshaderconstant above (same 1024/512/800/0x31C immediates).
//     if (pSelf) {
//       if (FUN_006c9340(&pSelf->pose.constantSet, index, constantName)) {   // == R_MapShaderConstantSet
//         FUN_006c9430(&pSelf->pose.constantSet, index, &{x,y,z,w});          // == R_SetShaderConstantSetValue
//         Scr_AddInt(1, CLIENT);
//         return;                                                            // (mirror write, see below, then return)
//       }
//     }
//     Scr_AddInt(0, CLIENT);
//   FUN_006c9340 (0x006c9340) disassembly (cdecl, 3 stack args -- Ghidra's own decompile
//   signature dropped the 3rd param, verified against the raw listing instead):
//     if (index<7 && FUN_00705b20(namePtr, &indexLocal))     // name -> constant-source lookup
//         { constantSource[index] = (byte)indexLocal; used |= 1<<index; return true; }
//     else return false;
//   FUN_00705b20/FUN_00705ad0 walk a 13-entry (0x9C/0xC) table of known constant names/ids --
//   exactly R_FindScriptableConstantSource_ByName's contract. This is FIELD-FOR-FIELD
//   identical to this tree's ACTUAL R_MapShaderConstantSet body
//   (Engine/Renderer/gfx_d3d/r_shader_constant_set.cpp):
//     if (index>6) return 0; if (!R_FindScriptableConstantSource_ByName(name,&source)) return 0;
//     scs->constantSource[index]=source; scs->used |= 1<<index; return 1;
//   So R_MapShaderConstantSet/R_SetShaderConstantSetValue in this tree ALREADY faithfully
//   reproduce retail bit-for-bit -- these two builtins are purely "wire GSC to the
//   already-correct engine calls", no new renderer-side logic required.
//
// UNRESOLVED, deliberately NOT reproduced in either body (documented gap, not a guess):
//   after the primary write, retail additionally does (both handlers, same shape):
//     linkedRec = *(int*)((byte*)CG_GetLocalClientGlobals(0) + 0x34);
//     if (linkedRec && *(byte*)(linkedRec + 0x13c) == (uint16)(entref.entnum|entref.classnum<<16))
//         [mapshaderconstant: re-run the FUN_006c9340 map step, only writing the value if it
//          again succeeds; setshaderconstant: write the value unconditionally]
//         onto a SECOND ShaderConstantSet embedded directly in cg_s at fixed offset +0x8be68.
//   cg_s+0x34 (a pointer), the linked record's +0x13c (a byte-sized entnum-like field), and
//   cg_s+0x8be68 were not identified this session -- no chain in this session ties them to a
//   named field in this tree's cg_s. Left unimplemented rather than guessed at: the compare
//   is against a BYTE, so it can only ever match entnum < 256, which structurally EXCLUDES a
//   dynamically spawned script_model TV monitor (entnum >= 1024), so omitting this mirror
//   write does not affect the frontend TV-monitor case this investigation was about. Flagged
//   for a future session (looks like a vehicle/turret- or camera-linked-entity mirror, given
//   the byte-sized entnum field) rather than wired up on a guess, per instructions not to
//   invent unverified behaviour in a live rendering path.
static void __cdecl CScr_SetShaderConstant(scr_entref_t entref)
{
    if (entref.classnum)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);

    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 6)
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "USAGE: ent setshaderconstant( <localClientNum>, <index>, <x>, <y>, <z>, <w>)\n",
            0);
    }

    int localClientNum = CScr_GetLocalClientNum(0);
    unsigned int index = Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT);
    float x = (float)Scr_GetFloat(2u, SCRIPTINSTANCE_CLIENT);
    float y = (float)Scr_GetFloat(3u, SCRIPTINSTANCE_CLIENT);
    float z = (float)Scr_GetFloat(4u, SCRIPTINSTANCE_CLIENT);
    float w = (float)Scr_GetFloat(5u, SCRIPTINSTANCE_CLIENT);

    centity_s *pSelf = CG_GetEntity(localClientNum, entref.entnum);
    if (pSelf)
        R_SetShaderConstantSetValue(&pSelf->pose.constantSet, index, x, y, z, w);
}

static void __cdecl CScr_MapShaderConstant(scr_entref_t entref)
{
    if (entref.classnum)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);

    int argCount = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if (argCount < 3 || argCount > 7)
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "USAGE: ent mapshaderconstant( <localClientNum>, <index>, <constant name>)\n",
            0);
    }

    int localClientNum = CScr_GetLocalClientNum(0);
    unsigned int index = Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT);
    char *constantName = Scr_GetString(2u, SCRIPTINSTANCE_CLIENT);

    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
    if (argCount > 3) x = (float)Scr_GetFloat(3u, SCRIPTINSTANCE_CLIENT);
    if (argCount > 4) y = (float)Scr_GetFloat(4u, SCRIPTINSTANCE_CLIENT);
    if (argCount > 5) z = (float)Scr_GetFloat(5u, SCRIPTINSTANCE_CLIENT);
    if (argCount > 6) w = (float)Scr_GetFloat(6u, SCRIPTINSTANCE_CLIENT);

    centity_s *pSelf = CG_GetEntity(localClientNum, entref.entnum);
    if (pSelf && R_MapShaderConstantSet(&pSelf->pose.constantSet, index, constantName))
    {
        R_SetShaderConstantSetValue(&pSelf->pose.constantSet, index, x, y, z, w);
        Scr_AddInt(1, SCRIPTINSTANCE_CLIENT);
        return;
    }

    Scr_AddInt(0, SCRIPTINSTANCE_CLIENT);
}

// setclientdvar -- retail SP handler 0x00894ae0, client_project_functions row
// 40. The whole handler is four calls and it pushes nothing:
//     name  = Scr_GetString(0, SCRIPTINSTANCE_CLIENT)   (0x00567cb0 ==
//             Scr_GetConstString + SL_ConvertToString)
//     if (!name  || !*name ) Scr_ParamError(0, "SetClientDvar: unknown dvar name",  CLIENT)
//     value = Scr_GetString(1, SCRIPTINSTANCE_CLIENT)
//     if (!value || !*value) Scr_ParamError(1, "SetClientDvar: unknown dvar value", CLIENT)
//     Dvar_SetFromStringByName(name, value)
// (0x0069edb0 is Scr_ParamError: it stores index+1 and the message into the
// per-instance error slot and calls Scr_ErrorInternal.)
// There is NO dvar-flag filtering in retail SP -- the handler goes straight to
// Dvar_SetFromStringByName. The previous stub comment speculated that such
// filtering existed and stubbed the row for that reason; the decompile shows it
// does not, so the row is implemented as-is.
static void __cdecl CScr_SetClientDvar()
{
    char *dvarName; // [esp+4h] [ebp-8h]
    char *dvarValue; // [esp+8h] [ebp-4h]

    dvarName = Scr_GetString(0, SCRIPTINSTANCE_CLIENT);
    if ( !dvarName || !*dvarName )
        Scr_ParamError(0, "SetClientDvar: unknown dvar name", SCRIPTINSTANCE_CLIENT);
    dvarValue = Scr_GetString(1u, SCRIPTINSTANCE_CLIENT);
    if ( !dvarValue || !*dvarValue )
        Scr_ParamError(1u, "SetClientDvar: unknown dvar value", SCRIPTINSTANCE_CLIENT);
    Dvar_SetFromStringByName(dvarName, dvarValue);
}

// forcegamemodemappings -- retail SP handler 0x00498c80, client_project_functions
// row 53. Pushes nothing. Structure straight from the decompile:
//     if (Scr_GetNumParam(CLIENT) != 2) { Scr_Error(<usage>); return; }
//     localClientNum = Scr_GetInt(0, CLIENT)     // raw, NOT CScr_GetLocalClientNum
//     modeName       = Scr_GetString(1, CLIENT)
//     "default":     if (Dvar_GetBool("gpad_enabled"))
//                        GamerProfile_ExecControllerBindings(<controllerIndex>)
//     "zombietron":  if (Dvar_GetBool("gpad_enabled")) exec thumbstick_default.cfg,
//                        buttons_default.cfg, buttons_zombietron
//     then, on every path, <clear keys>(localClientNum)
// The two unnamed callees were identified, not guessed:
//   0x004f3c70 -- a 5-int-stride table lookup keyed on localClientNum returning
//     field +4 (and -1 on miss) == Com_LocalClient_GetControllerIndex.
//   0x005d9410 -- memset of a 0x3F0-byte per-local-client record == CL_ClearKeys.
//     Confirmed by its only other caller, GamerProfile_UpdateProfileFromDvars
//     (0x005b0e41): this tree's GamerProfile_UpdateProfileFromDvars calls
//     CL_ClearKeys(Com_ControllerIndex_GetLocalClientNum(controllerIndex)) at
//     exactly that point (win_gamerprofile.cpp:386), and the surrounding
//     gpad_enabled / zombietron / three-cfg-exec sequence in that same function
//     (win_gamerprofile.cpp:403-411) matches this handler's "zombietron" arm
//     command-for-command.
static void __cdecl CScr_ForceGameModeMappings()
{
    int controllerIndex; // [esp+0h] [ebp-Ch]
    char *modeName; // [esp+4h] [ebp-8h]
    int localClientNum; // [esp+8h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 2 )
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to ForceGameModeMappings()\n"
            "USAGE: ForceGameModeMappings( <localClientNum>, <modeName>)\n",
            0);
        return;
    }
    localClientNum = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    modeName = Scr_GetString(1u, SCRIPTINSTANCE_CLIENT);
    if ( !I_strcmp(modeName, "default") )
    {
        if ( Dvar_GetBool("gpad_enabled") )
        {
            controllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
            GamerProfile_ExecControllerBindings(controllerIndex);
        }
    }
    else if ( !I_strcmp(modeName, "zombietron") )
    {
        if ( Dvar_GetBool("gpad_enabled") )
        {
            controllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
            Cmd_ExecuteSingleCommand(localClientNum, controllerIndex, (char *)"exec thumbstick_default.cfg\n");
            controllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
            Cmd_ExecuteSingleCommand(localClientNum, controllerIndex, (char *)"exec buttons_default.cfg\n");
            controllerIndex = Com_LocalClient_GetControllerIndex(localClientNum);
            Cmd_ExecuteSingleCommand(localClientNum, controllerIndex, (char *)"exec buttons_zombietron\n");
        }
    }
    CL_ClearKeys(localClientNum);
}

// Local implementation name only: the retail C identifier is unattested.
// Retail SP client_project_methods record 0x00A6082C pairs the exact script
// name "useweaponhidetags" with handler 0x008974D0.  The handler's two
// command-specific diagnostics independently identify its body.  It resolves
// the requested weapon, rebuilds entityState_s::partBits from the variant's
// 32-entry hideTags list, applies that mask to the entity DObj, and detaches FX
// marks from newly hidden bones.
//
// Retail passes an additional weapon-model byte and force-recreate flag to its
// six-argument CG_PreProcess_GetDObj.  This reconstruction's independently
// matched helper has the MP five-argument signature and derives the same entity
// state internally, so the semantic secondary-model argument remains null.
static void __cdecl CScr_UseWeaponHideTags_SP(scr_entref_t entref)
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) < 1 )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "useweaponhidetags <weaponName>.\n", 0);
        return;
    }

    char *weaponName = Scr_GetString(0, SCRIPTINSTANCE_CLIENT);
    const unsigned int weaponIndex = BG_GetWeaponIndexForName(weaponName);
    if ( !weaponIndex )
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            va("useweaponhidetags called with unknown weapon name %s\n", weaponName),
            0);
        return;
    }

    const WeaponVariantDef *weaponVariantDef = BG_GetWeaponVariantDef(weaponIndex);
    centity_s *cent = CG_GetEntity(entref.client, entref.entnum);
    memset(cent->nextState.partBits, 0, sizeof(cent->nextState.partBits));

    cgs_t *cgs = CG_GetLocalClientStaticGlobals(entref.client);
#ifdef KISAK_SP
    DObj *obj = CG_PreProcess_GetDObj(entref.client, cent->nextState.number, cent->nextState.eType, cgs->gameModels[cent->nextState.index.xmodel], cent->nextState.animTreeIndex, 0);
#else
    DObj *obj = CG_PreProcess_GetDObj(
        entref.client,
        cent->nextState.number,
        cent->nextState.eType,
        cgs->gameModels[cent->nextState.index.xmodel],
        0);
#endif
    if ( !obj )
        return;

    for ( int tagIndex = 0; tagIndex < 32 && weaponVariantDef->hideTags[tagIndex]; ++tagIndex )
    {
        unsigned char boneIndex = 0xFE;
        if ( DObjGetBoneIndex(obj, weaponVariantDef->hideTags[tagIndex], &boneIndex, -1) )
        {
            cent->nextState.partBits[boneIndex >> 5]
                |= 0x80000000u >> (boneIndex & 0x1F);
        }
    }

    unsigned int oldPartBits[5];
    DObjGetHidePartBits(obj, oldPartBits);
    DObjSetHidePartBits(obj, cent->nextState.partBits);
    FX_MarkEntUpdateHidePartBits(
        oldPartBits,
        cent->nextState.partBits,
        cent->pose.localClientNum,
        cent->nextState.number);
}

// Local implementation name only: this is the SP client-side sibling of the
// attested server GScr_CreateDynEntAndLaunch, not that server function itself.
// Retail client_project_functions record 0x00B84A48 pairs the script name
// "createdynentandlaunch" with handler 0x00557820.  Its client-only signature,
// exact diagnostic, model lookup, quaternion conversion and direct
// DynEntCl_CreateEntityModel -> DynEntCl_PlayBoltedFX dataflow distinguish it
// from the server event-producing implementation at 0x005182F0.
static void __cdecl CScr_CreateDynEntAndLaunch_SP()
{
    const unsigned int paramCount = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if ( paramCount < 6 )
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "CreateDynEntAndLaunch called with invalid params. CreateDynEntAndLaunch( <local client num>, <model>, <pos>, <angles>, <hitpos>, <force>, <fx>, <mature> )",
            0);
        return;
    }

    const int localClientNum = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    char *modelName = Scr_GetString(1, SCRIPTINSTANCE_CLIENT);
    const int modelIndex = CG_GetModelIndex(modelName, localClientNum);
    if ( modelIndex < 0 )
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            va("model '%s' not precached\n", modelName),
            1);
        return;
    }

    float position[3];
    float angles[3];
    float hitPosition[3];
    float force[3];
    Scr_GetVector(2, position, SCRIPTINSTANCE_CLIENT);
    Scr_GetVector(3, angles, SCRIPTINSTANCE_CLIENT);
    Scr_GetVector(4, hitPosition, SCRIPTINSTANCE_CLIENT);
    Scr_GetVector(5, force, SCRIPTINSTANCE_CLIENT);

    const int fxId = paramCount > 6 ? Scr_GetInt(6, SCRIPTINSTANCE_CLIENT) : 0;
    if ( paramCount > 7 && Scr_GetInt(7, SCRIPTINSTANCE_CLIENT) )
    {
        const int language = SEH_GetCurrentLanguage();
        if ( language == 3 || ((language == 11 || language == 13) && !zombiemode->current.enabled) )
            return;
    }

    cgs_t *cgs = CG_GetLocalClientStaticGlobals(localClientNum);
    float quat[4];
    AnglesToQuat(angles, quat);
    const unsigned __int16 dynEntId = DynEntCl_CreateEntityModel(
        cgs->gameModels[modelIndex],
        quat,
        position,
        hitPosition,
        force,
        0,
        0);
    if ( fxId )
        DynEntCl_PlayBoltedFX(cgs->fxs[fxId], dynEntId);
}

// Local implementation name only: the retail C identifier is unattested.
// Retail SP client_project_functions record 0x00B849DC pairs the exact script
// name "getweaponammoclip" with handler 0x00894E20.  Its command-specific
// wrong-parameter diagnostic is a second independent identity fact.  The body
// resolves the weapon variant's clip index and scans the predicted SP
// playerState's 15 AmmoClip slots, returning the matching count or zero.
static void __cdecl CScr_GetDynEnt_SP()
{
    unsigned int targetname = CScr_GetConstServerString(0);
    unsigned __int16 id = DynEnt_FindTargetname(targetname);
    if (id != 0xffff)
        Scr_AddEntityNum(id, 4, SCRIPTINSTANCE_CLIENT, 0);
    else
        Scr_Error(SCRIPTINSTANCE_CLIENT,
            va("getdynent, failed to find dyn ent with targetname %s.",
                SL_ConvertToString(targetname, SCRIPTINSTANCE_SERVER)), 0);
}

static void __cdecl CScr_GetDynEntArray_SP()
{
    unsigned int targetname = ~0u;
    if (static_cast<unsigned int>(Scr_GetNumParam(SCRIPTINSTANCE_CLIENT)) > 0)
        targetname = CScr_GetConstServerString(0);
    DynEnt_GetTargetnameArray_SP(targetname);
}

static void __cdecl CScr_SetDynEntEnabled_SP()
{
    const scr_entref_t entref = Scr_GetEntityRef(0, SCRIPTINSTANCE_CLIENT);
    bool enabled = false;
    if (static_cast<unsigned int>(Scr_GetNumParam(SCRIPTINSTANCE_CLIENT)) > 1)
        enabled = Scr_GetInt(1, SCRIPTINSTANCE_CLIENT) != 0;
    DynEntCl_SetEnabled_SP(entref.entnum, enabled);
}

static void __cdecl CScr_GetWeaponAmmoClip_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 2 )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "GetWeaponAmmoClip() called with wrong params.\n", 0);
        return;
    }

    const int localClientNum = CScr_GetLocalClientNum(0);
    char *weaponName = Scr_GetString(1, SCRIPTINSTANCE_CLIENT);
    const unsigned int weaponIndex = BG_FindWeaponIndexForName(weaponName);
    const playerState_s *ps = CG_GetPredictedPlayerState(localClientNum);
    const int clipIndex = BG_GetWeaponVariantDef(weaponIndex)->iClipIndex;

    for ( int slot = 0; slot < 15; ++slot )
    {
        if ( ps->ammoInClip[slot].clipIndex == clipIndex )
        {
            Scr_AddInt(ps->ammoInClip[slot].count, SCRIPTINSTANCE_CLIENT);
            return;
        }
    }

    Scr_AddInt(0, SCRIPTINSTANCE_CLIENT);
}

// Local implementation name only: the retail C identifier is unattested.
// Retail SP client_project_functions record 0x00B84A0C pairs "isonturret"
// with handler 0x008950A0; the handler's exact wrong-parameter diagnostic is
// the independent identity fact.  Its body returns whether either of the two
// EF_TURRET_ACTIVE bits (0x300) is set in the predicted player state.
static void __cdecl CScr_IsOnTurret_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 1 )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "isonturret() called with wrong params.\n", 0);
        return;
    }

    const int localClientNum = CScr_GetLocalClientNum(0);
    const playerState_s *ps = CG_GetPredictedPlayerState(localClientNum);
    Scr_AddInt((ps->eFlags & 0x300) != 0, SCRIPTINSTANCE_CLIENT);
}

static void __cdecl CScr_IsMeleeing_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 1 )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "IsMeleeing() called with wrong params.\n", 0);
        return;
    }

    const int localClientNum = CScr_GetLocalClientNum(0);
    const playerState_s *ps = CG_GetPredictedPlayerState(localClientNum);
    const int weaponState = ps->weaponstate;
    Scr_AddInt(weaponState == WEAPON_MELEE_INIT || weaponState == WEAPON_MELEE_FIRE
               || weaponState == WEAPON_MELEE_END,
               SCRIPTINSTANCE_CLIENT);
}

static void __cdecl CScr_IsThrowingGrenade_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 1 )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "IsThrowingGrenade() called with wrong params.\n", 0);
        return;
    }

    const int localClientNum = CScr_GetLocalClientNum(0);
    const playerState_s *ps = CG_GetPredictedPlayerState(localClientNum);
    const int weaponState = ps->weaponstate;
    Scr_AddInt(weaponState == WEAPON_OFFHAND_INIT || weaponState == WEAPON_OFFHAND_PREPARE
               || weaponState == WEAPON_OFFHAND_HOLD || weaponState == WEAPON_OFFHAND_START
               || weaponState == WEAPON_OFFHAND || weaponState == WEAPON_OFFHAND_END,
               SCRIPTINSTANCE_CLIENT);
}

// Local implementation name only: the retail C identifier is unattested.
// Retail SP client_project_functions record 0x00B849C4 pairs "isads" with
// handler 0x00652FD0; the handler's exact IsADS parameter diagnostic is an
// independent identity fact.  It returns whether predicted playerState offset
// 0x168 (fWeaponPosFrac in this reconstruction) is greater than zero.
static void __cdecl CScr_IsADS_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 1 )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "IsADS() called with wrong params.\n", 0);
        return;
    }

    const int localClientNum = CScr_GetLocalClientNum(0);
    const playerState_s *ps = CG_GetPredictedPlayerState(localClientNum);
    Scr_AddInt(ps->fWeaponPosFrac > 0.0f, SCRIPTINSTANCE_CLIENT);
}

static void CScr_HasCollectible_SP()
{
    Scr_AddInt(BG_HasCollectible(Scr_GetInt(0, SCRIPTINSTANCE_CLIENT)), SCRIPTINSTANCE_CLIENT);
}

static void CScr_SetCollectible_SP()
{
    BG_SetCollectible(Scr_GetInt(0, SCRIPTINSTANCE_CLIENT));
}

// Local implementation name only: the retail C identifier is unattested.
// Retail SP client_project_functions record 0x00B84AE4 pairs
// "visionsetnaked" with handler 0x008948E0; its exact wrong-parameter
// diagnostic independently identifies the body.  The handler accepts a local
// client, visionset name and optional seconds, then starts a smooth naked-mode
// visionset lerp (mode 0, style 3) with a one-second default.
static void __cdecl CScr_VisionSetNaked_SP()
{
    const int numParams = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if ( numParams != 2 && numParams != 3 )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "VisionSetNaked() called with wrong params.\n", 0);
        return;
    }

    int duration = 1000;
    if ( numParams == 3 )
        duration = (int)(Scr_GetFloat(2, SCRIPTINSTANCE_CLIENT) * 1000.0f + 9.313226e-10);

    const int localClientNum = CScr_GetLocalClientNum(0);
    const char *visionSetName = Scr_GetString(1, SCRIPTINSTANCE_CLIENT);
    CG_VisionSetStartLerp_To(
        localClientNum,
        VISIONSETMODE_NAKED,
        VISIONSETLERP_TO_SMOOTH,
        visionSetName,
        duration);
}

// Local implementation name only: the retail C identifier is unattested.
// Retail SP client_project_functions record 0x00B84AF0 directly pairs
// "getvisionsetnaked" with handler 0x008949A0. The handler independently reads
// integer argument 0 and passes the NUL-terminated string at cg_s +0xCC5A0 to
// client Scr_AddString. That field is visionNameNaked in this reconstruction.
static void __cdecl CScr_GetVisionSetNaked_SP()
{
    const int localClientNum = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    Scr_AddString(
        CG_GetLocalClientGlobals(localClientNum)->visionNameNaked,
        SCRIPTINSTANCE_CLIENT);
}

// Local implementation name only: this client-side handler has no attested C
// identifier. Retail client_project_functions record 0x00B84B14 pairs
// "setvolfog" with 0x00895340. Its body independently accepts the same 8/18
// float layouts parsed by CScr_SetClientVolumetricFog, derives reciprocal fog
// densities, calls R_SetFogFromServer for active local client zero, and then
// R_SwitchFog with cg->time plus the script transition duration. It is not the
// server Scr_SetVolumetricFog function (every retail accessor uses instance 1).
static void __cdecl CScr_SetSunlight_SP()
{
    float diffuseColor[3], specularColor[3];
    diffuseColor[0] = Scr_GetFloat(0, SCRIPTINSTANCE_CLIENT);
    diffuseColor[1] = Scr_GetFloat(1, SCRIPTINSTANCE_CLIENT);
    diffuseColor[2] = Scr_GetFloat(2, SCRIPTINSTANCE_CLIENT);
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 6)
    {
        specularColor[0] = Scr_GetFloat(3, SCRIPTINSTANCE_CLIENT);
        specularColor[1] = Scr_GetFloat(4, SCRIPTINSTANCE_CLIENT);
        specularColor[2] = Scr_GetFloat(5, SCRIPTINSTANCE_CLIENT);
    }
    else
    {
        specularColor[0] = diffuseColor[0];
        specularColor[1] = diffuseColor[1];
        specularColor[2] = diffuseColor[2];
    }
    R_SetSunLightOverride(diffuseColor, specularColor);
}

static void __cdecl CScr_ResetSunlight_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 0)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "Incorrect number of parameters\n", false);
    R_ResetSunLightOverride();
}

static void __cdecl CScr_SetVolFogForClient_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 19 && Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 9)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "Incorrect number of parameters\nUSAGE: setVolFog(<localClientNum>, <startDist>, <halfwayDist>, <halfwayHeight>, <baseHeight>, <red>, <green>, <blue>, <transition time>, <sun red>, <sun blue>, <sun green>, <sun dir X>, <sun dir Y>, <sun dir Z>, <sun start angle>, <sun end angle>, <max fog opacity>)\n", false);
    unsigned int localClientNum = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    if (localClientNum >= 2)
        return;
    float start = Scr_GetFloat(1, SCRIPTINSTANCE_CLIENT);
    if (start < 0.0f)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "setExpFog: startDist must be greater or equal to 0", false);
    float halfwayDist = Scr_GetFloat(2, SCRIPTINSTANCE_CLIENT);
    if (halfwayDist <= 0.0f)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "setVolFog: halfwayDist must be greater than 0", false);
    float halfwayHeight = Scr_GetFloat(3, SCRIPTINSTANCE_CLIENT);
    if (halfwayHeight < 0.0f)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "setVolFog: halfwayHeight must be greater or equal to 0", false);
    float baseHeight = Scr_GetFloat(4, SCRIPTINSTANCE_CLIENT);
    float density = 1.0f / halfwayDist;
    float heightDensity = halfwayHeight >= 1.0f ? 1.0f / halfwayHeight : 0.0f;
    float red = Scr_GetFloat(5, SCRIPTINSTANCE_CLIENT);
    float green = Scr_GetFloat(6, SCRIPTINSTANCE_CLIENT);
    float blue = Scr_GetFloat(7, SCRIPTINSTANCE_CLIENT);
    int startTime = CG_GetLocalClientGlobals(0)->time;
    float colorScale = 1.0f, sunRed = 0.0f, sunGreen = 0.0f, sunBlue = 0.0f;
    float sunX = 0.0f, sunY = 0.0f, sunZ = 0.0f, sunStart = 0.0f, sunEnd = 0.0f, maxOpacity = 1.0f;
    int transitionTime;
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 9)
        transitionTime = (int)((double)Scr_GetFloat(8, SCRIPTINSTANCE_CLIENT) * 1000.0);
    else
    {
        transitionTime = (int)((double)Scr_GetFloat(17, SCRIPTINSTANCE_CLIENT) * 1000.0);
        maxOpacity = Scr_GetFloat(18, SCRIPTINSTANCE_CLIENT);
        sunEnd = Scr_GetFloat(16, SCRIPTINSTANCE_CLIENT);
        sunStart = Scr_GetFloat(15, SCRIPTINSTANCE_CLIENT);
        sunZ = Scr_GetFloat(14, SCRIPTINSTANCE_CLIENT);
        sunY = Scr_GetFloat(13, SCRIPTINSTANCE_CLIENT);
        sunX = Scr_GetFloat(12, SCRIPTINSTANCE_CLIENT);
        sunBlue = Scr_GetFloat(11, SCRIPTINSTANCE_CLIENT);
        sunGreen = Scr_GetFloat(10, SCRIPTINSTANCE_CLIENT);
        sunRed = Scr_GetFloat(9, SCRIPTINSTANCE_CLIENT);
        colorScale = Scr_GetFloat(8, SCRIPTINSTANCE_CLIENT);
    }
    R_SetFogFromServer(localClientNum, start, red, green, blue, density, heightDensity, baseHeight,
        colorScale, sunRed, sunGreen, sunBlue, sunX, sunY, sunZ, sunStart, sunEnd, maxOpacity);
    R_SwitchFog(localClientNum, 1, startTime, transitionTime);
}

static void __cdecl CScr_SetVolFog_SP()
{
    const int numParams = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if ( numParams != 8 && numParams != 18 )
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters\nUSAGE: setVolFog(<startDist>, <halfwayDist>, <halfwayHeight>, <baseHeight>, <red>, <green>, <blue>, <transition time>, <sun red>, <sun blue>, <sun green>, <sun dir X>, <sun dir Y>, <sun dir Z>, <sun start angle>, <sun end angle>, <max fog opacity>)\n",
            0);
        return;
    }

    CScr_SetClientVolumetricFog();
    if ( !CL_LocalClient_IsFirstActive(0) )
        return;

    const int transitionParam = numParams == 8 ? 7 : 16;
    const int transitionTime =
        (int)(Scr_GetFloat(transitionParam, SCRIPTINSTANCE_CLIENT) * 1000.0f);
    cg_s *cgameGlob = CG_GetLocalClientGlobals(0);
    R_SetFogFromServer(
        0,
        cg_clientVolFog.fogStart,
        cg_clientVolFog.color[0],
        cg_clientVolFog.color[1],
        cg_clientVolFog.color[2],
        cg_clientVolFog.density,
        cg_clientVolFog.heightDensity,
        cg_clientVolFog.baseHeight,
        cg_clientVolFog.color[3],
        cg_clientVolFog.sunFogColor[0],
        cg_clientVolFog.sunFogColor[1],
        cg_clientVolFog.sunFogColor[2],
        cg_clientVolFog.sunFogDir[0],
        cg_clientVolFog.sunFogDir[1],
        cg_clientVolFog.sunFogDir[2],
        cg_clientVolFog.sunFogStartAng,
        cg_clientVolFog.sunFogEndAng,
        cg_clientVolFog.sunFogColor[3]);
    R_SwitchFog(0, 1, cgameGlob->time, transitionTime);
}

// Local implementation name only: the retail C identifier is unattested.
// Retail SP client_project_methods record 0x00A607A8 directly pairs "haseyes"
// with handler 0x00896970.  Its body independently resolves the real or fake
// client query sibling of the server-side setter at 0x008062E0.
static void __cdecl CScr_HasEyes_SP(scr_entref_t entref)
{
    const centity_s *cent;
    if ( entref.entnum < 0x400u )
        cent = CG_GetEntity(entref.client, entref.entnum);
    else
        cent = &CG_GetFakeEntity(entref.client, entref.entnum)->cent;

    Scr_AddInt((cent->currentState.eFlags & 0x20000) != 0, SCRIPTINSTANCE_CLIENT);
}

// Local implementation names only: neither retail C identifier is attested.
// Retail SP client_project_methods records 0x00A60838 and 0x00A60844 directly
// pair "usealternateaimparams" / "clearalternateaimparams" with handlers
// 0x00896C40 / 0x00896CC0. Both handlers first call 0x00667EF0, whose body
// independently identifies a player entity by walking active local clients and
// requiring nextSnap->ps.otherFlags & 6 plus a matching ps.clientNum. In zombie
// mode they then set cg_s +0xCE534 to one or zero for the matching local player.
// Twelve retail instructions access that field: these methods are stateful, not
// compile-only no-ops. The SP-only cg_s member above preserves that state while
// keeping the MP structure and behavior unchanged.
static bool CScr_IsPlayerEnt_SP(unsigned int entnum)
{
    for ( int localClientNum = 0; localClientNum < 1; ++localClientNum )
    {
        if ( !CL_LocalClient_IsFirstActive(localClientNum) || !cgArray )
            continue;

        const cg_s *cgameGlob = CG_GetLocalClientGlobals(localClientNum);
        if ( cgameGlob->nextSnap
            && (cgameGlob->nextSnap->ps.otherFlags & 6) != 0
            && (unsigned char)cgameGlob->nextSnap->ps.clientNum == entnum )
        {
            return true;
        }
    }

    return false;
}

static int CScr_BlurDurationMilliseconds_SP(float seconds)
{
    volatile float milliseconds = seconds * 1000.0f;
    if (!(milliseconds >= -2147483648.0f && milliseconds < 2147483648.0f))
        return INT_MIN;
    return (int)milliseconds;
}

static void CScr_SetBlur_SP(scr_entref_t entref)
{
    float blur = (float)Scr_GetFloat(0, SCRIPTINSTANCE_CLIENT);
    double seconds = Scr_GetFloat(1, SCRIPTINSTANCE_CLIENT);
    float roundedSeconds = (float)seconds;
    if (seconds < 0.0)
        Scr_ParamError(1, "Time must be positive", SCRIPTINSTANCE_CLIENT);
    int duration = CScr_BlurDurationMilliseconds_SP(roundedSeconds);
    if (blur < 0.0f)
        Scr_ParamError(0, "Blur value must be greater than 0", SCRIPTINSTANCE_CLIENT);
    if (!CScr_IsPlayerEnt_SP(entref.entnum))
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "You can only call GetLocalClientNumber on players.", false);
        return;
    }
    for (int localClientNum = 0; localClientNum < 1; ++localClientNum)
    {
        if (!CL_LocalClient_IsFirstActive(localClientNum))
            continue;
        cg_s *cgameGlob = cgArray;
        if (cgameGlob->nextSnap && cgameGlob->clientNum == entref.entnum)
        {
            CG_SetScriptedBlur_SP(localClientNum, duration, blur, 0, 1);
            return;
        }
    }
}

static void CScr_GetLocalClientNumber_SP(scr_entref_t entref)
{
    if (!CScr_IsPlayerEnt_SP(entref.entnum))
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "You can only call GetLocalClientNumber on players.", false);
        return;
    }
    const cg_s *cgameGlob = CG_GetLocalClientGlobals(0);
    if (CL_LocalClient_IsFirstActive(0) && cgameGlob->nextSnap && cgameGlob->clientNum == entref.entnum)
        Scr_AddInt(cgameGlob->localClientNum, SCRIPTINSTANCE_CLIENT);
}


static void CScr_SetAlternateAimParams_SP(scr_entref_t entref, bool enabled)
{
    if ( !CScr_IsPlayerEnt_SP(entref.entnum) )
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            enabled
                ? "You can only call UseAlternateAim on players."
                : "You can only call ClearAlternateAim on players.",
            0);
        return;
    }

    if ( !zombiemode->current.enabled )
        return;

    for ( int localClientNum = 0; localClientNum < 1; ++localClientNum )
    {
        if ( !CL_LocalClient_IsFirstActive(localClientNum) || !cgArray )
            continue;

        cg_s *cgameGlob = CG_GetLocalClientGlobals(localClientNum);
        if ( cgameGlob->nextSnap && cgameGlob->clientNum == entref.entnum )
        {
            cgameGlob->useAlternateAimParams = enabled;
            return;
        }
    }

    Scr_Error(
        SCRIPTINSTANCE_CLIENT,
        enabled
            ? "UseAlternateAimParams had no effect..."
            : "ClearAlternateAimParams had no effect...",
        0);
}

static void __cdecl CScr_UseAlternateAimParams_SP(scr_entref_t entref)
{
    CScr_SetAlternateAimParams_SP(entref, true);
}

static void __cdecl CScr_ClearAlternateAimParams_SP(scr_entref_t entref)
{
    CScr_SetAlternateAimParams_SP(entref, false);
}

// Local implementation name only: the retail C identifier for this method is
// unattested. The binary-owned client_project_methods record 0x00A6088C pairs
// "isspectating" with handler 0x00897410. Its body independently resolves the
// target centity (using predictedPlayerEntity for the local player), requires
// ET_PLAYER, then returns predictedPlayerState.pm_type == PM_SPECTATOR or bit 1
// of otherFlags. This is deliberately distinct from the existing no-argument
// CScr_IsSpectating function, whose MP body also considers demo-camera state.
static void __cdecl CScr_EntYawOverridesLinkYaw_SP(scr_entref_t entref)
{
    centity_s *cent = nullptr;
    if (entref.classnum)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", false);
    else if (CG_GetClientNumForLocalClient(entref.client) == entref.entnum)
        cent = &cgArray->predictedPlayerEntity;
    else
        cent = CG_GetEntity(entref.client, entref.entnum);

    if (Scr_GetInt(0, SCRIPTINSTANCE_CLIENT) == 1)
        cent->clientFlags |= 0x400000;
    else
        cent->clientFlags &= ~0x400000;
}

static void __cdecl CScr_EntGetsWeaponFireNotification_SP(scr_entref_t entref)
{
    if (entref.classnum)
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", false);
        return;
    }
    centity_s *cent;
    if (CG_GetClientNumForLocalClient(entref.client) == entref.entnum)
        cent = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
    else
        cent = CG_GetEntity(entref.client, entref.entnum);
    cent->bWeaponFireNotify = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT) == 1;
}

static void __cdecl CScr_IsSpectatingMethod_SP(scr_entref_t entref)
{
    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        return;
    }

    const centity_s *cent;
    if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
        cent = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
    else
        cent = CG_GetEntity(entref.client, entref.entnum);

    const playerState_s *ps = CG_GetPredictedPlayerState(entref.client);
    Scr_AddInt(
        cent->nextState.eType == ET_PLAYER
            && (ps->pm_type == 4 || (ps->otherFlags & 2) != 0),
        SCRIPTINSTANCE_CLIENT);
}
#endif // KISAK_SP

BuiltinFunctionDef client_project_functions[] =
{
  { "getgridfrompos", &CScr_GetGridFromPos, 0 },
  { "compassscale", &CScr_CompassScale, 0 },
  { "resetcompassscale", &CScr_ResetCompassScale, 0 },
  { "isdemoplaying", &CScr_IsDemoPlaying, 0 },
  { "isspectating", &CScr_IsSpectating, 0 },
  { "getlocalplayerteam", &CScr_GetLocalPlayerTeam, 0 },
  { "playfxontag", &CScr_PlayFXOnTag, 0 },
  { "playviewmodelfx", &CScr_PlayViewmodelFX, 0 },
  { "spawnfx", &CScr_SpawnFX, 0 },
  { "deletefx", &CScr_DeleteFX, 0 },
  { "getanimlength", &CScr_GetAnimLength, 0 },
  { "animateui", &CScr_AnimateUI, 0 },
  { "showui", &CScr_ShowUI, 0 },
  { "getcurrentweapon", &CScr_GetCurrentWeapon, 0 },
  { "getcurrentweaponincludingmelee", &CScr_GetCurrentWeaponIncludingMelee, 0 },
  { "hasweapon", &CScr_HasWeapon, 0 },
  { "gettotalammo", &CScr_GetTotalAmmo, 0 },
  { "setlocalradarenabled", &CScr_SetLocalRadarEnabled, 0 },
  { "setlocalradarposition", &CScr_SetLocalRadarPosition, 0 },
  { "setextracamentity", &CScr_SetExtraCamEntity, 0 },
  { "setextracamactive", &CScr_SetExtraCamActive, 0 },
  { "getextracamstatic", &CScr_GetExtraCamStatic, 0 },
  { "setextracamstatic", &CScr_SetExtraCamStatic, 0 },
  { "setextracamorigin", &CScr_SetExtraCamOrigin, 0 },
  { "setextracamangles", &CScr_SetExtraCamAngles, 0 },
  { "iscameraspiketoggled", &CScr_IsCameraSpikeToggled, 0 },
  // NEW FUNCS FROM BLOPS MP RETAIL (LATEST)
  { "setclientvolumetricfog", &CScr_SetClientVolumetricFog, 0 },
  { "switchtoservervolumetricfog", &CScr_SwitchToServerVolumetricFog, 0 },
  { "switchtoclientvolumetricfog", &CScr_SwitchToClientVolumetricFog, 0 },
  { "isinhelicopter", &CScr_IsInHelicopter, 0 },
#ifdef KISAK_SP
  { "playfxondynent", &CScr_PlayFXOnDynEnt_SP, 0 },
  // TODO(SP-STUB): retail-SP client builtin FUNCTIONS referenced by the SP
  // frontend script closure and absent from BOTH tables CScr_GetFunction
  // consults (client_functions[] then this one). Rows marked SP-IMPL have real
  // bodies reconstructed from their retail SP handlers; rows still marked
  // TODO(SP-STUB) are no-op stubs -- NOT implementations. See the TODO(SP-STUB)
  // header above for how the list was derived and why each stub's return shape
  // is what it is, and the "REAL SP BODIES" block for the implemented ones.
  // Verified before
  // adding: none of these names was already present in client_functions[] or
  // in this table. Trailing comment is the retail SP handler address and its
  // row index in SP's client_project_functions @ 0x00B84988.
  { "setextracamfov", CScr_SetExtraCamFov, 0 },                               // SP-IMPL    SP idx 49, 0x00895c70
  { "stopextracam", CScr_StopExtraCam, 0 },                                   // SP-IMPL    SP idx 48, 0x00895bf0
  { "setclientdvar", CScr_SetClientDvar, 0 },                                 // SP-IMPL    SP idx 40, 0x00894ae0
  { "getwaterheight", CScr_GetWaterHeight_SP, 0 },
  { "visionsetunderwater", CScr_VisionSetUnderwater_SP, 0 },
  { "getlocalclienthealth", CScr_GetLocalClientHealth, 0 },                   // SP-IMPL    SP idx  3, 0x00894d80
  { "visionsetdamage", CScr_VisionSetDamage_SP, 0 },
  { "forcegamemodemappings", CScr_ForceGameModeMappings, 0 },                 // SP-IMPL    SP idx 53, 0x00498c80
  { "updatedvarsfromprofile", CScr_SPStubFn_updatedvarsfromprofile, 0 },      // TODO(SP-STUB) SP idx  1, 0x00894c80
  { "updategamerprofile", CScr_UpdateGamerProfile_SP, 0 },                   // SP-IMPL    SP record 0x00B84988, 0x00894c30
  { "triggerfx", CScr_TriggerFX_SP, 0 },
  { "setwaterfog", CScr_SetWaterFog_SP, 0 },
  { "getlocalclientmaxhealth", CScr_GetLocalClientMaxHealth, 0 },             // SP-IMPL    SP idx  4, 0x00894dd0
  { "createdynentandlaunch", CScr_CreateDynEntAndLaunch_SP, 0 },              // SP-IMPL    SP record 0x00B84A48, 0x00557820
  { "getdynent", CScr_GetDynEnt_SP, 0 },
  { "getdynentarray", CScr_GetDynEntArray_SP, 0 },
  { "setdynentenabled", CScr_SetDynEntEnabled_SP, 0 },
  { "getweaponammoclip", CScr_GetWeaponAmmoClip_SP, 0 },                      // SP-IMPL    SP record 0x00B849DC, 0x00894E20
  { "isads", CScr_IsADS_SP, 0 },                                             // SP-IMPL    SP record 0x00B849C4, 0x00652FD0
  { "ismeleeing", CScr_IsMeleeing_SP, 0 },                                   // SP-IMPL    SP record 0x00B849F4, 0x00894F60
  { "isthrowinggrenade", CScr_IsThrowingGrenade_SP, 0 },                     // SP-IMPL    SP record 0x00B849E8, 0x00894ED0
  { "isonturret", CScr_IsOnTurret_SP, 0 },                                   // SP-IMPL    SP record 0x00B84A0C, 0x008950A0
  { "hascollectible", CScr_HasCollectible_SP, 0 },
  { "setcollectible", CScr_SetCollectible_SP, 0 },
  { "visionsetnaked", CScr_VisionSetNaked_SP, 0 },                           // SP-IMPL    SP record 0x00B84AE4, 0x008948E0
  { "getvisionsetnaked", CScr_GetVisionSetNaked_SP, 0 },                     // SP-IMPL    SP record 0x00B84AF0, 0x008949A0
  { "setsunlight", CScr_SetSunlight_SP, 0 },
  { "resetsunlight", CScr_ResetSunlight_SP, 0 },
  { "setvolfogforclient", CScr_SetVolFogForClient_SP, 0 },
  { "setvolfog", CScr_SetVolFog_SP, 0 },                                     // SP-IMPL    SP record 0x00B84B14, 0x00895340
#endif // KISAK_SP
};

// LWSS: Looks congruent to retail blops MP
// Size was written [29]; it is now unsized so the KISAK_SP rows below fit and so
// ARRAY_COUNT (used by CScr_GetMethodProjectSpecific) stays correct in both configs.
// The MP row set is unchanged, so ARRAY_COUNT is still 29 for KISAK_MP.
const BuiltinMethodDef client_project_methods[] =
{
  { "gettagorigin", &CScr_GetTagOrigin, 0 },
  { "gettagangles", &CScr_GetTagAngles, 0 },
  { "getinkillcam", &CScr_GetInKillcam, 0 },
  { "getowner", &CScrCmd_GetOwner, 0 },
  { "getanimstate", &CScr_GetAnimState, 0 },
  { "getanimstatecategory", &CScr_GetAnimStateCategory, 0 },
  { "getvehiclehealth", &CScr_GetVehicleHealth, 0 },
  { "getlefttreadhealth", &CScr_GetLeftTreadHealth, 0 },
  { "getrighttreadhealth", &CScr_GetRightTreadHealth, 0 },
  { "gethelidamagestate", &CScr_GetHeliDamageState, 0 },
  { "isburning", &CScrCmd_IsBurning, 0 },
  { "hasperk", &CPlayerCmd_HasPerk, 0 },
  { "getstance", &CScr_GetStance, 0 },
  { "shellshock", &CScrCmd_ShellShock, 0 },
  { "earthquake", &CScrCmd_Earthquake, 0 },
  { "setenemyglobalscrambler", &CScr_SetEnemyGlobalScrambler, 0 },
  { "setenemyscrambleramount", &CScr_SetEnemyScramblerAmount, 0 },
  { "getenemyscrambleramount", &CScr_GetEnemyScramblerAmount, 0 },
  { "isscrambled", &CScr_IsScrambled, 0 },
  { "setfriendlyscrambleramount", &CScr_SetFriendlyScramblerAmount, 0 },
  { "getfriendlyscrambleramount", &CScr_GetFriendlyScramblerAmount, 0 },
  { "addfriendlyscrambler", &CScr_AddFriendlyScrambler, 0 },
  { "clearnearestenemyscrambler", &CScr_ClearNearestEnemyScrambler, 0 },
  { "setnearestenemyscrambler", &CScr_SetNearestEnemyScrambler, 0 },
  { "removefriendlyscrambler", &CScr_RemoveFriendlyScrambler, 0 },
  { "removeallfriendlyscramblers", &CScr_RemoveAllFriendlyScramblers, 0 },
  { "hastacticalmaskoverlay", CScr_HasTacticalMaskOverlay, 0 },
  { "setflagasaway", &CScr_SetFlagAsAway, 0 },
  { "getparententity", &CScr_GetParentEntity, 0 }
#ifdef KISAK_SP
  ,
  // TODO(SP-STUB): retail-SP client builtin METHODS referenced by the SP
  // frontend script closure and absent from BOTH tables CScr_GetMethod
  // consults (client_methods[] then this one). The isextracam row has a real
  // body (see "REAL SP BODIES"); every other handler here is a no-op stub
  // -- NOT an implementation. See the TODO(SP-STUB) header above
  // client_project_functions[] for derivation and return-shape reasoning.
  // Verified before adding: none of these names was already present in
  // client_methods[] or in this table. Trailing comment is the retail SP
  // handler address and its row index in SP's client_project_methods
  // @ 0x00A606E8.
  { "getlocalclientnumber", CScr_GetLocalClientNumber_SP, 0 },
  { "entyawoverrideslinkyaw", CScr_EntYawOverridesLinkYaw_SP, 0 },
  { "setshaderconstant", CScr_SetShaderConstant, 0 },                         // SP-IMPL    SP idx 25, 0x00896380
  { "mapshaderconstant", CScr_MapShaderConstant, 0 },                         // SP-IMPL    SP idx 24, 0x008961a0
  { "isextracam", CScr_IsExtraCam, 0 },                                       // SP-IMPL    SP idx 26, 0x00895b60
  { "gettagforwardvector", CScr_GetTagForwardVector_SP, 0 },
  { "swimming", CScr_Swimming_SP, 0 },
  { "getorigin", CScr_SPStubMeth_getorigin, 0 },                              // TODO(SP-STUB) SP idx  3, 0x008964d0
  { "setblur", CScr_SetBlur_SP, 0 },
  { "getlinkedent", CScr_GetLinkedEnt_SP, 0 },
  { "geteye", CScr_GetEye_SP, 0 },
  { "getplayerangles", CScr_GetPlayerAngles_SP, 0 },
  { "linktocamera", CScr_LinkToCamera_SP, 0 },
  { "unlinkfromcamera", CScr_UnlinkFromCamera_SP, 0 },
  { "unlinkallfromcamera", CScr_UnlinkAllFromCamera_SP, 0 },
  { "getnormalizedmovement", CScr_GetNormalizedMovement_SP, 0 },
  { "getnormalizedcameramovement", CScr_GetNormalizedCameraMovement_SP, 0 },
  { "haseyes", CScr_HasEyes_SP, 0 },                                         // SP-IMPL    SP record 0x00A607A8, 0x00896970
  { "useweaponhidetags", CScr_UseWeaponHideTags_SP, 0 },                      // SP-IMPL    SP record 0x00A6082C, 0x008974D0
  { "usealternateaimparams", CScr_UseAlternateAimParams_SP, 0 },              // SP-IMPL    SP record 0x00A60838, 0x00896C40
  { "clearalternateaimparams", CScr_ClearAlternateAimParams_SP, 0 },           // SP-IMPL    SP record 0x00A60844, 0x00896CC0
  { "entgetsweaponfirenotification", CScr_EntGetsWeaponFireNotification_SP, 0 },
  { "isspectating", CScr_IsSpectatingMethod_SP, 0 }                            // SP-IMPL    SP record 0x00A6088C, 0x00897410
#endif // KISAK_SP
};

cached_tag_mat_t cg_cachedTagMat;

#ifdef KISAK_SP
static centity_s *CScr_GetTagEntity_SP(scr_entref_t entref)
{
    if (entref.classnum)
    {
        Scr_ObjectError("not an entity", SCRIPTINSTANCE_CLIENT);
        return 0;
    }
    if (CG_GetClientNumForLocalClient(entref.client) == entref.entnum)
        return &CG_GetLocalClientGlobals(0)->predictedPlayerEntity;
    return CG_GetEntity(entref.client, entref.entnum);
}

static void CScr_GetTagForwardVector_SP(scr_entref_t entref)
{
    centity_s *cent = CScr_GetTagEntity_SP(entref);
    unsigned int tagName = CScr_GetConstServerString(0);
    if (CScr_UpdateTagInternal(cent, tagName, &cg_cachedTagMat))
        Scr_AddVector(cg_cachedTagMat.tagMat[0], SCRIPTINSTANCE_CLIENT);
}
#endif

cscr_mp_data_t cg_scr_mp_data;

//void __cdecl CScrCmd_Earthquake(scr_entref_t entref)
//{
//    float Float; // [esp+20h] [ebp-1Ch]
//    float source[3]; // [esp+24h] [ebp-18h] BYREF
//    int duration; // [esp+30h] [ebp-Ch]
//    float radius; // [esp+34h] [ebp-8h]
//    float scale; // [esp+38h] [ebp-4h]
//
//    scale = Scr_GetFloat(0, SCRIPTINSTANCE_CLIENT);
//    Float = Scr_GetFloat(1u, SCRIPTINSTANCE_CLIENT);
//    duration = (int)((float)(Float * 1000.0) + 9.313225746154785e-10);
//    Scr_GetVector(2u, source, SCRIPTINSTANCE_CLIENT);
//    radius = Scr_GetFloat(3u, SCRIPTINSTANCE_CLIENT);
//    CG_StartShakeCamera(entref.client, scale, duration, source, radius);
//}

unsigned int __cdecl CScr_SpawnFXInternal(int localClientNum, int fxId, float (*axis)[3], float *pos, int time)
{
    const FxEffectDef *fxDef; // [esp+0h] [ebp-8h]

    fxDef = CG_GetLocalClientStaticGlobals(localClientNum)->fxs[fxId];
    if ( !fxDef
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 91, 0, "%s", "fxDef") )
    {
        __debugbreak();
    }
    return FX_SpawnOrientedEffect(localClientNum, fxDef, time, pos, axis, 0x3FFu);
}

void CScr_DeleteFX()
{
#ifdef KISAK_SP
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if (count != 2 && count != 3)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "Incorrect number of parameters for DeleteFX", 0);
    const int localClientNum = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    if (localClientNum != 0)
        Scr_Error(SCRIPTINSTANCE_CLIENT, va("Trying to get a local client index for a client '%d' that is not a local client.", localClientNum), 0);
    const unsigned int handle = Scr_GetInt(1, SCRIPTINSTANCE_CLIENT);
    const bool force = count >= 3 ? Scr_GetInt(2, SCRIPTINSTANCE_CLIENT) != 0 : true;
    FX_ThroughWithEffect(localClientNum, handle, force);
#else
    int localClientNum; // [esp+8h] [ebp-Ch]
    int intFxPtr; // [esp+Ch] [ebp-8h]

    localClientNum = CScr_GetLocalClientNum(0);
    intFxPtr = Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT);
    if ( (unsigned int)localClientNum >= 2
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                    118,
                    0,
                    "localClientNum not in [0, MAX_LOCAL_CLIENTS]\n\t%i not in [%i, %i]",
                    localClientNum,
                    0,
                    1) )
    {
        __debugbreak();
    }
    FX_ThroughWithEffect(localClientNum, intFxPtr, 1);
#endif
}

void CScr_SpawnFX()
{
#ifdef KISAK_SP
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if (count < 4 || count > 6)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "Incorrect number of parameters", 0);
    const int localClientNum = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    if (localClientNum != 0)
        Scr_Error(SCRIPTINSTANCE_CLIENT, va("Trying to get a local client index for a client '%d' that is not a local client.", localClientNum), 0);
    const int fxId = Scr_GetInt(1, SCRIPTINSTANCE_CLIENT);
    if (fxId <= 0 || fxId >= 196)
        Scr_Error(SCRIPTINSTANCE_CLIENT, va("CScr_PlayFX: invalid effect id %d", fxId), 0);
    float position[3], angles[3], axis[3][3];
    Scr_GetVector(2, position, SCRIPTINSTANCE_CLIENT);
    CScr_FXTimeMsec_SP(Scr_GetFloat(3, SCRIPTINSTANCE_CLIENT));
    if (count != 4)
    {
        Scr_GetVector(4, axis[0], SCRIPTINSTANCE_CLIENT);
        const float length = std::sqrt((axis[0][1] * axis[0][1] + axis[0][2] * axis[0][2]) + axis[0][0] * axis[0][0]);
        const float inverse = 1.0f / (-length >= 0.0f ? 1.0f : length);
        for (int i = 0; i < 3; ++i)
            axis[0][i] *= inverse;
        if (length == 0.0f)
            CScr_FxParamError(localClientNum, 4, "playFx called with (0 0 0) forward direction", fxId);
        if (count != 5)
        {
            Scr_GetVector(5, axis[2], SCRIPTINSTANCE_CLIENT);
            if (Vec3Normalize(axis[2]) == 0.0f)
                CScr_FxParamError(localClientNum, 5, "playFx called with (0 0 0) up direction", fxId);
        }
    }
    CScr_SetFxAngles(count == 4 ? 0 : count == 5 ? 1 : 2, axis, angles);
    centity_s *cent = CScr_SpawnFXEntity_SP(localClientNum, fxId, angles, position);
    if (cent)
        Scr_AddEntityNum(cent->nextState.number, 0, SCRIPTINSTANCE_CLIENT, localClientNum);
    else
        Scr_AddUndefined(SCRIPTINSTANCE_CLIENT);
#else
    unsigned int v0; // [esp+0h] [ebp-94h]
    unsigned int v1; // [esp+10h] [ebp-84h]
    unsigned int value; // [esp+20h] [ebp-74h]
    float Float; // [esp+34h] [ebp-60h]
    char *error; // [esp+38h] [ebp-5Ch]
    float pos[3]; // [esp+44h] [ebp-50h] BYREF
    int iTime; // [esp+50h] [ebp-44h]
    int localClientNum; // [esp+54h] [ebp-40h]
    int numParams; // [esp+58h] [ebp-3Ch]
    float angles[3]; // [esp+5Ch] [ebp-38h] BYREF
    int fxId; // [esp+68h] [ebp-2Ch]
    float axis[3][3]; // [esp+6Ch] [ebp-28h] BYREF
    float vecLength; // [esp+90h] [ebp-4h]

    numParams = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if ( numParams < 4 || numParams > 6 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "Incorrect number of parameters", 0);
    localClientNum = CScr_GetLocalClientNum(0);
    if ( (unsigned int)localClientNum >= 2
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                    161,
                    0,
                    "localClientNum not in [0, MAX_LOCAL_CLIENTS]\n\t%i not in [%i, %i]",
                    localClientNum,
                    0,
                    1) )
    {
        __debugbreak();
    }
    fxId = Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT);
    if ( fxId <= 0 || fxId >= 196 )
    {
        error = va("CScr_PlayFX: invalid effect id %d", fxId);
        Scr_Error(SCRIPTINSTANCE_CLIENT, error, 0);
    }
    Scr_GetVector(2u, pos, SCRIPTINSTANCE_CLIENT);
    Float = Scr_GetFloat(3u, SCRIPTINSTANCE_CLIENT);
    iTime = (int)((float)(Float * 1000.0) + 9.313225746154785e-10);
    if ( numParams == 4 )
    {
        CScr_SetFxAngles(0, axis, angles);
        value = CScr_SpawnFXInternal(localClientNum, fxId, axis, pos, iTime);
        Scr_AddInt(value, SCRIPTINSTANCE_CLIENT);
    }
    else
    {
        if ( numParams != 5
            && numParams != 6
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        179,
                        1,
                        "%s\n\t(numParams) = %i",
                        "(numParams == 5 || numParams == 6)",
                        numParams) )
        {
            __debugbreak();
        }
        Scr_GetVector(4u, axis[0], SCRIPTINSTANCE_CLIENT);
        vecLength = Vec3Normalize(axis[0]);
        if ( vecLength == 0.0 )
            CScr_FxParamError(localClientNum, 4u, "playFx called with (0 0 0) forward direction", fxId);
        if ( numParams == 5 )
        {
            CScr_SetFxAngles(1u, axis, angles);
            v1 = CScr_SpawnFXInternal(localClientNum, fxId, axis, pos, iTime);
            Scr_AddInt(v1, SCRIPTINSTANCE_CLIENT);
        }
        else
        {
            if ( numParams != 6
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                            193,
                            1,
                            "%s\n\t(numParams) = %i",
                            "(numParams == 6)",
                            numParams) )
            {
                __debugbreak();
            }
            Scr_GetVector(5u, axis[2], SCRIPTINSTANCE_CLIENT);
            vecLength = Vec3Normalize(axis[2]);
            if ( vecLength == 0.0 )
                CScr_FxParamError(localClientNum, 5u, "playFx called with (0 0 0) up direction", fxId);
            CScr_SetFxAngles(2u, axis, angles);
            v0 = CScr_SpawnFXInternal(localClientNum, fxId, axis, pos, iTime);
            Scr_AddInt(v0, SCRIPTINSTANCE_CLIENT);
        }
    }
#endif
}

void CScr_PlayFXOnTag()
{
    char *v0; // [esp+0h] [ebp-60h]
    char *error; // [esp+4h] [ebp-5Ch]
    scr_entref_t v2; // [esp+10h] [ebp-50h] BYREF
    scr_entref_t v3; // [esp+1Ah] [ebp-46h]
    scr_entref_t v4; // [esp+28h] [ebp-38h]
    scr_entref_t v5; // [esp+32h] [ebp-2Eh]
    scr_entref_t entref; // [esp+38h] [ebp-28h]
    unsigned int tagName; // [esp+40h] [ebp-20h]
    unsigned int effectHandle; // [esp+44h] [ebp-1Ch]
    int localClientNum; // [esp+48h] [ebp-18h]
    const FxEffectDef *fxDef; // [esp+4Ch] [ebp-14h]
    int numParams; // [esp+50h] [ebp-10h]
    const char *name; // [esp+54h] [ebp-Ch]
    cgs_t *cgs; // [esp+58h] [ebp-8h]
    int fxId; // [esp+5Ch] [ebp-4h]

    numParams = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if ( numParams != 4 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "Incorrect number of parameters for playfxontag", 0);
    localClientNum = CScr_GetLocalClientNum(0);
    fxId = Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT);
    v3 = Scr_GetEntityRef(2, SCRIPTINSTANCE_CLIENT);
    v4 = v3;
    v5 = v3;
    entref = v3;
#ifndef KISAK_SP
    // Retail SP CScr_PlayFXOnTag (BlackOps.exe 0x008944f0) has no DObj check:
    // CG_PlayBoltedEffect falls back to bone 0xff when the DObj isn't there yet.
    if ( !Com_GetClientDObj(v3.entnum, localClientNum) )
    {
        error = va(
                            "CScr_PlayFX: invalid entity for local client %i, either not in the snapshot for the client or dobj does not exist yet",
                            localClientNum);
        Scr_Error(SCRIPTINSTANCE_CLIENT, error, 0);
    }
#endif
    if ( fxId <= 0 || fxId >= 196 )
    {
        v0 = va("CScr_PlayFX: invalid effect id %d", fxId);
        Scr_Error(SCRIPTINSTANCE_CLIENT, v0, 0);
    }
    cgs = CG_GetLocalClientStaticGlobals(localClientNum);
    fxDef = cgs->fxs[fxId];
    if ( !fxDef
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 251, 0, "%s", "fxDef") )
    {
        __debugbreak();
    }
    tagName = Scr_GetConstLowercaseString(3u, SCRIPTINSTANCE_CLIENT);
    name = SL_ConvertToString(tagName, SCRIPTINSTANCE_CLIENT);
    tagName = SL_FindLowercaseString(name, SCRIPTINSTANCE_SERVER);
    effectHandle = CG_PlayBoltedEffect(localClientNum, fxDef, entref.entnum, tagName);
    Scr_AddInt(effectHandle, SCRIPTINSTANCE_CLIENT);
}

void CScr_PlayViewmodelFX()
{
    cg_s *LocalClientGlobals; // eax
    unsigned int v1; // eax
    const char *v2; // eax
    int v3; // eax
    unsigned int v4; // [esp-8h] [ebp-48h]
    char *v5; // [esp-4h] [ebp-44h]
    unsigned int v6; // [esp-4h] [ebp-44h]
    char *error; // [esp+0h] [ebp-40h]
    unsigned int handle; // [esp+20h] [ebp-20h]
    unsigned __int8 boneIndex; // [esp+27h] [ebp-19h] BYREF
    int weaponNum; // [esp+28h] [ebp-18h]
    int nparams; // [esp+2Ch] [ebp-14h]
    int localClientNum; // [esp+30h] [ebp-10h]
    const cgs_t *cgs; // [esp+34h] [ebp-Ch]
    int fxId; // [esp+38h] [ebp-8h]
    int realTagName; // [esp+3Ch] [ebp-4h]

    nparams = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if ( nparams != 3 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "PlayViewmodelFX() called with wrong params.\n", 0);
    localClientNum = CScr_GetLocalClientNum(0);
    if ( (unsigned int)localClientNum >= 2
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                    275,
                    0,
                    "localClientNum not in [0, MAX_LOCAL_CLIENTS]\n\t%i not in [%i, %i]",
                    localClientNum,
                    0,
                    1) )
    {
        __debugbreak();
    }
    fxId = Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT);
    if ( (unsigned int)fxId > 0xC4
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                    278,
                    0,
                    "fxId not in [0, MAX_EFFECT_NAMES]\n\t%i not in [%i, %i]",
                    fxId,
                    0,
                    196) )
    {
        __debugbreak();
    }
    realTagName = CScr_GetConstServerString(2u);
    if ( !realTagName )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "PlayViewmodelFX(): unable to find viewmodel tag.\n", 0);
    cgs = CG_GetLocalClientStaticGlobals(localClientNum);
    if ( !cgs && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 287, 0, "%s", "cgs") )
        __debugbreak();
    LocalClientGlobals = CG_GetLocalClientGlobals(localClientNum);
    weaponNum = BG_GetViewmodelWeaponIndex(&LocalClientGlobals->predictedPlayerState);
    boneIndex = -2;
    v4 = realTagName;
    v1 = CG_WeaponDObjHandle(localClientNum);
    if ( !CG_GetBoneIndex(localClientNum, v1, v4, &boneIndex) )
    {
        v5 = SL_ConvertToString(realTagName, SCRIPTINSTANCE_SERVER);
        v2 = BG_WeaponName(weaponNum);
        error = va("PlayViewmodelFX(): viewmodel weapon '%s', does not have bone '%s'", v2, v5);
        Scr_Error(SCRIPTINSTANCE_CLIENT, error, 0);
    }
    v6 = realTagName;
    v3 = CG_WeaponDObjHandle(localClientNum);
    handle = CG_PlayBoltedEffect(localClientNum, cgs->fxs[fxId], v3, v6);
    Scr_AddInt(handle, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_IsDemoPlaying()
{
    if ( Demo_IsPlaying() )
        Scr_AddInt(1, SCRIPTINSTANCE_CLIENT);
    else
        Scr_AddInt(0, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_IsSpectating()
{
    cg_s *cgameGlob; // [esp+8h] [ebp-Ch]
    VariableUnion localClientNum; // [esp+Ch] [ebp-8h]

    localClientNum.intValue = CScr_GetLocalClientNum(0);
    cgameGlob = CG_GetLocalClientGlobals(localClientNum.intValue);
    if ( !cgameGlob
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 343, 0, "%s", "cgameGlob") )
    {
        __debugbreak();
    }
    if ( cgameGlob == (cg_s *)-263324
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 346, 0, "%s", "ps") )
    {
        __debugbreak();
    }
    if ( Demo_IsPlaying() )
    {
        if ( Demo_IsMovieCamera() || Demo_IsThirdPersonCamera() )
        {
LABEL_17:
            Scr_AddInt(1, SCRIPTINSTANCE_CLIENT);
            return;
        }
    }
    else if ( ((unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) <= 1
                    || !Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT)
                    || cgameGlob->renderingThirdPerson
                    || (cgameGlob->predictedPlayerState.otherFlags & 2) == 0)
                 && (cgameGlob->predictedPlayerState.otherFlags & 0x1A) != 0 )
    {
        goto LABEL_17;
    }
    Scr_AddInt(0, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScrCmd_IsBurning(scr_entref_t entref)
{
    centity_s *pSelf; // [esp+Ch] [ebp-Ch]
    unsigned int isBurning; // [esp+14h] [ebp-4h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        389,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    isBurning = 0;
    if ( (pSelf->currentState.eFlags2 & 0x200000) != 0 || ((pSelf->clientFlags >> 5) & 1) != 0 )
        isBurning = 1;
    Scr_AddBool(isBurning, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CPlayerCmd_HasPerk(scr_entref_t entref)
{
    unsigned int value; // [esp+0h] [ebp-18h]
    char *error; // [esp+8h] [ebp-10h]
    char *perkName; // [esp+Ch] [ebp-Ch]
    cg_s *cGameGlob; // [esp+10h] [ebp-8h]
    unsigned int perkIndex; // [esp+14h] [ebp-4h]

    if ( entref.entnum >= 0x20u )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "hasperk() can only be called on players", 0);
    perkName = Scr_GetString(0, SCRIPTINSTANCE_CLIENT);
    perkIndex = BG_GetPerkIndexForName(perkName);
#ifdef KISAK_SP
    if ( perkIndex == BG_SP_PERK_COUNT )
#else
    if ( perkIndex == 52 )
#endif
    {
        error = va("Unknown perk: %s\n", perkName);
        Scr_Error(SCRIPTINSTANCE_CLIENT, error, 0);
    }
    cGameGlob = CG_GetLocalClientGlobals(0);
#ifdef KISAK_SP
    value = BG_HasSPPerk(cGameGlob->bgs.clientinfo[entref.entnum].perks, perkIndex);
#else
    value = BG_HasPerk(cGameGlob->bgs.clientinfo[entref.entnum].perks, perkIndex);
#endif
    Scr_AddBool(value, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_GetVehicleHealth(scr_entref_t entref)
{
    float value; // [esp+8h] [ebp-18h]
    centity_s *pSelf; // [esp+18h] [ebp-8h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        445,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    if ( pSelf->nextState.eType != 14 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "GetVehicleHealth() can only be called on vehicles", 0);
    value = CG_VehGetHealthPercentageEntity(pSelf);
    Scr_AddFloat(value, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_GetLeftTreadHealth(scr_entref_t entref)
{
    float value; // [esp+8h] [ebp-18h]
    centity_s *pSelf; // [esp+18h] [ebp-8h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        470,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    if ( pSelf->nextState.eType != 14 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "GetLeftTreadHealth() can only be called on vehicles", 0);
    value = CG_VehGetHealthPercentageLeftTread(pSelf);
    Scr_AddFloat(value, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_GetHeliDamageState(scr_entref_t entref)
{
    centity_s *pSelf; // [esp+10h] [ebp-8h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        495,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    if ( pSelf->nextState.eType != 12 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "GetHeliDamageState() can only be called on helicopters", 0);
    Scr_AddInt(pSelf->nextState.un1.scale, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_GetRightTreadHealth(scr_entref_t entref)
{
    float value; // [esp+8h] [ebp-18h]
    centity_s *pSelf; // [esp+18h] [ebp-8h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        520,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    if ( pSelf->nextState.eType != 14 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "GetRightTreadHealth() can only be called on vehicles", 0);
    value = CG_VehGetHealthPercentageRightTread(pSelf);
    Scr_AddFloat(value, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_GetInKillcam(scr_entref_t entref)
{
    cg_s *cGameGlob; // [esp+1Ch] [ebp-Ch]
    VariableUnion localClientNum; // [esp+20h] [ebp-8h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        546,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            CG_GetLocalClientGlobals(entref.client);
        else
            CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    localClientNum.intValue = CScr_GetLocalClientNum(0);
    cGameGlob = CG_GetLocalClientGlobals(localClientNum.intValue);
    Scr_AddInt(cGameGlob->inKillCam, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_GetAnimState(scr_entref_t entref)
{
    char *value; // [esp+0h] [ebp-18h]
    centity_s *pSelf; // [esp+10h] [ebp-8h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        569,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    if ( pSelf->nextState.eType != 17 && pSelf->nextState.eType != 19 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "GetAnimState() can only be called on actors", 0);
    value = BG_Actor_GetAnimStateName(pSelf->nextState.animState.state);
    Scr_AddString(value, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_GetAnimStateCategory(scr_entref_t entref)
{
    char *value; // [esp+0h] [ebp-18h]
    centity_s *pSelf; // [esp+10h] [ebp-8h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        593,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    if ( pSelf->nextState.eType != 17 && pSelf->nextState.eType != 19 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "GetAnimStateCategory() can only be called on actors", 0);
    value = BG_Actor_GetAnimStateCategoryName(pSelf->nextState.animState.state);
    Scr_AddString(value, SCRIPTINSTANCE_CLIENT);
}

void CScr_GetTotalAmmo()
{
    char *weaponName; // [esp+10h] [ebp-1Ch]
    int localClientNum; // [esp+18h] [ebp-14h]
    int ammoStock; // [esp+1Ch] [ebp-10h]
    int roundsInClip; // [esp+20h] [ebp-Ch]
    unsigned int weapIdx; // [esp+24h] [ebp-8h]
    const playerState_s *ps; // [esp+28h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 2 )
    {
        localClientNum = CScr_GetLocalClientNum(0);
        if ( !CG_GetLocalClientGlobals(localClientNum)
            && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 625, 0, "%s", "cgameGlob") )
        {
            __debugbreak();
        }
        weaponName = Scr_GetString(1u, SCRIPTINSTANCE_CLIENT);
        if ( !weaponName
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        627,
                        0,
                        "%s",
                        "weaponName") )
        {
            __debugbreak();
        }
        ps = CG_GetPredictedPlayerState(localClientNum);
        weapIdx = BG_FindWeaponIndexForName(weaponName);
        roundsInClip = BG_GetAmmoInClip(ps, weapIdx);
        ammoStock = BG_GetTotalAmmoReserve(ps, weapIdx);
        Scr_AddInt(ammoStock + roundsInClip, SCRIPTINSTANCE_CLIENT);
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to GetTotalAmmo()\nUSAGE: GetTotalAmmo( <localClientNum>, <weaponName> )\n",
            0);
    }
}

void CScr_GetCurrentWeapon()
{
    const cg_s *cgameGlob; // [esp+Ch] [ebp-Ch]
    VariableUnion localClientNum; // [esp+10h] [ebp-8h]
    const WeaponVariantDef *varDef; // [esp+14h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 1 )
    {
        localClientNum.intValue = CScr_GetLocalClientNum(0);
        cgameGlob = CG_GetLocalClientGlobals(localClientNum.intValue);
        if ( !cgameGlob
            && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 661, 0, "%s", "cgameGlob") )
        {
            __debugbreak();
        }
        varDef = BG_GetWeaponVariantDef(cgameGlob->predictedPlayerState.weapon);
        if ( !varDef
            && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 665, 0, "%s", "varDef") )
        {
            __debugbreak();
        }
        if ( !varDef->szInternalName
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        666,
                        0,
                        "%s",
                        "varDef->szInternalName") )
        {
            __debugbreak();
        }
        Scr_AddString((char *)varDef->szInternalName, SCRIPTINSTANCE_CLIENT);
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to GetCurrentWeapon()\nUSAGE: GetCurrentWeapon( <localClientNum> )\n",
            0);
    }
}

void CScr_GetCurrentWeaponIncludingMelee()
{
    const cg_s *cgameGlob; // [esp+Ch] [ebp-14h]
    unsigned __int16 weapon; // [esp+10h] [ebp-10h]
    VariableUnion localClientNum; // [esp+14h] [ebp-Ch]
    const WeaponVariantDef *varDef; // [esp+18h] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 1 )
    {
        localClientNum.intValue = CScr_GetLocalClientNum(0);
        cgameGlob = CG_GetLocalClientGlobals(localClientNum.intValue);
        if ( !cgameGlob
            && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 698, 0, "%s", "cgameGlob") )
        {
            __debugbreak();
        }
        if ( cgameGlob->predictedPlayerState.weaponstate == 17
            || cgameGlob->predictedPlayerState.weaponstate == 18
            || cgameGlob->predictedPlayerState.weaponstate == 19 )
        {
            weapon = cgameGlob->predictedPlayerState.meleeWeapon;
        }
        else
        {
            weapon = cgameGlob->predictedPlayerState.weapon;
        }
        varDef = BG_GetWeaponVariantDef(weapon);
        if ( !varDef
            && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 708, 0, "%s", "varDef") )
        {
            __debugbreak();
        }
        if ( !varDef->szInternalName
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        709,
                        0,
                        "%s",
                        "varDef->szInternalName") )
        {
            __debugbreak();
        }
        Scr_AddString((char *)varDef->szInternalName, SCRIPTINSTANCE_CLIENT);
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to GetCurrentWeapon()\nUSAGE: GetCurrentWeapon( <localClientNum> )\n",
            0);
    }
}

void CScr_HasWeapon()
{
    char *weaponName; // [esp+14h] [ebp-10h]
    const cg_s *cgameGlob; // [esp+18h] [ebp-Ch]
    VariableUnion localClientNum; // [esp+1Ch] [ebp-8h]
    unsigned int weapIdx; // [esp+20h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 2 )
    {
        localClientNum.intValue = CScr_GetLocalClientNum(0);
        cgameGlob = CG_GetLocalClientGlobals(localClientNum.intValue);
        weaponName = Scr_GetString(1u, SCRIPTINSTANCE_CLIENT);
        if ( !weaponName
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        739,
                        0,
                        "%s",
                        "weaponName") )
        {
            __debugbreak();
        }
        if ( !cgameGlob
            && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 740, 0, "%s", "cgameGlob") )
        {
            __debugbreak();
        }
        weapIdx = BG_FindWeaponIndexForName(weaponName);
        if ( weapIdx && BG_PlayerHasWeapon(&cgameGlob->predictedPlayerState, weapIdx) )
            Scr_AddBool(1u, SCRIPTINSTANCE_CLIENT);
        else
            Scr_AddBool(0, SCRIPTINSTANCE_CLIENT);
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to HasWeapon()\nUSAGE: HasWeapon( <localClientNum>, <weaponName> )\n",
            0);
    }
}

void CScr_SetLocalRadarEnabled()
{
    cg_s *cgameGlob; // [esp+Ch] [ebp-8h]
    VariableUnion localClientNum; // [esp+10h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 2 )
    {
        localClientNum.intValue = CScr_GetLocalClientNum(0);
        cgameGlob = CG_GetLocalClientGlobals(localClientNum.intValue);
        if ( Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT) )
            cgameGlob->hasLocalRadar = 1;
        else
            cgameGlob->hasLocalRadar = 0;
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to SetLocalRadarEnabled()\n"
            "USAGE: SetLocalRadarEnabled( <localClientNum>, <enabled> )\n",
            0);
    }
}

void CScr_SetLocalRadarPosition()
{
    float origin[3]; // [esp+Ch] [ebp-14h] BYREF
    cg_s *cgameGlob; // [esp+18h] [ebp-8h]
    int localClientNum; // [esp+1Ch] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 2 )
    {
        localClientNum = CScr_GetLocalClientNum(0);
        cgameGlob = CG_GetLocalClientGlobals(localClientNum);
        Scr_GetVector(1u, origin, SCRIPTINSTANCE_CLIENT);
        *(_QWORD *)cgameGlob->localRadarPos = *(_QWORD *)origin;
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to SetLocalRadarPosition()\n"
            "USAGE: SetLocalRadarPosition( <localClientNum>, <position> )\n",
            0);
    }
}

void CScr_SetExtraCamEntity()
{
    scr_entref_t v0; // [esp+0h] [ebp-38h] BYREF
    int v1; // [esp+Ah] [ebp-2Eh]
    cg_s *cgameGlob; // [esp+28h] [ebp-10h]
    int localClientNum; // [esp+2Ch] [ebp-Ch]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 2 )
    {
        localClientNum = CScr_GetLocalClientNum(0);
        cgameGlob = CG_GetLocalClientGlobals(localClientNum);
        if ( Scr_GetType(1u, SCRIPTINSTANCE_CLIENT) )
        {
            //v1 = *(unsigned int *)&Scr_GetEntityRef(&v0, 1u, SCRIPTINSTANCE_CLIENT)->entnum;
            v1 = Scr_GetEntityRef(1u, SCRIPTINSTANCE_CLIENT).entnum;
            cgameGlob->extraCamEntity = (unsigned __int16)v1;
        }
        else
        {
            cgameGlob->extraCamEntity = 1023;
        }
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to SetExtraCamEntity()\nUSAGE: SetExtraCam( <localClientNum>, <entity> )\n",
            0);
    }
}

void CScr_SetExtraCamActive()
{
    cg_s *cgameGlob; // [esp+8h] [ebp-Ch]
    VariableUnion localClientNum; // [esp+Ch] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 2 )
    {
        if ( CL_LocalClient_GetActiveCount() <= 1 )
        {
            localClientNum.intValue = CScr_GetLocalClientNum(0);
            cgameGlob = CG_GetLocalClientGlobals(localClientNum.intValue);
            if ( Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT) )
                cgameGlob->extraCamActive = 1;
            else
                cgameGlob->extraCamActive = 0;
        }
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to SetExtraCamActive()\nUSAGE: SetExtraCam( <localClientNum>, <active> )\n",
            0);
    }
}

void CScr_GetExtraCamStatic()
{
    VariableUnion localClientNum; // [esp+Ch] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 1 )
    {
        localClientNum.intValue = CScr_GetLocalClientNum(0);
        if ( CG_GetLocalClientGlobals(localClientNum.intValue)->extraCamStatic )
            Scr_AddInt(1, SCRIPTINSTANCE_CLIENT);
        else
            Scr_AddInt(0, SCRIPTINSTANCE_CLIENT);
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to GetExtraCamStatic()\nUSAGE: GetExtraCamStatic( <localClientNum> )\n",
            0);
    }
}

void CScr_SetExtraCamStatic()
{
    cg_s *cgameGlob; // [esp+8h] [ebp-Ch]
    VariableUnion localClientNum; // [esp+Ch] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 2 )
    {
        if ( CL_LocalClient_GetActiveCount() <= 1 )
        {
            localClientNum.intValue = CScr_GetLocalClientNum(0);
            cgameGlob = CG_GetLocalClientGlobals(localClientNum.intValue);
            if ( Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT) )
                cgameGlob->extraCamStatic = 1;
            else
                cgameGlob->extraCamStatic = 0;
        }
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to SetExtraCamStatic()\n"
            "USAGE: SetExtraCamStatic( <localClientNum>, <active> )\n",
            0);
    }
}

void CScr_SetExtraCamOrigin()
{
    float *extraCamOrigin; // [esp+0h] [ebp-20h]
    float origin[3]; // [esp+Ch] [ebp-14h] BYREF
    cg_s *cgameGlob; // [esp+18h] [ebp-8h]
    int localClientNum; // [esp+1Ch] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 2 )
    {
        localClientNum = CScr_GetLocalClientNum(0);
        cgameGlob = CG_GetLocalClientGlobals(localClientNum);
        Scr_GetVector(1u, origin, SCRIPTINSTANCE_CLIENT);
        extraCamOrigin = cgameGlob->extraCamOrigin;
        cgameGlob->extraCamOrigin[0] = origin[0];
        extraCamOrigin[1] = origin[1];
        extraCamOrigin[2] = origin[2];
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to SetExtraCamOrigin()\n"
            "USAGE: SetExtraCamOrigin( <localClientNum>, <origin> )\n",
            0);
    }
}

void CScr_SetExtraCamAngles()
{
    cg_s *cgameGlob; // [esp+Ch] [ebp-14h]
    VariableUnion localClientNum; // [esp+10h] [ebp-10h]
    float angles[3]; // [esp+14h] [ebp-Ch] BYREF

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 2 )
    {
        localClientNum.intValue = CScr_GetLocalClientNum(0);
        cgameGlob = CG_GetLocalClientGlobals(localClientNum.intValue);
        Scr_GetVector(1u, angles, SCRIPTINSTANCE_CLIENT);
        cgameGlob->extraCamAngles[0] = angles[0];
        cgameGlob->extraCamAngles[1] = angles[1];
        cgameGlob->extraCamAngles[2] = angles[2];
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to SetExtraCamAngles()\n"
            "USAGE: SetExtraCamAngles( <localClientNum>, <angles> )\n",
            0);
    }
}

void CScr_IsCameraSpikeToggled()
{
    VariableUnion localClientNum; // [esp+Ch] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) == 1 )
    {
        localClientNum.intValue = CScr_GetLocalClientNum(0);
        if ( (CG_GetLocalClientGlobals(localClientNum.intValue)->predictedPlayerState.weapFlags & 0x200000) != 0 )
            Scr_AddInt(1, SCRIPTINSTANCE_CLIENT);
        else
            Scr_AddInt(0, SCRIPTINSTANCE_CLIENT);
    }
    else
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters passed to IsCameraSpikeToggled\nUSAGE: IsCameraSpikeToggled( <localClientNum> )\n",
            0);
    }
}

void CScr_SetClientVolumetricFog()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 18 && Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 8)
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters\n"
            "USAGE: setClientVolFog(<startDist>, <halfwayDist>, <halfwayHeight>, <baseHeight>, <red>, <green>, <blue>, <transit"
            "ion time>, <sun red>, <sun blue>, <sun green>, <sun dir X>, <sun dir Y>, <sun dir Z>, <sun start angle>, <sun end "
            "angle>, <max fog opacity>)\n",
            0);
    }

    float startDist = Scr_GetFloat(0, SCRIPTINSTANCE_CLIENT);
    if (startDist < 0.0)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "setClientVolFog: startDist must be greater or equal to 0", 0);

    float halfwayDist = Scr_GetFloat(1u, SCRIPTINSTANCE_CLIENT);
    if (halfwayDist <= 0.0)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "setClientVolFog: halfwayDist must be greater than 0", 0);

    float halfwayHeight = Scr_GetFloat(2u, SCRIPTINSTANCE_CLIENT);
    if (halfwayHeight < 0.0)
        Scr_Error(SCRIPTINSTANCE_CLIENT, "setClientVolFog: halfwayHeight must be greater or equal to 0", 0);

    float baseHeight = Scr_GetFloat(3u, SCRIPTINSTANCE_CLIENT);
    float density = 1.0 / halfwayDist;

    float heightDensity;
    if (halfwayHeight < 1.0)
        heightDensity = 0.0;
    else
        heightDensity = 1.0 / halfwayHeight;

    float red = Scr_GetFloat(4u, SCRIPTINSTANCE_CLIENT);
    float green = Scr_GetFloat(5u, SCRIPTINSTANCE_CLIENT);
    float blue = Scr_GetFloat(6u, SCRIPTINSTANCE_CLIENT);
    int numParams = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);

    cg_clientVolFog.fogStart = startDist;
    cg_clientVolFog.color[0] = red;
    cg_clientVolFog.color[1] = green;
    cg_clientVolFog.color[2] = blue;
    cg_clientVolFog.density = density;
    cg_clientVolFog.heightDensity = heightDensity;
    cg_clientVolFog.baseHeight = baseHeight;

    if (numParams == 8)
    {
        cg_clientVolFog.color[3] = 1.0;
        cg_clientVolFog.sunFogColor[0] = 0.0;
        cg_clientVolFog.sunFogColor[1] = 0.0;
        cg_clientVolFog.sunFogColor[2] = 0.0;
        cg_clientVolFog.sunFogDir[0] = 0.0;
        cg_clientVolFog.sunFogDir[1] = 0.0;
        cg_clientVolFog.sunFogDir[2] = 0.0;
        cg_clientVolFog.sunFogStartAng = 0.0;
        cg_clientVolFog.sunFogEndAng = 0.0;
        cg_clientVolFog.sunFogColor[3] = 1.0;
    }
    else
    {
        cg_clientVolFog.color[3] = Scr_GetFloat(7u, SCRIPTINSTANCE_CLIENT);
        cg_clientVolFog.sunFogColor[0] = Scr_GetFloat(8u, SCRIPTINSTANCE_CLIENT);
        cg_clientVolFog.sunFogColor[1] = Scr_GetFloat(9u, SCRIPTINSTANCE_CLIENT);
        cg_clientVolFog.sunFogColor[2] = Scr_GetFloat(0xAu, SCRIPTINSTANCE_CLIENT);
        cg_clientVolFog.sunFogDir[0] = Scr_GetFloat(0xBu, SCRIPTINSTANCE_CLIENT);
        cg_clientVolFog.sunFogDir[1] = Scr_GetFloat(0xCu, SCRIPTINSTANCE_CLIENT);
        cg_clientVolFog.sunFogDir[2] = Scr_GetFloat(0xDu, SCRIPTINSTANCE_CLIENT);
        cg_clientVolFog.sunFogStartAng = Scr_GetFloat(0xEu, SCRIPTINSTANCE_CLIENT);
        cg_clientVolFog.sunFogEndAng = Scr_GetFloat(0xFu, SCRIPTINSTANCE_CLIENT);
        cg_clientVolFog.sunFogColor[3] = Scr_GetFloat(0x11u, SCRIPTINSTANCE_CLIENT);
    }
}

void CScr_SwitchToServerVolumetricFog()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 1)
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters\nUSAGE: SwitchToServerVolumetricFog(localClientNum)\n",
            0);
    }

    int clientIndex = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    if (clientIndex)
    {
        char* errorBuf = va("Trying to get a local client index for a client '%d' that is not a local client.", clientIndex);
        Scr_Error(SCRIPTINSTANCE_CLIENT, errorBuf, 0);
    }

    cg_s* cgameGlob = CG_GetLocalClientGlobals(clientIndex);
    R_SetFogFromServer(
        clientIndex,
        cg_serverVolFog.fogStart,
        cg_serverVolFog.color[0],
        cg_serverVolFog.color[1],
        cg_serverVolFog.color[2],
        cg_serverVolFog.density,
        cg_serverVolFog.heightDensity,
        cg_serverVolFog.baseHeight,
        cg_serverVolFog.color[3],
        cg_serverVolFog.sunFogColor[0],
        cg_serverVolFog.sunFogColor[1],
        cg_serverVolFog.sunFogColor[2],
        cg_serverVolFog.sunFogDir[0],
        cg_serverVolFog.sunFogDir[1],
        cg_serverVolFog.sunFogDir[2],
        cg_serverVolFog.sunFogStartAng,
        cg_serverVolFog.sunFogEndAng,
        cg_serverVolFog.sunFogColor[3]);

    R_SwitchFog(clientIndex, 1u, cgameGlob->time, 0);
}

void CScr_SwitchToClientVolumetricFog()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 1)
    {
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "Incorrect number of parameters\nUSAGE: SwitchToClientVolumetricFog(localClientNum)\n",
            0);
    }
        
    int clientIndex = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    if (clientIndex)
    {
        char* errorBuf = va("Trying to get a local client index for a client '%d' that is not a local client.", clientIndex);
        Scr_Error(SCRIPTINSTANCE_CLIENT, errorBuf, 0);
    }

    cg_s* cgameGlob = CG_GetLocalClientGlobals(clientIndex);
    R_SetFogFromServer(
        clientIndex,
        cg_clientVolFog.fogStart,
        cg_clientVolFog.color[0],
        cg_clientVolFog.color[1],
        cg_clientVolFog.color[2],
        cg_clientVolFog.density,
        cg_clientVolFog.heightDensity,
        cg_clientVolFog.baseHeight,
        cg_clientVolFog.color[3],
        cg_clientVolFog.sunFogColor[0],
        cg_clientVolFog.sunFogColor[1],
        cg_clientVolFog.sunFogColor[2],
        cg_clientVolFog.sunFogDir[0],
        cg_clientVolFog.sunFogDir[1],
        cg_clientVolFog.sunFogDir[2],
        cg_clientVolFog.sunFogStartAng,
        cg_clientVolFog.sunFogEndAng,
        cg_clientVolFog.sunFogColor[3]);

    R_SwitchFog(clientIndex, 1u, cgameGlob->time, 0);
}

void CScr_IsInHelicopter()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 1)
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "Incorrect number of parameters\nUSAGE: IsInHelicopter(localClientNum)\n", 0);
    }
    
    int clientIndex = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    if (clientIndex)
    {
        char* errorBuf = va("Trying to get a local client index for a client '%d' that is not a local client.", clientIndex);
        Scr_Error(SCRIPTINSTANCE_CLIENT, errorBuf, 0);
    }

    cg_s* cgameGlob = CG_GetLocalClientGlobals(clientIndex);
    
    int isInHelicopter = 0;
    if(cgameGlob->cameraData.lastCamMode == CAM_VEHICLE)
    {
        centity_s* vehicle = CG_GetEntity(clientIndex, cgameGlob->predictedPlayerState.viewlocked_entNum);
        if (vehicle)
        {
            isInHelicopter = CG_GetVehicleInfo(vehicle->nextState.vehicleState.vehicleInfoIndex)->type == 6;
        }
    }
    else
    {
        isInHelicopter = cgameGlob->cameraData.lastCamMode == CAM_VEHICLE_GUNNER;
    }

    Scr_AddInt(isInHelicopter, SCRIPTINSTANCE_CLIENT);
}

void CScr_GetGridFromPos()
{
    VariableUnion v0; // eax
    float pos[3]; // [esp+8h] [ebp-14h] BYREF
    char gridName[4]; // [esp+14h] [ebp-8h] BYREF
    int argc; // [esp+18h] [ebp-4h]

    argc = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    if ( argc != 2 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "CGScr_GetGridByPos( <clientNum> <pos> ) takes 2 parameters", 0);
    Scr_GetVector(1u, pos, SCRIPTINSTANCE_CLIENT);
    v0.intValue = CScr_GetLocalClientNum(0);
    CG_GetGridFromPos(v0.intValue, pos, gridName);
    Scr_AddString(gridName, SCRIPTINSTANCE_CLIENT);
}

void CScr_CompassScale()
{
    VariableUnion duration; // [esp+0h] [ebp-Ch]
    VariableUnion difference; // [esp+4h] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 2 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "CScr_CompassScale( <diff> <dur> ) takes 2 parameters", 0);
    difference.intValue = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    duration.intValue = Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT);
    Dvar_SetInt((dvar_s *)compassScaleDiff, difference.intValue);
    Dvar_SetInt((dvar_s *)compassScaleDuration, duration.intValue);
}

void CScr_ResetCompassScale()
{
    VariableUnion duration; // [esp+0h] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) != 1 )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "CScr_ResetCompassScale( <dur> ) takes 1 parameter", 0);
    duration.intValue = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    Dvar_SetBool((dvar_s *)compassScaleReset, 1);
    Dvar_SetInt((dvar_s *)compassScaleDuration, duration.intValue);
}

void __cdecl CScr_GetLocalPlayerTeam()
{
    cg_s *cGameGlob; // [esp+8h] [ebp-Ch]
    int localClientNum; // [esp+10h] [ebp-4h]

    localClientNum = CScr_GetLocalClientNum(0);
    if ( CL_LocalClient_IsActive(localClientNum) )
    {
        cGameGlob = CG_GetLocalClientGlobals(localClientNum);
        CScr_AddTeamName(cGameGlob->bgs.clientinfo[cGameGlob->clientNum].team);
    }
    else
    {
        Scr_AddUndefined(SCRIPTINSTANCE_CLIENT);
    }
}

void __cdecl CScr_AddTeamName(team_t team)
{
    switch ( team )
    {
        case TEAM_FREE:
            Scr_AddString("free", SCRIPTINSTANCE_CLIENT);
            break;
        case TEAM_AXIS:
            Scr_AddString("axis", SCRIPTINSTANCE_CLIENT);
            break;
        case TEAM_ALLIES:
            Scr_AddString("allies", SCRIPTINSTANCE_CLIENT);
            break;
        case TEAM_SPECTATOR:
            Scr_AddString("spectator", SCRIPTINSTANCE_CLIENT);
            break;
        default:
            return;
    }
}

void(__cdecl *__cdecl CScr_GetFunctionProjectSpecific(const char **pName, int *type))()
{
    unsigned int i; // [esp+18h] [ebp-4h]

    for (i = 0; i < ARRAY_COUNT(client_project_functions); ++i)
    {
        if (!strcmp(*pName, client_project_functions[i].actionString))
        {
            *pName = client_project_functions[i].actionString;
            *type = client_project_functions[i].type;
            return client_project_functions[i].actionFunc;
        }
    }
    return 0;
}

void __cdecl CScrCmd_GetOwner(scr_entref_t entref)
{
    centity_s *Entity; // eax
    int intValue; // [esp-4h] [ebp-30h]
    centity_s *pSelf; // [esp+1Ch] [ebp-10h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        1227,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    intValue = CScr_GetLocalClientNum(0);
    Entity = CG_GetEntity(intValue, (int)pSelf->nextState.faction.iHeadIconTeam >> 2);
    CScr_AddEntity(Entity, intValue);
}

void __cdecl CScr_GetTagOrigin(scr_entref_t entref)
{
#ifdef KISAK_SP
    centity_s *cent = CScr_GetTagEntity_SP(entref);
    unsigned int tagName = CScr_GetConstServerString(0);
    if (!CScr_UpdateTagInternal(cent, tagName, &cg_cachedTagMat))
        return;
    Scr_AddVector(cg_cachedTagMat.tagMat[3], SCRIPTINSTANCE_CLIENT);
#else

    VariableUnion v1; // eax
    unsigned int tagName; // [esp+Ch] [ebp-10h]
    centity_s *pSelf; // [esp+10h] [ebp-Ch]
    char *name; // [esp+14h] [ebp-8h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        1288,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    v1.intValue = Scr_GetConstLowercaseString(0, SCRIPTINSTANCE_CLIENT);
    name = SL_ConvertToString(v1.stringValue, SCRIPTINSTANCE_CLIENT);
    tagName = SL_FindLowercaseString(name, SCRIPTINSTANCE_SERVER);
    CScr_UpdateTagInternal(pSelf, tagName, &cg_cachedTagMat);
    Scr_AddVector(cg_cachedTagMat.tagMat[3], SCRIPTINSTANCE_CLIENT);

#endif
}

int __cdecl CScr_UpdateTagInternal(centity_s *ent, unsigned int tagName, cached_tag_mat_t *cachedTag)
{
    if ( !ent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 1236, 0, "%s", "ent") )
    {
        __debugbreak();
    }

#ifdef KISAK_SP
    const cg_s *cgameGlob = CG_GetLocalClientGlobals(0);
    if (ent->nextState.number != cachedTag->entnum || cgameGlob->time != cachedTag->time || tagName != cachedTag->name)
    {
        DObj *obj = Com_GetClientDObj(ent->nextState.number, ent->pose.localClientNum);
        if (!obj || !CG_DObjGetWorldTagMatrix(&ent->pose, obj, tagName, cachedTag->tagMat, cachedTag->tagMat[3]))
            return 0;
        cachedTag->entnum = ent->nextState.number;
        cachedTag->time = cgameGlob->time;
        cachedTag->name = (unsigned short)tagName;
    }
    return 1;
#else

    char *v4; // eax
    cg_s *LocalClientGlobals; // eax
    const char *name; // [esp-4h] [ebp-18h]
    char *error; // [esp+0h] [ebp-14h]
    DObj *obj; // [esp+Ch] [ebp-8h]
    cg_s *cgameGlob; // [esp+10h] [ebp-4h]

    cgameGlob = CG_GetLocalClientGlobals(ent->pose.localClientNum);
    if ( ent->nextState.number != cachedTag->entnum || cgameGlob->time != cachedTag->time || tagName != cachedTag->name )
    {
        obj = Com_GetClientDObj(ent->nextState.number, ent->pose.localClientNum);
        if ( !obj )
        {
            Scr_ObjectError("entity has no model defined", SCRIPTINSTANCE_SERVER);
            return 0;
        }
        if ( !CG_DObjGetWorldTagMatrix(&ent->pose, obj, tagName, cachedTag->tagMat, cachedTag->tagMat[3]) )
        {
            name = DObjGetModel(obj, 0)->name;
            v4 = SL_ConvertToString(tagName, SCRIPTINSTANCE_CLIENT);
            error = va("tag '%s' does not exist in model '%s' (or any attached submodels)", v4, name);
            Scr_Error(SCRIPTINSTANCE_CLIENT, error, 0);
            return 0;
        }
        LocalClientGlobals = CG_GetLocalClientGlobals(ent->pose.localClientNum);
        cachedTag->entnum = ent->nextState.number;
        cachedTag->time = LocalClientGlobals->time;
        cachedTag->name = tagName;
    }
    return 1;

#endif
}

void __cdecl CScr_GetTagAngles(scr_entref_t entref)
{
#ifdef KISAK_SP
    centity_s *cent = CScr_GetTagEntity_SP(entref);
    unsigned int tagName = CScr_GetConstServerString(0);
    if (!CScr_UpdateTagInternal(cent, tagName, &cg_cachedTagMat))
        return;
    float angles[3];
    AxisToAngles(cg_cachedTagMat.tagMat, angles);
    Scr_AddVector(angles, SCRIPTINSTANCE_CLIENT);
#else

    VariableUnion v1; // eax
    unsigned int tagName; // [esp+Ch] [ebp-1Ch]
    centity_s *pSelf; // [esp+10h] [ebp-18h]
    char *name; // [esp+14h] [ebp-14h]
    float angles[3]; // [esp+18h] [ebp-10h] BYREF
    fake_centity_s *pFake; // [esp+24h] [ebp-4h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
        pFake = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        1314,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum < 0x400u )
            pFake = 0;
        else
            pFake = CG_GetFakeEntity(entref.client, entref.entnum);
    }
    v1.intValue = Scr_GetConstLowercaseString(0, SCRIPTINSTANCE_CLIENT);
    name = SL_ConvertToString(v1.stringValue, SCRIPTINSTANCE_CLIENT);
    tagName = SL_FindLowercaseString(name, SCRIPTINSTANCE_SERVER);
    CScr_UpdateTagInternal(pSelf, tagName, &cg_cachedTagMat);
    AxisToAngles(cg_cachedTagMat.tagMat, angles);
    Scr_AddVector(angles, SCRIPTINSTANCE_CLIENT);

#endif
}

void __cdecl CScrCmd_ShellShock(scr_entref_t entref)
{
    shellshock_parms_t *ShellshockParms; // eax
    int time; // [esp+0h] [ebp-50h]
    char *v3; // [esp+8h] [ebp-48h]
    char *error; // [esp+Ch] [ebp-44h]
    float Float; // [esp+20h] [ebp-30h]
    int duration; // [esp+34h] [ebp-1Ch]
    char *shock; // [esp+38h] [ebp-18h]
    cg_s *cgameGlob; // [esp+3Ch] [ebp-14h]
    int localClientNum; // [esp+44h] [ebp-Ch]
    char *configString; // [esp+48h] [ebp-8h]
    signed int id; // [esp+4Ch] [ebp-4h]

    CG_GetEntity(entref.client, entref.entnum);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) < 3 )
        Scr_Error(
            SCRIPTINSTANCE_CLIENT,
            "USAGE: <player> shellshock( <local client number>, <shellshockname>, <duration>)\n",
            0);
    localClientNum = CScr_GetLocalClientNum(0);
    shock = Scr_GetString(1u, SCRIPTINSTANCE_CLIENT);
    Float = Scr_GetFloat(2u, SCRIPTINSTANCE_CLIENT);
    duration = (int)((float)(1000.0 * Float) + 9.313225746154785e-10);
    if ( (unsigned int)duration > 0xEA60 )
    {
        error = va("duration %g should be >= 0 and <= 60", (float)((float)duration * 0.001));
        Scr_ParamError(1u, error, SCRIPTINSTANCE_CLIENT);
    }
    cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    for ( id = 1; id < 16; ++id )
    {
        configString = CL_GetConfigString(id + 2532);
        if ( !I_stricmp(configString, shock) )
        {
            time = cgameGlob->time;
            ShellshockParms = BG_GetShellshockParms(id);
            CG_StartShellShock(cgameGlob, ShellshockParms, time, duration);
            return;
        }
    }
    v3 = va(
                 "shellshock '%s' was not precached.    You need to precache it in the server script before calling it on the client.\n",
                 shock);
    Scr_Error(SCRIPTINSTANCE_CLIENT, v3, 0);
}

void __cdecl CScr_SetEnemyGlobalScrambler(scr_entref_t entref)
{
    cg_s *cgameGlob; // [esp+0h] [ebp-8h]

    cgameGlob = CG_GetLocalClientGlobals(entref.client);
    cgameGlob->globalScramblerActive = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_SetEnemyScramblerAmount(scr_entref_t entref)
{
    float alphaAmount; // [esp+0h] [ebp-8h]
    cg_s *cgameGlob; // [esp+4h] [ebp-4h]

    cgameGlob = CG_GetLocalClientGlobals(entref.client);
    alphaAmount = Scr_GetFloat(0, SCRIPTINSTANCE_CLIENT);
    if ( alphaAmount < 0.0 || alphaAmount > 1.0 )
        Scr_Error("float value must be between 0 and 1", 0);
    cgameGlob->scramblerEnemyAlpha = alphaAmount;
}

void __cdecl CScr_SetFriendlyScramblerAmount(scr_entref_t entref)
{
    float alphaAmount; // [esp+0h] [ebp-8h]
    cg_s *cgameGlob; // [esp+4h] [ebp-4h]

    cgameGlob = CG_GetLocalClientGlobals(entref.client);
    alphaAmount = Scr_GetFloat(0, SCRIPTINSTANCE_CLIENT);
    if ( alphaAmount < 0.0 || alphaAmount > 1.0 )
        Scr_Error("float value must be between 0 and 1", 0);
    cgameGlob->scramblerFriendlyAlpha = alphaAmount;
}

void __cdecl CScr_GetFriendlyScramblerAmount(scr_entref_t entref)
{
    float value; // [esp+8h] [ebp-8h]

    value = CG_GetLocalClientGlobals(entref.client)->scramblerFriendlyAlpha;
    Scr_AddFloat(value, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_GetEnemyScramblerAmount(scr_entref_t entref)
{
    float value; // [esp+8h] [ebp-8h]

    value = CG_GetLocalClientGlobals(entref.client)->scramblerEnemyAlpha;
    Scr_AddFloat(value, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_IsScrambled(scr_entref_t entref)
{
    unsigned int IsScrambled; // [esp+4h] [ebp-4h]

    IsScrambled = CG_GetLocalClientGlobals(entref.client)->scramblerEnemyAlpha > 0.01;
    Scr_AddBool(IsScrambled, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_SetNearestEnemyScrambler(scr_entref_t entref)
{
    scr_entref_t v1; // [esp+8h] [ebp-2Ch] BYREF
    scr_entref_t v2; // [esp+12h] [ebp-22h]
    scr_entref_t v3; // [esp+18h] [ebp-1Ch]
    scr_entref_t v4; // [esp+22h] [ebp-12h]
    centity_s *cent; // [esp+28h] [ebp-Ch]
    scr_entref_t scramblerEntity; // [esp+2Ch] [ebp-8h]

    //v2 = *Scr_GetEntityRef(&v1, 0, SCRIPTINSTANCE_CLIENT);
    v2 = Scr_GetEntityRef(0, SCRIPTINSTANCE_CLIENT);
    v3 = v2;
    v4 = v2;
    scramblerEntity = v2;
    cent = CG_GetEntity(v2.client, v2.entnum);
    CG_AddEnemyScrambler(entref.client, cent);
}

void __cdecl CScr_ClearNearestEnemyScrambler(scr_entref_t entref)
{
    CG_ClearNearestEnemyScrambler(entref.client);
}

void __cdecl CScr_AddFriendlyScrambler(scr_entref_t entref)
{
    VariableUnion scramblerHandle; // [esp+Ch] [ebp-Ch]
    float scramblerY; // [esp+10h] [ebp-8h]
    float scramblerX; // [esp+14h] [ebp-4h]

    scramblerX = Scr_GetFloat(0, SCRIPTINSTANCE_CLIENT);
    scramblerY = Scr_GetFloat(1u, SCRIPTINSTANCE_CLIENT);
    scramblerHandle.intValue = Scr_GetInt(2u, SCRIPTINSTANCE_CLIENT);
    CG_AddFriendlyScrambler(entref.client, scramblerX, scramblerY, scramblerHandle.intValue);
}

void __cdecl CScr_RemoveFriendlyScrambler(scr_entref_t entref)
{
    VariableUnion scramblerHandle; // [esp+0h] [ebp-4h]

    scramblerHandle.intValue = Scr_GetInt(0, SCRIPTINSTANCE_CLIENT);
    CG_RemoveFriendlyScrambler(entref.client, scramblerHandle.intValue);
}

void __cdecl CScr_RemoveAllFriendlyScramblers(scr_entref_t entref)
{
    CG_RemoveAllFriendlyScramblers(entref.client);
}

void __cdecl CScr_HasTacticalMaskOverlay(scr_entref_t entref)
{
    if ( CG_IsShowingZombieMap() )
        Scr_AddInt(1, SCRIPTINSTANCE_CLIENT);
    else
        Scr_AddInt(0, SCRIPTINSTANCE_CLIENT);
}

void __cdecl CScr_GetStance(scr_entref_t entref)
{
    centity_s *pSelf; // [esp+Ch] [ebp-8h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        1663,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    if ( ((pSelf->clientFlags >> 1) & 1) == 0 )
        Scr_Error("GetStance can not be used on a player that is not in the snapshot.", 0);
    if ( pSelf->nextState.eType == 1 || pSelf->nextState.eType == 5 )
    {
        if ( (pSelf->nextState.lerp.eFlags & 8) != 0 )
        {
            Scr_AddConstString(cscr_const.prone, SCRIPTINSTANCE_SERVER);
        }
        else if ( (pSelf->nextState.lerp.eFlags & 4) != 0 )
        {
            Scr_AddConstString(cscr_const.crouch, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_AddConstString(cscr_const.stand, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        Scr_Error("GetStance is only defined for players.", 0);
    }
}

void __cdecl CScr_SetFlagAsAway(scr_entref_t entref)
{
    centity_s *pSelf; // [esp+14h] [ebp-18h]
    team_t team; // [esp+18h] [ebp-14h]
    cg_s *cgameGlob; // [esp+1Ch] [ebp-10h]
    VariableUnion localClientNum; // [esp+20h] [ebp-Ch]
    int away; // [esp+28h] [ebp-4h]

    if ( entref.classnum )
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "not an entity", 0);
        pSelf = 0;
    }
    else
    {
        if ( entref.entnum >= 0x600u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                        1702,
                        0,
                        "%s",
                        "entref.entnum < MAX_LOCAL_CENTITIES") )
        {
            __debugbreak();
        }
        if ( CG_GetClientNumForLocalClient(entref.client) == entref.entnum )
            pSelf = &CG_GetLocalClientGlobals(entref.client)->predictedPlayerEntity;
        else
            pSelf = CG_GetEntity(entref.client, entref.entnum);
        if ( entref.entnum >= 0x400u )
            CG_GetFakeEntity(entref.client, entref.entnum);
    }
    if ( ((pSelf->clientFlags >> 1) & 1) != 0 )
    {
        if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_CLIENT) >= 2 )
        {
            localClientNum.intValue = CScr_GetLocalClientNum(0);
            away = Scr_GetInt(1u, SCRIPTINSTANCE_CLIENT);
            cgameGlob = CG_GetLocalClientGlobals(localClientNum.intValue);
            team = (team_t)(pSelf->nextState.faction.iHeadIconTeam & 3);
            if ( team == TEAM_ALLIES )
            {
                cgameGlob->alliesFlagAway = away != 0;
            }
            else if ( team == TEAM_AXIS )
            {
                cgameGlob->axisFlagAway = away != 0;
            }
        }
        else
        {
            Scr_Error(SCRIPTINSTANCE_CLIENT, "Not enough parameters supplied to SetFlagAsAway", 0);
        }
    }
    else
    {
        Scr_Error(SCRIPTINSTANCE_CLIENT, "SetFlagAsAway must be called on a valid entity", 0);
    }
}

void __cdecl CScr_GetParentEntity(scr_entref_t entref)
{
    centity_s *cent; // [esp+8h] [ebp-8h]
    centity_s *parent; // [esp+Ch] [ebp-4h]

    cent = CG_GetEntity(entref.client, entref.entnum);
    parent = CG_EntGetLinkToParent(entref.client, cent);
    if ( parent )
        CScr_AddEntity(parent, entref.client);
    else
        Scr_AddUndefined(SCRIPTINSTANCE_CLIENT);
}

void (__cdecl *__cdecl CScr_GetMethodProjectSpecific(const char **pName, int *type))(scr_entref_t)
{
    unsigned int i; // [esp+18h] [ebp-4h]

    // Was a hardcoded 0x1D (== client_project_methods[]'s 29 MP rows), the same defect
    // CScr_GetFunction had: any appended row would never be compared. 0x1D ==
    // ARRAY_COUNT(client_project_methods) for the MP table, so MP behaviour is unchanged;
    // under KISAK_SP the table grows and this bound has to grow with it.
    for ( i = 0; i < ARRAY_COUNT(client_project_methods); ++i )
    {
        if ( !strcmp(*pName, client_project_methods[i].actionString) )
        {
            *pName = client_project_methods[i].actionString;
            *type = client_project_methods[i].type;
            return client_project_methods[i].actionFunc;
        }
    }
    return 0;
}

#ifdef KISAK_SP
cscr_sp_data_t cg_scr_sp_data;

// ===========================================================================
// SP consumer half, transcribed from FUN_00408ee0 (0x00408ee0) in retail
// BlackOps.exe -- the exact 1:1 partner of CGScr_LoadClientScripts_SP in
// cg_main_mp.cpp. THE TWO LISTS MUST STAY THE SAME LENGTH AND ORDER: each
// CScr_SetScriptAndLabel call consumes one slot of functions->address[] by
// position, and CGScr_LoadClientScriptsAndAnims Com_Error()s with "Script
// function count mismatch" if the counts diverge.
//
// Field mapping is not guesswork: SP writes cg_scr_data at 0x00c207d0 and
// every destination offset below matches cscr_data_t's declared layout
// exactly (delete_@0x00, initstructs@0x04, createstruct@0x08, findstruct@0x0c,
// levelscript@0x10, clientsysstatechange@0x14 ... gibEvent@0x64, sizeof 0x68).
// Note SP writes playerspawned (+0x50) and scriptmodelspawned (+0x28) in the
// opposite order to their struct positions -- transcribed as retail has it.
//
// *** THE "clientscripts/mp/_callbacks" STRINGS BELOW ARE DELIBERATE. ***
// Six rows -- playerspawned, the four CodeCallback_Player* rows, and
// glass_smash -- reference 0x00a39480 "clientscripts/mp/_callbacks" in retail
// SP's consumer even though the producer loads them from
// "clientscripts/_callbacks" (0x00a49b8c). CScr_SetScriptAndLabel uses these
// two strings ONLY to format its Com_Error text, so this is a cosmetic retail
// bug with no functional effect. If one of those six labels ever goes missing
// you will get an error naming a path SP never actually opened -- that is
// retail's behaviour, not a transcription error. Do not "fix" it without
// fixing the producer too, or the two halves stop matching the binary.
//
// The 4th argument (bEnforceExists) is read row-by-row from the decompile;
// a 0 there is what makes the absent clientscripts/_destructible(.csc) and
// clientscripts/_zombiemode(.csc) non-fatal.
// ===========================================================================
static void __cdecl CScr_SetUniqueClientScripts_SP(ScriptFunctions *functions)
{
    cg_scr_data.delete_               = CScr_SetScriptAndLabel(functions, "codescripts/delete", "main", 1);
    cg_scr_data.initstructs           = CScr_SetScriptAndLabel(functions, "codescripts/struct", "initstructs", 1);
    cg_scr_data.createstruct          = CScr_SetScriptAndLabel(functions, "codescripts/struct", "createstruct", 1);
    cg_scr_data.findstruct            = CScr_SetScriptAndLabel(functions, "codescripts/struct", "findstruct", 1);
    cg_scr_data.clientsysstatechange  = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "statechange", 1);
    cg_scr_data.maprestart            = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "maprestart", 1);
    cg_scr_data.localclientconnect    = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "localclientconnect", 1);
    cg_scr_data.localclientdisconnect = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "localclientdisconnect", 1);
    cg_scr_data.entityspawned         = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "entityspawned", 1);
    // retail-cosmetic "mp/" path, see header above
    cg_scr_data.playerspawned         = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "playerspawned", 1);
    cg_scr_data.scriptmodelspawned    = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "scriptmodelspawned", 1);
    cg_scr_data.clientFlagCB          = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "client_flag_callback", 0);
    cg_scr_data.clientFlagAsValCB     = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "client_flagasval_callback", 0);
    cg_scr_data.destructible_callback = CScr_SetScriptAndLabel(
                                            functions,
                                            "clientscripts/_destructible",
                                            "CodeCallback_DestructibleEvent",
                                            0);
    // retail-cosmetic "mp/" paths on the next four, see header above
    cg_scr_data.playerFootstep        = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "CodeCallback_PlayerFootstep", 1);
    cg_scr_data.playerJump            = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "CodeCallback_PlayerJump", 1);
    cg_scr_data.playerLand            = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "CodeCallback_PlayerLand", 1);
    cg_scr_data.playerFoliage         = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "CodeCallback_PlayerFoliage", 1);
    cg_scr_sp_data.aiFootstep         = CScr_SetScriptAndLabel(functions, "clientscripts/_footsteps", "playAIFootstep", 1);
    cg_scr_sp_data.activateExploder   = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "callback_activate_exploder", 1);
    cg_scr_sp_data.deactivateExploder = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "callback_deactivate_exploder", 1);
    cg_scr_data.levelnotify           = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "level_notify", 1);
    // SP reuses cscr_data_t's dogSoundNotify slot (+0x2c) for the generic
    // "sound_notify" callback; there is no _dogs script in the SP list at all.
    cg_scr_data.dogSoundNotify        = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "sound_notify", 0);
    cg_scr_sp_data.zombieEyeCallback  = CScr_SetScriptAndLabel(functions, "clientscripts/_zombiemode", "zombie_eye_callback", 0);
    cg_scr_sp_data.weaponDeathEffects = CScr_SetScriptAndLabel(
                                            functions,
                                            "clientscripts/_callbacks",
                                            "CodeCallback_PlayWeaponDeathEffects",
                                            0);
    cg_scr_sp_data.weaponDamageEffects = CScr_SetScriptAndLabel(
                                            functions,
                                            "clientscripts/_callbacks",
                                            "CodeCallback_PlayWeaponDamageEffects",
                                            0);
    cg_scr_data.airsupport            = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "airsupport", 1);
    cg_scr_data.entityshutdownCB      = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "entityshutdown_callback", 1);
    // retail-cosmetic "mp/" path, see header above
    cg_scr_data.glassSmash            = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "glass_smash", 0);
    cg_scr_data.gibEvent              = CScr_SetScriptAndLabel(functions, "clientscripts/_callbacks", "CodeCallback_GibEvent", 0);
}
#endif // KISAK_SP

void __cdecl CScr_SetUniqueClientScripts(ScriptFunctions *functions)
{
#ifdef KISAK_SP
    CScr_SetUniqueClientScripts_SP(functions);
    return;
#else
    cg_scr_data.delete_ = CScr_SetScriptAndLabel(functions, "codescripts/delete", "main", 1);
    cg_scr_data.initstructs = CScr_SetScriptAndLabel(functions, "codescripts/struct", "initstructs", 1);
    cg_scr_data.createstruct = CScr_SetScriptAndLabel(functions, "codescripts/struct", "createstruct", 1);
    cg_scr_data.findstruct = CScr_SetScriptAndLabel(functions, "codescripts/struct", "findstruct", 1);
    cg_scr_data.clientsysstatechange = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "statechange", 1);
    cg_scr_data.maprestart = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "maprestart", 1);
    cg_scr_data.localclientconnect = CScr_SetScriptAndLabel(
                                                                         functions,
                                                                         "clientscripts/mp/_callbacks",
                                                                         "localclientconnect",
                                                                         1);
    cg_scr_data.localclientdisconnect = CScr_SetScriptAndLabel(
                                                                                functions,
                                                                                "clientscripts/mp/_callbacks",
                                                                                "localclientdisconnect",
                                                                                1);
    cg_scr_data.entityspawned = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "entityspawned", 1);
    cg_scr_data.playerspawned = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "playerspawned", 1);
    cg_scr_data.clientFlagCB = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "client_flag_callback", 1);
    cg_scr_data.clientFlagAsValCB = CScr_SetScriptAndLabel(
                                                                        functions,
                                                                        "clientscripts/mp/_callbacks",
                                                                        "client_flagasval_callback",
                                                                        1);
    cg_scr_data.destructible_callback = CScr_SetScriptAndLabel(
                                                                                functions,
                                                                                "clientscripts/_destructible",
                                                                                "CodeCallback_DestructibleEvent",
                                                                                0);
    cg_scr_data.corpse_callback = CScr_SetScriptAndLabel(
                                                                    functions,
                                                                    "clientscripts/mp/_callbacks",
                                                                    "CodeCallback_CreatingCorpse",
                                                                    1);
    cg_scr_data.playerFootstep = CScr_SetScriptAndLabel(
                                                                 functions,
                                                                 "clientscripts/mp/_callbacks",
                                                                 "CodeCallback_PlayerFootstep",
                                                                 1);
    cg_scr_data.playerJump = CScr_SetScriptAndLabel(
                                                         functions,
                                                         "clientscripts/mp/_callbacks",
                                                         "CodeCallback_PlayerJump",
                                                         1);
    cg_scr_data.playerLand = CScr_SetScriptAndLabel(
                                                         functions,
                                                         "clientscripts/mp/_callbacks",
                                                         "CodeCallback_PlayerLand",
                                                         1);
    cg_scr_data.playerFoliage = CScr_SetScriptAndLabel(
                                                                functions,
                                                                "clientscripts/mp/_callbacks",
                                                                "CodeCallback_PlayerFoliage",
                                                                1);
    cg_scr_mp_data.dogstep = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_dogs", "playDogstep", 1);
    cg_scr_data.levelnotify = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "level_notify", 1);
    cg_scr_data.dogSoundNotify = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_dogs", "soundNotify", 1);
    cg_scr_mp_data.airsupport = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "airsupport", 1);
    cg_scr_mp_data.demo_jump = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "demo_jump", 1);
    cg_scr_mp_data.demo_playerSwitch = CScr_SetScriptAndLabel(
                                                                             functions,
                                                                             "clientscripts/mp/_callbacks",
                                                                             "demo_player_switch",
                                                                             1);
    cg_scr_data.entityshutdownCB = CScr_SetScriptAndLabel(
                                                                     functions,
                                                                     "clientscripts/mp/_callbacks",
                                                                     "entityshutdown_callback",
                                                                     1);
    cg_scr_data.glassSmash = CScr_SetScriptAndLabel(functions, "clientscripts/mp/_callbacks", "glass_smash", 0);
#endif // !KISAK_SP
}

void __cdecl CG_SendSwimNotify(int localClientNum, unsigned int clientNum, int start)
{
    unsigned __int16 swimming_begin; // [esp+2h] [ebp-2h]

    if ( start )
        swimming_begin = cscr_const.swimming_begin;
    else
        swimming_begin = cscr_const.swimming_end;
    CScr_NotifyNum(localClientNum, clientNum, 0, swimming_begin, 0);
}

void __cdecl CScr_GetEntityByIndex(centity_s *cent, const cent_field_s *pField)
{
    centity_s *Entity; // eax
    unsigned int index; // [esp+10h] [ebp-4h]

    if ( !cent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 1867, 0, "%s", "cent") )
    {
        __debugbreak();
    }
    if ( !pField
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 1868, 0, "%s", "pField") )
    {
        __debugbreak();
    }
    index = GetField((const int *)((char *)cent + pField->ofs), pField->size[0]);
    if ( index < 0x3FE )
    {
        Entity = CG_GetEntity(cent->pose.localClientNum, index);
        CScr_AddEntity(Entity, cent->pose.localClientNum);
    }
}

int __cdecl GetField(const int *i, int size)
{
    switch ( size )
    {
        case 1:
            return *(char *)i;
        case 2:
            return *(__int16 *)i;
        case 4:
            return *i;
    }
    if ( !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                    1861,
                    0,
                    "unknown field size") )
        __debugbreak();
    return 0;
}

void __cdecl CScr_GetTeamName(centity_s *cent, const cent_field_s *pField)
{
    team_t team; // [esp+0h] [ebp-4h]

    if ( !cent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 1902, 0, "%s", "cent") )
    {
        __debugbreak();
    }
    if ( !pField
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 1903, 0, "%s", "pField") )
    {
        __debugbreak();
    }
    team = GetTeam(cent);
    CScr_AddTeamName(team);
}

team_t __cdecl GetTeam(centity_s *cent)
{
    cg_s *cgameGlob; // [esp+0h] [ebp-8h]
    unsigned int localClientNum; // [esp+4h] [ebp-4h]

    if ( !cent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp", 1882, 0, "%s", "cent") )
    {
        __debugbreak();
    }
    if ( cent->nextState.eType != 1 )
        return (team_t)(cent->nextState.faction.iHeadIconTeam & 3);
    localClientNum = RETURN_ZERO32();
    if ( localClientNum >= 2
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                    1888,
                    0,
                    "localClientNum not in [0, MAX_LOCAL_CLIENTS]\n\t%i not in [%i, %i]",
                    localClientNum,
                    0,
                    1) )
    {
        __debugbreak();
    }
    cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    if ( cgameGlob->bgs.clientinfo[cent->nextState.clientNum].infoValid )
        return cgameGlob->bgs.clientinfo[cent->nextState.clientNum].team;
    else
        return TEAM_BAD;
}

unsigned __int16 __cdecl CScr_GetFootTag(eFoot foot)
{
    if ( (unsigned int)foot >= FOOTSTEP_COUNT
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_scr_main_mp.cpp",
                    1923,
                    0,
                    "foot doesn't index FOOTSTEP_COUNT\n\t%i not in [0, %i)",
                    foot,
                    4) )
    {
        __debugbreak();
    }
    return *footTags[foot];
}

float footprintGroundTraceUp = 15.0f;
float footprintGroundTraceDown = 15.0f;

#ifdef KISAK_SP
void __cdecl CScr_PlayAIFootstepSound_SP(int localClientNum, centity_s *cent, bool leftFoot, bool isDog)
{
    if (!cg_scr_sp_data.aiFootstep)
        return;
    DObj *obj = Com_GetClientDObj(cent->nextState.number, localClientNum);
    const cg_s *cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    float footPos[3];
    float axis[3][3];
    bool haveTag = false;
    if (obj && cgameGlob)
    {
        const float dz = cent->pose.origin[2] - cgameGlob->refdef.vieworg[2];
        const float dy = cent->pose.origin[1] - cgameGlob->refdef.vieworg[1];
        const float dx = cent->pose.origin[0] - cgameGlob->refdef.vieworg[0];
        const float distanceSq = (dz * dz + dy * dy) + dx * dx;
        if (distanceSq < 1048576.0f)
            haveTag = CG_DObjGetWorldTagMatrix(&cent->pose, obj,
                leftFoot ? scr_const.j_ball_le : scr_const.j_ball_ri, axis, footPos) != 0;
    }
    if (!haveTag)
    {
        footPos[0] = cent->pose.origin[0];
        footPos[1] = cent->pose.origin[1];
        footPos[2] = cent->pose.origin[2];
    }
    const float start[3] = {footPos[0], footPos[1], footPos[2] + footprintGroundTraceUp};
    const float end[3] = {footPos[0], footPos[1], footPos[2] - footprintGroundTraceDown};
    const int surface = (CM_TracePointDown(start, end, 0x02820011, 0x03f00000, footPos, NULL, NULL) >> 20) & 63;
    unsigned int count = 5;
    if (isDog)
    {
        Scr_AddInt(1, SCRIPTINSTANCE_CLIENT);
        count = 6;
    }
    Scr_AddInt((cent->currentState.eFlags2 & 0x200000u) != 0 || (cent->clientFlags & 0x20u) != 0, SCRIPTINSTANCE_CLIENT);
    Scr_AddString(Com_SurfaceTypeToName(surface), SCRIPTINSTANCE_CLIENT);
    Scr_AddVector(footPos, SCRIPTINSTANCE_CLIENT);
    Scr_AddEntityNum(cent->nextState.number, 0, SCRIPTINSTANCE_CLIENT, static_cast<unsigned __int16>(localClientNum));
    Scr_AddInt(localClientNum, SCRIPTINSTANCE_CLIENT);
    const unsigned __int16 thread = Scr_ExecThread(SCRIPTINSTANCE_CLIENT, cg_scr_sp_data.aiFootstep, count);
    Scr_FreeThread(thread, SCRIPTINSTANCE_CLIENT);
}
#endif

void __cdecl CScr_PlayDogstepSound(int localClientNum, centity_s *cent, eFoot foot)
{
    unsigned __int16 FootTag; // ax
    char *value; // [esp+0h] [ebp-68h]
    unsigned __int16 t; // [esp+14h] [ebp-54h]
    DObj *obj; // [esp+18h] [ebp-50h]
    float start[3]; // [esp+1Ch] [ebp-4Ch] BYREF
    float end[3]; // [esp+28h] [ebp-40h] BYREF
    int surfType; // [esp+34h] [ebp-34h]
    float footMatrix[4][3]; // [esp+38h] [ebp-30h] BYREF

    if ( cg_scr_mp_data.dogstep )
    {
        obj = Com_GetClientDObj(cent->nextState.number, localClientNum);
        if ( !obj
            || (FootTag = CScr_GetFootTag(foot),
                    !CG_DObjGetWorldTagMatrix(&cent->pose, obj, FootTag, footMatrix, footMatrix[3])) )
        {
            footMatrix[3][0] = cent->pose.origin[0];
            footMatrix[3][1] = cent->pose.origin[1];
            footMatrix[3][2] = cent->pose.origin[2];
        }
        *(_QWORD *)start = *(_QWORD *)&footMatrix[3][0];
        start[2] = footMatrix[3][2] + footprintGroundTraceUp;
        *(_QWORD *)end = *(_QWORD *)&footMatrix[3][0];
        end[2] = footMatrix[3][2] - footprintGroundTraceDown;
        surfType = (CM_TracePointDown(start, end, 2065, 0x3F00000, footMatrix[3], 0, 0) & 0x3F00000) >> 20;
        if ( (cent->currentState.eFlags2 & 0x200000) != 0 || ((cent->clientFlags >> 5) & 1) != 0 )
            Scr_AddInt(1, SCRIPTINSTANCE_CLIENT);
        else
            Scr_AddInt(0, SCRIPTINSTANCE_CLIENT);
        value = (char *)Com_SurfaceTypeToName(surfType);
        Scr_AddString(value, SCRIPTINSTANCE_CLIENT);
        Scr_AddVector(footMatrix[3], SCRIPTINSTANCE_CLIENT);
        CScr_AddEntity(cent, localClientNum);
        Scr_AddInt(localClientNum, SCRIPTINSTANCE_CLIENT);
        t = Scr_ExecThread(SCRIPTINSTANCE_CLIENT, cg_scr_mp_data.dogstep, 5u);
        Scr_FreeThread(t, SCRIPTINSTANCE_CLIENT);
    }
}

