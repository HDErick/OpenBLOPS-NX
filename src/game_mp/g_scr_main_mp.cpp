#include <bgame/bg_unlock_policy.h>
#include "g_checkpoint_sp.h"
#include <cgame/cg_spawn.h>
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
#include <client/splitscreen.h>
#endif
#ifdef KISAK_SP
#include <win32/win_gamerprofile.h>
#include <qcommon/com_clients.h>
#include <live/live_win.h>
#endif
#include <limits>
#include <bit>
#include <server_mp/sv_offline_stats.h>
#include <live/live_stats.h>          // LiveStats_GetCacDDLState (getloadoutitemfromprofile)
#include <live/live_storage.h>        // LiveStorage_GetStatsBuffer
#include <ddl/ddl_api.h>              // DDL_MoveTo, DDL_GetInt
#include <ui_mp/ui_gametype_custom_mp.h>   // ui_useCustomClassInfo, GetCustomClassLoadoutItemForClassName
#include <game/g_sp_crosshair.h>
#include "g_scr_main_mp.h"
#include <qcommon/com_gamemodes.h>
#ifdef KISAK_SP
#include <qcommon/common.h>                // zombiemode, SpawnVar
#include <game/g_load_utils.h>             // G_ParseSpawnVars, G_SpawnString, parse point
#include <game/actor_state.h>              // Actor_PushState / Actor_PopState
#include <game/actor_badplace.h>
#include <game/actor_grenade.h>
#include <game/actor_navigation.h>
#include <game/actor_aim.h>                // Actor_FillWeaponParms, retail getweaponforwarddir actor branch
#include <game/g_scr_vehicle.h>
#include <universal/q_shared.h>            // I_strnicmp, I_stricmp, I_strncpyz
#include <bgame/bg_weapons_def.h>          // BG_GetWeaponDef, BG_GetWeaponIndexForName
#include <xanim/xanim.h>                   // XAnim scripted root-motion alignment
#include <clientscript/cscr_animtree.h>    // Scr_GetAnimsIndex, retail SP anim-tree identity handoff
#include <universal/com_math.h>            // matrix/root-motion helpers
#include <cstddef>                         // offsetof, for the WeaponDef layout asserts
#include <climits>
#include <cmath>                           // nearbyint, matching retail x87 FISTP rounding
#include <game_mp/g_anim_commands_sp.h>
#include <game/actor_corpse.h>
#include <clientscript/cscr_memorytree.h>
#include <client/client.h>                 // retail SP player-count connection-state gate
#include <physics/destructible.h>
#include <physics/rope.h>
#include <qcommon/cm_load.h>
#include <qcommon/cm_staticmodel.h>
#include <DynEntity/DynEntity_client.h>
#include <EffectsCore/fx_system.h>
#endif
#ifdef KISAK_SP
// GScr_LoadGameTypeScript's "%s" selector is ui_gametype on SP, not g_gametype -- see the
// retail-verified note there. Declared in ui/ui_main.h:404.
#include <ui/ui_main.h>
#endif
#include <clientscript/cscr_vm.h>
#include <clientscript/cscr_parser.h>
#include "g_main_mp.h"
#include <cgame/cg_scr_main.h>
#include "g_spawn_mp.h"
#include <clientscript/cscr_stringlist.h>
#include <universal/com_memory.h>
#include <client/con_channels.h>
#include <server/sv_game.h>
#include <server_mp/sv_main_mp.h>
#include <client/cl_debugdata.h>
#include <cgame/cg_drawtools.h>
#include <game/g_debug.h>
#include <game/g_weapon.h>
#ifdef KISAK_SP
#include <game/bullet.h>
#endif
#include <xanim/xmodel_utils.h>
#include <bgame/bg_weapons_ammo.h>
#include "g_utils_mp.h"
#include <clientscript/scr_const.h>
#include <server/sv_world.h>
#include <game/turret.h>
#include <game/g_scr_helicopter.h>
#include <qcommon/dobj_management.h>
#include <xanim/dobj_utils.h>
#include <game/actor_script_cmd.h>
#include <game/actor_threat.h>
#ifdef KISAK_SP
#include <game/actor_event_listeners.h>
#endif
#include <game_mp/actor_mp.h>
#include "g_combat_mp.h"
#include <game/g_mover.h>
#include <server_mp/sv_init_mp.h>
#include <bgame/bg_perks.h>
#include <demo/demo_recording.h>
#include <qcommon/cm_world.h>
#include <bgame/bg_weapons_load_obj.h>
#include "g_client_script_cmd_mp.h"
#include <universal/surfaceflags.h>
#include <universal/com_math_anglevectors.h>
#include <cstring>
#include <bgame/bg_misc.h>
#include <database/db_assetnames.h>
#include <glass/glass_server.h>
#include <game/g_missile.h>
#include "g_active_mp.h"
#include <game/actor_spawner.h>
#include <gfx_d3d/r_reflection_probe.h>
#ifdef KISAK_SP
#include <gfx_d3d/r_cinematic.h>
#include <gfx_d3d/r_dvars.h>
#endif
#include "g_cmds_mp.h"
#include <client_mp/g_client_mp.h>
#include <client_mp/sv_client_mp.h>
#include <server_mp/sv_main_pc_mp.h>
#include <universal/com_files.h>
#include <universal/q_parse.h>
#include <DW/MatchRecorder.h>
#include <client/splitscreen.h>
#include <bgame/bg_unlockable_items.h>
#include <live/live_stats.h>
#include <live/live_contracts.h>
#include <ui_mp/ui_gametype_custom_mp.h>
#include <gfx_d3d/r_fog.h>
#include <game/bullet.h>
#include <cgame_mp/cg_ents_mp.h>
#include <cgame/cg_event.h>
#include "g_spawnsystem_mp.h"
#include <ui_mp/ui_gametype_variants_mp.h>
#include "pregame.h"
#include <live/live_storage_win.h>
#include <live/live_counter.h>
#include <game/g_client_fields.h>
#include <game/g_scr_mover.h>
#include "actor_mp.h"
#include <win32/win_shared.h>
#include <qcommon/cm_load.h>
#include <game/g_targets.h>
#include <stringed/stringed_hooks.h>
#include <gfx_d3d/r_dpvs.h>
#include <sound/snd_bank.h>
#ifdef KISAK_SP
#include <sound/snd_utils.h>
#endif
#include <gfx_d3d/r_primarylights.h>

#ifdef KISAK_SP
static void GScr_SetHeliHeightPatchEnabled_SP();
#endif

scr_data_t g_scr_data;

void assertCmd()
{
    if ( !Scr_GetInt(0, SCRIPTINSTANCE_CLIENT) )
        Scr_Error(SCRIPTINSTANCE_CLIENT, "assert fail", 1);
}

void assertexCmd()
{
    char *error; // [esp+0h] [ebp-8h]
    char *String; // [esp+4h] [ebp-4h]

    if ( !Scr_GetInt(0, SCRIPTINSTANCE_CLIENT) )
    {
        String = Scr_GetString(1u, SCRIPTINSTANCE_CLIENT);
        error = va("assert fail: %s", String);
        Scr_Error(SCRIPTINSTANCE_CLIENT, error, 1);
    }
}

void assertmsgCmd()
{
    char *error; // [esp+0h] [ebp-8h]
    char *String; // [esp+4h] [ebp-4h]

    String = Scr_GetString(0, SCRIPTINSTANCE_CLIENT);
    error = va("assert fail: %s", String);
    Scr_Error(SCRIPTINSTANCE_CLIENT, error, 1);
}

void print()
{
    char *DebugString; // [esp+0h] [ebp-Ch]
    int num; // [esp+4h] [ebp-8h]
    signed int i; // [esp+8h] [ebp-4h]

    num = Scr_GetNumParam(SCRIPTINSTANCE_CLIENT);
    for (i = 0; i < num; ++i)
    {
        DebugString = Scr_GetDebugString(i, SCRIPTINSTANCE_CLIENT);
        Com_Printf(cg_level.scriptPrintChannel, "%s", DebugString);
    }
}

void println()
{
    print();
    Com_Printf(cg_level.scriptPrintChannel, "\n");
}

void GScr_IsCollectors()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_ParamError(0, "Usage : IsCollectors [player]", SCRIPTINSTANCE_SERVER);
    if ( !Scr_GetEntity(0) )
        Scr_ParamError(0, "IsCollectors Error: param 1 is not an entity.", SCRIPTINSTANCE_SERVER);
    Scr_AddBool(0, SCRIPTINSTANCE_SERVER);
}

unsigned int __cdecl    GScr_AllocString(const char *s)
{
    return Scr_AllocString(s, 1, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_LoadLevel()
{
    unsigned __int16 t; // [esp+0h] [ebp-4h]

    if ( g_scr_data.levelscript )
    {
        t = Scr_ExecThread(SCRIPTINSTANCE_SERVER, g_scr_data.levelscript, 0);
        Scr_FreeThread(t, SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl Scr_LoadPreGame()
{
    unsigned __int16 t; // [esp+0h] [ebp-4h]

    if (g_scr_data.pregamescript)
    {
        t = Scr_ExecThread(SCRIPTINSTANCE_SERVER, g_scr_data.pregamescript, 0);
        Scr_FreeThread(t, SCRIPTINSTANCE_SERVER);
    }
}

#ifdef KISAK_SP
void __cdecl Scr_LoadInitMode_SP()
{
    if (g_scr_data.init_mode_sp)
    {
        const unsigned __int16 thread = Scr_ExecThread(SCRIPTINSTANCE_SERVER, g_scr_data.init_mode_sp, 0);
        Scr_FreeThread(thread, SCRIPTINSTANCE_SERVER);
    }
}
#endif

// SP script-path prefixes. SP's compiled GSC assets live under bare "maps/" and
// "maps/gametypes/", never "maps/mp/...". Evidence, measured by decompressing the shipped
// zones and reading each asset's payload length (so "exists" is a real asset, not a stray
// string match):
//   maps/_destructible      1793 B in code_post_gfx.ff   (MP: maps/mp/_destructible, common_mp.ff)
//   maps/_callbacksetup     2701 B in code_post_gfx.ff   (MP: maps/mp/gametypes/_callbacksetup, 2141 B)
//   maps/frontend.gsc       7926 B in frontend.ff        (MP: maps/mp/<map>)
//   maps/gametypes/cmp.gsc    17 B in code_post_gfx.ff   (see the TODO in GScr_LoadGameTypeScript)
//   animscripts/dog_combat.gsc 7682 B in common.ff       (MP: maps/mp/animscripts/<name>)
//   animscripts/traverse/*     9 assets in common.ff and frontend.ff
//                                                        (MP: maps/mp/animscripts/traverse/*, 11 in common_mp.ff)
// None of the MP spellings appear in ANY zone SP loads, so every one of these is a guaranteed
// Com_Error(ERR_DROP, "Could not find script '%s'") at GScr_LoadScriptAndLabel
// (g_scr_main_mp.cpp:243) once SV_SpawnServer("frontend") reaches G_InitGame -> GScr_LoadScripts.
// Audit finding B3 (frontend-map-load audit).
// TODO(SP): the CodeCallback_* LABELS inside maps/_callbacksetup are unverified. Compiled GSC
// stores no plaintext label names (control: the string "CodeCallback" has zero hits in BOTH the
// SP and MP zones, so the scan is blind here rather than the labels being absent), so all that
// is proven is that the SP script exists and is larger than MP's. If a label is genuinely
// missing, GScr_LoadScriptAndLabel will Com_Error "Could not find label '<X>' in script
// 'maps/_callbacksetup'" - which names the exact missing label, so a run settles it instantly.
// The MP path fatals strictly earlier ("Could not find script"), so this is unambiguously an
// improvement either way. Ghidra is unavailable in this session.
#ifdef KISAK_SP
#define GSCR_DESTRUCTIBLE_SCRIPT  "maps/_destructible"
#define GSCR_CALLBACKSETUP_SCRIPT "maps/_callbacksetup"
#define GSCR_GAMETYPE_DIR         "maps/gametypes/"
#define GSCR_LEVEL_DIR            "maps/"
#define GSCR_ANIMSCRIPTS_DIR      "animscripts/"
#else
#define GSCR_DESTRUCTIBLE_SCRIPT  "maps/mp/_destructible"
#define GSCR_CALLBACKSETUP_SCRIPT "maps/mp/gametypes/_callbacksetup"
#define GSCR_GAMETYPE_DIR         "maps/mp/gametypes/"
#define GSCR_LEVEL_DIR            "maps/mp/"
#define GSCR_ANIMSCRIPTS_DIR      "maps/mp/animscripts/"
#endif

void __cdecl GScr_LoadGameTypeScript()
{
    char filename[68]; // [esp+0h] [ebp-48h] BYREF

#ifdef KISAK_SP
    // *** RETAIL-VERIFIED 2026-08-26 against GScr_LoadGameTypeScript (0x006124b0), read by
    //     disassembly and by read_memory on every string operand. Two facts about THIS load were
    //     wrong and are corrected here; a third is confirmed. ***
    //
    //  (a) THE LABEL IS "init", NOT "main". Retail 0x00612a78 pushes 0x00a0ff44, which reads
    //      byte-for-byte as "init\0". This is safe to change unconditionally: bEnforceExists is
    //      already 0 on this path, so a missing label cannot become a Com_Error either way.
    //
    //  (b) THE "%s" SELECTOR IS ui_gametype, NOT g_gametype. Retail 0x00612a52 loads the cached
    //      dvar pointer DAT_02562a14 and 0x00612a61 reads its +0x18 (current.string) straight
    //      into the va() call at 0x00612a6a. DAT_02562a14 is identified by the registrar
    //      string-table walk, run against this binary this pass: its sole WRITE is 0x00836079,
    //      and the name pushed for the register call one earlier (0x00836056) is 0x009c69c0,
    //      which reads "ui_gametype". Retail registers it as a string dvar with value "" and
    //      flags 0 -- exactly this tree's ui_main.cpp:3389. The next registration in program
    //      order is 0x009c3cb0 ("ui_mapname"), which is the expected neighbour.
    //      Retail additionally guards the WHOLE block on the pointer being non-NULL
    //      (0x00612a5d TEST EAX,EAX / JZ 0x00612a8c), which is transcribed below -- ui_gametype
    //      is registered by UI init, and this runs from the server side.
    //
    //  (c) CONFIRMED, no change needed: retail stores this handle with NO error check at all.
    //      The Scr_LoadScript at 0x00612a73 is not tested, and the Scr_GetFunctionHandle result
    //      at 0x00612a7f is stored to 0x01c8750c at 0x00612a87 with no TEST/Com_Error --
    //      unlike all the CodeCallback_* sites around it. That independently VINDICATES this
    //      file's existing bEnforceExists = 0 under KISAK_SP, which had been reasoned to from
    //      asset contents rather than from the binary.
    //
    // The original bEnforceExists reasoning, retained because it is still true and still the
    // reason a future pass must not "restore" the 1:
    // bEnforceExists is 0 on SP: SP has no usable campaign gametype script, so requiring one is
    // an unconditional fatal.
    // Evidence: SP ships maps/gametypes/{cmp,arc,bnk,zom,sop}.txt but a .gsc for cmp only, and
    // that cmp.gsc is EMPTY - its stored payload is 17 bytes, of which the 9-byte zlib stream
    // (78 9c 63 00 00 00 01 00 01) inflates to a single 0x00 byte. There is no room for a "main"
    // label, so even with the path corrected this load can only ever produce
    // Com_Error(ERR_DROP, "Could not find label 'main' in script 'maps/gametypes/cmp'").
    // Additionally g_gametype is currently stomped to "dm" before we get here (see the TODO(SP)
    // in SV_SetGametype, sv_game.cpp), and no dm/tdm script exists in any SP zone.
    // Keeping the load (rather than deleting it) means a zone that DOES ship a real gametype
    // script - e.g. a zombie map's own .ff - still works; it just no longer fatals when absent.
    // The consumer is guarded to match: Scr_LoadGameType (below) skips its assert+exec when the
    // handle is 0, so this does not repeat the "suppress the load, leave the consumer running"
    // mistake.
    // RESOLVED, replacing the old "unconfirmed whether retail SP calls this at all" note: retail
    // SP does call it, from GScr_LoadGameTypeScript (0x006124b0), and the block is reached
    // whenever ui_gametype is registered.
    //
    // RUNTIME-CONFIRMED 2026-08-26 (90s SP run of this build, console_mp.log): G_InitGame DOES
    // reach GScr_LoadScripts on the frontend load -- the log carries exactly one new line,
    // `Error: Could not load rawfile "maps/gametypes/.gsc"`, which is this call with
    // ui_gametype's registered default of "". So ui_gametype is non-NULL here (the guard does not
    // trip) and bEnforceExists = 0 correctly keeps the miss non-fatal. That both answers the old
    // open question and exercises the whole callback block: the same log has ZERO
    // "Could not find label" and ZERO "Could not find script" lines in 630 KB.
    //
    // Loose end for a future pass, recorded because the log makes it visible: ls_gametype is
    // "cmp" at this moment while ui_gametype is still "". Reading ui_gametype is what retail
    // does and is not the bug; something else is meant to populate it before a level load, and
    // nothing in this tree does. Until then this load can only ever miss -- which is the same
    // outcome the old g_gametype spelling had, so this is not a regression.
    if ( ui_gametype )
    {
        Com_sprintf(filename, 0x40u, GSCR_GAMETYPE_DIR "%s", ui_gametype->current.string);
        g_scr_data.gametype.main = GScr_LoadScriptAndLabel(SCRIPTINSTANCE_SERVER, filename, "init", 0);
    }
    else
    {
        // Retail simply skips the store here (it never writes 0x01c8750c on this path). Zeroing
        // is the equivalent for a function that re-runs per level load, and it is what
        // Scr_LoadGameType's own handle==0 guard below already expects.
        g_scr_data.gametype.main = 0;
    }
#else
    Com_sprintf(filename, 0x40u, GSCR_GAMETYPE_DIR "%s", g_gametype->current.string);
    g_scr_data.gametype.main = GScr_LoadScriptAndLabel(SCRIPTINSTANCE_SERVER, filename, "main", 1);
#endif
    g_scr_data.gametype.startupgametype = GScr_LoadScriptAndLabel(
                                                                                    SCRIPTINSTANCE_SERVER,
                                                                                    GSCR_CALLBACKSETUP_SCRIPT,
                                                                                    "CodeCallback_StartGameType",
                                                                                    1);
    g_scr_data.gametype.playerconnect = GScr_LoadScriptAndLabel(
                                                                                SCRIPTINSTANCE_SERVER,
                                                                                GSCR_CALLBACKSETUP_SCRIPT,
                                                                                "CodeCallback_PlayerConnect",
                                                                                1);
    g_scr_data.gametype.playerdisconnect = GScr_LoadScriptAndLabel(
                                                                                     SCRIPTINSTANCE_SERVER,
                                                                                     GSCR_CALLBACKSETUP_SCRIPT,
                                                                                     "CodeCallback_PlayerDisconnect",
                                                                                     1);
    g_scr_data.gametype.playerdamage = GScr_LoadScriptAndLabel(
                                                                             SCRIPTINSTANCE_SERVER,
                                                                             GSCR_CALLBACKSETUP_SCRIPT,
                                                                             "CodeCallback_PlayerDamage",
                                                                             1);
    g_scr_data.gametype.playerkilled = GScr_LoadScriptAndLabel(
                                                                             SCRIPTINSTANCE_SERVER,
                                                                             GSCR_CALLBACKSETUP_SCRIPT,
                                                                             "CodeCallback_PlayerKilled",
                                                                             1);
    g_scr_data.gametype.actordamage = GScr_LoadScriptAndLabel(
                                                                            SCRIPTINSTANCE_SERVER,
                                                                            GSCR_CALLBACKSETUP_SCRIPT,
                                                                            "CodeCallback_ActorDamage",
                                                                            1);
    g_scr_data.gametype.actorkilled = GScr_LoadScriptAndLabel(
                                                                            SCRIPTINSTANCE_SERVER,
                                                                            GSCR_CALLBACKSETUP_SCRIPT,
                                                                            "CodeCallback_ActorKilled",
                                                                            1);
    g_scr_data.gametype.vehicledamage = GScr_LoadScriptAndLabel(
                                                                                SCRIPTINSTANCE_SERVER,
                                                                                GSCR_CALLBACKSETUP_SCRIPT,
                                                                                "CodeCallback_VehicleDamage",
                                                                                1);
#ifndef KISAK_SP
    // SP retail divergence (2026-08-07). SP's callbacksetup script does not define this callback.
    // Verified by inflating SP's maps/_callbacksetup.gsc out of code_post_gfx.ff (2693 -> 11697
    // bytes) and counting: it defines exactly 18 CodeCallback_* functions, and
    // "VehicleRadiusDamage" occurs ZERO times ("VehicleDamage" occurs 4). MP control:
    // common_mp.ff's maps/mp/gametypes/_callbacksetup.gsc does define it. Every one of the other 12
    // labels this file requests IS present in the SP script, so this is a single genuine gap, not
    // a wrong path. GScr_LoadScriptAndLabel passes bEnforceExists = 1, making this a hard
    // Com_Error(ERR_DROP) inside G_InitGame on the frontend load.
    // See the matching early-return in Scr_VehicleRadiusDamage below -- guarded as a coherent unit.
    g_scr_data.gametype.vehicleradiusdamage = GScr_LoadScriptAndLabel(
                                                                                            SCRIPTINSTANCE_SERVER,
                                                                                            GSCR_CALLBACKSETUP_SCRIPT,
                                                                                            "CodeCallback_VehicleRadiusDamage",
                                                                                            1);
#endif
    g_scr_data.gametype.playerlaststand = GScr_LoadScriptAndLabel(
                                                                                    SCRIPTINSTANCE_SERVER,
                                                                                    GSCR_CALLBACKSETUP_SCRIPT,
                                                                                    "CodeCallback_PlayerLastStand",
                                                                                    1);

    g_scr_data.levelnotify = GScr_LoadScriptAndLabel(
                                        SCRIPTINSTANCE_SERVER,
                                        GSCR_CALLBACKSETUP_SCRIPT,
                                        "CodeCallback_LevelNotify",
                                        1);
#ifdef KISAK_SP
    g_scr_data.actorShouldReact = GScr_LoadScriptAndLabel(SCRIPTINSTANCE_SERVER,
        "maps/_callbacksetup", "CodeCallback_ActorShouldReact", true);
#endif
    g_scr_data.faceeventnotify = GScr_LoadScriptAndLabel(
                         SCRIPTINSTANCE_SERVER,
                         GSCR_CALLBACKSETUP_SCRIPT,
                         "CodeCallback_FaceEventNotify",
                         0);
#ifdef KISAK_SP
    // *** RETAIL-VERIFIED 2026-08-27 against GScr_LoadGameTypeScript (0x006124b0) by disassembly
    //     plus read_memory on every string operand. ***
    //
    // These are retail slots 18 and 19, emitted in exactly this position -- immediately before
    // CodeCallback_GlassSmash (slot 20) and after the maps/gametypes/<ui_gametype> handle
    // (slot 17). Ordering transcribed from the binary, not assumed.
    //
    //   0x00612ab2  PUSH 0x9e6b68 ("CodeCallback_MenuMessage")  / PUSH 0x9d6aa8
    //               ("maps/_callbacksetup") / CALL 0x004e3470
    //   0x00612ac7  TEST EDI,EDI / JNZ 0x00612ae4 -> the fallthrough calls Com_Error(0x00651d90)
    //               with 0x9a7c7c ("\x15Could not find label '%s' in script '%s'")
    //   0x00612aea  MOV [0x01c87514],EDI
    //
    //   0x00612b10  PUSH 0x9eeee0 ("CodeCallback_Dec20Message") / PUSH 0x9d6aa8 / CALL 0x004e3470
    //   0x00612b25  TEST EDI,EDI / JNZ 0x00612b42 -> same Com_Error tail
    //   0x00612b48  MOV [0x01c87518],EDI
    //
    // So bEnforceExists = 1 on BOTH -- read out of the binary, not inferred. Contrast the
    // gametype handle at 0x00612a87 (MOV [0x01c8750c],EAX with no preceding TEST), which is why
    // that one stays 0 above, and CodeCallback_GlassSmash at 0x00612b5e..0x00612b67 (again no
    // TEST), which is why the load below stays 0.
    //
    // HAZARD, recorded deliberately: bEnforceExists = 1 makes a missing label a hard
    // Com_Error(ERR_DROP) inside G_InitGame, i.e. the frontend load dies. Both labels were found
    // in the extracted SP code_post_gfx/maps/_callbacksetup.gsc (CodeCallback_MenuMessage taking
    // two params, with CodeCallback_Dec20Message immediately after), and the count cross-check in
    // the note below holds: retail asks _callbacksetup for exactly eighteen labels and the
    // inflated SP script defines exactly eighteen CodeCallback_* functions. If a future run ever
    // does drop here, the error names the exact missing label -- fall back to 0 on that one and
    // guard its consumer on handle != 0.
    g_scr_data.menumessage = GScr_LoadScriptAndLabel(
                                        SCRIPTINSTANCE_SERVER,
                                        GSCR_CALLBACKSETUP_SCRIPT,
                                        "CodeCallback_MenuMessage",
                                        1);
    g_scr_data.dec20message = GScr_LoadScriptAndLabel(
                                        SCRIPTINSTANCE_SERVER,
                                        GSCR_CALLBACKSETUP_SCRIPT,
                                        "CodeCallback_Dec20Message",
                                        1);
#endif
    g_scr_data.glassSmash = GScr_LoadScriptAndLabel(
                                        SCRIPTINSTANCE_SERVER,
                                        GSCR_CALLBACKSETUP_SCRIPT,
                                        "CodeCallback_GlassSmash",
                                        0);
#ifdef KISAK_SP
    // TODO(SP): retail's callback list is LONGER than this one. Recorded here rather than
    // transcribed, deliberately -- see the reason at the bottom.
    //
    // The full retail list was read out of GScr_LoadGameTypeScript (0x006124b0) this pass by
    // disassembly plus a read_memory on every single string operand (no decompiler inference).
    // In retail order, with the script each is loaded from and whether the site carries the
    // Com_Error check (i.e. this file's bEnforceExists):
    //
    //     maps/_callbacksetup:
    //       1  CodeCallback_SaveRestored            enforce 1   *** NOT LOADED HERE ***
    //       2  CodeCallback_StartGameType           enforce 1
    //       3  CodeCallback_PlayerConnect           enforce 1
    //       4  CodeCallback_PlayerDisconnect        enforce 1
    //       5  CodeCallback_ActorDamage             enforce 1
    //       6  CodeCallback_PlayerDamage            enforce 1
    //       7  CodeCallback_PlayerKilled            enforce 1
    //       8  CodeCallback_ActorKilled             enforce 1
    //       9  CodeCallback_PlayerRevive            enforce 1   *** NOT LOADED HERE ***
    //      10  CodeCallback_PlayerLastStand         enforce 1
    //      11  CodeCallback_LevelNotify             enforce 1
    //      12  CodeCallback_VehicleDamage           enforce 1
    //      13  CodeCallback_ActorShouldReact        enforce 1   *** NOT LOADED HERE ***
    //      14  CodeCallback_FaceEventNotify         enforce 0
    //      15  CodeCallback_DisconnectedDuringLoad  enforce 1   *** NOT LOADED HERE ***
    //     maps/_destructible:
    //      16  CodeCallback_DestructibleEvent       enforce 1   (this tree loads it, but from
    //                                                            GScr_LoadScripts instead)
    //     maps/gametypes/<ui_gametype>:
    //      17  init                                 enforce 0   (implemented above)
    //     maps/_callbacksetup:
    //      18  CodeCallback_MenuMessage             enforce 1   (LOADED as of 2026-08-27)
    //      19  CodeCallback_Dec20Message            enforce 1   (LOADED as of 2026-08-27)
    //      20  CodeCallback_GlassSmash              enforce 0
    //
    // Cross-check worth keeping: retail requests exactly EIGHTEEN labels from _callbacksetup
    // (1-15 plus 18-20), and this file's own note at GScr_LoadGameTypeScript records that SP's
    // inflated maps/_callbacksetup.gsc defines exactly eighteen CodeCallback_* functions. Those
    // two independent counts agreeing is a strong sign the list above is complete and that every
    // one of the six missing labels really does exist in SP's script.
    //
    // *** DISCREPANCY WITH THE HANDOFF NOTE THIS PASS WAS GIVEN, recorded because the next pass
    //     will meet the same note: it listed these as bare names -- "SaveRestored",
    //     "StartGameType", "PlayerConnect", ... -- with the CodeCallback_ prefix present on
    //     DestructibleEvent only. In the binary EVERY one of the nineteen carries the
    //     CodeCallback_ prefix. The note's ORDER and its enforce-0 positions (FaceEventNotify,
    //     GlassSmash, and the gametype handle) are correct; only the spellings were short. ***
    //
    // UPDATE 2026-08-27: MenuMessage and Dec20Message ARE now loaded, above, exactly per the
    // rule the paragraph below lays down -- their storage (scr_data_t::menumessage /
    // ::dec20message) and their consumer (Cmd_MenuLevelMessage_f, g_cmds_mp.cpp, dispatched from
    // ClientCommand on "mlvl") landed in the same change. Dec20Message has storage but still no
    // consumer; it is loaded only because retail loads it and the label is present, so its handle
    // costs nothing.
    //
    // WHY THE REMAINING FOUR ARE NOT ADDED: three of them (SaveRestored, ActorShouldReact,
    // DisconnectedDuringLoad) have no field in scr_data_t_unnamed_type_gametype at all, and the
    // fourth (playerrevive) has a field that NOTHING in this tree reads. Adding them therefore
    // buys no behaviour, while each one is a new bEnforceExists=1 site, i.e. a new hard
    // Com_Error(ERR_DROP) at level load if any single assumption about SP's shipped
    // _callbacksetup is wrong. That trade is the wrong way round. A future pass that wants them
    // should add the storage fields and their consumers first, and land the loads as part of that
    // unit.
    //
    // ALSO NOT DONE, deliberately: retail loads CodeCallback_DestructibleEvent from inside THIS
    // function (slot 16), where this tree loads it from GScr_LoadScripts. Both run inside the
    // same Scr_BeginLoadScripts/Scr_EndLoadScripts window with the same script, label and
    // enforce flag, so moving it is pure restructuring with no behavioural difference. Retail's
    // ordering of the ActorDamage/PlayerDamage/VehicleDamage group also differs from this file's;
    // each load is independent, so that too is cosmetic.
#endif
}

int __cdecl GScr_LoadScriptAndLabel(scriptInstance_t inst, const char *filename, const char *label, int bEnforceExists)
{
    int func; // [esp+4h] [ebp-4h]

    if ( !g_loadScripts || !g_loadScripts->current.enabled )
        return 0;
    if ( !Scr_LoadScript(inst, (char*)filename) && bEnforceExists )
        Com_Error(ERR_DROP, "Could not find script '%s'", filename);
    func = Scr_GetFunctionHandle(inst, filename, label);
    if ( !func )
    {
        if ( bEnforceExists )
            Com_Error(ERR_DROP, "Could not find label '%s' in script '%s'", label, filename);
    }
    return func;
}

void __cdecl    GScr_LoadScripts(scriptInstance_t inst)
{
    Scr_BeginLoadScripts(inst, 1);

    g_scr_data.delete_ = GScr_LoadScriptAndLabel(inst, "codescripts/delete", "main", 1);
    g_scr_data.initstructs = GScr_LoadScriptAndLabel(inst, "codescripts/struct", "initstructs", 1);
    g_scr_data.createstruct = GScr_LoadScriptAndLabel(inst, "codescripts/struct", "createstruct", 1);
    g_scr_data.findstruct = GScr_LoadScriptAndLabel(inst, "codescripts/struct", "findstruct", 1);
    g_scr_data.destructible_callback = GScr_LoadScriptAndLabel(
        inst,
        GSCR_DESTRUCTIBLE_SCRIPT,
        "CodeCallback_DestructibleEvent",
        1);
#ifndef KISAK_SP
    g_scr_data.updatespawnpoints = GScr_LoadScriptAndLabel(
        inst,
        "maps/mp/gametypes/_spawning",
        "CodeCallback_UpdateSpawnPoints",
        1);
#else
    // MP spawn-point management has no SP counterpart and no SP asset to point at.
    // Evidence: "_spawning.gsc" occurs exactly once across all shipped zones - in common_mp.ff -
    // and zero times in every SP zone (code_pre_gfx, code_post_gfx, common, patch, frontend).
    // Neither "maps/_spawning" nor "maps/gametypes/_spawning" exists. With bEnforceExists=1 this
    // is a guaranteed Com_Error(ERR_DROP, "Could not find script 'maps/mp/gametypes/_spawning'")
    // inside G_InitGame -> GScr_LoadScripts. Audit finding B3 (frontend-map-load audit).
    // Consumer checked, per this project's "guard the coherent unit" rule: the ONLY reader of
    // g_scr_data.updatespawnpoints is Scr_UpdateSpawnPoints (below), which is guarded to match,
    // and its only three callers are in radiant_remote.cpp (the live-Radiant dev bridge), not on
    // any boot or gameplay path.
    g_scr_data.updatespawnpoints = 0;
#endif
#ifdef KISAK_SP
    // Retail SP (GScr_LoadScripts 0x00580370) picks the animscript set by the `zombiemode` dvar --
    // the same dvar, read the same way (its +0x18 current.enabled), that gates the dog set inside
    // GScr_LoadScriptsAndAnimsForEntities:
    //     if (!zombiemode) LoadHuman(); else { LoadZombie(); LoadZombieDog(); }
    // and it does NOT load the dog set here. The dog set is LAZY: it is loaded only if the map
    // actually contains an actor_* classname matching dog/hound, from the entity walk below. That
    // is why the frontend -- which has no dogs -- must not pay for dog_*.gsc at all.
    //
    // This tree previously called GScr_LoadDogAnimScripts unconditionally on every map, which
    // meant every SP actor was initialised by animscripts/dog_init: g_animScriptTable held one
    // entry and Actor_SetDefaults wrote species 0 into it. That is what left human actors without
    // an `animname` and produced ~4,400 script exceptions per frontend run.
    if ( !zombiemode->current.enabled )
    {
        GScr_LoadHumanAnimScripts(inst);
    }
    else
    {
        GScr_LoadZombieAnimScripts(inst);
        GScr_LoadZombieDogAnimScripts(inst);
    }
#else
    GScr_LoadDogAnimScripts(inst);
#endif

    GScr_SetScriptsForPathNodes();
    GScr_LoadPreGameScript();
    GScr_LoadGameTypeScript();
    GScr_LoadLevelScript();
#ifdef KISAK_SP
    // Must run AFTER the level script (it walks the map's own entity string) and before the
    // post-compile pass. Retail has it in the same relative position.
    GScr_LoadScriptsAndAnimsForEntities(inst);
#endif
    Scr_PostCompileScripts(inst);
    GScr_PostLoadScripts(inst);
#ifdef KISAK_SP
    G_CacheCubaScriptFields_SP();
#endif

    Scr_EndLoadScripts(inst);
}

void __cdecl    GScr_LoadDogAnimScripts(scriptInstance_t inst)
{
  GScr_LoadSingleAnimScript (inst, &g_scr_data.dogAnim.combat, "dog_combat");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.dogAnim.death, "dog_death");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.dogAnim.init, "dog_init");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.dogAnim.pain, "dog_pain");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.dogAnim.move, "dog_move");
#ifdef KISAK_SP
  // *** RESOLVED 2026-08-28. The TODO(SP) that used to sit in the #ifndef block below asked
  //     whether retail SP loads a DIFFERENT dog animscript list or skips the dog set entirely.
  //     Ghidra was available this pass and the answer is neither. ***
  //
  // Retail SP splits animscript loading into two passes that must agree entry-for-entry (a
  // "Script function count mismatch" Com_Error at 0x007e2f20 welds them). The dog set is
  //     LOAD pass  0x007ee760  -- Com_sprintf("animscripts/%s", name) + GScr_LoadScriptAndLabel
  //     SET  pass  0x007ef000  -- GScr_SetSingleAnimScript into the dog AnimScriptList
  // Both were disassembled and they agree on EIGHT entries in this exact order:
  //     dog_combat, dog_death, dog_init, dog_pain, dog_move, dog_scripted, dog_stop, dog_flashed
  // i.e. SP's set is MP's set minus dog_jump/dog_turn PLUS dog_scripted. The SET pass also gives
  // the member offsets, which pin dog_scripted to AnimScriptList::scripted (+0x98) -- see the
  // offset table in Game/Server/game/actor_animapi.h.
  //
  // bEnforceExists = 1 is safe here: animscripts/dog_scripted.gsc is present in BOTH common.ff
  // and frontend.ff, extracted and listed with tools/gsc_extract.py this pass (the same scan that
  // re-confirmed dog_jump.gsc and dog_turn.gsc are absent from every SP zone).
  GScr_LoadSingleAnimScript (inst, &g_scr_data.dogAnim.scripted, "dog_scripted");
#endif
  GScr_LoadSingleAnimScript (inst, &g_scr_data.dogAnim.stop, "dog_stop");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.dogAnim.flashed, "dog_flashed");
#ifndef KISAK_SP
  // SP retail divergence (2026-08-07). SP's dog animscript set has different MEMBERSHIP, not just
  // a different path prefix -- animscripts/dog_jump.gsc and animscripts/dog_turn.gsc ship ONLY in
  // common_mp.ff. Verified by scanning all 139 shipped zones case-insensitively, with
  // animscripts/dog_combat.gsc as a positive control (it is present in common.ff, frontend.ff AND
  // common_mp.ff, so the scan does reach SP zones). GScr_LoadSingleAnimScript passes
  // bEnforceExists = 1, so each of these is a guaranteed Com_Error(ERR_DROP, "Could not find
  // script") inside G_InitGame during the frontend load. Independently found by two auditors via
  // different methods; a third confirmed dog_jump is a whole-binary zero in the SP executable.
  //
  // Consumers deliberately NOT guarded, having been checked rather than assumed: the three uses in
  // Game/Server/game/actor_dog_exposed.cpp (:173, :176, :315) compare self->pAnimScriptFunc against
  // the ADDRESS of these members (&g_scr_data.dogAnim.jump), never dereferencing the handle. An
  // address comparison is well-defined whatever the handle holds; it simply never matches. This is
  // the coherent-unit check the project requires after an earlier fix guarded a load but left its
  // consumers walking unpopulated data.
  //
  // RESOLVED 2026-08-28: retail SP's dog set is now read directly from the SP binary and is
  // reproduced in the #ifdef KISAK_SP block above. Skipping these two on SP was correct.
  GScr_LoadSingleAnimScript (inst, &g_scr_data.dogAnim.jump, "dog_jump");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.dogAnim.turn, "dog_turn");
#endif
  g_animScriptTable[AI_SPECIES_DOG] = &g_scr_data.dogAnim;
}

#ifdef KISAK_SP
// ===========================================================================
// SP ANIMSCRIPT SETS. Recovered 2026-08-28 from the retail SP binary. Retail splits animscript
// loading into two passes over one shared handle array -- a LOAD pass that compiles each script
// and appends its handle, and a SET pass that walks the array back out in the SAME order and
// stores each handle into its named AnimScriptList member. G_InitGame welds the two with a
// "Script function count mismatch" Com_Error (0x007e2f20) if they ever disagree.
//
//   species     LOAD        SET         list base    entries
//   human       0x007ee3b0  0x007eeda0  0x01c79b64   23 + 2 standalone handles
//   dog         0x007ee760  0x007ef000  0x01c7a024    8
//   zombie      0x007ee8b0  0x007ef090  0x01c7a4e4    7 + 1 standalone handle
//   zombie_dog  0x007ee9e0  0x007ef190  0x01c7a9a4    9
//
// This tree keeps its own SINGLE-pass shape (GScr_LoadSingleAnimScript both compiles and stores).
// That is a deliberate divergence: the only thing retail's split buys is the count cross-check,
// which cannot desynchronise here because there is one call site per member. The observable
// behaviour -- which scripts are compiled, into which member, under which condition -- is
// reproduced exactly, and the load ORDER is kept identical so the two can be diffed later.
//
// Every script named below was confirmed present in the zones this build actually mounts by
// extracting common.ff and frontend.ff with tools/gsc_extract.py. That matters because
// GScr_LoadSingleAnimScript passes bEnforceExists = 1, making a missing script a Com_Error
// inside G_InitGame rather than a log line.
// ===========================================================================

void __cdecl    GScr_LoadHumanAnimScripts(scriptInstance_t inst)
{
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.combat, "combat");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.concealment_crouch, "concealment_crouch");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.concealment_prone, "concealment_prone");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.concealment_stand, "concealment_stand");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.cover_arrival, "cover_arrival");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.cover_crouch, "cover_crouch");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.cover_left, "cover_left");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.cover_pillar, "cover_pillar");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.cover_prone, "cover_prone");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.cover_right, "cover_right");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.cover_stand, "cover_stand");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.cover_wide_left, "cover_wide_left");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.cover_wide_right, "cover_wide_right");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.death, "death");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.grenade_return_throw, "grenade_return_throw");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.init, "init");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.pain, "pain");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.react, "react");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.move, "move");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.scripted, "scripted");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.stop, "stop");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.grenade_cower, "grenade_cower");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.humanAnim.flashed, "flashed");

  // Two standalone handles, both at label "init" rather than "main", and both given as explicit
  // paths instead of going through the "animscripts/%s" format (retail loads them with EDI set
  // directly at 0x007ee72a and 0x007ee73d).
  g_scr_data.scripted = GScr_LoadScriptAndLabel(inst, "animscripts/scripted", "init", 1);
  g_scr_data.init_mode_sp = GScr_LoadScriptAndLabel(inst, "animscripts/init_mode_sp", "init", 1);

  g_animScriptTable[AI_SPECIES_HUMAN] = &g_scr_data.humanAnim;
}

void __cdecl    GScr_LoadZombieAnimScripts(scriptInstance_t inst)
{
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieAnim.combat, "zombie_combat");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieAnim.death, "zombie_death");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieAnim.init, "zombie_init");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieAnim.pain, "zombie_pain");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieAnim.move, "zombie_move");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieAnim.scripted, "zombie_scripted");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieAnim.stop, "zombie_stop");

  // The zombie branch's counterpart to the human branch's animscripts/scripted -- same global.
  g_scr_data.scripted = GScr_LoadScriptAndLabel(inst, "animscripts/zombie_scripted", "init", 1);

  g_animScriptTable[AI_SPECIES_ZOMBIE] = &g_scr_data.zombieAnim;
}

void __cdecl    GScr_LoadZombieDogAnimScripts(scriptInstance_t inst)
{
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieDogAnim.combat, "zombie_dog_combat");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieDogAnim.death, "zombie_dog_death");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieDogAnim.init, "zombie_dog_init");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieDogAnim.pain, "zombie_dog_pain");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieDogAnim.move, "zombie_dog_move");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieDogAnim.scripted, "zombie_dog_scripted");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieDogAnim.stop, "zombie_dog_stop");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieDogAnim.flashed, "zombie_dog_flashed");
  GScr_LoadSingleAnimScript (inst, &g_scr_data.zombieDogAnim.turn, "zombie_dog_turn");

  g_animScriptTable[AI_SPECIES_ZOMBIE_DOG] = &g_scr_data.zombieDogAnim;
}

// ===========================================================================
// GScr_LoadScriptsAndAnimsForEntities -- retail SP 0x005efbb0, called from GScr_LoadScripts at
// 0x0058042a. Absent from this MP-derived tree entirely: MP has no per-AI-type script layer.
//
// Retail walks the map's entity string once and dispatches on each entity's `classname`:
//
//  * classname starting "actor_"  -> load aitype/<classname+6>.gsc at three labels, "main",
//    "precache" and "spawner"; and, the FIRST time a classname containing "dog" or "hound" is
//    seen while zombiemode is off, load the dog animscript set. That gate is the only thing in
//    retail SP that loads dog_*.gsc at all, which is why GScr_LoadScripts no longer does.
//
//  * classname "node_negotiation_begin" -> load animscripts/traverse/<animscript>.
//    DELIBERATELY NOT REPRODUCED. That branch is the LOAD half of retail's two-pass split, and
//    this tree already does the entire job in GScr_SetScriptsForPathNode above -- the direct
//    counterpart of retail's SET half at 0x007eec60, matching it down to the
//    "Pathnode (%s) at (%g %g %g) cannot find animscript '%s'" text and the Hunk_FindDataForFile
//    dedupe. Porting it here would double-load every traverse script.
//
//  * classname "misc_mg42" / "misc_turret" -> load the turret animscript named by the weapon def
//    behind the entity's `weaponinfo` key. NOT reproduced -- see the TODO(SP) below.
//
// WHY THIS FUNCTION MATTERS. aitype/<name>.gsc is where an SP actor's identity lives: it sets
// self.type (which routes through ActorScr_SetSpecies to pick the species, and therefore the
// animscript set), plus team, health and weapons, and it supplies the `spawner` and `precache`
// entry points. With no aitype loaded, spawning an actor from a spawner produces nothing. That is
// exactly the frontend interrogator failure: maps/frontend_anim.gsc's window_ambient_anims does
//     interrogator = simple_spawn_single("hudson");
//     interrogator.animname = "generic";
// and needs aitype/hudson_int_silhoutte.gsc, which ships in frontend.ff and whose main() sets
// self.type = "human". The spawn returned undefined, the field assignment threw, and the enclosing
// while(1) then called anim_loop_aligned(undefined, ...) forever -- about 4,400 script exceptions
// per run, every one of them downstream of this single missing call.
// ===========================================================================

static void GScr_SetTurretAnimScript_SP(scriptInstance_t inst, SpawnVar *spawnVar, const char *classname)
{
    const char *weaponName;
    G_SpawnString(spawnVar, "weaponinfo", "", &weaponName);
    const unsigned int weapon = G_GetWeaponIndexForName(const_cast<char *>(weaponName));
    if (!weapon)
        Com_Error(ERR_DROP, "\x15" "could not find weapon info '%s' for %s", weaponName, classname);
    const WeaponDef *def = BG_GetWeaponDef(weapon);
    if (def->weapClass != WEAPCLASS_TURRET)
        Com_Error(ERR_DROP, "\x15weapClass in weapon info '%s' for %s must be 'turret'", weaponName, classname);
    scr_animscript_t *script = &g_scr_data.humanAnim.weapons[weapon];
    if (*def->szScript)
    {
        char filename[64];
        Com_sprintf(filename, sizeof(filename), GSCR_ANIMSCRIPTS_DIR "%s", def->szScript);
        script->func = GScr_LoadScriptAndLabel(inst, filename, "main", 1);
        script->name = GScr_AllocString(def->szScript);
    }
    else
    {
        script->func = 0;
        script->name = 0;
    }
}

void __cdecl    GScr_LoadScriptsAndAnimsForEntities(scriptInstance_t inst)
{
    SpawnVar spawnVar;                  // retail's [esp+9Ch] local; sizeof(SpawnVar) == 0xA0C
    const char *classname;
    const char *aitype;
    char filename[64];
    AITypeScript *typeScript;
    bool bDogAnimsLoaded;

    bDogAnimsLoaded = 0;

    // The cursor is at the start of the entity string on entry: SV_InitGameVM calls
    // G_ResetEntityParsePoint() before G_InitGame, and every other walker in this tree
    // (G_SpawnEntitiesFromString, G_LoadStructs) resets on the way OUT rather than in. Retail
    // relies on the same invariant -- it opens with a bare G_ParseSpawnVars and treats a false
    // return as "no entities". This first parse consumes worldspawn, which retail also discards.
    if ( !G_ParseSpawnVars(&spawnVar) )
        Com_Error(ERR_DROP, "GScr_LoadScriptsAndAnimsForEntities: no entities");

    while ( G_ParseSpawnVars(&spawnVar) )
    {
        if ( !G_SpawnString(&spawnVar, "classname", "", &classname) )
            continue;

        if ( !strcmp(classname, "misc_mg42") || !strcmp(classname, "misc_turret") )
        {
            GScr_SetTurretAnimScript_SP(inst, &spawnVar, classname);
            continue;
        }
        if ( I_strnicmp(classname, "actor_", 6) )
            continue;

        aitype = classname + 6;

        // DEDUPE, and simultaneously the thing that makes the loaded handles reachable.
        // Retail dedupes with a GSC array used as a set (Scr_AllocArray up front, an
        // add-if-absent probe per name at 0x0059b7f0, a free at the end). This tree has a better
        // fit already in use two functions below in GScr_SetScriptsForPathNode: the
        // Hunk_FindDataForFile / Hunk_SetDataForFile file-data registry. Using it here is not
        // just a dedupe -- registry type 0 keyed by the aitype name is EXACTLY where retail keeps
        // these too. Retail registers them in its SET pass, GScr_SetScriptsAndAnimsForEntities
        // (0x008071e0): Hunk_FindDataForFile(0, classname+6) as its own skip-if-present test,
        // Hunk_AllocLow(0xC), stores [+0]=main [+4]=precache [+8]=spawner, then
        // Hunk_SetDataForFile(0, name, block, alloc). Retail therefore dedupes TWICE -- the GSC
        // array in the load pass and this registry in the set pass -- and collapsing the two
        // into one lookup here is sound. It is also where Actor_FinishSpawning
        // (actor_mp.cpp:841) reads them back:
        //     typeScript = (AITypeScript *)Hunk_FindDataForFile(0, classname + 6);
        //     iassert(typeScript); iassert(typeScript->main);
        //     Scr_ExecEntThread(ent, typeScript->main, 0);
        // and AITypeScript is { int main; int precache; int spawner; } -- the same three labels
        // retail loads here, in the same order. So loading without registering would leave every
        // spawned actor asserting on a null typeScript.
        //
        // Hunk_SetDataForFile STORES THE POINTER, it does not copy (com_memory.cpp: the body is
        // `fileData->data = data;`), so the AITypeScript has to outlive this frame -- hence the
        // hunk allocation rather than a local. It also asserts !Hunk_FindDataForFileInternal on
        // the way in, so the Find above is required, not merely an optimisation.
        if ( Hunk_FindDataForFile(0, aitype) )
            continue;

        // The dog gate sits INSIDE the dedupe in retail too (the add-if-absent probe at
        // 0x005efc6a guards everything that follows), and it tests the FULL classname rather than
        // classname+6 -- harmless either way, since "actor_" contains neither substring.
        if ( !zombiemode->current.enabled
            && !bDogAnimsLoaded
            && (strstr(classname, "dog") || strstr(classname, "hound")) )
        {
            GScr_LoadDogAnimScripts(inst);
            bDogAnimsLoaded = 1;
        }

        Com_sprintf(filename, sizeof(filename), "aitype/%s", aitype);
        typeScript = (AITypeScript *)GScr_AnimscriptAlloc(sizeof(AITypeScript));
        // bEnforceExists = 0 is a DELIBERATE SOFTENING OF RETAIL, not retail's shape. An earlier
        // revision of this comment said retail's behaviour "was not established"; it since has
        // been. Retail's LOAD half is non-fatal (Com_Printf "Could not find script '%s'", stores
        // a null handle), but its SET half -- GScr_SetScriptsAndAnimsForEntities, 0x008071e0 --
        // then raises Com_Error(1, "Could not find label '%s' in script '%s'") for each of the
        // three handles, so on retail a missing aitype script kills the load outright.
        //
        // The softer reading is kept on purpose: a map naming an actor type whose script did not
        // ship should cost that one NPC, not the whole boot. The cost is not silent either way --
        // Actor_FinishSpawning asserts on a null ->main, so it becomes a stop at spawn instead of
        // a stop at load. Revisit if SP ever needs retail's fail-fast behaviour.
        typeScript->main = GScr_LoadScriptAndLabel(inst, filename, "main", 0);
        typeScript->precache = GScr_LoadScriptAndLabel(inst, filename, "precache", 0);
        typeScript->spawner = GScr_LoadScriptAndLabel(inst, filename, "spawner", 0);
        Hunk_SetDataForFile(0, aitype, typeScript, (void *(__cdecl *)(int))GScr_AnimscriptAlloc);
    }


    G_ResetEntityParsePoint();
}
#endif // KISAK_SP

void __cdecl    GScr_LoadSingleAnimScript(scriptInstance_t inst, scr_animscript_t *pAnim, const char *name)
{
    char filename[64]; // [esp+0h] [ebp-48h] BYREF

    iassert(pAnim);
    iassert(name);

    Com_sprintf(filename, sizeof(filename), GSCR_ANIMSCRIPTS_DIR "%s", name);

    pAnim->name = GScr_AllocString(name);
    pAnim->func = GScr_LoadScriptAndLabel(inst, filename, "main", 1);
}

void GScr_SetScriptsForPathNodes()
{
    if ( !G_ExitAfterToolComplete() )
        Path_CallFunctionForNodes(SCRIPTINSTANCE_SERVER, GScr_SetScriptsForPathNode);
}

void __cdecl GScr_SetScriptsForPathNode(scriptInstance_t inst, pathnode_t *loadNode)
{
    char *animscript; // [esp+1Ch] [ebp-4Ch]
    char filename[68]; // [esp+20h] [ebp-48h] BYREF

    if ( !G_ExitAfterToolComplete() && loadNode->constant.type )
    {
        if ( loadNode->constant.type == NODE_NEGOTIATION_BEGIN )
        {
            if ( loadNode->constant.animscript )
            {
                animscript = SL_ConvertToString(loadNode->constant.animscript, SCRIPTINSTANCE_SERVER);
                if ( !animscript
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                                328,
                                0,
                                "%s",
                                "animscript") )
                {
                    __debugbreak();
                }
                loadNode->constant.animscriptfunc = (int)Hunk_FindDataForFile(1, animscript);
                if ( !loadNode->constant.animscriptfunc )
                {
                    Com_sprintf(filename, 0x40u, GSCR_ANIMSCRIPTS_DIR "traverse/%s", animscript);
                    loadNode->constant.animscriptfunc = GScr_LoadScriptAndLabel(inst, filename, "main", 1);
                    Hunk_SetDataForFile(
                        1,
                        animscript,
                        (void *)loadNode->constant.animscriptfunc,
                        (void *(__cdecl *)(int))GScr_AnimscriptAlloc);
                }
                if ( !loadNode->constant.animscriptfunc )
                {
                    Com_PrintError(
                        1,
                        "ERROR: Pathnode (%s) at (%g %g %g) cannot find animscript '%s'\n",
                        nodeStringTable[loadNode->constant.type],
                        loadNode->constant.vOrigin[0],
                        loadNode->constant.vOrigin[1],
                        loadNode->constant.vOrigin[2],
                        animscript);
                    loadNode->constant.type = NODE_BADNODE;
                }
            }
            else
            {
                Com_PrintError(
                    1,
                    "ERROR: Pathnode (%s) at (%g %g %g) has no animscript specified\n",
                    nodeStringTable[loadNode->constant.type],
                    loadNode->constant.vOrigin[0],
                    loadNode->constant.vOrigin[1],
                    loadNode->constant.vOrigin[2]);
                loadNode->constant.type = NODE_BADNODE;
            }
        }
        else if ( loadNode->constant.animscript )
        {
            if ( !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            316,
                            0,
                            "%s",
                            "!loadNode->constant.animscript") )
                __debugbreak();
        }
    }
}

unsigned __int8 *__cdecl GScr_AnimscriptAlloc(unsigned int size)
{
    return Hunk_AllocLow(size, "GScr_AnimscriptAlloc", 5);
}

int GScr_LoadLevelScript()
{
    int result; // eax
    char filename[64]; // [esp+0h] [ebp-48h] BYREF
    const dvar_s *mapname; // [esp+44h] [ebp-4h]

    mapname = _Dvar_RegisterString("mapname", (char *)"", 0x44u, "The current map name");
    Com_sprintf(filename, 0x40u, GSCR_LEVEL_DIR "%s", mapname->current.string);
    result = GScr_LoadScriptAndLabel(SCRIPTINSTANCE_SERVER, filename, "main", 0);
    g_scr_data.levelscript = result;
    return result;
}

int GScr_LoadPreGameScript()
{
    int result; // eax
    char filename[68]; // [esp+0h] [ebp-48h] BYREF

#ifdef KISAK_SP
    // MP pre-game (lobby countdown) script. No SP counterpart exists: neither
    // "maps/mp/gametypes/_pregame" nor "maps/_pregame" nor "maps/gametypes/_pregame" appears in
    // any SP zone. bEnforceExists is already 0 so this was never fatal - it is skipped only to
    // avoid a pointless lookup and to record the finding. Consumer is provably null-safe:
    // Scr_LoadPreGame (g_scr_main_mp.cpp) tests `if (g_scr_data.pregamescript)` before exec'ing,
    // and the whole branch is additionally gated on Pregame_ShouldLoadPregame().
    // Audit finding B3 (frontend-map-load audit).
    g_scr_data.pregamescript = 0;
    return 0;
#else
    Com_sprintf(filename, 0x40u, "maps/mp/gametypes/_pregame");
    result = GScr_LoadScriptAndLabel(SCRIPTINSTANCE_SERVER, filename, "main", 0);
    g_scr_data.pregamescript = result;
    return result;
#endif
}

void __cdecl GScr_PostLoadScripts(scriptInstance_t inst)
{
    signed int classnum; // [esp+0h] [ebp-4h]

    for ( classnum = 0; classnum < 5; ++classnum )
        Scr_SetClassMap(inst, classnum);
    GScr_AddFieldsForEntity();
    GScr_AddFieldsForHudElems();
    GScr_AddFieldsForPathnode();
    GScr_AddFieldsForVehicleNode();
    GScr_AddFieldsForRadiant();
}

void __cdecl GScr_FreeScripts(scriptInstance_t inst)
{
    signed int classnum; // [esp+0h] [ebp-4h]

    for ( classnum = 0; classnum < 5; ++classnum )
        Scr_RemoveClassMap(inst, classnum);
}

void __cdecl ScrCmd_GetClanId(scr_entref_t entref)
{
    Scr_AddString("0", SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetClanName(scr_entref_t entref)
{
    Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
}

void GScr_CreatePrintChannel()
{
    char *name; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("illegal call to createprintchannel()", 0);
    name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( !Con_OpenChannel(name, 1) )
        Scr_Error("Unable to create new channel.    Maximum number of channels exeeded.", 0);
}

void GScr_printChannelSet()
{
    int Type; // [esp+0h] [ebp-10h]
    int oldChannel; // [esp+4h] [ebp-Ch]
    int channel; // [esp+8h] [ebp-8h] BYREF
    const char *name; // [esp+Ch] [ebp-4h]

    channel = 25;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
    {
        Scr_Error("illegal call to setprintchannel()", 0);
        return;
    }
    oldChannel = level.scriptPrintChannel;
    Type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    if ( Type == 2 )
    {
        name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        if ( !Con_GetChannel(name, &channel) )
        {
            Scr_ParamError(0, "Invalid Print Channel", SCRIPTINSTANCE_SERVER);
            return;
        }
    }
    else if ( Type != 6 || (channel = Scr_GetInt(0, SCRIPTINSTANCE_SERVER), !Con_IsChannelOpen(channel)) )
    {
        Scr_ParamError(0, "Invalid Print Channel", SCRIPTINSTANCE_SERVER);
        return;
    }
    if ( Con_ScriptHasPermission(channel) )
    {
        level.scriptPrintChannel = channel;
        Scr_AddInt(oldChannel, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_ParamError(0, "Script does not have permission to print to this channel", SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl Scr_LocalizationError(unsigned int iParm, const char *pszErrorMessage)
{
    Scr_ParamError(iParm, pszErrorMessage, SCRIPTINSTANCE_SERVER);
}

#ifdef KISAK_SP
static bool Scr_IsMessageControlCharacter_SP(signed char character)
{
    return (character >= 1 && character <= 6)
        || (character >= 14 && character <= 25)
        || (character >= 28 && character <= 31);
}
#endif

void __cdecl Scr_ConstructMessageString(
                int firstParmIndex,
                int lastParmIndex,
                const char *errorContext,
                char *string,
                unsigned int stringLimit)
{
#ifdef KISAK_SP
    unsigned int stringLen = 0;
    string[0] = 0;
    for (; firstParmIndex <= lastParmIndex; ++firstParmIndex)
    {
        const int type = Scr_GetType(firstParmIndex, SCRIPTINSTANCE_SERVER);
        const char *token;
        unsigned int tokenLen;
        if (type == 3)
        {
            token = Scr_GetIString(firstParmIndex, SCRIPTINSTANCE_SERVER);
            tokenLen = strlen(token);
            Scr_ValidateLocalizedStringRef(firstParmIndex, token, tokenLen);
            if (stringLen + tokenLen + 1 >= stringLimit)
                Scr_ParamError(firstParmIndex, va("%s is too long. Max length is %i\n", errorContext, stringLimit), SCRIPTINSTANCE_SERVER);
            if (stringLen)
                string[stringLen++] = 20;
        }
        else if (type == 1 && Scr_GetPointerType(firstParmIndex, SCRIPTINSTANCE_SERVER) == 19)
        {
            gentity_s *ent = Scr_GetEntity(firstParmIndex);
            if (!ent->client)
                Scr_ParamError(firstParmIndex, "Entity is not a player", SCRIPTINSTANCE_SERVER);
            token = va("%s^7", CS_DisplayName(&ent->client->sess.cs, 3));
            tokenLen = strlen(token);
            if (stringLen + tokenLen + 1 >= stringLimit)
                Scr_ParamError(firstParmIndex, va("%s is too long. Max length is %i\n", errorContext, stringLimit), SCRIPTINSTANCE_SERVER);
            if (tokenLen)
                string[stringLen++] = 21;
        }
        else
        {
            token = Scr_GetString(firstParmIndex, SCRIPTINSTANCE_SERVER);
            tokenLen = strlen(token);
            bool letters = false;
            for (unsigned int i = 0; i < tokenLen; ++i)
            {
                const signed char character = static_cast<signed char>(token[i]);
                if (character == 20 || character == 21 || character == 22)
                    Scr_ParamError(firstParmIndex, va("bad escape character (%i) present in string", character), SCRIPTINSTANCE_SERVER);
                if (I_isalpha(character))
                {
                    letters = true;
                    if (loc_warnings->current.enabled)
                    {
                        if (loc_warningsAsErrors->current.enabled)
                            Com_Error(ERR_LOCALIZATION, va("non-localized %s strings are not allowed to have letters in them: \"%s\"", errorContext, token));
                        else
                            Com_PrintWarning(24, "WARNING: Non-localized %s string is not allowed to have letters in it. Must be changed over to a localized string: \"%s\"\n", errorContext, token);
                    }
                    break;
                }
            }
            if (stringLen + tokenLen + 1 >= stringLimit)
                Scr_ParamError(firstParmIndex, va("%s is too long. Max length is %i\n", errorContext, stringLimit), SCRIPTINSTANCE_SERVER);
            if (tokenLen)
                string[stringLen++] = 21;
            if (letters && loc_warnings->current.enabled)
            {
                if (stringLen + tokenLen + 14 >= stringLimit)
                    Scr_ParamError(firstParmIndex, va("%s is too long. Max length is %i\n", errorContext, stringLimit), SCRIPTINSTANCE_SERVER);
                memcpy(string + stringLen, "UNLOCALIZED: ", 13);
                stringLen += 13;
            }
        }
        for (unsigned int i = 0; i < tokenLen; ++i)
        {
            const signed char character = static_cast<signed char>(token[i]);
            string[stringLen++] = character < 31 && !Scr_IsMessageControlCharacter_SP(character) ? '.' : token[i];
        }
    }
    string[stringLen] = 0;
#else
    const char *v5; // eax
    const char *v6; // eax
    const char *v7; // eax
    const char *v8; // eax
    const char *v9; // eax
    const char *v10; // eax
    unsigned int v11; // [esp+0h] [ebp-54h]
    unsigned int v12; // [esp+10h] [ebp-44h]
    unsigned int charIndex; // [esp+34h] [ebp-20h]
    unsigned int charIndexa; // [esp+34h] [ebp-20h]
    unsigned int tokenLen; // [esp+38h] [ebp-1Ch]
    int type; // [esp+40h] [ebp-14h]
    gentity_s *ent; // [esp+44h] [ebp-10h]
    char *token; // [esp+4Ch] [ebp-8h]
    unsigned int stringLen; // [esp+50h] [ebp-4h]

    stringLen = 0;
    while ( firstParmIndex <= lastParmIndex )
    {
        type = Scr_GetType(firstParmIndex, SCRIPTINSTANCE_SERVER);
        if ( type == 3 )
        {
            token = Scr_GetIString(firstParmIndex, SCRIPTINSTANCE_SERVER);
            tokenLen = strlen(token);
            Scr_ValidateLocalizedStringRef(firstParmIndex, token, tokenLen);
            if ( stringLen + tokenLen + 1 >= stringLimit )
            {
                v5 = va("%s is too long. Max length is %i\n", errorContext, stringLimit);
                Scr_ParamError(firstParmIndex, v5, SCRIPTINSTANCE_SERVER);
            }
            if ( stringLen )
                string[stringLen++] = 20;
        }
        else if ( type == 1 && Scr_GetPointerType(firstParmIndex, SCRIPTINSTANCE_SERVER) == 19 )
        {
            ent = Scr_GetEntity(firstParmIndex);
            if ( !ent->client )
                Scr_ParamError(firstParmIndex, "Entity is not a player", SCRIPTINSTANCE_SERVER);
            v6 = CS_DisplayName(&ent->client->sess.cs, 3);
            token = va("%s^7", v6);
            v12 = strlen(token);
            tokenLen = v12;
            if ( stringLen + v12 + 1 >= stringLimit )
            {
                v7 = va("%s is too long. Max length is %i\n", errorContext, stringLimit);
                Scr_ParamError(firstParmIndex, v7, SCRIPTINSTANCE_SERVER);
            }
            if ( v12 )
                string[stringLen++] = 21;
        }
        else
        {
            token = Scr_GetString(firstParmIndex, SCRIPTINSTANCE_SERVER);
            v11 = strlen(token);
            tokenLen = v11;
            for ( charIndex = 0; charIndex < v11; ++charIndex )
            {
                if ( token[charIndex] == 20 || token[charIndex] == 21 || token[charIndex] == 22 )
                {
                    v8 = va("bad escape character (%i) present in string", token[charIndex]);
                    Scr_ParamError(firstParmIndex, v8, SCRIPTINSTANCE_SERVER);
                }
                if ( isalpha(token[charIndex]) )
                {
                    if ( loc_warnings->current.enabled )
                    {
                        if ( loc_warningsAsErrors->current.enabled )
                        {
                            v9 = va("non-localized %s strings are not allowed to have letters in them: \"%s\"", errorContext, token);
                            Scr_LocalizationError(firstParmIndex, v9);
                        }
                        else
                        {
                            Com_PrintWarning(
                                17,
                                "WARNING: Non-localized %s string is not allowed to have letters in it. Must be changed over to a localiz"
                                "ed string: \"%s\"\n",
                                errorContext,
                                token);
                        }
                    }
                    break;
                }
            }
            if ( stringLen + v11 + 1 >= stringLimit )
            {
                v10 = va("%s is too long. Max length is %i\n", errorContext, stringLimit);
                Scr_ParamError(firstParmIndex, v10, SCRIPTINSTANCE_SERVER);
            }
            if ( v11 )
                string[stringLen++] = 21;
        }
        for ( charIndexa = 0; charIndexa < tokenLen; ++charIndexa )
        {
            if ( token[charIndexa] == 20 || token[charIndexa] == 21 || token[charIndexa] == 22 )
                string[stringLen] = 46;
            else
                string[stringLen] = token[charIndexa];
            ++stringLen;
        }
        ++firstParmIndex;
    }
    string[stringLen] = 0;
#endif
}

void __cdecl Scr_ValidateLocalizedStringRef(unsigned int parmIndex, const char *token, int tokenLen)
{
    const char *v3; // eax
    int charIter; // [esp+0h] [ebp-4h]

    if ( !token
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 627, 0, "%s", "token") )
    {
        __debugbreak();
    }
    if ( tokenLen < 0
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 628, 0, "%s", "tokenLen >= 0") )
    {
        __debugbreak();
    }
    if ( tokenLen > 1 )
    {
        for ( charIter = 0; charIter < tokenLen; ++charIter )
        {
#ifdef KISAK_SP
            if (!I_isalnum(static_cast<signed char>(token[charIter])) && token[charIter] != 95)
#else
            if ( !isalnum(token[charIter]) && token[charIter] != 95 )
#endif
            {
                v3 = va(
                             "Illegal localized string reference: %s must contain only alpha-numeric characters and underscores",
                             token);
                Scr_ParamError(parmIndex, v3, SCRIPTINSTANCE_SERVER);
            }
        }
    }
}

void __cdecl Scr_MakeGameMessage(int iClientNum, const char *pszCmd)
{
    int NumParam; // eax
    const char *v3; // eax
    char string[1028]; // [esp+0h] [ebp-408h] BYREF

    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    Scr_ConstructMessageString(0, NumParam - 1, "Game Message", string, 0x400u);
    v3 = va("%s \"%s\"", pszCmd, string);
    SV_GameSendServerCommand(iClientNum, SV_CMD_CAN_IGNORE, v3);
}

void __cdecl Scr_VerifyWeaponIndex(int weaponIndex, const char *weaponName)
{
    const char *v2; // eax

    if ( !weaponName
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 761, 0, "%s", "weaponName") )
    {
        __debugbreak();
    }
    if ( !weaponIndex )
    {
        if ( I_stricmp("none", weaponName) )
            v2 = va(
                         "Unknown weapon name \"%s\": script may need to call PreCacheItem(\"%s\") during level init.\n",
                         weaponName,
                         weaponName);
        else
            v2 = va("Weapon name \"%s\" is not valid.\n", weaponName);
        Scr_ParamError(0, v2, SCRIPTINSTANCE_SERVER);
    }
}

void iprintln()
{
    const char *v0; // eax

    v0 = va("%c", 102);
    Scr_MakeGameMessage(-1, v0);
}

void iprintlnbold()
{
    const char *v0; // eax

    v0 = va("%c", 103);
    Scr_MakeGameMessage(-1, v0);
}

void GScr_print3d()
{
    VariableUnion duration; // [esp+10h] [ebp-34h]
    float origin[3]; // [esp+14h] [ebp-30h] BYREF
    float rgb[3]; // [esp+20h] [ebp-24h] BYREF
    float scale; // [esp+2Ch] [ebp-18h]
    float color[4]; // [esp+30h] [ebp-14h] BYREF
    const char *text; // [esp+40h] [ebp-4h]

    duration.intValue = 1;
    scale = 1.0f;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 1.0f;
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 2:
            goto $LN11_31;
        case 3:
            goto $LN3_68;
        case 4:
            goto $LN4_64;
        case 5:
            goto $LN5_57;
        case 6:
            duration.intValue = Scr_GetInt(5u, SCRIPTINSTANCE_SERVER);
$LN5_57:
            scale = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
$LN4_64:
            color[3] = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
$LN3_68:
            Scr_GetVector(2u, rgb, SCRIPTINSTANCE_SERVER);
            color[0] = rgb[0];
            color[1] = rgb[1];
            color[2] = rgb[2];
$LN11_31:
            text = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
            Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
            G_AddDebugString(origin, color, scale, (char *)text, duration.intValue);
            break;
        default:
            Scr_Error("illegal call to print3d()", 0);
            break;
    }
}

void GScr_line()
{
    VariableUnion duration; // [esp+4h] [ebp-3Ch]
    float rgb[3]; // [esp+8h] [ebp-38h] BYREF
    float start[3]; // [esp+14h] [ebp-2Ch] BYREF
    float end[3]; // [esp+20h] [ebp-20h] BYREF
    float color[4]; // [esp+2Ch] [ebp-14h] BYREF
    int depthTest; // [esp+3Ch] [ebp-4h]

    duration.intValue = 0;
    depthTest = 0;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 1.0f;
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 2:
            goto $LN11_32;
        case 3:
            goto $LN3_69;
        case 4:
            goto $LN4_65;
        case 5:
            goto $LN5_58;
        case 6:
            duration.intValue = Scr_GetInt(5u, SCRIPTINSTANCE_SERVER);
$LN5_58:
            depthTest = Scr_GetInt(4u, SCRIPTINSTANCE_SERVER);
$LN4_65:
            color[3] = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
$LN3_69:
            Scr_GetVector(2u, rgb, SCRIPTINSTANCE_SERVER);
            color[0] = rgb[0];
            color[1] = rgb[1];
            color[2] = rgb[2];
$LN11_32:
            Scr_GetVector(1u, end, SCRIPTINSTANCE_SERVER);
            Scr_GetVector(0, start, SCRIPTINSTANCE_SERVER);
            break;
        default:
            Scr_Error("illegal call to line()", 0);
            break;
    }
    CL_AddDebugLine(start, end, color, depthTest, duration.intValue);
}

void GScr_box()
{
    float pos[3]; // [esp+14h] [ebp-4Ch] BYREF
    int duration; // [esp+20h] [ebp-40h]
    float rgb[3]; // [esp+24h] [ebp-3Ch] BYREF
    float mins[3]; // [esp+30h] [ebp-30h] BYREF
    float yaw; // [esp+3Ch] [ebp-24h]
    float maxs[3]; // [esp+40h] [ebp-20h] BYREF
    float color[4]; // [esp+4Ch] [ebp-14h] BYREF
    int depthTest; // [esp+5Ch] [ebp-4h]

    duration = 0;
    depthTest = 0;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 1.0f;
    yaw = 0.0f;
    mins[0] = -10.0f;
    mins[1] = -10.0f;
    mins[2] = -10.0f;
    maxs[0] = 10.0f;
    maxs[1] = 10.0f;
    maxs[2] = 10.0f;
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 1:
            goto $LN2_64;
        case 2:
            goto $LN3_70;
        case 3:
            goto $LN15_17;
        case 4:
            goto $LN20_16;
        case 5:
            goto $LN6_61;
        case 6:
            goto $LN7_47;
        case 7:
            goto $LN8_39;
        case 8:
            duration = Scr_GetInt(7u, SCRIPTINSTANCE_SERVER);
$LN8_39:
            depthTest = Scr_GetInt(6u, SCRIPTINSTANCE_SERVER);
$LN7_47:
            color[3] = Scr_GetFloat(5u, SCRIPTINSTANCE_SERVER);
$LN6_61:
            Scr_GetVector(4u, rgb, SCRIPTINSTANCE_SERVER);
            color[0] = rgb[0];
            color[1] = rgb[1];
            color[2] = rgb[2];
$LN20_16:
            yaw = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
            if ( (yaw < 0.0 || yaw > 360.0)
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            906,
                            0,
                            "%s",
                            "yaw >= 0.0f && yaw <= 360.0f") )
            {
                __debugbreak();
            }
$LN15_17:
            Scr_GetVector(2u, maxs, SCRIPTINSTANCE_SERVER);
$LN3_70:
            Scr_GetVector(1u, mins, SCRIPTINSTANCE_SERVER);
$LN2_64:
            Scr_GetVector(0, pos, SCRIPTINSTANCE_SERVER);
            break;
        default:
            Scr_Error("illegal call to box()", 0);
            break;
    }
    CG_DebugBox(pos, mins, maxs, yaw, color, depthTest, duration);
}

void GScr_debugstar()
{
    int NumParam; // [esp+0h] [ebp-30h]
    VariableUnion duration; // [esp+4h] [ebp-2Ch]
    float rgb[3]; // [esp+8h] [ebp-28h] BYREF
    float location[3]; // [esp+14h] [ebp-1Ch] BYREF
    float color[4]; // [esp+20h] [ebp-10h] BYREF

    duration.intValue = 10;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 1.0f;
    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( NumParam != 1 )
    {
        if ( NumParam != 2 )
        {
            if ( NumParam != 3 )
            {
                Scr_Error("illegal call to debugstar()", 0);
                goto LABEL_8;
            }
            Scr_GetVector(2u, rgb, SCRIPTINSTANCE_SERVER);
            color[0] = rgb[0];
            color[1] = rgb[1];
            color[2] = rgb[2];
        }
        duration.intValue = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    }
    Scr_GetVector(0, location, SCRIPTINSTANCE_SERVER);
LABEL_8:
    CL_AddDebugStar(location, color, duration.intValue);
}

void GScr_circle()
{
    VariableUnion onGround; // [esp+18h] [ebp-38h]
    VariableUnion duration; // [esp+1Ch] [ebp-34h]
    float rgb[3]; // [esp+20h] [ebp-30h] BYREF
    float radius; // [esp+2Ch] [ebp-24h]
    float color[4]; // [esp+30h] [ebp-20h] BYREF
    int depthTest; // [esp+40h] [ebp-10h]
    float center[3]; // [esp+44h] [ebp-Ch] BYREF

    duration.intValue = 0;
    onGround.intValue = 0;
    depthTest = 0;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 1.0f;
    radius = 10.0f;
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 1:
            goto $LN2_65;
        case 2:
            goto $LN12_22;
        case 3:
            goto $LN4_66;
        case 4:
            goto $LN5_59;
        case 5:
            goto $LN6_62;
        case 6:
            duration.intValue = Scr_GetInt(5u, SCRIPTINSTANCE_SERVER);
$LN6_62:
            onGround.intValue = Scr_GetInt(4u, SCRIPTINSTANCE_SERVER);
$LN5_59:
            depthTest = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
$LN4_66:
            Scr_GetVector(2u, rgb, SCRIPTINSTANCE_SERVER);
            color[0] = rgb[0];
            color[1] = rgb[1];
            color[2] = rgb[2];
$LN12_22:
            radius = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
$LN2_65:
            Scr_GetVector(0, center, SCRIPTINSTANCE_SERVER);
            break;
        default:
            Scr_Error("illegal call to circle()", 0);
            break;
    }
    G_DebugCircle(center, radius, color, depthTest, onGround.intValue, duration.intValue);
}

void GScr_sphere()
{
    VariableUnion sideCount; // [esp+18h] [ebp-38h]
    VariableUnion duration; // [esp+1Ch] [ebp-34h]
    float rgb[3]; // [esp+20h] [ebp-30h] BYREF
    float radius; // [esp+2Ch] [ebp-24h]
    float color[4]; // [esp+30h] [ebp-20h] BYREF
    int depthTest; // [esp+40h] [ebp-10h]
    float center[3]; // [esp+44h] [ebp-Ch] BYREF

    duration.intValue = 0;
    sideCount.intValue = 10;
    depthTest = 0;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 1.0f;
    radius = 10.0f;
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 1:
            goto $LN2_66;
        case 2:
            goto $LN13_16;
        case 3:
            goto $LN4_67;
        case 4:
            goto $LN5_60;
        case 5:
            goto $LN6_63;
        case 6:
            goto $LN7_49;
        case 7:
            duration.intValue = Scr_GetInt(6u, SCRIPTINSTANCE_SERVER);
$LN7_49:
            sideCount.intValue = Scr_GetInt(5u, SCRIPTINSTANCE_SERVER);
$LN6_63:
            depthTest = Scr_GetInt(4u, SCRIPTINSTANCE_SERVER);
$LN5_60:
            color[3] = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
$LN4_67:
            Scr_GetVector(2u, rgb, SCRIPTINSTANCE_SERVER);
            color[0] = rgb[0];
            color[1] = rgb[1];
            color[2] = rgb[2];
$LN13_16:
            radius = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
$LN2_66:
            Scr_GetVector(0, center, SCRIPTINSTANCE_SERVER);
            break;
        default:
            Scr_Error("illegal call to sphere()", 0);
            break;
    }
    CG_DebugSphere(center, radius, color, sideCount.intValue, depthTest, duration.intValue);
}

int __cdecl Scr_GetArrayValues_Vector(
                unsigned int parameter_index,
                unsigned int parent_id,
                float (*vector_array)[3],
                int vector_array_size,
                const char *array_type_description)
{
    const char *v5; // eax
    const char *v6; // eax
    float *v8; // [esp+0h] [ebp-18h]
    float *next; // [esp+4h] [ebp-14h]
    int script_array_size; // [esp+8h] [ebp-10h]
    signed int vector_array_index; // [esp+Ch] [ebp-Ch]
    VariableValueInternal *entry_value; // [esp+10h] [ebp-8h]
    int id; // [esp+14h] [ebp-4h]

    vector_array_index = 0;
    if ( !vector_array
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 1097, 0, "%s", "vector_array") )
    {
        __debugbreak();
    }
    if ( !parent_id
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 1098, 0, "%s", "parent_id") )
    {
        __debugbreak();
    }
    if ( GetObjectType(SCRIPTINSTANCE_SERVER, parent_id) != 20
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    1099,
                    0,
                    "%s",
                    "GetObjectType(SCRIPTINSTANCE_SERVER, parent_id)==VAR_ARRAY") )
    {
        __debugbreak();
    }
    script_array_size = GetArraySize(SCRIPTINSTANCE_SERVER, parent_id);
    if ( script_array_size > vector_array_size )
    {
        v6 = va("contents of vector array are too large (must be <= %ld) (%s)", vector_array_size, array_type_description);
        Scr_ParamError(parameter_index, v6, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        while ( vector_array_index < script_array_size )
        {
            id = GetArrayVariable(SCRIPTINSTANCE_SERVER, parent_id, vector_array_index);
            if ( !id
                && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 1106, 0, "%s", "id") )
            {
                __debugbreak();
            }
            entry_value = &gScrVarGlob[0].variableList[id + 0x8000];
            if ( (entry_value->w.status & 0x60) == 0
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            1108,
                            0,
                            "%s",
                            "(entry_value->w.status & VAR_STAT_MASK) != VAR_STAT_FREE") )
            {
                __debugbreak();
            }
            if ( (entry_value->w.status & 0x1F) != 4 )
            {
                v5 = va("contents of array must be vectors (%s)", array_type_description);
                Scr_ParamError(parameter_index, v5, SCRIPTINSTANCE_SERVER);
                return 0;
            }
            v8 = &(*vector_array)[3 * vector_array_index];
            next = (float *)entry_value->u.next;
            *v8 = *next;
            v8[1] = next[1];
            v8[2] = next[2];
            ++vector_array_index;
        }
    }
    return vector_array_index;
}

void GScr_linelist()
{
    VariableUnion v0; // eax
    int point_index; // [esp+4h] [ebp-C30h]
    VariableUnion depth_test; // [esp+8h] [ebp-C2Ch]
    float points[256][3]; // [esp+Ch] [ebp-C28h] BYREF
    int duration; // [esp+C10h] [ebp-24h]
    float rgb[3]; // [esp+C14h] [ebp-20h] BYREF
    int point_count; // [esp+C20h] [ebp-14h]
    float color[4]; // [esp+C24h] [ebp-10h] BYREF

    point_count = 0;
    duration = 0;
    depth_test.intValue = 0;
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    color[3] = 1.0f;
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 1:
            goto $LN14_16;
        case 2:
            goto $LN6_64;
        case 3:
            goto $LN7_50;
        case 4:
            goto $LN8_41;
        case 5:
            duration = Scr_GetInt(4u, SCRIPTINSTANCE_SERVER);
$LN8_41:
            depth_test.intValue = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
$LN7_50:
            color[3] = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
$LN6_64:
            Scr_GetVector(1u, rgb, SCRIPTINSTANCE_SERVER);
            color[0] = rgb[0];
            color[1] = rgb[1];
            color[2] = rgb[2];
$LN14_16:
            v0.intValue = Scr_GetObject(0, SCRIPTINSTANCE_SERVER);
            point_count = Scr_GetArrayValues_Vector(0, v0.stringValue, points, 256, "line list");
            break;
        default:
            Scr_Error("illegal call to linelist()", 0);
            break;
    }
    for ( point_index = 0; point_index < point_count - 1; point_index += 2 )
        CL_AddDebugLine(points[point_index], points[point_index + 1], color, depth_test.intValue, duration);
}

void GScr_IsDefined()
{
    int type; // [esp+4h] [ebp-4h]
    signed int typea; // [esp+4h] [ebp-4h]

    type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    if ( type == 1 )
    {
        typea = Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER);
        if ( typea < 13
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        1228,
                        0,
                        "%s",
                        "type >= FIRST_OBJECT") )
        {
            __debugbreak();
        }
        if ( typea >= 21 || typea == 18 )
            Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        if ( type >= 13
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        1233,
                        0,
                        "%s",
                        "type < FIRST_OBJECT") )
        {
            __debugbreak();
        }
        Scr_AddInt(type != 0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_AddDebugCommand()
{
    char *String; // eax

    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    Cbuf_AddText(0, String);
}

void GScr_IsMP()
{
    Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
}

void GScr_IsFloat()
{
    int Type; // eax

    Type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(Type == 5, SCRIPTINSTANCE_SERVER);
}

void GScr_IsInt()
{
    int Type; // eax

    Type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(Type == 6, SCRIPTINSTANCE_SERVER);
}

void GScr_IsVec()
{
    int Type; // eax

    Type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(Type == 4, SCRIPTINSTANCE_SERVER);
}

void GScr_IsString()
{
    int Type; // eax

    Type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(Type == 2, SCRIPTINSTANCE_SERVER);
}

void GScr_IsArray()
{
    int type; // [esp+0h] [ebp-4h]
    signed int typea; // [esp+0h] [ebp-4h]

    type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    if ( type == 1 )
    {
        typea = Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER);
        if ( typea < 13
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        1326,
                        0,
                        "%s",
                        "type >= FIRST_OBJECT") )
        {
            __debugbreak();
        }
        Scr_AddInt(typea == 20, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        if ( type >= 13
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        1331,
                        0,
                        "%s",
                        "type < FIRST_OBJECT") )
        {
            __debugbreak();
        }
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_IsAlive()
{
    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == 1
        && Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == 19
        && Scr_GetEntity(0)->health > 0 )
    {
        Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_GetDvar()
{
    VariableUnion v0; // eax
    char *dvarName; // [esp+0h] [ebp-8h]
    char *dvarValue; // [esp+4h] [ebp-4h]

    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == 6 )
    {
        v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        dvarValue = (char *)SV_Archived_Dvar_GetVariantString(v0.intValue);
    }
    else
    {
        dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        dvarValue = (char *)SV_Archived_Dvar_GetVariantString(dvarName);
    }
    Scr_AddString(dvarValue, SCRIPTINSTANCE_SERVER);
}

void GScr_GetDvarInt()
{
#ifdef KISAK_SP
    if (Scr_GetType(0, SCRIPTINSTANCE_SERVER) == VAR_INTEGER)
    {
        const int dvarHash = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        Scr_AddInt(Dvar_GetInt(dvarHash), SCRIPTINSTANCE_SERVER);
        return;
    }

    char *dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const char *dvarValue = SV_Archived_Dvar_GetVariantString(dvarName);
    Scr_AddInt(atoi(dvarValue), SCRIPTINSTANCE_SERVER);
#else
    VariableUnion v0; // eax
    const char *VariantString; // eax
    char *dvarName; // [esp+0h] [ebp-8h]
    int dvarValue; // [esp+4h] [ebp-4h]

    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == 6 )
    {
        v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        VariantString = SV_Archived_Dvar_GetVariantString(v0.intValue);
    }
    else
    {
        dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        VariantString = SV_Archived_Dvar_GetVariantString(dvarName);
    }
    dvarValue = atoi(VariantString);
    Scr_AddInt(dvarValue, SCRIPTINSTANCE_SERVER);
#endif
}

void GScr_GetDvarFloat()
{
#ifdef KISAK_SP
    float dvarValue;
    if (Scr_GetType(0, SCRIPTINSTANCE_SERVER) == VAR_INTEGER)
    {
        const int dvarHash = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        dvarValue = (float)Dvar_GetFloat(dvarHash);
    }
    else
    {
        const char *dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        dvarValue = (float)atof(SV_Archived_Dvar_GetVariantString(dvarName));
    }
    Scr_AddFloat(dvarValue, SCRIPTINSTANCE_SERVER);
#else
    VariableUnion v0; // eax
    const char *VariantString; // eax
    char *dvarName; // [esp+8h] [ebp-8h]
    float dvarValue; // [esp+Ch] [ebp-4h]

    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == 6 )
    {
        v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        VariantString = SV_Archived_Dvar_GetVariantString(v0.intValue);
    }
    else
    {
        dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        VariantString = SV_Archived_Dvar_GetVariantString(dvarName);
    }
    dvarValue = atof(VariantString);
    Scr_AddFloat(dvarValue, SCRIPTINSTANCE_SERVER);
#endif
}

void GScr_GetDvarColorRed()
{
    VariableUnion v0; // eax
    char *dvarName; // [esp+8h] [ebp-8h]
    float value; // [esp+Ch] [ebp-4h]

    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == 6 )
    {
        v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        value = Dvar_GetColorRed(v0.intValue);
    }
    else
    {
        dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        value = Dvar_GetColorRed(dvarName);
    }
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void GScr_GetDvarColorGreen()
{
    VariableUnion v0; // eax
    char *dvarName; // [esp+8h] [ebp-8h]
    float value; // [esp+Ch] [ebp-4h]

    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == 6 )
    {
        v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        value = Dvar_GetColorGreen(v0.intValue);
    }
    else
    {
        dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        value = Dvar_GetColorGreen(dvarName);
    }
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void GScr_GetDvarColorBlue()
{
    VariableUnion v0; // eax
    char *dvarName; // [esp+8h] [ebp-8h]
    float value; // [esp+Ch] [ebp-4h]

    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == 6 )
    {
        v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        value = Dvar_GetColorBlue(v0.intValue);
    }
    else
    {
        dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        value = Dvar_GetColorBlue(dvarName);
    }
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

#ifdef KISAK_SP
void GScr_SetDvar()
{
    const char *dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    char localizedValue[1024];
    const char *dvarValue;
    if (Scr_GetType(1, SCRIPTINSTANCE_SERVER) == 3)
    {
        Scr_ConstructMessageString(1, Scr_GetNumParam(SCRIPTINSTANCE_SERVER) - 1,
            "Dvar Value", localizedValue, sizeof(localizedValue));
        dvarValue = localizedValue;
    }
    else
    {
        dvarValue = Scr_GetString(1, SCRIPTINSTANCE_SERVER);
    }

    char cleanValue[1024];
    CleanDvarValue(dvarValue, cleanValue, sizeof(cleanValue));
    if (!Dvar_IsValidName(dvarName))
    {
        Scr_Error(va("Dvar %s has an invalid dvar name", dvarName), 0);
        return;
    }

    const dvar_s *dvar = Dvar_FindVar(dvarName);
    if (dvar)
    {
        if (!(dvar->flags & 0x4000u))
        {
            if (!(dvar->flags & 0x800u))
            {
                Scr_Error(va("Invalid Dvar set: %s - Internal Dvars cannot be changed by script. Use 'setsaveddvar' to alter SAVED internal dvars\n", dvarName), 0);
            }
            else if (sv_mapname && com_sv_running->current.enabled && Com_IsMenuLevel(0))
            {
                Dvar_SetFromStringByNameFromSource(dvarName, cleanValue, DVAR_SOURCE_SCRIPT, 0);
            }
            else
            {
                Scr_Error(va("Invalid Dvar set: %s - Menu-Writable Dvars can be changed by script, but only from levels that start with '%s'.\n", dvarName, "menu_"), 0);
            }
        }
        if (!(dvar->flags & 0x4000u))
            return;
    }
    Dvar_SetFromStringByNameFromSource(dvarName, cleanValue, DVAR_SOURCE_SCRIPT, 0);
}
#else
void GScr_SetDvar()
{
    const char *v0; // eax
    int NumParam; // eax
    char string[1024]; // [esp+8h] [ebp-818h] BYREF
    char outString[1028]; // [esp+408h] [ebp-418h] BYREF
    const char *dvarName; // [esp+810h] [ebp-10h]
    int type; // [esp+814h] [ebp-Ch]
    const dvar_s *dvar; // [esp+818h] [ebp-8h]
    const char *dvarValue; // [esp+81Ch] [ebp-4h]

    dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( !Dvar_IsValidName(dvarName) )
    {
        v0 = va("Dvar %s has an invalid dvar name", dvarName);
        Scr_Error(v0, 0);
    }
    type = Scr_GetType(1u, SCRIPTINSTANCE_SERVER);
    if ( type == 3 )
    {
        NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
        Scr_ConstructMessageString(1, NumParam - 1, "Dvar Value", string, 0x400u);
        dvarValue = string;
    }
    else
    {
        dvarValue = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    }
    CleanDvarValue(dvarValue, outString, 1024);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 3 )
        Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    Dvar_SetFromStringByName(dvarName, (char *)dvarValue);
    dvar = Dvar_FindVar(dvarName);
    if ( !dvar
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 1515, 0, "%s", "dvar") )
    {
        __debugbreak();
    }
    Dvar_AddFlags(dvar, 1024);
}
#endif

void __cdecl CleanDvarValue(const char *dvarValue, char *outString, int size)
{
    int i; // [esp+0h] [ebp-8h]

    for ( i = 0; i < size - 1 && dvarValue[i]; ++i )
    {
        *outString = I_CleanChar(dvarValue[i]);
        if ( *outString == 34 )
            *outString = 39;
        ++outString;
    }
    *outString = 0;
}

void GScr_GetTime()
{
    Scr_AddInt(level.time, SCRIPTINSTANCE_SERVER);
}

// LWSS ADD
#ifdef KISAK_SP
void GScr_GetCorpseArray()
{
    int i;
    gentity_s *ent;
    short eType;

    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
        Scr_Error("cannot call getcorpsearray with parameters", 0);

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);

    i = 0;
    if (level.num_entities > 0)
    {
        do
        {
            ent = &level.gentities[i];
            if (ent->r.inuse)
            {
                eType = ent->s.eType;
                if (eType == ET_VEHICLE_CORPSE || eType == ET_ACTOR_CORPSE)
                {
                    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
                    Scr_AddArray(SCRIPTINSTANCE_SERVER);
                }
            }
            ++i;
        } while (i < level.num_entities);
    }
}
#else
void GScr_GetCorpseArray()
{
    // REBUILT FROM IDA RETAIL MP BLOPS (LATEST)
    int i;
    gentity_s *ent;
    short eType;

    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
        Scr_Error("cannot call getcorpsearray with parameters", 0);

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);

    i = 0;
    if (level.num_entities > 0)
    {
        do
        {
            ent = &level.gentities[i];
            if (ent->r.linked)
            {
                eType = ent->s.eType;
                if (eType == ET_PLAYER_CORPSE || eType == ET_ACTOR_CORPSE)
                {
                    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
                    Scr_AddArray(SCRIPTINSTANCE_SERVER);
                }
            }
            ++i;
        } while (i < level.num_entities);
    }
}
#endif
// LWSS END

void GScr_GetAttachmentIndex()
{
    eAttachment attachmentIndex; // [esp+0h] [ebp-8h]
    char *attachmentName; // [esp+4h] [ebp-4h]

    attachmentName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    attachmentIndex = BG_GetAttachmentIndex(attachmentName);
    Scr_AddInt(attachmentIndex, SCRIPTINSTANCE_SERVER);
}

void Scr_GetEntByNum()
{
    int entnum; // [esp+4h] [ebp-4h]

    entnum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)entnum < 0x400 )
    {
        if ( g_entities[entnum].r.inuse )
            Scr_AddEntity(&g_entities[entnum], SCRIPTINSTANCE_SERVER);
    }
}

void Scr_GetWeaponStowedModel()
{
    char *v0; // eax
    unsigned int iWeaponIndex; // [esp+4h] [ebp-Ch]
    char *pszWeaponName; // [esp+8h] [ebp-8h]
    const WeaponDef *weapDef; // [esp+Ch] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName(pszWeaponName);
    weapDef = BG_GetWeaponDef(iWeaponIndex);
    if ( iWeaponIndex )
    {
        if ( weapDef->worldModel[1]
            && weapDef->weapClass != WEAPCLASS_GRENADE
            && weapDef->weapClass != WEAPCLASS_KILLSTREAK_ALT_STORED_WEAPON )
        {
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
            return;
        }
    }
    else if ( *pszWeaponName )
    {
        if ( I_stricmp(pszWeaponName, "none") )
        {
            v0 = va("unknown weapon '%s' in WeaponHasStowedModel\n", pszWeaponName);
            Com_Printf(17, v0);
        }
    }
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}

void Scr_GetWeaponModel()
{
    char *v0; // eax
    const WeaponDef *WeaponDef; // eax
    char *Name; // eax
    unsigned int weaponModel; // [esp+0h] [ebp-Ch]
    unsigned int iWeaponIndex; // [esp+4h] [ebp-8h]
    char *pszWeaponName; // [esp+8h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName(pszWeaponName);
    weaponModel = 0;
    if ( iWeaponIndex )
    {
        if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
        {
            weaponModel = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
            if ( weaponModel < 0x100 )
            {
                if ( !BG_GetWeaponDef(iWeaponIndex)->worldModel[weaponModel] )
                    weaponModel = 0;
            }
            else
            {
                weaponModel = 0;
            }
        }
        WeaponDef = BG_GetWeaponDef(iWeaponIndex);
        Name = (char *)XModelGetName(WeaponDef->worldModel[weaponModel]);
        Scr_AddString(Name, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        if ( *pszWeaponName )
        {
            if ( I_stricmp(pszWeaponName, "none") )
            {
                v0 = va("unknown weapon '%s' in getWeaponModel\n", pszWeaponName);
                Com_Printf(17, v0);
            }
        }
        Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_GetAmmoCount(scr_entref_t entref)
{
    int v1; // eax
    char *weaponName; // [esp+0h] [ebp-Ch]
    unsigned int weaponIndex; // [esp+4h] [ebp-8h]
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetPlayerEntity(entref);
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 1630, 0, "%s", "ent") )
        __debugbreak();
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 1631, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    if ( weaponIndex )
    {
        v1 = BG_WeaponAmmo(&ent->client->ps, weaponIndex);
        Scr_AddInt(v1, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
}

gentity_s *__cdecl GetPlayerEntity(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    const char *v4; // [esp+24h] [ebp-8h]
    gentity_s *ent; // [esp+28h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 463, 0, "%s", "ent") )
        __debugbreak();
    if ( !ent->client )
    {
        if ( ent->targetname )
            v4 = SL_ConvertToString(ent->targetname, SCRIPTINSTANCE_SERVER);
        else
            v4 = "<undefined>";
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va(
                     "only valid on players; called on entity %i at %.0f %.0f %.0f classname %s targetname %s\n",
                     entref.entnum,
                     ent->r.currentOrigin[0],
                     ent->r.currentOrigin[1],
                     ent->r.currentOrigin[2],
                     v1,
                     v4);
        Scr_Error(v2, 0);
    }
    return ent;
}

gentity_s *__cdecl GetEntity(scr_entref_t entref)
{
    if ( entref.classnum )
    {
        Scr_ObjectError("not an entity", SCRIPTINSTANCE_SERVER);
        return 0;
    }
    else
    {
        if ( entref.entnum >= 0x400u
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        449,
                        0,
                        "%s",
                        "entref.entnum < MAX_GENTITIES") )
        {
            __debugbreak();
        }
        return &g_entities[entref.entnum];
    }
}

void GScr_GetAnimLength()
{
    float value; // [esp+0h] [ebp-14h]
    scr_anim_s anim; // [esp+Ch] [ebp-8h]
    XAnim_s *anims; // [esp+10h] [ebp-4h]

    anim = Scr_GetAnim(0, 0, SCRIPTINSTANCE_SERVER);
    anims = Scr_GetAnims(anim.tree, SCRIPTINSTANCE_SERVER);
    if ( !XAnimIsPrimitive(anims, anim.index) )
        Scr_ParamError(0, "non-primitive animation has no concept of length", SCRIPTINSTANCE_SERVER);
    value = XAnimGetLength(anims, anim.index);
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void GScr_AnimHasNotetrack()
{
    scr_anim_s anim = Scr_GetAnim(0, 0, SCRIPTINSTANCE_SERVER);
    uint16_t name = Scr_GetConstString(1, SCRIPTINSTANCE_SERVER);
    Scr_AddBool(XAnimNotetrackExists(Scr_GetAnims(anim.tree, SCRIPTINSTANCE_SERVER), anim.index, name), SCRIPTINSTANCE_SERVER);
}

void GScr_GetNotetrackTimes()
{
    scr_anim_s anim = Scr_GetAnim(0, 0, SCRIPTINSTANCE_SERVER);
    uint32_t name = Scr_GetConstString(1, SCRIPTINSTANCE_SERVER);
    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    XAnimAddNotetrackTimesToScriptArray(Scr_GetAnims(anim.tree, SCRIPTINSTANCE_SERVER), anim.index, name);
}

void GScr_GetBrushModelCenter()
{
    gentity_s *pEnt; // [esp+8h] [ebp-10h]
    float vCenter[3]; // [esp+Ch] [ebp-Ch] BYREF

    pEnt = Scr_GetEntity(0);
    vCenter[0] = pEnt->r.absmin[0] + pEnt->r.absmax[0];
    vCenter[1] = pEnt->r.absmin[1] + pEnt->r.absmax[1];
    vCenter[2] = pEnt->r.absmin[2] + pEnt->r.absmax[2];
    vCenter[0] = 0.5 * vCenter[0];
    vCenter[1] = 0.5 * vCenter[1];
    vCenter[2] = 0.5 * vCenter[2];
    Scr_AddVector(vCenter, SCRIPTINSTANCE_SERVER);
}

void GScr_Spawn()
{
    char *v0; // eax
    const char *v1; // eax
    float *currentOrigin; // [esp+0h] [ebp-1Ch]
    float origin[3]; // [esp+4h] [ebp-18h] BYREF
    int iSpawnFlags; // [esp+10h] [ebp-Ch]
    unsigned __int16 classname; // [esp+14h] [ebp-8h]
    gentity_s *ent; // [esp+18h] [ebp-4h]

    classname = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 2 )
        iSpawnFlags = 0;
    else
        iSpawnFlags = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    ent = G_Spawn();
    Scr_SetString(&ent->classname, classname, SCRIPTINSTANCE_SERVER);
    currentOrigin = ent->r.currentOrigin;
    ent->r.currentOrigin[0] = origin[0];
    currentOrigin[1] = origin[1];
    currentOrigin[2] = origin[2];
    ent->spawnflags = iSpawnFlags;
    if ( G_CallSpawnEntity(ent) )
    {
        Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        v0 = SL_ConvertToString(classname, SCRIPTINSTANCE_SERVER);
        v1 = va("unable to spawn \"%s\" entity", v0);
        Scr_Error(v1, 0);
    }
}

void GScr_SpawnCollision()
{
    const char *v0; // eax
    float *currentAngles; // [esp+0h] [ebp-2Ch]
    float *currentOrigin; // [esp+4h] [ebp-28h]
    float origin[3]; // [esp+8h] [ebp-24h] BYREF
    float angles[3]; // [esp+14h] [ebp-18h] BYREF
    unsigned int targetname; // [esp+20h] [ebp-Ch]
    gentity_s *ent; // [esp+24h] [ebp-8h]
    const char *modelname; // [esp+28h] [ebp-4h]

    modelname = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    targetname = Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2u, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(3u, angles, SCRIPTINSTANCE_SERVER);
    ent = G_Spawn();
    Scr_SetString(&ent->classname, scr_const.script_model, SCRIPTINSTANCE_SERVER);
    G_SetModel(ent, (char *)modelname);
    if ( !ent->model )
    {
        v0 = va("SpawnCollision: Collision model name %s is not valid.", modelname);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    Scr_SetString(&ent->targetname, targetname, SCRIPTINSTANCE_SERVER);
    currentOrigin = ent->r.currentOrigin;
    ent->r.currentOrigin[0] = origin[0];
    currentOrigin[1] = origin[1];
    currentOrigin[2] = origin[2];
    currentAngles = ent->r.currentAngles;
    ent->r.currentAngles[0] = angles[0];
    currentAngles[1] = angles[1];
    currentAngles[2] = angles[2];
    G_CallSpawnEntity(ent);
    ent->s.lerp.eFlags |= 0x200u;
    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
}

void GScr_SpawnVehicle()
{
    const char *v0; // eax
    char *v1; // eax
    char *v2; // eax
    float *currentAngles; // [esp+0h] [ebp-34h]
    float *currentOrigin; // [esp+4h] [ebp-30h]
    float origin[3]; // [esp+8h] [ebp-2Ch] BYREF
    float angles[3]; // [esp+14h] [ebp-20h] BYREF
    unsigned int targetname; // [esp+20h] [ebp-14h]
    unsigned int vehicletype; // [esp+24h] [ebp-10h]
    gentity_s *ent; // [esp+28h] [ebp-Ch]
    unsigned int destructibledef; // [esp+2Ch] [ebp-8h]
    const char *modelname; // [esp+30h] [ebp-4h]

    modelname = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    targetname = Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    vehicletype = Scr_GetConstString(2u, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(3u, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(4u, angles, SCRIPTINSTANCE_SERVER);
    ent = G_Spawn();
    Scr_SetString(&ent->classname, scr_const.script_vehicle, SCRIPTINSTANCE_SERVER);
    G_SetModel(ent, (char *)modelname);
    if ( !ent->model )
    {
        v0 = va("SpawnVehicle: Vehicle model name %s is not valid.", modelname);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    Scr_SetString(&ent->targetname, targetname, SCRIPTINSTANCE_SERVER);
    currentOrigin = ent->r.currentOrigin;
    ent->r.currentOrigin[0] = origin[0];
    currentOrigin[1] = origin[1];
    currentOrigin[2] = origin[2];
    currentAngles = ent->r.currentAngles;
    ent->r.currentAngles[0] = angles[0];
    currentAngles[1] = angles[1];
    currentAngles[2] = angles[2];
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 5 )
    {
        destructibledef = Scr_GetConstString(5u, SCRIPTINSTANCE_SERVER);
        v1 = SL_ConvertToString(destructibledef, SCRIPTINSTANCE_SERVER);
        G_SetupDestructible(ent, v1);
    }
    v2 = SL_ConvertToString(vehicletype, SCRIPTINSTANCE_SERVER);
    G_SpawnVehicle(ent, v2, 0);
    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
    G_MakeVehicleUsable(ent);
}

void GScr_SpawnPlane()
{
    char *v0; // eax
    const char *v1; // eax
    float *currentOrigin; // [esp+0h] [ebp-28h]
    float origin[3]; // [esp+4h] [ebp-24h] BYREF
    int iSpawnFlags; // [esp+10h] [ebp-18h]
    int team; // [esp+14h] [ebp-14h]
    gentity_s *owner; // [esp+18h] [ebp-10h]
    unsigned __int16 classname; // [esp+1Ch] [ebp-Ch]
    gentity_s *ent; // [esp+20h] [ebp-8h]
    int ownerIndex; // [esp+24h] [ebp-4h]

    owner = Scr_GetEntity(0);
    if ( !owner->client )
        Scr_ParamError(0, "Owner entity is not a player", SCRIPTINSTANCE_SERVER);
    classname = (unsigned __int16)Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2u, origin, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 3 )
        iSpawnFlags = 0;
    else
        iSpawnFlags = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
    ent = G_Spawn();
    Scr_SetString(&ent->classname, classname, SCRIPTINSTANCE_SERVER);
    currentOrigin = ent->r.currentOrigin;
    ent->r.currentOrigin[0] = origin[0];
    currentOrigin[1] = origin[1];
    currentOrigin[2] = origin[2];
    ent->spawnflags = iSpawnFlags;
    team = owner->client->sess.cs.team;
    if ( (unsigned int)team >= 4
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    1876,
                    0,
                    "team doesn't index (1 << 2)\n\t%i not in [0, %i)",
                    team,
                    4) )
    {
        __debugbreak();
    }
    ownerIndex = owner->client - level.clients;
    if ( (unsigned int)ownerIndex >= level.maxclients
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    1878,
                    0,
                    "ownerIndex doesn't index level.maxclients\n\t%i not in [0, %i)",
                    ownerIndex,
                    level.maxclients) )
    {
        __debugbreak();
    }
    if ( G_CallSpawnEntity(ent) )
    {
        Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
        ent->s.eType = ET_PLANE;
        ent->s.faction.iHeadIconTeam = team | (4 * ownerIndex);
    }
    else
    {
        ent->s.eType = ET_PLANE;
        ent->s.faction.iHeadIconTeam = team | (4 * ownerIndex);
        v0 = SL_ConvertToString(classname, SCRIPTINSTANCE_SERVER);
        v1 = va("unable to spawn \"%s\" entity", v0);
        Scr_Error(v1, 0);
    }
}

void GScr_SpawnTimedFX()
{
    unsigned __int8 v0; // al
    char *weaponName; // [esp+Ch] [ebp-2Ch]
    float origin[3]; // [esp+10h] [ebp-28h] BYREF
    int time; // [esp+1Ch] [ebp-1Ch]
    int weaponIndex; // [esp+20h] [ebp-18h]
    gentity_s *ent; // [esp+24h] [ebp-14h]
    float direction[3]; // [esp+28h] [ebp-10h] BYREF
    const WeaponDef *weapDef; // [esp+34h] [ebp-4h]
    int savedregs; // [esp+38h] [ebp+0h] BYREF

    direction[0] = 0.0f;
    direction[1] = 0.0f;
    direction[2] = 1.0f;
    time = 10;
    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 2 )
        Scr_GetVector(2u, direction, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 3 )
        time = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    weapDef = BG_GetWeaponDef(weaponIndex);
    ent = G_Spawn();
    G_SetOrigin(ent, origin);
    ent->s.weapon = weaponIndex;
    Vec3Normalize(direction);
    ent->s.eType = ET_GENERAL;
    ent->s.lerp.eFlags |= 0x20u;
    ent->s.weapon = ent->s.weapon;
    G_BroadcastEntity(ent);
    v0 = DirToByte(direction);
    G_AddEvent(ent, 0x43u, v0);
    ent->s.lerp.pos.trBase[0] = (float)(int)ent->s.lerp.pos.trBase[0];
    ent->s.lerp.pos.trBase[1] = (float)(int)ent->s.lerp.pos.trBase[1];
    ent->s.lerp.pos.trBase[2] = (float)(int)ent->s.lerp.pos.trBase[2];
    G_SetOrigin(ent, ent->s.lerp.pos.trBase);
    ent->s.lerp.eFlags |= 0x4000u;
    ent->s.lerp.u.actor.actorNum = level.time;
    ent->s.time2 = level.time + 1000 * time;
    ent->s.lerp.eFlags |= 0x10u;
    ent->handler = 11;
    ent->nextthink = level.time + 1;
    SV_LinkEntity( ent);
}

gentity_s *__cdecl SpawnTurretInternal(unsigned int classname, float *origin, const char *weaponinfoname)
{
    gentity_s *ent; // [esp+4h] [ebp-4h]

#ifdef KISAK_SP
    G_RecordSpawnAttempt_SP(classname);
#endif
    ent = G_Spawn();
    Scr_SetString(&ent->classname, classname, SCRIPTINSTANCE_SERVER);
    ent->r.currentOrigin[0] = *origin;
    ent->r.currentOrigin[1] = origin[1];
    ent->r.currentOrigin[2] = origin[2];
    G_SpawnTurret(ent, weaponinfoname, 0);
    return ent;
}

void GScr_SpawnTurret()
{
    float origin[3]; // [esp+0h] [ebp-18h] BYREF
    unsigned __int16 classname; // [esp+Ch] [ebp-Ch]
    gentity_s *ent; // [esp+10h] [ebp-8h]
    const char *weaponinfoname; // [esp+14h] [ebp-4h]
    int savedregs; // [esp+18h] [ebp+0h] BYREF

    ent = 0;
    classname = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    weaponinfoname = Scr_GetString(2u, SCRIPTINSTANCE_SERVER);
    ent = SpawnTurretInternal(classname, origin, weaponinfoname);
    ent->takedamage = 1;
    ent->r.svFlags = 4;
    SV_LinkEntity(ent);
    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
}

void GScr_SpawnHelicopter()
{
    float *currentAngles; // [esp+0h] [ebp-30h]
    float *currentOrigin; // [esp+4h] [ebp-2Ch]
    float origin[3]; // [esp+8h] [ebp-28h] BYREF
    gentity_s *owner; // [esp+14h] [ebp-1Ch]
    const char *vehicleInfoName; // [esp+18h] [ebp-18h]
    float angles[3]; // [esp+1Ch] [ebp-14h] BYREF
    gentity_s *ent; // [esp+28h] [ebp-8h]
    const char *modelname; // [esp+2Ch] [ebp-4h]

    owner = Scr_GetEntity(0);
    if ( !owner->client )
        Scr_ParamError(0, "Owner entity is not a player", SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2u, angles, SCRIPTINSTANCE_SERVER);
    vehicleInfoName = Scr_GetString(3u, SCRIPTINSTANCE_SERVER);
    modelname = Scr_GetString(4u, SCRIPTINSTANCE_SERVER);
    ent = G_Spawn();
    Scr_SetString(&ent->classname, scr_const.script_vehicle, SCRIPTINSTANCE_SERVER);
    currentOrigin = ent->r.currentOrigin;
    ent->r.currentOrigin[0] = origin[0];
    currentOrigin[1] = origin[1];
    currentOrigin[2] = origin[2];
    currentAngles = ent->r.currentAngles;
    ent->r.currentAngles[0] = angles[0];
    currentAngles[1] = angles[1];
    currentAngles[2] = angles[2];
    G_SpawnHelicopter(ent, owner, (char *)vehicleInfoName, (char *)modelname);
    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetTurretCarried(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    VariableUnion v3; // eax
    gentity_s *self; // [esp+8h] [ebp-4h]

    self = GetEntity(entref);
    if ( !self->pTurretInfo )
    {
        v1 = SL_ConvertToString(self->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    v3.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    Turret_SetTurretCarried(self, v3.intValue);
}

void GScr_GetAnimTreesLoaded()
{
    XAnim_s *animTree; // [esp+10h] [ebp-8h]
    unsigned __int16 treeIndex; // [esp+14h] [ebp-4h]

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for (treeIndex = 1; ; ++treeIndex)
    {
        if (treeIndex > gScrAnimPub[0].xanim_num[1])
            break;
        animTree = gScrAnimPub[0].xanim_lookup[1][treeIndex].anims;
        if (animTree
            && animTree->size > 1
            && animTree->debugName
            && &animTree->debugName[strlen(animTree->debugName) + 1] != animTree->debugName + 1)
        {
            Scr_AddString((char *)animTree->debugName, SCRIPTINSTANCE_SERVER);
            Scr_AddArray(SCRIPTINSTANCE_SERVER);
        }
    }
}

void GScr_FindAnimByName()
{
    char *v1; // eax
    char *v2; // eax
    unsigned __int16 i; // [esp+48h] [ebp-1Ch]
    XAnim_s *animTree; // [esp+4Ch] [ebp-18h]
    scr_anim_s retAnim; // [esp+50h] [ebp-14h]
    char *temp; // [esp+54h] [ebp-10h]
    char *tempa; // [esp+54h] [ebp-10h]
    const char *treeNameParam; // [esp+5Ch] [ebp-8h]
    unsigned __int16 treeIndex; // [esp+60h] [ebp-4h]

    animTree = 0;
    temp = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if (strlen(temp))
    {
        treeNameParam = temp;
        tempa = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        if (strlen(tempa))
        {
            for (treeIndex = 1; treeIndex <= gScrAnimPub[0].xanim_num[1]; ++treeIndex)
            {
                if (!strcmp(gScrAnimPub[0].xanim_lookup[1][treeIndex].anims->debugName, treeNameParam))
                {
                    animTree = gScrAnimPub[0].xanim_lookup[1][treeIndex].anims;
                    break;
                }
            }
            if (animTree)
            {
                for (i = 1; i <= animTree->size - 1; ++i)
                {
                    if (animTree->entries[i].bCreated && !strcmp(animTree->debugAnimNames[i], tempa))
                    {
                        retAnim.tree = treeIndex;
                        retAnim.index = i;
                        Scr_AddAnim(retAnim, SCRIPTINSTANCE_SERVER);
                        return;
                    }
                }
                Scr_AddAnim((scr_anim_s)65537, SCRIPTINSTANCE_SERVER);
                v2 = va("Couldn't find anim %s in animtree %s\n", tempa, treeNameParam);
                Scr_Error(v2, 0);
            }
            else
            {
                v1 = va("Couldn't find animtree %s\n", treeNameParam);
                Scr_Error(v1, 0);
            }
        }
    }
}

void GScr_PrecacheTurret()
{
    char *turretInfo; // [esp+0h] [ebp-4h]

    if ( !level.initializing )
        Scr_Error("precacheTurret must be called before any wait statements in the level script\n", 0);
    turretInfo = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    G_GetWeaponIndexForName(turretInfo);
}

void __cdecl ScrCmd_SetMoveSpeedScale(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetPlayerEntity(entref);
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 2217, 0, "%s", "ent") )
        __debugbreak();
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 2218, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    ent->client->sess.moveSpeedScaleMultiplier = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetMoveSpeedScale(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetPlayerEntity(entref);
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 2230, 0, "%s", "ent") )
        __debugbreak();
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 2231, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    Scr_AddFloat(ent->client->sess.moveSpeedScaleMultiplier, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_attach(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    char *v3; // eax
    const char *v4; // eax
    char *String; // [esp+10h] [ebp-2Ch]
    VariableUnion v6; // [esp+18h] [ebp-24h]
    VariableUnion v7; // [esp+1Ch] [ebp-20h]
    int i; // [esp+20h] [ebp-1Ch]
    char *modelName; // [esp+2Ch] [ebp-10h]
    gentity_s *ent; // [esp+30h] [ebp-Ch]

    ent = GetEntity(entref);
    modelName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 2 )
        v7.intValue = scr_const._;
    else
        v7.intValue = Scr_GetConstLowercaseString(1u, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 3 )
        v6.intValue = 0;
    else
        v6.intValue = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 4 )
        Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 5 )
        String = (char *)"";
    else
        String = Scr_GetString(4u, SCRIPTINSTANCE_SERVER);
    if ( G_EntDetach(ent, modelName, v7.stringValue) )
    {
        v1 = SL_ConvertToString(v7.stringValue, SCRIPTINSTANCE_SERVER);
        v2 = va("model '%s' already attached to tag '%s'", modelName, v1);
        Scr_Error(v2, 0);
    }
    if ( !G_EntAttach(ent, modelName, v7.stringValue, v6.intValue) )
    {
        v3 = SL_ConvertToString(v7.stringValue, SCRIPTINSTANCE_SERVER);
        v4 = va("failed to attach model '%s' to tag '%s'", modelName, v3);
        Scr_Error(v4, 0);
    }
    if ( strlen(String) && ent->client )
    {
        ent->client->ps.stowedWeapon = BG_FindWeaponIndexForName(String);
        ent->client->ps.stowedWeaponCamo = 0;
        for ( i = 0; i < 15; ++i )
        {
            if ( ent->client->ps.heldWeapons[i].weapon == ent->client->ps.stowedWeapon )
            {
                ent->client->ps.stowedWeaponCamo = ent->client->ps.heldWeapons[i].options.i & 0x3F;
                break;
            }
        }
    }
#ifdef KISAK_SP
    // THE NULL DEREF. This else runs whenever the 5th (stowed-weapon-name) argument is absent,
    // INCLUDING when ent->client is null -- the && above short-circuits into here rather than
    // skipping the block. On a non-player entity that is a write through a null pointer.
    //
    // It is unreachable in MP because attach() is only ever called on players there, and it was
    // unreachable in this tree until aitype/animscript code started running on AI: every SP
    // character script attaches a head model (character/c_usa_interrogation_sillhouette.gsc:6
    // `self attach(self.headModel, "", true)`) and animscripts/shared.gsc:145 attaches the
    // weapon model on every AI weapon change. Measured: EXCEPTION_ACCESS_VIOLATION at
    // OpenBlops.exe+0x398688 == ScrCmd_attach+0x198 == this line, EAX/ECX = 0, which the SP
    // unhandled-exception filter reports as a bare "Com_ERROR: Fatal Error".
    //
    // Retail SP is immune for a stronger reason: its ScrCmd_attach (0x007f1a10) ENDS at the
    // G_EntAttach check. It reads three parameters (model, tag, ignoreCollision), calls
    // G_EntDetach / G_EntAttach with the two Scr_Error paths above, and returns. There is no
    // 4th or 5th parameter, no stowedWeapon, and no dobjDirty -- stowed weapons are an MP-only
    // feature. So on SP the whole tail could be deleted; it is only guarded here, to keep an
    // SP player attach marking its dobj dirty exactly as it does today.
    //
    // The same defect is live in MP as a latent null deref on any non-player attach. Not fixed
    // there: MP output must stay byte-identical.
    else if ( ent->client )
    {
        ent->client->ps.stowedWeapon = 0;
    }
#else
    else
    {
        ent->client->ps.stowedWeapon = 0;
    }
#endif
    if ( ent->client )
        level_bgs.clientinfo[ent->s.number].dobjDirty = 1;
}

void __cdecl ScrCmd_detach(scr_entref_t entref)
{
    unsigned int v1; // eax
    char *v2; // eax
    char *v3; // eax
    const char *v4; // eax
    char *v5; // [esp-4h] [ebp-18h]
    VariableUnion v6; // [esp+0h] [ebp-14h]
    char *modelName; // [esp+8h] [ebp-Ch]
    gentity_s *ent; // [esp+Ch] [ebp-8h]
    int i; // [esp+10h] [ebp-4h]

    ent = GetEntity(entref);
    modelName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 2 )
        v6.intValue = scr_const._;
    else
        v6.intValue = Scr_GetConstLowercaseString(1u, SCRIPTINSTANCE_SERVER);
    if ( !G_EntDetach(ent, modelName, v6.stringValue) )
    {
        Com_Printf(24, "Current attachments:\n");
#ifdef KISAK_SP
        for ( i = 0; i < ARRAY_COUNT(ent->attachModelNames); ++i )
#else
        for ( i = 0; i < 19; ++i )
#endif
        {
            if ( ent->attachModelNames[i] )
            {
                if ( ent->attachTagNames[i] )
                {
                    v5 = SL_ConvertToString(ent->attachTagNames[i], SCRIPTINSTANCE_SERVER);
                    v1 = G_ModelName(ent->attachModelNames[i]);
                    v2 = SL_ConvertToString(v1, SCRIPTINSTANCE_SERVER);
                    Com_Printf(24, "model: '%s', tag: '%s'\n", v2, v5);
                }
            }
        }
        v3 = SL_ConvertToString(v6.stringValue, SCRIPTINSTANCE_SERVER);
        v4 = va("failed to detach model '%s' from tag '%s'", modelName, v3);
        Scr_Error(v4, 0);
    }
}

void __cdecl ScrCmd_detachAll(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    G_EntDetachAll(ent);
}

void __cdecl ScrCmd_GetAttachSize(scr_entref_t entref)
{
    gentity_s *ent; // [esp+0h] [ebp-8h]
    int i; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
#ifdef KISAK_SP
    for ( i = 0; i < ARRAY_COUNT(ent->attachModelNames) && ent->attachModelNames[i]; ++i )
#else
    for ( i = 0; i < 19 && ent->attachModelNames[i]; ++i )
#endif
        ;
    Scr_AddInt(i, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetAttachModelName(scr_entref_t entref)
{
    unsigned int v1; // eax
    gentity_s *ent; // [esp+0h] [ebp-8h]
    int i; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    i = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
    if ( (unsigned int)i >= ARRAY_COUNT(ent->attachModelNames) || !ent->attachModelNames[i] )
#else
    if ( (unsigned int)i >= 0x13 || !ent->attachModelNames[i] )
#endif
        Scr_ParamError(0, "bad index", SCRIPTINSTANCE_SERVER);
    v1 = G_ModelName(ent->attachModelNames[i]);
    Scr_AddConstString(v1, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetAttachTagName(scr_entref_t entref)
{
    gentity_s *ent; // [esp+0h] [ebp-8h]
    int i; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    i = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
    if ( (unsigned int)i >= ARRAY_COUNT(ent->attachModelNames) || !ent->attachModelNames[i] )
#else
    if ( (unsigned int)i >= 0x13 || !ent->attachModelNames[i] )
#endif
        Scr_ParamError(0, "bad index", SCRIPTINSTANCE_SERVER);
    if ( !ent->attachTagNames[i]
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    2364,
                    0,
                    "%s",
                    "ent->attachTagNames[i]") )
    {
        __debugbreak();
    }
    Scr_AddConstString(ent->attachTagNames[i], SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetAttachIgnoreCollision(scr_entref_t entref)
{
    gentity_s *ent; // [esp+0h] [ebp-8h]
    int i; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    i = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
    if ( (unsigned int)i >= ARRAY_COUNT(ent->attachModelNames) || !ent->attachModelNames[i] )
#else
    if ( (unsigned int)i >= 0x13 || !ent->attachModelNames[i] )
#endif
        Scr_ParamError(0, "bad index", SCRIPTINSTANCE_SERVER);
    Scr_AddBool((ent->attachIgnoreCollision & (1 << i)) != 0, SCRIPTINSTANCE_SERVER);
}

void __cdecl G_EntityStateSetPartBits(gentity_s *ent, const unsigned int *partBits)
{
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 2387, 0, "%s", "ent") )
        __debugbreak();
    ent->s.partBits[0] = *partBits;
    ent->s.partBits[1] = partBits[1];
    ent->s.partBits[2] = partBits[2];
    ent->s.partBits[3] = partBits[3];
    ent->s.partBits[4] = partBits[4];
}

void __cdecl G_EntityStateGetPartBits(const gentity_s *ent, unsigned int *partBits)
{
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 2394, 0, "%s", "ent") )
        __debugbreak();
    *partBits = ent->s.partBits[0];
    partBits[1] = ent->s.partBits[1];
    partBits[2] = ent->s.partBits[2];
    partBits[3] = ent->s.partBits[3];
    partBits[4] = ent->s.partBits[4];
}

void __cdecl ScrCmd_hidepart(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    char *v3; // eax
    const char *v4; // eax
    const char *v5; // [esp-8h] [ebp-30h]
    unsigned int tagName; // [esp+0h] [ebp-28h]
    unsigned __int8 boneIndex; // [esp+7h] [ebp-21h] BYREF
    DObj *obj; // [esp+8h] [ebp-20h]
    const char *modelName; // [esp+Ch] [ebp-1Ch]
    gentity_s *ent; // [esp+10h] [ebp-18h]
    unsigned int partBits[5]; // [esp+14h] [ebp-14h] BYREF

    ent = GetEntity(entref);
    obj = Com_GetServerDObj(ent->s.number);
    if ( !obj )
        Scr_Error("entity has no model", 0);
    boneIndex = -2;
    tagName = Scr_GetConstLowercaseString(0, SCRIPTINSTANCE_SERVER);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        if ( !DObjGetBoneIndex(obj, tagName, &boneIndex, -1) )
        {
            v1 = SL_ConvertToString(tagName, SCRIPTINSTANCE_SERVER);
            v2 = va("cannot find part '%s' in entity model", v1);
            Scr_Error(v2, 0);
        }
    }
    else
    {
        modelName = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        if ( !DObjGetModelBoneIndex(obj, modelName, tagName, &boneIndex) )
        {
            v5 = modelName;
            v3 = SL_ConvertToString(tagName, SCRIPTINSTANCE_SERVER);
            v4 = va("cannot find part '%s' in entity model '%s'", v3, v5);
            Scr_Error(v4, 0);
        }
    }
    G_EntityStateGetPartBits(ent, partBits);
    partBits[(int)boneIndex >> 5] |= 0x80000000 >> (boneIndex & 0x1F);
    DObjSetHidePartBits(obj, partBits);
    G_EntityStateSetPartBits(ent, partBits);
}

void __cdecl ScrCmd_showpart(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    char *v3; // eax
    const char *v4; // eax
    const char *v5; // [esp-8h] [ebp-30h]
    unsigned int tagName; // [esp+0h] [ebp-28h]
    unsigned __int8 boneIndex; // [esp+7h] [ebp-21h] BYREF
    DObj *obj; // [esp+8h] [ebp-20h]
    const char *modelName; // [esp+Ch] [ebp-1Ch]
    gentity_s *ent; // [esp+10h] [ebp-18h]
    unsigned int partBits[5]; // [esp+14h] [ebp-14h] BYREF

    ent = GetEntity(entref);
    obj = Com_GetServerDObj(ent->s.number);
    if ( !obj )
        Scr_Error("entity has no model", 0);
    boneIndex = -2;
    tagName = Scr_GetConstLowercaseString(0, SCRIPTINSTANCE_SERVER);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        if ( !DObjGetBoneIndex(obj, tagName, &boneIndex, -1) )
        {
            v1 = SL_ConvertToString(tagName, SCRIPTINSTANCE_SERVER);
            v2 = va("cannot find part '%s' in entity model", v1);
            Scr_Error(v2, 0);
        }
    }
    else
    {
        modelName = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        if ( !DObjGetModelBoneIndex(obj, modelName, tagName, &boneIndex) )
        {
            v5 = modelName;
            v3 = SL_ConvertToString(tagName, SCRIPTINSTANCE_SERVER);
            v4 = va("cannot find part '%s' in entity model '%s'", v3, v5);
            Scr_Error(v4, 0);
        }
    }
    G_EntityStateGetPartBits(ent, partBits);
    partBits[(int)boneIndex >> 5] &= ~(0x80000000 >> (boneIndex & 0x1F));
    DObjSetHidePartBits(obj, partBits);
    G_EntityStateSetPartBits(ent, partBits);
}

void __cdecl ScrCmd_showallparts(scr_entref_t entref)
{
    DObj *obj; // [esp+0h] [ebp-1Ch]
    gentity_s *ent; // [esp+4h] [ebp-18h]
    unsigned int partBits[5]; // [esp+8h] [ebp-14h] BYREF

    ent = GetEntity(entref);
    obj = Com_GetServerDObj(ent->s.number);
    if ( !obj )
        Scr_Error("entity has no model", 0);
    memset(partBits, 0, sizeof(partBits));
    DObjSetHidePartBits(obj, partBits);
    G_EntityStateSetPartBits(ent, partBits);
}

#ifdef KISAK_SP
void __cdecl ScrCmd_SetVisibleToPlayer(scr_entref_t entref)
{
    gentity_s *ent;
    gentity_s *player;

    ent = GetEntity(entref);
    player = Scr_GetEntity(0);
    ent->r.clientMask[0] = -1;
    if ( !player->client
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    2496,
                    0,
                    "%s",
                    "player->client") )
    {
        __debugbreak();
    }
    const unsigned int clientNum = static_cast<unsigned char>(player->client->ps.clientNum);
    const unsigned int mask = static_cast<unsigned int>(ent->r.clientMask[clientNum >> 5]) & ~(1u << (clientNum & 31));
    std::memcpy(&ent->r.clientMask[clientNum >> 5], &mask, sizeof(mask));
}
#else
void __cdecl ScrCmd_SetVisibleToPlayer(scr_entref_t entref)
{
    gentity_s *ent; // [esp+0h] [ebp-Ch]
    gentity_s *player; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    player = Scr_GetEntity(0);
    if ( !player->client
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    2496,
                    0,
                    "%s",
                    "player->client") )
    {
        __debugbreak();
    }
    ent->r.clientMask[player->s.number >> 5] &= ~(1 << (player->s.number & 0x1F));
}
#endif

void __cdecl ScrCmd_SetInvisibleToPlayer(scr_entref_t entref)
{
    int v1; // eax
    int invisible; // [esp+0h] [ebp-10h]
    gentity_s *ent; // [esp+4h] [ebp-Ch]
    int clientNum; // [esp+8h] [ebp-8h]
    gentity_s *player; // [esp+Ch] [ebp-4h]

    invisible = 1;
    ent = GetEntity(entref);
    player = Scr_GetEntity(0);
    if ( !player->client
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    2512,
                    0,
                    "%s",
                    "player->client") )
    {
        __debugbreak();
    }
#ifdef KISAK_SP
    clientNum = static_cast<unsigned char>(player->client->ps.clientNum);
#else
    clientNum = player->s.number;
#endif
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
        invisible = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    if ( invisible )
        v1 = ent->r.clientMask[clientNum >> 5] | (1 << (clientNum & 0x1F));
    else
        v1 = ent->r.clientMask[clientNum >> 5] & ~(1 << (clientNum & 0x1F));
    ent->r.clientMask[clientNum >> 5] = v1;
}

void __cdecl ScrCmd_SetVisibleToAll(scr_entref_t entref)
{
    GetEntity(entref)->r.clientMask[0] = 0;
}

// LWSS ADD
void ScrCmd_OverrideLightingOrigin(scr_entref_t entref)
{
    // LWSS: I think this is a new flag that needs impl elsewhere (this is useless to do without that) KISAKTODO
    //gentity_s *pSelf;
    //
    //pSelf = GetEntity(entref);
    //pSelf->s.lerp.eFlags |= 0x8000000;
}
// LWSS END

void __cdecl ScrCmd_SetForceNoCull(scr_entref_t entref)
{
    gentity_s *Entity; // edx

    Entity = GetEntity(entref);
    Entity->s.lerp.eFlags2 |= 0x4000000u;
}

void __cdecl ScrCmd_SetInvisibleToAll(scr_entref_t entref)
{
    GetEntity(entref)->r.clientMask[0] = -1;
}

void __cdecl ScrCmd_SetVisibleToTeam(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    int teamNum; // [esp+0h] [ebp-14h]
    unsigned __int16 team; // [esp+4h] [ebp-10h]
    int entIndex; // [esp+8h] [ebp-Ch]
    gentity_s *ent; // [esp+Ch] [ebp-8h]
    gentity_s *clientEnt; // [esp+10h] [ebp-4h]

    ent = GetEntity(entref);
    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( team != scr_const.allies && team != scr_const.axis )
    {
        v1 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v2 = va("Illegal team string '%s'. Must be allies, or axis.", v1);
        Scr_Error(v2, 0);
    }
    if ( team == scr_const.allies )
        teamNum = 2;
    else
        teamNum = 1;
    ent->r.clientMask[0] = -1;
    clientEnt = g_entities;
    for ( entIndex = 0; entIndex < com_maxclients->current.integer; ++entIndex )
    {
        if ( clientEnt->r.inuse )
        {
            if ( !clientEnt->client
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            2621,
                            0,
                            "%s",
                            "clientEnt->client") )
            {
                __debugbreak();
            }
            if ( clientEnt->client->sess.cs.team == teamNum )
                ent->r.clientMask[clientEnt->s.number >> 5] &= ~(1 << (clientEnt->s.number & 0x1F));
        }
        ++clientEnt;
    }
}

void __cdecl ScrCmd_IsLinkedTo(scr_entref_t entref)
{
    bool IsLinkedTo; // eax
    gentity_s *parent; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) != 1 || Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) != 19 )
        Scr_ParamError(0, "not an entity", SCRIPTINSTANCE_SERVER);
    if ( (ent->flags & FL_SUPPORTS_LINKTO) != 0 )
    {
        parent = Scr_GetEntity(0);
        if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) != 1 || Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) != 19 )
            Scr_ParamError(0, "not an entity", SCRIPTINSTANCE_SERVER);
        IsLinkedTo = G_EntIsLinkedTo(ent, parent);
        Scr_AddInt(IsLinkedTo, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl ScrCmd_LinkTo(scr_entref_t entref)
{
#ifdef KISAK_SP
    float anglesOffset[3] = {};
    gentity_s *ent = GetEntity(entref);
    if (Scr_GetType(0, SCRIPTINSTANCE_SERVER) != VAR_POINTER
        || Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) != VAR_ENTITY)
        Scr_ParamError(0, "not an entity", SCRIPTINSTANCE_SERVER);
    if (!(ent->flags & FL_SUPPORTS_LINKTO))
    {
        const char *typeName = G_GetEntityTypeName(ent);
        const char *classname = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        Scr_ObjectError(va("entity (classname: '%s', type: '%s') does not currently support linkTo",
            classname, typeName), SCRIPTINSTANCE_SERVER);
    }
    gentity_s *parent = Scr_GetEntity(0);
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    unsigned int tagName = 0;
    if (count >= 2)
    {
        tagName = Scr_GetConstLowercaseString(1, SCRIPTINSTANCE_SERVER);
        if (tagName == scr_const._)
            tagName = 0;
    }
    int linked;
    if (count > 2)
    {
        float originOffset[3];
        Scr_GetVector(2, originOffset, SCRIPTINSTANCE_SERVER);
        if (count == 4)
            Scr_GetVector(3, anglesOffset, SCRIPTINSTANCE_SERVER);
        linked = G_EntLinkToWithOffset(ent, parent, tagName, originOffset, anglesOffset);
    }
    else
        linked = G_EntLinkTo(ent, parent, tagName);
    if (linked)
        return;

    if (!SV_DObjExists(parent))
    {
        if (!parent->model)
            Scr_Error("failed to link entity since parent has no model", 0);
        const char *modelName = SL_ConvertToString(G_ModelName(parent->model), SCRIPTINSTANCE_SERVER);
        Scr_Error(va("failed to link entity since parent model '%s' is invalid", modelName), 0);
    }
    if (tagName && SV_DObjGetBoneIndex(parent, tagName) < 0)
    {
        SV_DObjDumpInfo(parent);
        const char *modelName = SL_ConvertToString(G_ModelName(parent->model), SCRIPTINSTANCE_SERVER);
        const char *tag = SL_ConvertToString(tagName, SCRIPTINSTANCE_SERVER);
        Scr_Error(va("failed to link entity since tag '%s' does not exist in parent model '%s'", tag, modelName), 0);
    }
    Scr_Error("failed to link entity", 0);
#else

    char *v1; // eax
    const char *v2; // eax
    unsigned int v3; // eax
    char *v4; // eax
    const char *v5; // eax
    unsigned int v6; // eax
    char *v7; // eax
    const char *v8; // eax
    char *v9; // [esp-8h] [ebp-30h]
    VariableUnion tagName; // [esp+0h] [ebp-28h]
    float originOffset[3]; // [esp+4h] [ebp-24h] BYREF
    float anglesOffset[3]; // [esp+10h] [ebp-18h] BYREF
    int numParam; // [esp+1Ch] [ebp-Ch]
    gentity_s *parent; // [esp+20h] [ebp-8h]
    gentity_s *ent; // [esp+24h] [ebp-4h]

    ent = GetEntity(entref);
    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) != 1 || Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) != 19 )
        Scr_ParamError(0, "not an entity", SCRIPTINSTANCE_SERVER);
    if ( (ent->flags & FL_SUPPORTS_LINKTO) == 0 )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity (classname: '%s') does not currently support linkTo", v1);
        Scr_ObjectError(v2, SCRIPTINSTANCE_SERVER);
    }
    parent = Scr_GetEntity(0);
    numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    tagName.intValue = 0;
    if ( numParam >= 2 )
    {
        tagName.intValue = Scr_GetConstLowercaseString(1u, SCRIPTINSTANCE_SERVER);
        if ( tagName.intValue == scr_const._ )
            tagName.intValue = 0;
    }
    if ( numParam > 2 )
    {
        Scr_GetVector(2u, originOffset, SCRIPTINSTANCE_SERVER);
        Scr_GetVector(3u, anglesOffset, SCRIPTINSTANCE_SERVER);
        if ( G_EntLinkToWithOffset(ent, parent, tagName.stringValue, originOffset, anglesOffset) )
            return;
    }
    else if ( G_EntLinkTo(ent, parent, tagName.stringValue) )
    {
        return;
    }
    if ( !SV_DObjExists(parent) )
    {
        if ( !parent->model )
            Scr_Error("failed to link entity since parent has no model", 0);
        v3 = G_ModelName(parent->model);
        v4 = SL_ConvertToString(v3, SCRIPTINSTANCE_SERVER);
        v5 = va("failed to link entity since parent model '%s' is invalid", v4);
        Scr_Error(v5, 0);
    }
    if ( !parent->model
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    2712,
                    0,
                    "%s",
                    "parent->model") )
    {
        __debugbreak();
    }
    if ( tagName.intValue )
    {
        if ( SV_DObjGetBoneIndex(parent, tagName.stringValue) < 0 )
        {
            SV_DObjDumpInfo(parent);
            v6 = G_ModelName(parent->model);
            v9 = SL_ConvertToString(v6, SCRIPTINSTANCE_SERVER);
            v7 = SL_ConvertToString(tagName.stringValue, SCRIPTINSTANCE_SERVER);
            v8 = va("failed to link entity since tag '%s' does not exist in parent model '%s'", v7, v9);
            Scr_Error(v8, 0);
        }
    }
    Scr_Error("failed to link entity", 0);

#endif
}

void __cdecl ScrCmd_PlayerLinkToDelta(scr_entref_t entref)
{
    float *linkAngles; // edx
    float v2; // [esp+0h] [ebp-90h]
    float v3; // [esp+4h] [ebp-8Ch]
    float v4; // [esp+8h] [ebp-88h]
    float v5; // [esp+Ch] [ebp-84h]
    float v7; // [esp+18h] [ebp-78h]
    float v8; // [esp+1Ch] [ebp-74h]
    float v9; // [esp+20h] [ebp-70h]
    float v10; // [esp+24h] [ebp-6Ch]
    float v11; // [esp+28h] [ebp-68h]
    float v12; // [esp+2Ch] [ebp-64h]
    float v13; // [esp+30h] [ebp-60h]
    float v14; // [esp+34h] [ebp-5Ch]
    VariableUnion tagName; // [esp+38h] [ebp-58h]
    float originOffset[3]; // [esp+3Ch] [ebp-54h] BYREF
    float anglesOffset[3]; // [esp+48h] [ebp-48h] BYREF
    int numParam; // [esp+54h] [ebp-3Ch]
    gentity_s *parent; // [esp+58h] [ebp-38h]
    float parentAxis[4][3]; // [esp+5Ch] [ebp-34h] BYREF
    gentity_s *ent; // [esp+8Ch] [ebp-4h]

    ent = GetEntity(entref);
    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) != 1 || Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) != 19 )
        Scr_ParamError(0, "not an entity", SCRIPTINSTANCE_SERVER);
    if ( !ent->client )
        Scr_ObjectError("not a player entity", SCRIPTINSTANCE_SERVER);

    iassert(ent->flags & FL_SUPPORTS_LINKTO);

    parent = Scr_GetEntity(0);
    numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    tagName.intValue = 0;
    if ( numParam > 1 )
    {
        if ( Scr_GetType(1u, SCRIPTINSTANCE_SERVER) )
        {
            tagName.intValue = Scr_GetConstLowercaseString(1u, SCRIPTINSTANCE_SERVER);
            if ( tagName.intValue == scr_const._ )
                tagName.intValue = 0;
        }
    }

    ent->client->linkAnglesFrac = (numParam > 2) ? Scr_GetFloat(2, SCRIPTINSTANCE_SERVER) : 0.0f;
    ent->client->linkAnglesLocked = 0;
    if ( numParam <= 3 )
        v13 = 180.0f;
    else
        v13 = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    if ( (float)(v13 - 180.0) < 0.0 )
        v14 = v13;
    else
        v14 = 180.0f;
    if ( (float)(0.0 - v13) < 0.0 )
        v5 = v14;
    else
        v5 = 0.0f;
    ent->client->linkAnglesMinClamp[1] = -v5;
    if ( numParam <= 4 )
        v11 = 180.0f;
    else
        v11 = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
    if ( (float)(v11 - 180.0) < 0.0 )
        v12 = v11;
    else
        v12 = 180.0f;
    if ( (float)(0.0 - v11) < 0.0 )
        v4 = v12;
    else
        v4 = 0.0f;
    ent->client->linkAnglesMaxClamp[1] = v4;
    if ( numParam <= 5 )
        v9 = 180.0f;
    else
        v9 = Scr_GetFloat(5u, SCRIPTINSTANCE_SERVER);
    if ( (float)(v9 - 180.0) < 0.0 )
        v10 = v9;
    else
        v10 = 180.0f;
    if ( (float)(0.0 - v9) < 0.0 )
        v3 = v10;
    else
        v3 = 0.0f;
    ent->client->linkAnglesMinClamp[0] = -v3;
    if ( numParam <= 6 )
        v7 = 180.0f;
    else
        v7 = Scr_GetFloat(6u, SCRIPTINSTANCE_SERVER);
    if ( (float)(v7 - 180.0) < 0.0 )
        v8 = v7;
    else
        v8 = 180.0f;
    if ( (float)(0.0 - v7) < 0.0 )
        v2 = v8;
    else
        v2 = 0.0f;
    ent->client->linkAnglesMaxClamp[0] = v2;
    G_UpdateViewAngleClamp(ent->client, parent->r.currentAngles);
    if ( numParam > 7 && Scr_GetInt(7u, SCRIPTINSTANCE_SERVER) )
        ent->client->ps.linkFlags |= 2u;
    else
        ent->client->ps.linkFlags &= ~2u;
#ifdef KISAK_SP
    // Retail SP rotates the eye offset with the attachment in both camera link modes.
    ent->client->ps.linkFlags |= 1u;
    // Delta links allow the player to look around the attachment within the view clamps.
    ent->client->ps.pm_flags &= ~0x4000000u;
#endif
    ent->client->prevLinkAnglesSet = 0;
    parent->r.svFlags &= ~1u;
    if ( numParam > 8 )
    {
        Scr_GetVector(8u, originOffset, SCRIPTINSTANCE_SERVER);
        Scr_GetVector(9u, anglesOffset, SCRIPTINSTANCE_SERVER);
        if ( !G_EntLinkToWithOffset(ent, parent, tagName.stringValue, originOffset, anglesOffset) )
        {
            Scr_Error("failed to link entity", 0);
            return;
        }
    }
    else if ( !G_EntLinkTo(ent, parent, tagName.stringValue) )
    {
        Scr_Error("failed to link entity", 0);
        return;
    }
    if ( (ent->client->ps.linkFlags & 2) != 0 )
    {
        G_CalcTagParentAxis(ent, parentAxis);
        AxisToAngles(parentAxis, ent->client->ps.linkAngles);
    }
    else
    {
        Vec3Clear(ent->client->ps.linkAngles);
    }
}

void __cdecl ScrCmd_Unlink(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->client && (ent->client->ps.eFlags & 0x4000) != 0 )
        VEH_UnlinkPlayer(ent, 0, (char*)"ScrCmd_Unlink");
    else
        G_EntUnlink(ent);
#ifdef KISAK_SP
    if (ent->client)
    {
        ent->client->linkAnglesFrac = 0.0f;
        ent->client->linkAnglesLocked = false;
        ent->client->ps.pm_flags &= ~0x04000000u;
    }
#endif
}

void __cdecl ScrCmd_EnableLinkTo(scr_entref_t entref)
{
#ifdef KISAK_SP
    gentity_s *ent = GetEntity(entref);
    if (ent->flags & FL_SUPPORTS_LINKTO)
        Scr_ObjectError("entity already has linkTo enabled", SCRIPTINSTANCE_SERVER);
    if (ent->s.eType != ET_GENERAL || ent->physicsObject)
    {
        const char *typeName = G_GetEntityTypeName(ent);
        const char *classname = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        Scr_ObjectError(va("entity (classname: '%s', type: '%s') does not currently support enableLinkTo",
            classname, typeName), SCRIPTINSTANCE_SERVER);
    }
    ent->flags |= FL_SUPPORTS_LINKTO;
#else

    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( (ent->flags & FL_SUPPORTS_LINKTO) != 0 )
        Scr_ObjectError("entity already has linkTo enabled", SCRIPTINSTANCE_SERVER);
    if ( ent->s.eType || ent->physicsObject )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity (classname: '%s') does not currently support enableLinkTo", v1);
        Scr_ObjectError(v2, SCRIPTINSTANCE_SERVER);
    }
    if ( ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 2832, 0, "%s", "!ent->client") )
    {
        __debugbreak();
    }
    ent->flags |= FL_SUPPORTS_LINKTO;

#endif
}

void __cdecl ScrCmd_GetOrigin(scr_entref_t entref)
{
    float origin[3]; // [esp+4h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+10h] [ebp-4h]

    ent = GetEntity(entref);
#ifdef KISAK_SP
    if ( ent->r.bmodel )
        G_EntityCentroid(ent, origin);
    else
#endif
    {
        origin[0] = ent->r.currentOrigin[0];
        origin[1] = ent->r.currentOrigin[1];
        origin[2] = ent->r.currentOrigin[2];
    }
    Scr_AddVector(origin, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetAngles(scr_entref_t entref)
{
    gentity_s *pSelf; // [esp+8h] [ebp-4h]

    pSelf = GetEntity(entref);
    Scr_AddVector(pSelf->r.currentAngles, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetMins(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    Scr_AddVector(ent->r.mins, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetMaxs(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    Scr_AddVector(ent->r.maxs, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetAbsMins(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    Scr_AddVector(ent->r.absmin, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetAbsMaxs(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    Scr_AddVector(ent->r.absmax, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetPointInBounds(scr_entref_t entref)
{
    float ratioX; // [esp+10h] [ebp-1Ch]
    float ratioZ; // [esp+14h] [ebp-18h]
    float result[3]; // [esp+18h] [ebp-14h] BYREF
    gentity_s *ent; // [esp+24h] [ebp-8h]
    float ratioY; // [esp+28h] [ebp-4h]

    ent = GetEntity(entref);
    ratioX = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    ratioY = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    ratioZ = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    G_GetEntityBoundsPoint(ent, ratioX, ratioY, ratioZ, result);
    Scr_AddVector(result, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetEye(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+0h] [ebp-10h]
    float eye[3]; // [esp+4h] [ebp-Ch] BYREF

    ent = GetEntity(entref);
    if ( !ent->sentient )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("getEye must be called on an AI or player, not on a '%s'", v1);
        Scr_Error(v2, 0);
    }
    Sentient_GetEyePosition(ent->sentient, eye);
    Scr_AddVector(eye, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_GetEyeApprox(scr_entref_t entref)
{
    gentity_s *ent; // [esp+4h] [ebp-10h]
    float eye[3]; // [esp+8h] [ebp-Ch] BYREF

    ent = GetEntity(entref);
    eye[0] = ent->r.currentOrigin[0];
    eye[1] = ent->r.currentOrigin[1];
    eye[2] = ent->r.currentOrigin[2] + 40.0;
    Scr_AddVector(eye, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_UseBy(scr_entref_t entref)
{
    gentity_s *pOther; // [esp+0h] [ebp-Ch]
    void (__cdecl *use)(gentity_s *, gentity_s *, gentity_s *); // [esp+4h] [ebp-8h]
    gentity_s *pEnt; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    pOther = Scr_GetEntity(0);
    Scr_AddEntity(pOther, SCRIPTINSTANCE_SERVER);
    Scr_Notify(pEnt, scr_const.trigger, 1u);
    use = entityHandlers[pEnt->handler].use;
    if ( use )
        use(pEnt, pOther, pOther);
}

void __cdecl ScrCmd_IsTouching(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    char *v3; // eax
    const char *v4; // eax
    char *v5; // eax
    const char *v6; // eax
    const gentity_s *pOther; // [esp+34h] [ebp-38h]
    float vMins[3]; // [esp+38h] [ebp-34h] BYREF
    gentity_s *pEnt; // [esp+48h] [ebp-24h]
    int bTouching; // [esp+4Ch] [ebp-20h]
    gentity_s *pTemp; // [esp+50h] [ebp-1Ch]
    float vMaxs[3]; // [esp+54h] [ebp-18h] BYREF
    float extraBoundary[3]; // [esp+60h] [ebp-Ch] BYREF

    PROF_SCOPED("ScrCmd_IsTouching");

    bTouching = 0;
    pEnt = GetEntity(entref);
    if ( pEnt->r.bmodel || (pEnt->r.svFlags & 0x60) != 0 )
    {
        pTemp = pEnt;
        pEnt = Scr_GetEntity(0);
        if ( pEnt->r.bmodel || (pEnt->r.svFlags & 0x60) != 0 )
            Scr_Error("istouching cannot be called on 2 brush/cylinder entities", 0);
        pOther = pTemp;
    }
    else
    {
        pOther = Scr_GetEntity(0);
    }
    if ( !pEnt
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 3003, 0, "%s", "pEnt") )
    {
        __debugbreak();
    }
    if ( pEnt->r.maxs[0] < pEnt->r.mins[0] )
    {
        v1 = SL_ConvertToString(pEnt->classname, SCRIPTINSTANCE_SERVER);
        v2 = va(
                     "entnum: %d, origin: %g %g %g, classname: %s",
                     pEnt->s.number,
                     pEnt->r.currentOrigin[0],
                     pEnt->r.currentOrigin[1],
                     pEnt->r.currentOrigin[2],
                     v1);
        if ( !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        3004,
                        0,
                        "%s\n\t%s",
                        "pEnt->r.maxs[0] >= pEnt->r.mins[0]",
                        v2) )
            __debugbreak();
    }
    if ( pEnt->r.maxs[1] < pEnt->r.mins[1] )
    {
        v3 = SL_ConvertToString(pEnt->classname, SCRIPTINSTANCE_SERVER);
        v4 = va(
                     "entnum: %d, origin: %g %g %g, classname: %s",
                     pEnt->s.number,
                     pEnt->r.currentOrigin[0],
                     pEnt->r.currentOrigin[1],
                     pEnt->r.currentOrigin[2],
                     v3);
        if ( !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        3005,
                        0,
                        "%s\n\t%s",
                        "pEnt->r.maxs[1] >= pEnt->r.mins[1]",
                        v4) )
            __debugbreak();
    }
    if ( pEnt->r.maxs[2] < pEnt->r.mins[2] )
    {
        v5 = SL_ConvertToString(pEnt->classname, SCRIPTINSTANCE_SERVER);
        v6 = va(
                     "entnum: %d, origin: %g %g %g, classname: %s",
                     pEnt->s.number,
                     pEnt->r.currentOrigin[0],
                     pEnt->r.currentOrigin[1],
                     pEnt->r.currentOrigin[2],
                     v5);
        if ( !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        3006,
                        0,
                        "%s\n\t%s",
                        "pEnt->r.maxs[2] >= pEnt->r.mins[2]",
                        v6) )
            __debugbreak();
    }
    vMins[0] = pEnt->r.currentOrigin[0] + pEnt->r.mins[0];
    vMins[1] = pEnt->r.currentOrigin[1] + pEnt->r.mins[1];
    vMins[2] = pEnt->r.currentOrigin[2] + pEnt->r.mins[2];
    vMaxs[0] = pEnt->r.currentOrigin[0] + pEnt->r.maxs[0];
    vMaxs[1] = pEnt->r.currentOrigin[1] + pEnt->r.maxs[1];
    vMaxs[2] = pEnt->r.currentOrigin[2] + pEnt->r.maxs[2];
    memset(extraBoundary, 0, sizeof(extraBoundary));
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
        Scr_GetVector(1u, extraBoundary, SCRIPTINSTANCE_SERVER);
    vMins[0] = vMins[0] - extraBoundary[0];
    vMins[1] = vMins[1] - extraBoundary[1];
    vMins[2] = vMins[2] - extraBoundary[2];
    vMaxs[0] = vMaxs[0] + extraBoundary[0];
    vMaxs[1] = vMaxs[1] + extraBoundary[1];
    vMaxs[2] = vMaxs[2] + extraBoundary[2];
    ExpandBoundsToWidth(vMins, vMaxs);
    bTouching = SV_EntityContact(vMins, vMaxs, pOther);
    Scr_AddInt(bTouching, SCRIPTINSTANCE_SERVER);
}

// LWSS ADD
void ScrCmd_IsTouchingVolume(scr_entref_t entref)
{
    iassert(0); // KISAKTODO :)
}
// LWSS END

void __cdecl ScrCmd_IsTouchingSwept(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    char *v3; // eax
    const char *v4; // eax
    char *v5; // eax
    const char *v6; // eax
    float v7; // [esp+1Ch] [ebp-54h]
    float v8; // [esp+20h] [ebp-50h]
    float endZ; // [esp+3Ch] [ebp-34h]
    float startZ; // [esp+40h] [ebp-30h]
    const gentity_s *pOther; // [esp+44h] [ebp-2Ch]
    float vMins[3]; // [esp+48h] [ebp-28h] BYREF
    gentity_s *pEnt; // [esp+58h] [ebp-18h]
    int bTouching; // [esp+5Ch] [ebp-14h]
    gentity_s *pTemp; // [esp+60h] [ebp-10h]
    float vMaxs[3]; // [esp+64h] [ebp-Ch] BYREF

    PROF_SCOPED("ScrCmd_IsTouchingSwept");

    bTouching = 0;
    pEnt = GetEntity(entref);
    if ( pEnt->r.bmodel || (pEnt->r.svFlags & 0x60) != 0 )
    {
        pTemp = pEnt;
        pEnt = Scr_GetEntity(0);
        if ( pEnt->r.bmodel || (pEnt->r.svFlags & 0x60) != 0 )
            Scr_Error("istouchingswept cannot be called on 2 brush/cylinder entities", 0);
        pOther = pTemp;
    }
    else
    {
        pOther = Scr_GetEntity(0);
    }
    if ( !pEnt
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 3049, 0, "%s", "pEnt") )
    {
        __debugbreak();
    }
    if ( pEnt->r.maxs[0] < pEnt->r.mins[0] )
    {
        v1 = SL_ConvertToString(pEnt->classname, SCRIPTINSTANCE_SERVER);
        v2 = va(
                     "entnum: %d, origin: %g %g %g, classname: %s",
                     pEnt->s.number,
                     pEnt->r.currentOrigin[0],
                     pEnt->r.currentOrigin[1],
                     pEnt->r.currentOrigin[2],
                     v1);
        if ( !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        3050,
                        0,
                        "%s\n\t%s",
                        "pEnt->r.maxs[0] >= pEnt->r.mins[0]",
                        v2) )
            __debugbreak();
    }
    if ( pEnt->r.maxs[1] < pEnt->r.mins[1] )
    {
        v3 = SL_ConvertToString(pEnt->classname, SCRIPTINSTANCE_SERVER);
        v4 = va(
                     "entnum: %d, origin: %g %g %g, classname: %s",
                     pEnt->s.number,
                     pEnt->r.currentOrigin[0],
                     pEnt->r.currentOrigin[1],
                     pEnt->r.currentOrigin[2],
                     v3);
        if ( !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        3051,
                        0,
                        "%s\n\t%s",
                        "pEnt->r.maxs[1] >= pEnt->r.mins[1]",
                        v4) )
            __debugbreak();
    }
    if ( pEnt->r.maxs[2] < pEnt->r.mins[2] )
    {
        v5 = SL_ConvertToString(pEnt->classname, SCRIPTINSTANCE_SERVER);
        v6 = va(
                     "entnum: %d, origin: %g %g %g, classname: %s",
                     pEnt->s.number,
                     pEnt->r.currentOrigin[0],
                     pEnt->r.currentOrigin[1],
                     pEnt->r.currentOrigin[2],
                     v5);
        if ( !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        3052,
                        0,
                        "%s\n\t%s",
                        "pEnt->r.maxs[2] >= pEnt->r.mins[2]",
                        v6) )
            __debugbreak();
    }
    vMins[0] = pEnt->r.currentOrigin[0] + pEnt->r.mins[0];
    vMins[1] = pEnt->r.currentOrigin[1] + pEnt->r.mins[1];
    vMins[2] = pEnt->r.currentOrigin[2] + pEnt->r.mins[2];
    vMaxs[0] = pEnt->r.currentOrigin[0] + pEnt->r.maxs[0];
    vMaxs[1] = pEnt->r.currentOrigin[1] + pEnt->r.maxs[1];
    vMaxs[2] = pEnt->r.currentOrigin[2] + pEnt->r.maxs[2];
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
    {
        startZ = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
        if ( (float)(vMins[2] - startZ) < 0.0 )
            v8 = vMins[2];
        else
            v8 = startZ;
        vMins[2] = v8;
    }
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 2 )
    {
        endZ = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
        if ( (float)(endZ - vMaxs[2]) < 0.0 )
            v7 = vMaxs[2];
        else
            v7 = endZ;
        vMaxs[2] = v7;
    }
    ExpandBoundsToWidth(vMins, vMaxs);
    bTouching = SV_EntityContact(vMins, vMaxs, pOther);
    Scr_AddInt(bTouching, SCRIPTINSTANCE_SERVER);
}

void ScrCmd_SoundExists()
{
    snd_alias_list_t *Alias; // eax
    char *soundName; // [esp+0h] [ebp-4h]

    soundName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    Alias = SND_FindAlias(soundName);
    Scr_AddBool(Alias != 0, SCRIPTINSTANCE_SERVER);
}

void __cdecl ScrCmd_PlaySound(scr_entref_t entref)
{
    int NumParam; // eax
    const char *v2; // eax
    char *String; // eax
    gentity_s *Entity; // eax
    unsigned int AliasId; // [esp-Ch] [ebp-Ch]

#ifdef KISAK_SP
    // Retail SP 0x007F45D0 accepts zero or more arguments. Argument 0 is the
    // alias, argument 1 is an optional notify string, and later arguments are
    // intentionally ignored (animscripts/face.gsc passes a third boolean).
    unsigned int notifyString = 0;
    AliasId = 0;
    const int numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( numParam > 0 )
        AliasId = SND_HashName(Scr_GetString(0, SCRIPTINSTANCE_SERVER));
    if ( numParam > 1 )
        notifyString = Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    Entity = GetEntity(entref);
    if ( Entity && AliasId )
        G_PlaySoundAlias(Entity, AliasId, notifyString, 0);
#else
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
    {
        NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
        v2 = va("playsound has %d parameters.    There should be exactly one.", NumParam);
        Scr_Error(v2, 0);
    }
    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    AliasId = SND_FindAliasId(String);
    Entity = GetEntity(entref);
    G_PlaySoundAlias(Entity, AliasId, 0, 0);
#endif
}

#ifdef KISAK_SP
// Retail SP builtin method "stopsound" -- methods_3 idx 56, handler 0x00807B30.
// Transcribed from that decompile: GetEntity, SND_FindAliasId(Scr_GetString(0)),
// then (only when the alias resolves) a G_TempEntity(ent->r.currentOrigin,
// EV_STOP_SOUND_ALIAS) carrying the alias in s.loopSoundId and the source entity
// in s.otherEntityNum. Field writes are the same three G_PlaySoundAlias
// (g_utils_mp.cpp:2015) performs, at the same SP gentity_s offsets
// (r.currentOrigin +0x11C, s.loopSoundId +0x7C, s.otherEntityNum +0xC4).
// NOTE: unlike G_PlaySoundAlias, the SP body does NOT set tmp->r.svFlags |= 8 --
// verified absent from 0x00807B30's decompile, not an omission here.
// The client half already exists: cg_event.cpp:503 handles EV_STOP_SOUND_ALIAS.
void __cdecl ScrCmd_StopSound(scr_entref_t entref)
{
    gentity_s *ent; // [esp+0h] [ebp-8h]
    gentity_s *tmp; // [esp+4h] [ebp-4h]
    unsigned int AliasId;

    ent = GetEntity(entref);
    AliasId = SND_FindAliasId(Scr_GetString(0, SCRIPTINSTANCE_SERVER));
    if ( AliasId )
    {
        tmp = G_TempEntity(ent->r.currentOrigin, EV_STOP_SOUND_ALIAS);
        tmp->s.loopSoundId = AliasId;
        AssignToSmallerType<short>(&tmp->s.otherEntityNum, ent->s.number);
    }
}
#endif

void __cdecl ScrCmd_PlaySoundOnTag(scr_entref_t entref)
{
    char *String; // eax
    unsigned int v2; // eax
    char *v3; // eax
    const char *v4; // eax
    char *v5; // eax
    char *v6; // [esp-8h] [ebp-18h]
    unsigned int tag; // [esp+0h] [ebp-10h]
    int tagIndex; // [esp+4h] [ebp-Ch]
    int sound; // [esp+8h] [ebp-8h]
    gentity_s *ent; // [esp+Ch] [ebp-4h]

    ent = GetEntity(entref);
    sound = 0;
    tagIndex = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        sound = SND_FindAliasId(String);
    }
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 2 )
    {
        tag = Scr_GetConstLowercaseString(1u, SCRIPTINSTANCE_SERVER);
        if ( tag == scr_const.tag_origin || SV_DObjGetBoneIndex(ent, tag) >= 0 )
        {
            v5 = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
            tagIndex = G_TagIndex(v5);
        }
        else
        {
            SV_DObjDumpInfo(ent);
            v2 = G_ModelName(ent->model);
            v6 = SL_ConvertToString(v2, SCRIPTINSTANCE_SERVER);
            v3 = SL_ConvertToString(tag, SCRIPTINSTANCE_SERVER);
            v4 = va("tag '%s' does not exist on entity with model '%s'", v3, v6);
            Scr_ParamError(1u, v4, SCRIPTINSTANCE_SERVER);
        }
    }
    if ( ent )
    {
        if ( sound )
            G_PlaySoundAlias(ent, sound, 0, tagIndex);
    }
}

void __cdecl ScrCmd_PlaySoundToTeam(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    char *String; // eax
    gentity_s *Entity; // eax
    unsigned int AliasId; // [esp-Ch] [ebp-38h]
    gentity_s *tempEnt; // [esp+10h] [ebp-1Ch]
    unsigned __int16 team; // [esp+18h] [ebp-14h]
    gentity_s *ignoreClientEnt; // [esp+1Ch] [ebp-10h]
    int entIndex; // [esp+20h] [ebp-Ch]
    gentity_s *clientEnt; // [esp+28h] [ebp-4h]

    PROF_SCOPED("ScrCmd_PlaySoundToTeam");

    team = (unsigned __int16)Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    if ( team != scr_const.allies && team != scr_const.axis )
    {
        v1 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v2 = va("Illegal team string '%s'. Must be allies, or axis.", v1);
        Scr_Error(v2, 0);
    }
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 3 )
    {
        ignoreClientEnt = Scr_GetEntity(2u);
        if ( !ignoreClientEnt->client )
        {
            v3 = va("entity %i is not a player", ignoreClientEnt->s.number);
            Scr_ObjectError(v3, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        ignoreClientEnt = 0;
    }
    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    AliasId = SND_FindAliasId(String);
    Entity = GetEntity(entref);
    tempEnt = G_PlaySoundAlias(Entity, AliasId, 0, 0);
    if ( !tempEnt )
    {
        return;
    }
    tempEnt->r.clientMask[0] = -1;
    clientEnt = g_entities;
    for ( entIndex = 0; entIndex < com_maxclients->current.integer; ++entIndex )
    {
        if ( clientEnt->r.inuse && clientEnt != ignoreClientEnt )
        {
            if ( !clientEnt->client
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            3203,
                            0,
                            "%s",
                            "clientEnt->client") )
            {
                __debugbreak();
            }
            tempEnt->r.clientMask[clientEnt->s.number >> 5] &= ~(1 << (clientEnt->s.number & 0x1F));
        }
        ++clientEnt;
    }
}

void __cdecl ScrCmd_PlayBattleChatterToTeam(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    gentity_s *tempEnt; // [esp+0h] [ebp-18h]
    int teamNum; // [esp+4h] [ebp-14h]
    unsigned __int16 team; // [esp+8h] [ebp-10h]
    gentity_s *ignoreClientEnt; // [esp+Ch] [ebp-Ch]
    int entIndex; // [esp+10h] [ebp-8h]
    gentity_s *clientEnt; // [esp+14h] [ebp-4h]

    tempEnt = StartScriptPlayBattleChatterOnEnt(entref);
    team = (unsigned __int16)Scr_GetConstString(2u, SCRIPTINSTANCE_SERVER);
    if ( team != scr_const.allies && team != scr_const.axis )
    {
        v1 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v2 = va("Illegal team string '%s'. Must be allies, or axis.", v1);
        Scr_Error(v2, 0);
    }
    if ( team == scr_const.allies )
        teamNum = 2;
    else
        teamNum = 1;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 4 )
    {
        ignoreClientEnt = Scr_GetEntity(3u);
        if ( !ignoreClientEnt->client )
        {
            v3 = va("entity %i is not a player", ignoreClientEnt->s.number);
            Scr_ObjectError(v3, SCRIPTINSTANCE_SERVER);
        }
    }
    tempEnt->r.clientMask[0] = -1;
    clientEnt = g_entities;
    for ( entIndex = 0; entIndex < com_maxclients->current.integer; ++entIndex )
    {
        if ( clientEnt->r.inuse )
        {
            if ( !clientEnt->client
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            3269,
                            0,
                            "%s",
                            "clientEnt->client") )
            {
                __debugbreak();
            }
            if ( clientEnt->client->sess.cs.team == teamNum )
                tempEnt->r.clientMask[clientEnt->s.number >> 5] &= ~(1 << (clientEnt->s.number & 0x1F));
        }
        ++clientEnt;
    }
}

gentity_s *__cdecl StartScriptPlayBattleChatterOnEnt(scr_entref_t entref)
{
    char *String; // eax
    char *v2; // eax
    gentity_s *tmp; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    tmp = G_TempEntity(ent->r.currentOrigin, EV_SOUND_BATTLECHAT_ALIAS);
    tmp->r.svFlags |= 8u;
    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    tmp->s.loopSoundId = SND_FindAliasId(String);
    v2 = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    tmp->s.un3.item = SND_FindAliasId(v2);
    return tmp;
}

void __cdecl ScrCmd_PlaySoundToPlayer(scr_entref_t entref)
{
    const char *v1; // eax
    char *String; // eax
    gentity_s *Entity; // eax
    unsigned int AliasId; // [esp-Ch] [ebp-14h]
    gentity_s *tempEnt; // [esp+0h] [ebp-8h]
    gentity_s *clientEnt; // [esp+4h] [ebp-4h]

    clientEnt = Scr_GetEntity(1u);
    if ( !clientEnt->client )
    {
        v1 = va("entity %i is not a player", clientEnt->s.number);
        Scr_ObjectError(v1, SCRIPTINSTANCE_SERVER);
    }
    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    AliasId = SND_FindAliasId(String);
    Entity = GetEntity(entref);
    tempEnt = G_PlaySoundAlias(Entity, AliasId, 0, 0);
    if ( tempEnt )
    {
        tempEnt->r.clientMask[0] = -1;
        tempEnt->r.clientMask[clientEnt->s.number >> 5] &= ~(1 << (clientEnt->s.number & 0x1F));
    }
}

void  Scr_PlaySoundAtPosition()
{
    int NumParam; // eax
    char *v2; // eax
    char *String; // eax
    unsigned int AliasId; // eax
    float origin[3]; // [esp+0h] [ebp-Ch] BYREF

    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2)
    {
        NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
        v2 = va("playsoundatposition has %d parameters.  There should be two.", NumParam);
        Scr_Error(v2, 0);
    }
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    AliasId = SND_FindAliasId(String);
    G_PlaySoundAliasAtPoint(origin, AliasId);
}

#ifdef KISAK_SP
static short Scr_LoopSoundFadeMilliseconds_SP(float fade, float scale)
{
    const volatile float milliseconds = fade * scale;
    const int value = milliseconds >= -2147483648.0f && milliseconds < 2147483648.0f
        ? static_cast<int>(milliseconds) : INT_MIN;
    const unsigned short bits = static_cast<unsigned short>(value);
    short result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

void __cdecl ScrCmd_PlayLoopSound(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    const char *sound = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    ent->r.svFlags &= ~1u;
    ent->s.loopSoundId = SND_FindAliasId(sound);
    float fade = 0.0f;
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2)
    {
        const double value = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
        fade = static_cast<float>(value);
        if (value < 0.0 || fade > 32.0f)
            Scr_ParamError(1, va("playloopsound: invalid fade value %f. it must be between 0 and 32 seconds.", fade), SCRIPTINSTANCE_SERVER);
    }
    ent->s.loopSoundFade = Scr_LoopSoundFadeMilliseconds_SP(fade, 1000.0f);
}
#else
void __cdecl ScrCmd_PlayLoopSound(scr_entref_t entref)
{
    char *String; // eax
    const char *v2; // eax
    float fadeTime; // [esp+Ch] [ebp-8h]
    float fadeTimea; // [esp+Ch] [ebp-8h]
    gentity_s *pEnt; // [esp+10h] [ebp-4h]

    pEnt = GetEntity(entref);
    pEnt->r.broadcastTime = -1;
    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    pEnt->s.loopSoundId = SND_FindAliasId(String);
    fadeTime = 0.0f;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        fadeTimea = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
        if ( fadeTimea < 0.0 || fadeTimea > 32.0 )
        {
            v2 = va("playloopsound: invalid fade value %f. it must be between 0 and 32 seconds.", fadeTimea);
            Scr_ParamError(1u, v2, SCRIPTINSTANCE_SERVER);
        }
        fadeTime = fadeTimea * 1000.0;
    }
    pEnt->s.loopSoundFade = (int)fadeTime;
}
#endif

#ifdef KISAK_SP
void __cdecl ScrCmd_StopLoopSound(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    float fade = 0.0f;
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
    {
        const double value = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
        fade = static_cast<float>(value);
        if (value < 0.0 || fade > 32.0f)
            Scr_ParamError(0, va("stoploopsound: invalid fade value %f. it must be between 0 and 32 seconds.", fade), SCRIPTINSTANCE_SERVER);
    }
    else
    {
        ent->s.loopSoundId = 0;
    }
    ent->s.loopSoundFade = Scr_LoopSoundFadeMilliseconds_SP(fade, -1000.0f);
}
#else
void __cdecl ScrCmd_StopLoopSound(scr_entref_t entref)
{
    const char *v1;
    float fadeTime;
    float fadeTimea;
    gentity_s *pEnt;

    pEnt = GetEntity(entref);
    pEnt->r.broadcastTime = level.time + 300;
    fadeTime = 0.0f;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        fadeTimea = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
        if ( fadeTimea < 0.0 || fadeTimea > 32.0 )
        {
            v1 = va("stoploopsound: invalid fade value %f. it must be between 0 and 32 seconds.", fadeTimea);
            Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
        }
        fadeTime = fadeTimea * -1000.0;
    }
    else
    {
        pEnt->s.loopSoundId = 0;
    }
    pEnt->s.loopSoundFade = (int)fadeTime;
}
#endif

void __cdecl ScrCmd_Delete(scr_entref_t entref)
{
    gentity_s *pEnt; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    if ( pEnt->client )
        Scr_Error("Cannot delete a client entity", 0);
    if ( level.currentEntityThink == pEnt->s.number )
        Scr_Error("Cannot delete entity during its think", 0);
    Scr_Notify(pEnt, scr_const.death, 0);
    G_FreeEntity(pEnt);
}

void __cdecl SetModelInternal(gentity_s *ent, char *modelName)
{
    DObj *obj; // [esp+0h] [ebp-4h]
    int savedregs; // [esp+4h] [ebp+0h] BYREF

    G_SetModel(ent, modelName);
    G_DObjUpdate(ent);
    if ( ent->s.eType == 6 )
    {
        obj = Com_GetServerDObj(ent->s.number);
        if ( obj )
        {
            ent->r.contents |= DObjGetContents(obj);
            DObjCalcBounds(obj, ent->r.mins, ent->r.maxs);
        }
    }
    SV_LinkEntity(ent);
}

void __cdecl ScrCmd_SetModel(scr_entref_t entref)
{
    char *modelName; // [esp+0h] [ebp-8h]
    gentity_s *pEnt; // [esp+4h] [ebp-4h]

    pEnt = GetEntity(entref);
    modelName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    SetModelInternal(pEnt, modelName);
}

void __cdecl ScrCmd_SetEnemyModel(scr_entref_t entref)
{
    char *modelName; // [esp+0h] [ebp-Ch]
    gentity_s *pEnt; // [esp+4h] [ebp-8h]
    int modelIndex; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    modelName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( *modelName )
    {
        modelIndex = G_ModelIndex(modelName);
        if ( modelIndex != (unsigned __int16)modelIndex
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        3558,
                        0,
                        "%s",
                        "modelIndex == (modelNameIndex_t) modelIndex") )
        {
            __debugbreak();
        }
        pEnt->s.enemyModel = modelIndex;
    }
    else
    {
        pEnt->s.enemyModel = 0;
    }
}

void __cdecl ScrCmd_GetNormalHealth(scr_entref_t entref)
{
#ifdef KISAK_SP
    gentity_s *ent = GetEntity(entref);
    if (ent->client)
    {
        Scr_AddFloat(ent->health ? (float)ent->health / (float)ent->client->sess.maxHealth : 0.0f,
            SCRIPTINSTANCE_SERVER);
    }
    else if (ent->maxHealth)
    {
        Scr_AddFloat((float)ent->health / (float)ent->maxHealth, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddFloat(0.0f, SCRIPTINSTANCE_SERVER);
        Com_PrintWarning(24, "WARNING: GetNormalHealth called on entity with 0 maxHealth.\n");
    }
#else
    gentity_s *pEnt; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    if ( pEnt->client )
    {
        if ( pEnt->health )
            Scr_AddFloat((float)pEnt->health / (float)pEnt->client->sess.maxHealth, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddFloat(0.0, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddFloat((float)pEnt->health, SCRIPTINSTANCE_SERVER);
    }
#endif
}

void __cdecl ScrCmd_SetNormalHealth(scr_entref_t entref)
{
#ifdef KISAK_SP
    gentity_s *ent = GetEntity(entref);
    float normalHealth = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if (normalHealth > 1.0f)
        normalHealth = 1.0f;
    if (normalHealth <= 0.0f)
    {
        const char *name = ent->targetname ? SL_ConvertToString(ent->targetname, SCRIPTINSTANCE_SERVER) : "<not set>";
        Scr_Error(va("setNormalHealth must be greater than 0 (tried to set %g on ent %i, name %s)\n",
            normalHealth, ent->s.number, name), 0);
    }
    int maximum;
    if (ent->client)
    {
        maximum = ent->client->sess.maxHealth;
    }
    else
    {
        if (ent->maxHealth <= 0)
        {
            const char *name = ent->targetname ? SL_ConvertToString(ent->targetname, SCRIPTINSTANCE_SERVER) : "<not set>";
            Scr_Error(va("entity's max health must be greater than 0 to call setNormalHealth (ent %i, name %s)\n",
                ent->s.number, name), 0);
            return;
        }
        if (ent->health <= 0)
        {
            const char *name = ent->targetname ? SL_ConvertToString(ent->targetname, SCRIPTINSTANCE_SERVER) : "<not set>";
            Com_DPrintf(24, "^2Cannot setNormalHealth on dead entities (health %i, max %i, ent %i, name %s)\n",
                ent->health, ent->maxHealth, ent->s.number, name);
        }
        maximum = ent->maxHealth;
    }
    const float health = (float)maximum * normalHealth;
    const double rounded = std::nearbyint((double)health + 9.313225746154785e-10);
    const int newHealth = rounded >= -2147483648.0 && rounded < 2147483648.0 ? (int)rounded : INT_MIN;
    if (ent->client)
        SV_GameSendServerCommand(ent - g_entities, SV_CMD_CAN_IGNORE, va("%c \"%i\"", 74, 0));
    ent->health = newHealth < 1 ? 1 : newHealth;
#else
    const char *v1; // eax
    int newHealth; // [esp+10h] [ebp-Ch]
    float normalHealth; // [esp+14h] [ebp-8h]
    gentity_s *ent; // [esp+18h] [ebp-4h]

    ent = GetEntity(entref);
    normalHealth = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( normalHealth > 1.0 )
        normalHealth = 1.0f;
    if ( ent->client )
    {
        newHealth = (int)((float)((float)ent->client->sess.maxHealth * normalHealth) + 9.313225746154785e-10);
        v1 = va("%c \"%i\"", 74, 0);
        SV_GameSendServerCommand(ent - g_entities, SV_CMD_CAN_IGNORE, v1);
    }
    else if ( ent->maxHealth )
    {
        newHealth = (int)(float)((float)ent->maxHealth * normalHealth);
    }
    else
    {
        newHealth = (int)normalHealth;
    }
    if ( newHealth > 0 )
        ent->health = newHealth;
    else
        Com_PrintError(24, "ERROR: Cannot setnormalhealth to 0 or below.\n");
#endif
}

#ifdef KISAK_SP
// Local adapter for the reviewed SP-only piece path at 0x0062F780.
static float GScr_DamageDestructiblePiece_SP(gentity_s *self, const float *dir,
    const float *point, float damage, int mod, int index)
{
    if (!self->destructible)
        return 0.0f;
    const DestructibleDef *def = self->destructible->ddef;
    if (index >= def->numPieces)
        return damage;
    if (def->clientOnly)
        return 0.0f;
    float hitdir[3];
    Vec3Copy(dir, hitdir);
    Vec3NormalizeFast(hitdir);
    const DestructiblePiece &piece = def->pieces[index];
    damage *= piece.bulletDamageScale;
    const float entityDamage = piece.entityDamageTransfer > 0.0f
        ? damage * piece.entityDamageTransfer : 0.0f;
    if (DamagePiece(self, (unsigned char)index, (int)damage, point, hitdir, mod, true, -1, NULL, 0))
    {
        DestructibleBulletDamageEvent(self, point, hitdir, mod);
        G_DObjUpdate(self);
    }
    return entityDamage;
}
#endif

void __cdecl ScrCmd_DoDamage(scr_entref_t entref)
{
#ifdef KISAK_SP
    // Retail 0x007F5080: SP has no separate inflictor/headshot/dflags/weapon
    // slots. Zombie melee supplies (damage, origin, attacker, 0, "MOD_MELEE").
    const unsigned int argc = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (argc < 2 || argc > 6)
    {
        Scr_Error("Usage: doDamage( <health>, <source position>, <attacker>, <destructible_piece_index>, <means of death> )\n", 0);
        return;
    }
    hitLocation_t hitLoc = HITLOC_NONE;
    if (argc == 6)
        hitLoc = (hitLocation_t)G_GetHitLocationIndexFromString(
            Scr_GetConstLowercaseString(5, SCRIPTINSTANCE_SERVER));
    const char *modName = NULL;
    if (argc >= 5 && Scr_GetType(4, SCRIPTINSTANCE_SERVER) == VAR_STRING)
        modName = Scr_GetString(4, SCRIPTINSTANCE_SERVER);
    int pieceIndex = -1;
    if (argc >= 4 && Scr_GetType(3, SCRIPTINSTANCE_SERVER) == VAR_INTEGER)
        pieceIndex = Scr_GetInt(3, SCRIPTINSTANCE_SERVER);
    gentity_s *attacker = NULL;
    if (argc >= 3 && (Scr_GetType(2, SCRIPTINSTANCE_SERVER) == VAR_POINTER
        || Scr_GetType(2, SCRIPTINSTANCE_SERVER) == VAR_ENTITY))
        attacker = Scr_GetEntity(2);
    gentity_s *ent = GetEntity(entref);
    float damage = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    float source[3], from[3];
    Scr_GetVector(1, source, SCRIPTINSTANCE_SERVER);
    if ((LODWORD(source[0]) & 0x7F800000) == 0x7F800000
        || (LODWORD(source[1]) & 0x7F800000) == 0x7F800000
        || (LODWORD(source[2]) & 0x7F800000) == 0x7F800000)
        Scr_Error(va("Source Damage vector is invalid : %f %f %f", source[0], source[1], source[2]), 0);
    const float *origin = ent->client ? ent->client->ps.origin : ent->r.currentOrigin;
    Vec3Sub(origin, source, from);
    if (Vec3Normalize(from) == 0.0f)
    {
        from[0] = from[1] = 0.0f;
        from[2] = 1.0f;
    }
    meansOfDeath_t mod = MOD_UNKNOWN;
    if (modName)
    {
        for (int i = 0; i < MOD_NUM; ++i)
        {
            if (!I_stricmp(modName, SL_ConvertToString(*modNames[i], SCRIPTINSTANCE_SERVER)))
            {
                mod = (meansOfDeath_t)i;
                break;
            }
        }
    }
    if (ent->destructible)
    {
        damage = pieceIndex < 0
            ? (float)DestructibleRadiusDamage(ent, source, damage, 10.0f, 400.0f, MOD_EXPLOSIVE, attacker)
            : GScr_DamageDestructiblePiece_SP(ent, from, source, damage, mod, pieceIndex);
    }
    G_Damage(ent, attacker, attacker, from, source, (int)damage, 0, mod, -1, hitLoc, 0, 0, 0);
#else
    char *String; // eax
    const char *v2; // eax
    gclient_s *client; // edx
    meansOfDeath_t v4; // [esp+1Ch] [ebp-9Ch]
    gentity_s *attacker; // [esp+78h] [ebp-40h]
    float damage; // [esp+7Ch] [ebp-3Ch]
    float source[3]; // [esp+80h] [ebp-38h] BYREF
    meansOfDeath_t mod; // [esp+8Ch] [ebp-2Ch]
    float from[3]; // [esp+90h] [ebp-28h] BYREF
    float *dir; // [esp+9Ch] [ebp-1Ch]
    int weapon; // [esp+A0h] [ebp-18h]
    int dflags; // [esp+A4h] [ebp-14h]
    gentity_s *ent; // [esp+ACh] [ebp-Ch]
    gentity_s *inflictor; // [esp+B0h] [ebp-8h]
    hitLocation_t hitLoc; // [esp+B4h] [ebp-4h]

    PROF_SCOPED("ScrCmd_DoDamage");

    dflags = 0;
    attacker = 0;
    inflictor = 0;
    hitLoc = HITLOC_HEAD;
    mod = MOD_UNKNOWN;
    weapon = -1;
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 2:
            goto $LN9_44;
        case 3:
            goto $LN10_40;
        case 4:
            goto $LN12_23;
        case 5:
            goto $LN13_17;
        case 6:
            goto $LN14_17;
        case 7:
            goto $LN15_18;
        case 8:
            String = Scr_GetString(7u, SCRIPTINSTANCE_SERVER);
            weapon = G_GetWeaponIndexForName(String);
$LN15_18:
            dflags = Scr_GetInt(6u, SCRIPTINSTANCE_SERVER);
$LN14_17:
            mod = (meansOfDeath_t)G_MeansOfDeathFromScriptParam(5u);
$LN13_17:
            if ( !Scr_GetInt(4u, SCRIPTINSTANCE_SERVER) )
                hitLoc = HITLOC_NONE;
$LN12_23:
            inflictor = Scr_GetEntity(3u);
$LN10_40:
            attacker = Scr_GetEntity(2u);
$LN9_44:
            Scr_GetVector(1u, source, SCRIPTINSTANCE_SERVER);
            damage = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
            ent = GetEntity(entref);
            if ( (LODWORD(source[0]) & 0x7F800000) == 0x7F800000
                || (LODWORD(source[1]) & 0x7F800000) == 0x7F800000
                || (LODWORD(source[2]) & 0x7F800000) == 0x7F800000 )
            {
                v2 = va("Source Damage vector is invalid : %f %f %f", source[0], source[1], source[2]);
                Scr_Error(v2, 0);
            }
            if ( ent->client )
            {
                if ( ((LODWORD(ent->client->ps.origin[0]) & 0x7F800000) == 0x7F800000
                     || (LODWORD(ent->client->ps.origin[1]) & 0x7F800000) == 0x7F800000
                     || (LODWORD(ent->client->ps.origin[2]) & 0x7F800000) == 0x7F800000)
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                                3704,
                                0,
                                "%s",
                                "!IS_NAN((ent->client->ps.origin)[0]) && !IS_NAN((ent->client->ps.origin)[1]) && !IS_NAN((ent->client->ps.origin)[2])") )
                {
                    __debugbreak();
                }
                client = ent->client;
                from[0] = client->ps.origin[0] - source[0];
                from[1] = client->ps.origin[1] - source[1];
                from[2] = client->ps.origin[2] - source[2];
            }
            else
            {
                if ( ((LODWORD(ent->r.currentOrigin[0]) & 0x7F800000) == 0x7F800000
                     || (LODWORD(ent->r.currentOrigin[1]) & 0x7F800000) == 0x7F800000
                     || (LODWORD(ent->r.currentOrigin[2]) & 0x7F800000) == 0x7F800000)
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                                3709,
                                0,
                                "%s",
                                "!IS_NAN((ent->r.currentOrigin)[0]) && !IS_NAN((ent->r.currentOrigin)[1]) && !IS_NAN((ent->r.currentOrigin)[2])") )
                {
                    __debugbreak();
                }
                from[0] = ent->r.currentOrigin[0] - source[0];
                from[1] = ent->r.currentOrigin[1] - source[1];
                from[2] = ent->r.currentOrigin[2] - source[2];
            }
            if ( ((LODWORD(from[0]) & 0x7F800000) == 0x7F800000
                 || (LODWORD(from[1]) & 0x7F800000) == 0x7F800000
                 || (LODWORD(from[2]) & 0x7F800000) == 0x7F800000)
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            3713,
                            0,
                            "%s",
                            "!IS_NAN((from)[0]) && !IS_NAN((from)[1]) && !IS_NAN((from)[2])") )
            {
                __debugbreak();
            }
            if ( Vec3Normalize(from) == 0.0 )
                dir = 0;
            else
                dir = from;
            if ( !ent->scr_vehicle && ent->destructible )
            {
                if ( mod )
                    v4 = mod;
                else
                    v4 = MOD_EXPLOSIVE;
                DestructibleRadiusDamage(ent, source, damage, 10.0, 400.0, v4, attacker);
                dflags |= 0x10u;
            }
            G_Damage(ent, inflictor, attacker, dir, source, (int)damage, dflags, mod, weapon, hitLoc, 0, 0, 0);
            break;
        default:
            Scr_Error("Usage: doDamage( <health>, <source position>, <attacker>, <inflictor>, <mod> )\n", 0);
            break;
    }
#endif
}

void __cdecl ScrCmd_GetVelocity(scr_entref_t entref)
{
    scr_vehicle_s *scr_vehicle; // ecx
    gclient_s *client; // ecx
    float velocity[3]; // [esp+Ch] [ebp-10h] BYREF
    gentity_s *ent; // [esp+18h] [ebp-4h]

    memset(velocity, 0, sizeof(velocity));
    ent = GetEntity(entref);
    if ( ent->sentient )
    {
        Sentient_GetVelocity(ent->sentient, velocity);
    }
    else if ( ent->scr_vehicle )
    {
        scr_vehicle = ent->scr_vehicle;
        velocity[0] = scr_vehicle->phys.vel[0];
        velocity[1] = scr_vehicle->phys.vel[1];
        velocity[2] = scr_vehicle->phys.vel[2];
    }
    else if ( ent->client )
    {
        client = ent->client;
        velocity[0] = client->ps.velocity[0];
        velocity[1] = client->ps.velocity[1];
        velocity[2] = client->ps.velocity[2];
    }
    else
    {
        velocity[0] = ent->s.lerp.pos.trDelta[0];
        velocity[1] = ent->s.lerp.pos.trDelta[1];
        velocity[2] = ent->s.lerp.pos.trDelta[2];
    }
    Scr_AddVector(velocity, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_FakeFire(scr_entref_t entref)
{
    const char *v1; // eax
    float origin[3]; // [esp+0h] [ebp-20h] BYREF
    int iWeaponIndex; // [esp+Ch] [ebp-14h]
    gentity_s *owner; // [esp+10h] [ebp-10h]
    gentity_s *ent; // [esp+14h] [ebp-Ch]
    const char *pszWeaponName; // [esp+18h] [ebp-8h]
    int argc; // [esp+1Ch] [ebp-4h]

    ent = GetEntity(entref);
    argc = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( argc != 4 )
        Scr_Error("FakeFire( <owner>, <origin>, <weapon>, <shot count> ) takes 4 parameters", 0);
    owner = Scr_GetEntity(0);
    if ( !owner->client )
        Scr_ParamError(0, "Owner entity is not a player", SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    pszWeaponName = Scr_GetString(2u, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( !iWeaponIndex )
    {
        if ( *pszWeaponName )
        {
            v1 = va("Invalid weapon name %s", pszWeaponName);
            Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_ParamError(0, "Invalid weapon name", SCRIPTINSTANCE_SERVER);
        }
    }
    ent->s.lerp.eFlags |= 0x40u;
    if ( ent->s.eType == 1 )
    {
        ent = G_TempEntity(origin, EV_FAKE_FIRE);
        ent->s.eventParm = iWeaponIndex;
    }
    else
    {
        G_AddEvent(ent, 0x45u, iWeaponIndex);
    }
    ent->s.otherEntityNum = owner->s.number;
    ent->s.un1.scale = (unsigned __int8)Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetCameraSpikeActive(scr_entref_t entref)
{
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->client )
    {
        if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
        {
            if ( BG_GetWeaponDef(ent->client->ps.weapon)->guidedMissileType != MISSILE_GUIDANCE_TVGUIDED
                || ent->client->ps.fWeaponPosFrac < 0.5 )
            {
                if ( Scr_GetInt(0, SCRIPTINSTANCE_SERVER) )
                    ent->client->ps.weapFlags |= 0x200000u;
                else
                    ent->client->ps.weapFlags &= ~0x200000u;
            }
        }
        else
        {
            Scr_Error("USAGE: <toggle> not specified\n", 0);
        }
    }
    else
    {
        Scr_Error("USAGE: Must be called on a client\n", 0);
    }
}

void __cdecl ScrCmd_MakeUsable(scr_entref_t entref)
{
    __int16 team; // [esp+0h] [ebp-Ch]
    char *teamString; // [esp+4h] [ebp-8h]
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->s.eType == 14 )
        G_MakeVehicleUsable(ent);
    else
        ent->r.contents |= 0x200000u;
    if ( ent->s.eType == 6 && Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        teamString = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        if ( I_stricmp(teamString, "allies") )
            team = I_stricmp(teamString, "axis") == 0;
        else
            team = 2;
        ent->s.eventParm &= 0x3FFFu;
        ent->s.eventParm |= team << 14;
    }
}

void __cdecl ScrCmd_MakeUnusable(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->s.eType == 14 )
        G_MakeVehicleUsable(ent);
    else
        ent->r.contents &= ~0x200000u;
}

void __cdecl ScrCmd_Show(scr_entref_t entref)
{
    gentity_s *pEnt; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    pEnt->s.lerp.eFlags &= ~0x20u;
    if ( pEnt->client )
        pEnt->client->ps.eFlags &= ~0x20u;
#ifndef KISAK_SP
    pEnt->r.clientMask[0] = 0;
#endif
}

void __cdecl ScrCmd_Hide(scr_entref_t entref)
{
    gentity_s *pEnt; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    pEnt->s.lerp.eFlags |= 0x20u;
    if ( pEnt->client )
        pEnt->client->ps.eFlags |= 0x20u;
#ifndef KISAK_SP
    pEnt->r.clientMask[0] = -1;
    if ( pEnt->s.eType == 14 )
        G_HideVehicle(pEnt);
    G_ClearGroundEntityRefs(pEnt);
#endif
}

void __cdecl ScrCmd_Ghost(scr_entref_t entref)
{
    gentity_s *pEnt; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    pEnt->s.lerp.eFlags |= 0x20u;
    if ( pEnt->client )
        pEnt->client->ps.eFlags |= 0x20u;
}

void __cdecl ScrCmd_ShowToPlayer(scr_entref_t entref)
{
    gentity_s *pEnt; // [esp+0h] [ebp-8h]
    gentity_s *clientEnt; // [esp+4h] [ebp-4h]

    pEnt = GetEntity(entref);
    clientEnt = Scr_GetEntity(0);
    if ( clientEnt->s.number < 32 )
    {
        pEnt->s.lerp.eFlags &= ~0x20u;
        pEnt->r.clientMask[clientEnt->s.number >> 5] &= ~(1 << (clientEnt->s.number & 0x1F));
    }
    else
    {
        Scr_Error("showToClient error: param must be a client entity\n", 0);
    }
}

void __cdecl ScrCmd_SetContents(scr_entref_t entref)
{
    int contents; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]
    int savedregs; // [esp+8h] [ebp+0h] BYREF

    ent = GetEntity(entref);
    contents = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(ent->r.contents, SCRIPTINSTANCE_SERVER);
    ent->r.contents = contents;
    SV_LinkEntity(ent);
}

#ifdef KISAK_SP
static void GScr_StopUsingTurret_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->client || (ent->client->ps.eFlags & 0x300) == 0)
        return;
    gentity_s *turret = &g_entities[ent->client->ps.viewlocked_entNum];
    if (turret->s.eType == ET_MG42)
        G_ClientStopUsingTurret(turret);
}

static void GScr_TurretFireDisable_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->pTurretInfo)
        Scr_Error(va("entity type '%s' is not a turret",
                     SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);
    ent->pTurretInfo->flags |= 0x4000u;
}

static void GScr_TurretFireEnable_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->pTurretInfo)
        Scr_Error(va("entity type '%s' is not a turret",
                     SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);
    ent->pTurretInfo->flags &= ~0x4000u;
}
#endif

void __cdecl GScr_StartFiring(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    TurretInfo *pTurretInfo; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    pTurretInfo = ent->pTurretInfo;
    if ( !pTurretInfo
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 4104, 0, "%s", "pTurretInfo") )
    {
        __debugbreak();
    }
    pTurretInfo->flags |= 4u;
}

void __cdecl GScr_StopFiring(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    TurretInfo *pTurretInfo; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    pTurretInfo = ent->pTurretInfo;
    if ( !pTurretInfo
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 4130, 0, "%s", "pTurretInfo") )
    {
        __debugbreak();
    }
    pTurretInfo->flags &= ~4u;
}

void __cdecl GScr_ShootTurret(scr_entref_t entref)
{
    gentity_s *ent; // [esp+Ch] [ebp-4h]

    PROF_SCOPED("shootturret");
    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        Scr_Error(va("entity type '%s' is not a turret", SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), 0);
    }
    turret_shoot(ent);
}

void __cdecl GScr_StopShootTurret(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    ent->s.lerp.eFlags &= ~0x40u;
}

void __cdecl GScr_SetMode(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    TurretInfo *pTurretInfo; // [esp+0h] [ebp-Ch]
    gentity_s *ent; // [esp+4h] [ebp-8h]
    unsigned int mode; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    pTurretInfo = ent->pTurretInfo;
    if ( !pTurretInfo
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 4202, 0, "%s", "pTurretInfo") )
    {
        __debugbreak();
    }
    mode = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( mode == scr_const.auto_ai )
    {
        pTurretInfo->flags &= 0xFFFFFFFC;
        pTurretInfo->flags |= 3u;
    }
    else if ( mode == scr_const.manual )
    {
        pTurretInfo->flags &= 0xFFFFFFFC;
    }
    else if ( mode == scr_const.manual_ai )
    {
        pTurretInfo->flags &= 0xFFFFFFFC;
        pTurretInfo->flags |= 1u;
    }
    else if ( mode == scr_const.auto_nonai )
    {
        pTurretInfo->flags &= 0xFFFFFFFC;
        pTurretInfo->flags |= 2u;
    }
    else
    {
        Scr_Error("Error setting the mode of a turret.\n", 0);
    }
}

void __cdecl GScr_GetTurretOwner(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *v3; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( ent->active )
    {
        if ( ent->r.ownerNum.isDefined() )
        {
            v3 = ent->r.ownerNum.ent();
            Scr_AddEntity(v3, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_AddEntity(&g_entities[1023], SCRIPTINSTANCE_SERVER);
        }
    }
}

void __cdecl GScr_SetTargetEntity(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *Entity; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4272,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    Entity = Scr_GetEntity(0);
    ent->pTurretInfo->manualTarget.setEnt(Entity);
}

void __cdecl GScr_SetAiSpread(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4297,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    ent->pTurretInfo->aiSpread = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetPlayerSpread(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4322,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    ent->pTurretInfo->playerSpread = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetConvergenceTime(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    char *typeString; // [esp+2Ch] [ebp-Ch]
    gentity_s *ent; // [esp+30h] [ebp-8h]
    int type; // [esp+34h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4350,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    type = 1;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
    {
        typeString = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        if ( !strcmp(typeString, "yaw") )
        {
            type = 1;
        }
        else if ( !strcmp(typeString, "pitch") )
        {
            type = 0;
        }
        else
        {
            Scr_Error("Convergence type should be either 'pitch' or 'yaw'", 0);
        }
    }
    ent->pTurretInfo->convergenceTime[type] = (int)(Scr_GetFloat(0, SCRIPTINSTANCE_SERVER) * 1000.0);
}

void __cdecl GScr_SetSuppressionTime(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4390,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    ent->pTurretInfo->suppressTime = (int)(Scr_GetFloat(0, SCRIPTINSTANCE_SERVER) * 1000.0);
}

void __cdecl GScr_ClearTargetEntity(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4414,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    ent->pTurretInfo->manualTarget.setEnt(0);
}

void __cdecl GScr_SetTurretTeam(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    gentity_s *ent; // [esp+0h] [ebp-8h]
    char *pszTeam; // [esp+4h] [ebp-4h]

    pszTeam = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    ent->s.faction.iHeadIconTeam &= ~1u;
    ent->s.faction.iHeadIconTeam &= ~2u;
    ent->s.faction.iHeadIconTeam = ent->s.faction.iHeadIconTeam;
    if ( I_stricmp(pszTeam, "axis") )
    {
        if ( I_stricmp(pszTeam, "allies") )
        {
            if ( I_stricmp(pszTeam, "free") )
            {
                v3 = va("unknown team '%s', should be 'axis' or 'allies' or 'free'\n", pszTeam);
                Scr_Error(v3, 0);
            }
            else
            {
                ent->pTurretInfo->eTeam = TEAM_FREE;
                ent->s.faction.iHeadIconTeam = ent->s.faction.iHeadIconTeam;
            }
        }
        else
        {
            ent->pTurretInfo->eTeam = TEAM_ALLIES;
            ent->s.faction.iHeadIconTeam |= 2u;
        }
    }
    else
    {
        ent->pTurretInfo->eTeam = TEAM_AXIS;
        ent->s.faction.iHeadIconTeam |= 1u;
    }
}

void __cdecl GScr_SetTurretIgnoreGoals(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4484,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    if ( Scr_GetInt(0, SCRIPTINSTANCE_SERVER) )
        ent->pTurretInfo->flags |= 0x2000u;
    else
        ent->pTurretInfo->flags &= ~0x2000u;
}

void __cdecl GScr_MakeTurretUsable(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4513,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    ent->pTurretInfo->flags |= 0x1000u;
}

void __cdecl GScr_MakeTurretUnusable(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4536,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    ent->pTurretInfo->flags &= ~0x1000u;
}

void __cdecl GScr_SetTurretAccuracy(scr_entref_t entref)
{
    Com_PrintWarning(24, "WARNING: Turret Accuracy no longer has any effect\n");
}

void __cdecl GScr_GetTurretTarget(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *v3; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4576,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    if ( ent->pTurretInfo->target.isDefined() )
    {
        if ( (ent->pTurretInfo->flags & 0x40) != 0 )
        {
            v3 = ent->pTurretInfo->target.ent();
            Scr_AddEntity(v3, SCRIPTINSTANCE_SERVER);
        }
    }
}

void __cdecl GScr_DisconnectPaths(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !Path_IsDynamicBlockingEntity(ent) )
    {
        if ( ent->classname == scr_const.script_brushmodel )
            Scr_Error("script_brushmodel must have DYNAMICPATH set to disconnect paths", 0);
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity of type '%s' cannot disconnect paths.\n \n ", v1);
        Scr_Error(v2, 0);
    }
    Path_DisconnectPathsForEntity(ent);
}

void __cdecl GScr_ConnectPaths(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !Path_IsDynamicBlockingEntity(ent) )
    {
        if ( ent->classname == scr_const.script_brushmodel )
            Scr_Error("script_brushmodel must have DYNAMICPATH set to connect paths", 0);
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity of type '%s' cannot connect paths \n\n", v1);
        Scr_Error(v2, 0);
    }
#ifdef KISAK_SP
    const int flagsBefore = ent->flags;
    const unsigned int disconnectedLinksBefore = ent->disconnectedLinks;
    Com_Printf(
        15,
        "ZM_BARRICADE connectpaths_begin ent=%d flags=0x%08X disconnected=%u\n",
        ent->s.number,
        flagsBefore,
        disconnectedLinksBefore);
#endif
    Path_ConnectPathsForEntity(ent);
#ifdef KISAK_SP
    // Behavior-neutral Zombies barricade probe. The shipped spawner script
    // calls connectpaths() only after the board-tear animation finishes. This
    // records whether that wrapper reached the native dynamic-path pipeline
    // and whether it consumed the entity's disconnected-link chain.
    Com_Printf(
        15,
        "ZM_BARRICADE connectpaths ent=%d flags=0x%08X->0x%08X disconnected=%u->%u\n",
        ent->s.number,
        flagsBefore,
        ent->flags,
        disconnectedLinksBefore,
        ent->disconnectedLinks);
#endif
}

// LWSS ADD
void __cdecl ScrCmd_SetStance(scr_entref_t entref)
{
    gentity_s *ent;
    unsigned short stance;

    ent = GetEntity(entref);
    stance = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);

    if (!ent->client)
    {
        Scr_Error("SetStance is only defined for players.", SCRIPTINSTANCE_SERVER);
        return;
    }

    if (stance == scr_const.stand)
    {
        ent->client->ps.pm_flags &= ~3;
        ent->client->ps.viewHeightTarget = 60;
        G_AddEvent(ent, 8, SCRIPTINSTANCE_SERVER);
    }
    else if (stance == scr_const.crouch)
    {
        ent->client->ps.pm_flags = (ent->client->ps.pm_flags & ~3) | 2;
        ent->client->ps.viewHeightTarget = 40;
        G_AddEvent(ent, 9, SCRIPTINSTANCE_SERVER);
    }
    else if (stance == scr_const.prone)
    {
        if (!(ent->client->ps.pm_flags & 1))
            ent->client->ps.proneDirection = ent->client->ps.viewHeightCurrent;
        ent->client->ps.pm_flags = (ent->client->ps.pm_flags & ~3) | 1;
        ent->client->ps.viewHeightTarget = 11;
        G_AddEvent(ent, 10, SCRIPTINSTANCE_SERVER);
    }
}
// LWSS END

void __cdecl ScrCmd_GetStance(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->client )
    {
        if ( (ent->client->ps.pm_flags & 1) != 0 )
        {
            Scr_AddConstString(scr_const.prone, SCRIPTINSTANCE_SERVER);
        }
        else if ( (ent->client->ps.pm_flags & 2) != 0 )
        {
            Scr_AddConstString(scr_const.crouch, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_AddConstString(scr_const.stand, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        Scr_Error("GetStance is only defined for players.", 0);
    }
}

void __cdecl Scr_SetStableMissile(scr_entref_t entref)
{
    unsigned int v1; // eax
    int stableMissile; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    stableMissile = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( ent->s.eType != 1 )
        Scr_Error("Type should be a player", 0);
    if ( stableMissile )
        v1 = ent->flags | 0x20000;
    else
        v1 = ent->flags & 0xFFFDFFFF;
    ent->flags = v1;
}

void __cdecl GScr_SetCursorHint(scr_entref_t entref)
{
    const char *v1; // eax
    char *pszHint; // [esp+0h] [ebp-Ch]
    gentity_s *pEnt; // [esp+4h] [ebp-8h]
    int i; // [esp+8h] [ebp-4h]
    int ia; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    if ( pEnt->s.eType == 4
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    4687,
                    0,
                    "%s",
                    "pEnt->s.eType != ET_MISSILE") )
    {
        __debugbreak();
    }
    pszHint = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( (pEnt->classname == scr_const.trigger_use
         || pEnt->classname == scr_const.trigger_use_touch
         || pEnt->classname == scr_const.trigger_radius
         || pEnt->classname == scr_const.trigger_radius_use)
        && !I_stricmp(pszHint, "HINT_INHERIT") )
    {
        pEnt->s.un3.item = -1;
    }
    else
    {
        for ( i = 1; i < 8; ++i )
        {
            if ( !I_stricmp(pszHint, hintStrings[i]) )
            {
                pEnt->s.un3.item = i;
                return;
            }
        }
        Com_Printf(24, "List of valid hint type strings\n");
        if ( pEnt->classname == scr_const.trigger_use
            || pEnt->classname == scr_const.trigger_use_touch
            || pEnt->classname == scr_const.trigger_radius
            || pEnt->classname == scr_const.trigger_radius_use )
        {
            Com_Printf(24, "HINT_INHERIT (for trigger_use or trigger_use_touch or trigger_radius entities only)\n");
        }
        for ( ia = 1; ia < 8; ++ia )
            Com_Printf(24, "%s\n", hintStrings[ia]);
        v1 = va("%s is not a valid hint type. See above for list of valid hint types\n", pszHint);
        Scr_Error(v1, 0);
    }
}

int __cdecl G_GetHintStringIndex(int *piIndex, char *pszString)
{
    char szConfigString[1024]; // [esp+14h] [ebp-408h] BYREF
    int i; // [esp+418h] [ebp-4h]

    for ( i = 0; i < 96; ++i )
    {
        SV_GetConfigstring(i + 419, szConfigString, 1024);
        if ( !szConfigString[0] )
        {
            SV_SetConfigstring(i + 419, pszString);
            *piIndex = i;
            return 1;
        }
        if ( !strcmp(pszString, szConfigString) )
        {
            *piIndex = i;
            return 1;
        }
    }
    *piIndex = -1;
    return 0;
}

void __cdecl GScr_SetHintString(scr_entref_t entref)
{
    char *String; // eax
    int NumParam; // eax
    const char *v3; // eax
    char szHint[1024]; // [esp+0h] [ebp-410h] BYREF
    int type; // [esp+404h] [ebp-Ch]
    gentity_s *pEnt; // [esp+408h] [ebp-8h]
    int i; // [esp+40Ch] [ebp-4h] BYREF

    pEnt = GetEntity(entref);
    if ( pEnt->classname != scr_const.trigger_use
        && pEnt->classname != scr_const.trigger_use_touch
        && pEnt->classname != scr_const.trigger_radius
        && pEnt->classname != scr_const.trigger_radius_use
        && pEnt->s.eType != 6 )
    {
        Scr_Error(
            "The setHintString command only works on trigger_use, trigger_radius, trigger_radius_use, trigger_use_touch, or scr"
            "ipt mover entities.\n",
            0);
    }
    type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    if ( type != 2 || (String = Scr_GetString(0, SCRIPTINSTANCE_SERVER), I_stricmp(String, "")) )
    {
        NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
        Scr_ConstructMessageString(0, NumParam - 1, "Hint String", szHint, 0x400u);
        if ( !G_GetHintStringIndex(&i, szHint) )
        {
            v3 = va("Too many different hintstring values. Max allowed is %i different strings", 96);
            Scr_Error(v3, 0);
        }
        pEnt->s.un1.scale = i;
    }
    else
    {
        pEnt->s.un1.scale = -1;
    }
}

void __cdecl GScr_SetHintStringForPerk(scr_entref_t entref)
{
    char *String; // eax
    char *v2; // eax
    const char *v3; // eax
    int hintIndex; // [esp+0h] [ebp-414h] BYREF
    char szHint[1024]; // [esp+4h] [ebp-410h] BYREF
    int perkIndex; // [esp+408h] [ebp-Ch]
    int type; // [esp+40Ch] [ebp-8h]
    gentity_s *pEnt; // [esp+410h] [ebp-4h]

    pEnt = GetEntity(entref);
    if ( pEnt->s.eType != 6 )
        Scr_Error("The SetHintStringForPerk command only works on script mover entities.\n", 0);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("SetHintStringForPerk Usage: <perk>, <hint string>", 0);
    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    perkIndex = BG_GetPerkIndexForName(String);
    pEnt->s.eventParm &= 0xC000u;
    type = Scr_GetType(1u, SCRIPTINSTANCE_SERVER);
    if ( type != 2 || (v2 = Scr_GetString(1u, SCRIPTINSTANCE_SERVER), I_stricmp(v2, "")) )
    {
        Scr_ConstructMessageString(1, 1, "Hint String", szHint, 0x400u);
        if ( !G_GetHintStringIndex(&hintIndex, szHint) )
        {
            v3 = va("Too many different hintstring values. Max allowed is %i different strings", 96);
            Scr_Error(v3, 0);
        }
        if ( perkIndex >= 64
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        4832,
                        0,
                        "%s",
                        "perkIndex < 64") )
        {
            __debugbreak();
        }
        if ( perkIndex >= 64 )
            perkIndex = 0;
        pEnt->s.eventParm |= (_WORD)perkIndex << 8;
        pEnt->s.eventParm |= (unsigned __int8)hintIndex;
    }
}

void __cdecl GScr_SetHintLowPriority(scr_entref_t entref)
{
    unsigned int v1; // ecx
    gentity_s *pEnt; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("SetHintLowPriority Usage: <bool>", 0);
    if ( Scr_GetInt(0, SCRIPTINSTANCE_SERVER) )
        v1 = pEnt->flags | 0x200;
    else
        v1 = pEnt->flags & 0xFFFFFDFF;
    pEnt->flags = v1;
}

void __cdecl GScr_SetReviveHintString(scr_entref_t entref)
{
    char *String; // eax
    const char *v2; // eax
    char *team; // [esp+28h] [ebp-414h]
    char szHint[1024]; // [esp+2Ch] [ebp-410h] BYREF
    int type; // [esp+430h] [ebp-Ch]
    gentity_s *pEnt; // [esp+434h] [ebp-8h]
    int i; // [esp+438h] [ebp-4h] BYREF

    pEnt = GetEntity(entref);
    if ( pEnt->classname != scr_const.trigger_use
        && pEnt->classname != scr_const.trigger_use_touch
        && pEnt->classname != scr_const.trigger_radius )
    {
        Scr_Error("The setHintString command only works on trigger_use, trigger_radius or trigger_use_touch entities.\n", 0);
    }
    type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    if ( type != 2 || (String = Scr_GetString(0, SCRIPTINSTANCE_SERVER), I_stricmp(String, "")) )
    {
        if ( pEnt->classname == scr_const.trigger_radius )
        {
            pEnt->team = 0;
            if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
            {
                team = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
                if ( !strcmp(team, "allies") )
                {
                    pEnt->team = 2;
                }
                else if ( !strcmp(team, "axis") )
                {
                    pEnt->team = 1;
                }
            }
        }
        Scr_ConstructMessageString(0, 0, "Hint String", szHint, 0x400u);
        if ( !G_GetHintStringIndex(&i, szHint) )
        {
            v2 = va("Too many different hintstring values. Max allowed is %i different strings", 96);
            Scr_Error(v2, 0);
        }
        pEnt->s.un1.scale = i;
    }
    else
    {
        pEnt->s.un1.scale = -1;
    }
}

#ifdef KISAK_SP
void __cdecl GScr_UseTriggerRequireLookAt(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (ent->classname != scr_const.trigger_use
        && ent->classname != scr_const.trigger_radius_use)
    {
        Scr_Error("The UseTriggerRequireLookAt command only works on trigger_use and trigger_radius_use entities.\n", false);
    }
    ent->trigger.requireLookAt = true;
}
#else
void __cdecl GScr_UseTriggerRequireLookAt(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->classname != scr_const.trigger_use )
        Scr_Error("The UseTriggerRequireLookAt command only works on trigger_use entities.\n", 0);
    ent->trigger.requireLookAt = 1;
}
#endif

void __cdecl GScr_IsMartyrdomGrenade(scr_entref_t entref)
{
    gentity_s *pEnt; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    Scr_AddBool((pEnt->flags & 0x8000) != 0, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_GetEntityNumber(scr_entref_t entref)
{
    gentity_s *pEnt; // [esp+8h] [ebp-4h]

    pEnt = GetEntity(entref);
    Scr_AddInt(pEnt->s.number, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_EnableGrenadeTouchDamage(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->classname != scr_const.trigger_damage )
        Scr_Error("Currently on supported on damage triggers", 0);
    ent->flags |= 0x4000u;
}

void __cdecl GScr_DisableGrenadeTouchDamage(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->classname != scr_const.trigger_damage )
        Scr_Error("Currently on supported on damage triggers", 0);
    ent->flags &= ~0x4000u;
}

void __cdecl GScr_MissileSetTarget(scr_entref_t entref)
{
    const char *v1; // eax
    gentity_s *Entity; // [esp+0h] [ebp-14h]
    gentity_s *missile; // [esp+Ch] [ebp-8h]

    missile = GetEntity(entref);
    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) )
        Entity = Scr_GetEntity(0);
    else
        Entity = 0;
    if ( missile->classname != scr_const.rocket )
    {
        v1 = va("Entity %i is not a rocket\n", missile->s.number);
        Scr_Error(v1, 0);
    }
    missile->missileTargetEnt.setEnt(Entity);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 1 )
    {
        missile->mover.pos3[2] = 0.0f;
        missile->mover.apos1[0] = 0.0f;
        missile->mover.apos1[1] = 0.0f;
    }
    else
    {
        Scr_GetVector(1u, &missile->mover.pos3[2], SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_EnableAimAssist(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
#ifndef KISAK_SP
    if ( !ent->r.bmodel )
        Scr_Error("Currently only supported on entities with brush models", 0);
#endif
    ent->s.lerp.eFlags |= 0x800u;
}

void __cdecl GScr_DisableAimAssist(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
#ifndef KISAK_SP
    if ( !ent->r.bmodel )
        Scr_Error("Currently only supported on entities with brush models", 0);
#endif
    ent->s.lerp.eFlags &= ~0x800u;
}

void __cdecl G_InitObjectives()
{
    int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < 32; ++i )
    {
#ifdef KISAK_SP
        SV_SetConfigstring(CS_SP_OBJECTIVES + i, NULL);
#endif
        ClearObjective(&level.objectives[i]);
    }
}

int __cdecl ObjectiveStateIndexFromString(objectiveState_t *piStateIndex, unsigned int stateString)
{
    if ( stateString == scr_const.empty )
    {
        *piStateIndex = OBJST_EMPTY;
    }
    else if ( stateString == scr_const.invisible )
    {
        *piStateIndex = OBJST_INVISIBLE;
    }
#ifdef KISAK_SP
    else if (stateString == scr_const.done)
    {
        *piStateIndex = OBJST_DONE;
    }
    else if (stateString == scr_const.failed)
    {
        *piStateIndex = OBJST_FAILED;
    }
#endif
    else if ( stateString == scr_const.current )
    {
        *piStateIndex = OBJST_CURRENT;
    }
    else
    {
        if ( stateString != scr_const.active )
        {
            *piStateIndex = OBJST_EMPTY;
            return 0;
        }
        *piStateIndex = OBJST_ACTIVE;
    }
    return 1;
}

void __cdecl ClearObjective(objective_t *obj)
{
    obj->state = OBJST_EMPTY;
    obj->origin[0] = 0.0f;
    obj->origin[1] = 0.0f;
    obj->origin[2] = 0.0f;
#ifdef KISAK_SP
    obj->size[0] = 0.0f;
    obj->size[1] = 0.0f;
#endif
    obj->entNum = 1023;
    obj->teamNum = 0;
    obj->icon = 0;
}

#ifdef KISAK_SP
static void G_AddObjective_SP(unsigned int firstParam, int teamNum);
#endif

void Scr_Objective_Add()
{
#ifdef KISAK_SP
    G_AddObjective_SP(0, -1);
#else
    const char *v0; // eax
    char *v1; // eax
    const char *v2; // eax
    objectiveState_t state; // [esp+0h] [ebp-14h] BYREF
    objective_t *obj; // [esp+4h] [ebp-10h]
    int numParam; // [esp+8h] [ebp-Ch]
    unsigned __int16 stateName; // [esp+Ch] [ebp-8h]
    int objNum; // [esp+10h] [ebp-4h]

    numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( numParam < 2 )
        Scr_Error(
            "objective_add needs at least the first two parameters out of its parameter list of: index state [string] [position]\n",
            0);
    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objNum >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    obj = &level.objectives[objNum];
    level.objectivesClientMask[objNum][0] = 0;
    level.objectivesClientMask[objNum][1] = 0;
    ClearObjective_OnEntity(obj);
    stateName = (unsigned __int16)Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    if ( !ObjectiveStateIndexFromString(&state, stateName) )
    {
        v1 = SL_ConvertToString(stateName, SCRIPTINSTANCE_SERVER);
        v2 = va("Illegal objective state \"%s\". Valid states are \"empty\", \"invisible\", \"current\", \"active\"\n", v1);
        Scr_ParamError(1u, v2, SCRIPTINSTANCE_SERVER);
    }
    obj->state = state;
    if ( numParam >= 3 )
    {
        Scr_GetVector(2u, obj->origin, SCRIPTINSTANCE_SERVER);
        obj->origin[0] = (float)(int)obj->origin[0];
        obj->origin[1] = (float)(int)obj->origin[1];
        obj->origin[2] = (float)(int)obj->origin[2];
        obj->entNum = 1023;
        if ( numParam >= 4 )
            SetObjectiveIcon(obj, 3u);
    }
    obj->teamNum = 0;
#endif
}

void __cdecl ClearObjective_OnEntity(objective_t *obj)
{
    gentity_s *pEnt; // [esp+0h] [ebp-4h]

    if ( obj->entNum != 1023 )
    {
        pEnt = &g_entities[obj->entNum];
        if ( pEnt->r.inuse )
            pEnt->r.svFlags &= ~0x10u;
        obj->entNum = 1023;
    }
}

#ifdef KISAK_SP
static int G_GetObjectiveIconIndex_SP(unsigned int paramNum)
{
    const char *name = Scr_GetString(paramNum, SCRIPTINSTANCE_SERVER);
    int length = 0;
    for (; name[length]; ++length)
    {
        const signed char character = static_cast<signed char>(name[length]);
        if (character <= 31 || character >= 127)
        {
            Scr_ParamError(3, va("Illegal character '%c'(ascii %i) in objective icon name: %s\n",
                static_cast<int>(character), static_cast<unsigned char>(character), name), SCRIPTINSTANCE_SERVER);
        }
    }
    if (length >= 64)
        Scr_ParamError(3, va("Objective icon name is too long (> %i): %s\n", 63, name), SCRIPTINSTANCE_SERVER);
    return G_MaterialIndex(name);
}

static void G_SetObjectiveIconConfig_SP(char *config, unsigned int paramNum)
{
    Info_SetValueForKey(config, "icon", va("%i", G_GetObjectiveIconIndex_SP(paramNum)));
}
#endif

void __cdecl SetObjectiveIcon(objective_t *obj, unsigned int paramNum)
{
#ifdef KISAK_SP
    obj->icon = G_GetObjectiveIconIndex_SP(paramNum);
#else
    const char *v2; // eax
    const char *v3; // eax
    char *shaderName; // [esp+0h] [ebp-8h]
    int i; // [esp+4h] [ebp-4h]

    shaderName = Scr_GetString(paramNum, SCRIPTINSTANCE_SERVER);
    for ( i = 0; shaderName[i]; ++i )
    {
        if ( shaderName[i] <= 31 || shaderName[i] >= 127 )
        {
            v2 = va(
                         "Illegal character '%c'(ascii %i) in objective icon name: %s\n",
                         shaderName[i],
                         (unsigned __int8)shaderName[i],
                         shaderName);
            Scr_ParamError(3u, v2, SCRIPTINSTANCE_SERVER);
        }
    }
    if ( i >= 64 )
    {
        v3 = va("Objective icon name is too long (> %i): %s\n", 63, shaderName);
        Scr_ParamError(3u, v3, SCRIPTINSTANCE_SERVER);
    }
    obj->icon = G_MaterialIndex(shaderName);
#endif
}

void Scr_Objective_Delete()
{
#ifdef KISAK_SP
    const int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (static_cast<unsigned int>(index) >= 32)
        Scr_Error(va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
    char config[4];
    SV_GetConfigstring(CS_SP_OBJECTIVES + index, config, sizeof(config));
    config[0] = 0;
    SV_SetConfigstring(CS_SP_OBJECTIVES + index, config);
    ClearObjective_OnEntity(&level.objectives[index]);
    ClearObjective(&level.objectives[index]);
#else
    const char *v0; // eax
    int objNum; // [esp+0h] [ebp-4h]

    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objNum >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    ClearObjective_OnEntity(&level.objectives[objNum]);
    ClearObjective(&level.objectives[objNum]);
    level.objectivesClientMask[objNum][0] = 0;
    level.objectivesClientMask[objNum][1] = 0;
#endif
}

#ifdef KISAK_SP
static void Scr_Objective_Ring_SP()
{
    const int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (static_cast<unsigned int>(index) >= 32)
        Scr_Error(va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
    char config[1024];
    SV_GetConfigstring(CS_SP_OBJECTIVES + index, config, sizeof(config));
    const char *value = Info_ValueForKey(config, "ring");
    const int ring = value[0] ? atoi(value) : 0;
    Info_SetValueForKey(config, "ring", va("%i", ring == 0));
    SV_SetConfigstring(CS_SP_OBJECTIVES + index, config);
}
#endif

#ifdef KISAK_SP
static void Scr_ValidateObjectiveString_SP(char *string)
{
    const int length = strlen(string);
    if (length >= 1024)
        Scr_Error(va("Objective strings is too long (> %i): %s\n", 1023, string), SCRIPTINSTANCE_SERVER);
    if (!Info_Validate(string) || strchr(string, '\\'))
        Scr_Error(va("Objective strings can not have a \", a ;, or a \\ in them. Illegal objective string: %s\n", string), SCRIPTINSTANCE_SERVER);
    for (int i = 0; i < length; ++i)
    {
        const unsigned char character = static_cast<unsigned char>(string[i]);
        if ((character < 32 && character != 20 && character != 21) || character >= 127)
            Scr_Error(va("Illegal character '%c'(ascii %i) in objective string: %s\n",
                static_cast<signed char>(character), character, string), SCRIPTINSTANCE_SERVER);
    }
}

static void Scr_Objective_Set3D_SP()
{
    const unsigned int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    const int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (static_cast<unsigned int>(index) >= 32)
        Scr_Error(va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
    objective_t *obj = &level.objectives[index];
    obj->use3D = 0;
    obj->color = 0;
    char config[1025];
    char message[1024];
    message[0] = 0;
    float offset[3] = {};
    SV_GetConfigstring(CS_SP_OBJECTIVES + index, config, 1024);
    switch (count)
    {
    case 6:
        Scr_GetVector(5, offset, SCRIPTINSTANCE_SERVER);
        [[fallthrough]];
    case 5:
    {
        const int menu = Scr_GetInt(4, SCRIPTINSTANCE_SERVER);
        const unsigned int selection = menu < 0 ? 0 : static_cast<unsigned int>(menu) + 1;
        obj->use3D |= (selection & 15) << 1;
        [[fallthrough]];
    }
    case 4:
        Scr_ConstructMessageString(3, 3, "Objective String", message, sizeof(message));
        Scr_ValidateObjectiveString_SP(message);
        [[fallthrough]];
    case 3:
    {
        if (Scr_GetType(2, SCRIPTINSTANCE_SERVER) == VAR_VECTOR)
        {
            float rgb[3];
            Scr_GetVector(2, rgb, SCRIPTINSTANCE_SERVER);
            obj->color = SP_OBJECTIVE_PACKED_RGB;
            for (int i = 0; i < 3; ++i)
            {
                if (!std::isfinite(rgb[i]) || rgb[i] < 0.0f || rgb[i] > 1.0f)
                    Scr_ParamError(2, "Objective color components must be between 0 and 1", SCRIPTINSTANCE_SERVER);
                obj->color |= static_cast<int>(rgb[i] * 255.0f + 0.5f) << (16 - i * 8);
            }
        }
        else
        {
            const char *color = Scr_GetString(2, SCRIPTINSTANCE_SERVER);
            if (!I_stricmp(color, "white"))
                obj->color = 1;
            else if (!I_stricmp(color, "red"))
                obj->color = 2;
            else if (!I_stricmp(color, "green"))
                obj->color = 3;
            else if (!I_stricmp(color, "blue"))
                obj->color = 4;
            else if (!I_stricmp(color, "ltblue"))
                obj->color = 5;
        }
        [[fallthrough]];
    }
    case 2:
        obj->use3D |= Scr_GetInt(1, SCRIPTINSTANCE_SERVER) & 1;
        break;
    default:
        Scr_Error(va("Invalid argument count. Expected 2-6, given %d\n", count), SCRIPTINSTANCE_SERVER);
        break;
    }
    Info_SetValueForKey(config, "txt3d", message);
    Info_SetValueForKey(config, "use3d", va("%i", obj->use3D));
    Info_SetValueForKey(config, "col3d", va("%i", obj->color));
    const char *offsetKeys[] = { "offx", "offy", "offz" };
    for (int i = 0; i < 3; ++i)
    {
        const int coordinate = offset[i] >= -2147483648.0f && offset[i] < 2147483648.0f ? (int)offset[i] : INT_MIN;
        Info_SetValueForKey(config, offsetKeys[i], va("%i", coordinate));
    }
    SV_SetConfigstring(CS_SP_OBJECTIVES + index, config);
    if (Dvar_GetBool("g_cubaProgressDebug") && !I_stricmp(Dvar_GetString("mapname"), "cuba"))
        Com_Printf(15, "CUBA_OBJECTIVE time=%i slot=%i visible=%i target=%i label=%s\n",
            level.time, index, obj->use3D & 1, obj->entNum, message);
}
#endif

#ifdef KISAK_SP
static bool G_SendObjectiveNotification_SP(unsigned short stateName, const char *message, int clientNum)
{
    if (zombiemode->current.enabled)
        return false;
    if (stateName == scr_const.active || stateName == scr_const.current)
    {
        SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, va("%c \"%i\" \"%s\"", 'V', 0, message));
    }
    else if (stateName == scr_const.done || stateName == scr_const.failed)
    {
        SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, va("%c \"%i\"", 'V', stateName == scr_const.done ? 1 : 2));
    }
    else
    {
        return false;
    }
    return true;
}
#endif

#ifdef KISAK_SP
static const char *G_GetObjectiveString_SP(unsigned int paramNum, char *message)
{
    const bool localized = Scr_GetType(paramNum, SCRIPTINSTANCE_SERVER) == 3;
    const char *text = localized ? Scr_GetIString(paramNum, SCRIPTINSTANCE_SERVER)
                                 : Scr_GetString(paramNum, SCRIPTINSTANCE_SERVER);
    const int length = strlen(text);
    if (localized)
    {
        if (length > 1)
        {
            for (int i = 0; i < length; ++i)
            {
                if (!I_isalnum(static_cast<signed char>(text[i])) && text[i] != '_')
                    Scr_ParamError(paramNum, va("Illegal localized string reference: %s (must contain only alpha-numeric characters and underscores", text), SCRIPTINSTANCE_SERVER);
            }
        }
    }
    else
    {
        for (int i = 0; i < length; ++i)
        {
            const signed char character = static_cast<signed char>(text[i]);
            if (character == 20 || character == 21 || character == 22)
                Scr_ParamError(paramNum, va("bad escape character (%i) present in string", character), SCRIPTINSTANCE_SERVER);
            if (I_isalpha(character))
            {
                if (loc_warnings->current.enabled)
                {
                    if (loc_warningsAsErrors->current.enabled)
                        Com_Error(ERR_LOCALIZATION, va("non-localized %s strings are not allowed to have letters in them: \"%s\"", "Objective String", text));
                    else
                        Com_PrintWarning(24, "WARNING: Non-localized %s string is not allowed to have letters in it. Must be changed over to a localized string: \"%s\"\n", "Objective String", text);
                    if (length + 14 >= 1024)
                        Scr_ParamError(paramNum, va("%s is too long. Max length is %i\n", "Objective String", 1024), SCRIPTINSTANCE_SERVER);
                    memcpy(message, "UNLOCALIZED: ", 13);
                    memcpy(message + 13, text, length + 1);
                    text = message;
                }
                break;
            }
        }
    }
    if (length >= 1024)
        Scr_Error(va("Objective strings is too long (> %i): %s\n", 1023, text), SCRIPTINSTANCE_SERVER);
    if (!Info_Validate(const_cast<char *>(text)) || strchr(text, '\\'))
        Scr_Error(va("Objective strings can not have a \", a ;, or a \\ in them. Illegal objective string: %s\n", text), SCRIPTINSTANCE_SERVER);
    for (int i = 0; i < length; ++i)
    {
        const unsigned char character = static_cast<unsigned char>(text[i]);
        if (character < 32 || character >= 127)
            Scr_Error(va("Illegal character '%c'(ascii %i) in objective string: %s\n",
                static_cast<signed char>(character), character, text), SCRIPTINSTANCE_SERVER);
    }
    return text;
}

static void G_AddObjective_SP(unsigned int firstParam, int teamNum)
{
    const unsigned int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (count < firstParam + 2)
        Scr_Error("objective_add needs at least the first two parameters out of its parameter list of: index state [string] [position]\n", SCRIPTINSTANCE_SERVER);
    const int index = Scr_GetInt(firstParam, SCRIPTINSTANCE_SERVER);
    if (static_cast<unsigned int>(index) >= 32)
        Scr_Error(va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
    char config[1025];
    char message[1024];
    SV_GetConfigstring(CS_SP_OBJECTIVES + index, config, 1024);
    const unsigned short stateName = Scr_GetConstString(firstParam + 1, SCRIPTINSTANCE_SERVER);
    objectiveState_t state;
    if (!ObjectiveStateIndexFromString(&state, stateName))
        Scr_Error(va("Illegal objective state \"%s\". Valid states are \"empty\", \"active\", \"invisible\", \"done\", \"current\", \"failed\"\n",
            Scr_GetString(firstParam + 1, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);
    Info_SetValueForKey(config, "state", va("%i", state));
    objective_t *obj = &level.objectives[index];
    ClearObjective_OnEntity(obj);
    obj->state = state;
    obj->alt_3D_text = NULL;
    obj->use3D = 0;
    const char *text = "GAME_OBJECTIVESUPDATED";
    if (count > firstParam + 2)
    {
        text = G_GetObjectiveString_SP(firstParam + 2, message);
        Info_SetValueForKey(config, "str", text);
        if (count > firstParam + 3)
        {
            const unsigned int positionParam = firstParam + 3;
            if (Scr_GetType(positionParam, SCRIPTINSTANCE_SERVER) == 4)
            {
                float position[3];
                Scr_GetVector(positionParam, position, SCRIPTINSTANCE_SERVER);
                int coordinates[3];
                for (int i = 0; i < 3; ++i)
                    coordinates[i] = position[i] >= -2147483648.0f && position[i] < 2147483648.0f ? (int)position[i] : INT_MIN;
                Info_SetValueForKey(config, "org0", va("%i %i %i", coordinates[0], coordinates[1], coordinates[2]));
                Info_RemoveKey(config, "ent0");
                Vec3Copy(position, obj->origin);
            }
            else
            {
                gentity_s *entity = Scr_GetEntity(positionParam);
                Info_SetValueForKey(config, "ent0", va("%i", entity->s.number));
                Info_RemoveKey(config, "org0");
                obj->entNum = entity->s.number;
            }
            if (count > firstParam + 4)
            {
                G_SetObjectiveIconConfig_SP(config, firstParam + 4);
                SetObjectiveIcon(obj, firstParam + 4);
            }
        }
    }
    if (teamNum == -1)
    {
        SV_SetConfigstring(CS_SP_OBJECTIVES + index, config);
        SV_GameSendServerCommand(-1, SV_CMD_CAN_IGNORE, va("%c \"%i\"", 'J', 3));
        G_SendObjectiveNotification_SP(stateName, text, -1);
        obj->teamNum = 0;
    }
    else
    {
        Info_SetValueForKey(config, "t", va("%i", teamNum));
        obj->teamNum = teamNum;
        SV_SetConfigstring(CS_SP_OBJECTIVES + index, config);
        for (int clientNum = 0; clientNum < com_maxclients->current.integer; ++clientNum)
        {
            gentity_s *entity = &g_entities[clientNum];
            if (svs.clients[clientNum].header.state >= CS_CONNECTED && entity->client && entity->client->sess.cs.team == teamNum)
            {
                SV_GameSendServerCommand(clientNum, SV_CMD_CAN_IGNORE, va("%c \"%i\"", 'J', 3));
                G_SendObjectiveNotification_SP(stateName, text, clientNum);
            }
        }
    }
}

static void Scr_Objective_AddTeam_SP()
{
    const unsigned short teamName = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    const int teamNum = teamName == scr_const.allies ? 2 : teamName == scr_const.axis ? 1 : -1;
    G_AddObjective_SP(1, teamNum);
}
#endif

#ifdef KISAK_SP
static void Scr_Objective_String_SP()
{
    const int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (static_cast<unsigned int>(index) >= 32)
        Scr_Error(va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
    char config[1025];
    char message[1024];
    SV_GetConfigstring(CS_SP_OBJECTIVES + index, config, 1024);
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    Scr_ConstructMessageString(1, count - 1, "Objective String", message, sizeof(message));
    Scr_ValidateObjectiveString_SP(message);
    Info_SetValueForKey(config, "str", message);
    SV_SetConfigstring(CS_SP_OBJECTIVES + index, config);
    const char *value = Info_ValueForKey(config, "state");
    const int state = value[0] ? atoi(value) : 0;
    if (static_cast<unsigned int>(state) >= 6)
        return;
    const unsigned short states[] = {
        scr_const.empty, scr_const.active, scr_const.invisible,
        scr_const.done, scr_const.current, scr_const.failed
    };
    const unsigned short stateName = states[state];
    if (stateName != scr_const.done)
        G_SendObjectiveNotification_SP(stateName, message, -1);
}
#endif

void Scr_Objective_State()
{
#ifdef KISAK_SP
    int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ((unsigned int)index >= 32)
        Scr_Error(va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
    char config[1024];
    SV_GetConfigstring(CS_SP_OBJECTIVES + index, config, sizeof(config));
    unsigned short stateName = Scr_GetConstString(1, SCRIPTINSTANCE_SERVER);
    objectiveState_t state;
    if (!ObjectiveStateIndexFromString(&state, stateName))
    {
        Scr_Error(va("Illegal objective state \"%s\". Valid states are \"empty\", \"active\", \"invisible\", \"done\", \"current\", \"failed\"\n",
            Scr_GetString(1, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);
    }
    Info_SetValueForKey(config, "state", va("%i", state));
    SV_SetConfigstring(CS_SP_OBJECTIVES + index, config);
    objective_t *obj = &level.objectives[index];
    obj->state = state;
    if (state == OBJST_EMPTY || state == OBJST_INVISIBLE)
        ClearObjective_OnEntity(obj);
    G_SendObjectiveNotification_SP(stateName, "GAME_OBJECTIVESUPDATED", -1);
#else
    const char *v0; // eax
    char *String; // eax
    const char *v2; // eax
    objectiveState_t state; // [esp+0h] [ebp-10h] BYREF
    objective_t *obj; // [esp+4h] [ebp-Ch]
    unsigned __int16 stateName; // [esp+8h] [ebp-8h]
    int objNum; // [esp+Ch] [ebp-4h]

    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objNum >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    obj = &level.objectives[objNum];
    stateName = (unsigned __int16)Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    if ( !ObjectiveStateIndexFromString(&state, stateName) )
    {
        String = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        v2 = va(
                     "Illegal objective state \"%s\". Valid states are \"empty\", \"invisible\", \"current\", \"active\"\n",
                     String);
        Scr_ParamError(1u, v2, SCRIPTINSTANCE_SERVER);
    }
    obj->state = state;
    if ( state == OBJST_EMPTY || state == OBJST_INVISIBLE )
        ClearObjective_OnEntity(obj);
#endif
}

void Scr_Objective_Icon()
{
#ifdef KISAK_SP
    const int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (static_cast<unsigned int>(index) >= 32)
        Scr_ParamError(0, va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
    char config[1024];
    SV_GetConfigstring(CS_SP_OBJECTIVES + index, config, sizeof(config));
    G_SetObjectiveIconConfig_SP(config, 1);
    SV_SetConfigstring(CS_SP_OBJECTIVES + index, config);
    SetObjectiveIcon(&level.objectives[index], 1);
#else
    const char *v0; // eax
    int objNum; // [esp+0h] [ebp-4h]

    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objNum >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    SetObjectiveIcon(&level.objectives[objNum], 1u);
#endif
}

#ifdef KISAK_SP
static void Scr_Objective_AdditionalPosition_SP()
{
    const int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (static_cast<unsigned int>(index) >= 32)
        Scr_Error(va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
    const int positionIndex = Scr_GetInt(1, SCRIPTINSTANCE_SERVER);
    if (static_cast<unsigned int>(positionIndex) >= 8)
        Scr_Error(va("index %i is an illegal position index. Valid indexes are 0 to %i\n", positionIndex, 7), SCRIPTINSTANCE_SERVER);
    char config[1025];
    char key[8];
    SV_GetConfigstring(CS_SP_OBJECTIVES + index, config, 1024);
    if (Scr_GetType(2, SCRIPTINSTANCE_SERVER) == VAR_VECTOR)
    {
        float position[3];
        Scr_GetVector(2, position, SCRIPTINSTANCE_SERVER);
        int coordinates[3];
        for (int i = 0; i < 3; ++i)
            coordinates[i] = position[i] >= -2147483648.0f && position[i] < 2147483648.0f ? (int)position[i] : INT_MIN;
        Com_sprintf(key, sizeof(key), "org%d", positionIndex);
        Info_SetValueForKey(config, key, va("%i %i %i", coordinates[0], coordinates[1], coordinates[2]));
        Com_sprintf(key, sizeof(key), "ent%d", positionIndex);
    }
    else
    {
        gentity_s *ent = Scr_GetEntity(2);
        Com_sprintf(key, sizeof(key), "ent%d", positionIndex);
        Info_SetValueForKey(config, key, va("%i", ent->s.number));
        Com_sprintf(key, sizeof(key), "org%d", positionIndex);
    }
    Info_RemoveKey(config, key);
    SV_SetConfigstring(CS_SP_OBJECTIVES + index, config);
}
#endif

void Scr_Objective_Position()
{
#ifdef KISAK_SP
    const int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (static_cast<unsigned int>(index) >= 32)
        Scr_Error(va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
    char config[1025];
    SV_GetConfigstring(CS_SP_OBJECTIVES + index, config, 1024);
    const bool vectorPosition = Scr_GetType(1, SCRIPTINSTANCE_SERVER) == 4;
    int coordinates[3];
    if (vectorPosition)
    {
        float position[3];
        Scr_GetVector(1, position, SCRIPTINSTANCE_SERVER);
        for (int i = 0; i < 3; ++i)
            coordinates[i] = position[i] >= -2147483648.0f && position[i] < 2147483648.0f ? (int)position[i] : INT_MIN;
        Info_SetValueForKey(config, "org0", va("%i %i %i", coordinates[0], coordinates[1], coordinates[2]));
        Info_RemoveKey(config, "ent0");
    }
    else
    {
        gentity_s *entity = Scr_GetEntity(1);
        Info_SetValueForKey(config, "ent0", va("%i", entity->s.number));
        Info_RemoveKey(config, "org0");
    }
    SV_SetConfigstring(CS_SP_OBJECTIVES + index, config);
    objective_t *obj = &level.objectives[index];
    ClearObjective_OnEntity(obj);
    if (vectorPosition)
    {
        for (int i = 0; i < 3; ++i)
            obj->origin[i] = (float)coordinates[i];
    }
#else
    const char *v0; // eax
    objective_t *obj; // [esp+0h] [ebp-8h]
    int objNum; // [esp+4h] [ebp-4h]

    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objNum >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    obj = &level.objectives[objNum];
    ClearObjective_OnEntity(obj);
    Scr_GetVector(1u, obj->origin, SCRIPTINSTANCE_SERVER);
    obj->origin[0] = (float)(int)obj->origin[0];
    obj->origin[1] = (float)(int)obj->origin[1];
    obj->origin[2] = (float)(int)obj->origin[2];
#endif
}

void Scr_Objective_OnEntity()
{
    const char *v0; // eax
    gentity_s *Entity; // eax
    int objNum; // [esp+8h] [ebp-4h]

    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objNum >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    ClearObjective_OnEntity(&level.objectives[objNum]);
    Entity = Scr_GetEntity(1u);
    Entity->r.svFlags |= 0x10u;
    level.objectives[objNum].entNum = Entity->s.number;   // nx-port: was through objectivesClientMask at x86 offsets
}

#ifdef KISAK_SP
static void G_SetCurrentObjectives_SP()
{
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    int makeCurrent[32];
    memset(makeCurrent, 0, sizeof(makeCurrent));
    for (int i = 0; i < count; ++i)
    {
        const int index = Scr_GetInt(i, SCRIPTINSTANCE_SERVER);
        if (static_cast<unsigned int>(index) >= 32)
            Scr_ParamError(i, va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
        makeCurrent[index] = 1;
    }
    for (int i = 0; i < 32; ++i)
    {
        if (makeCurrent[i])
            level.objectives[i].state = OBJST_CURRENT;
        else if (level.objectives[i].state == OBJST_CURRENT)
            level.objectives[i].state = OBJST_ACTIVE;
    }
}
#endif

void Scr_Objective_Current()
{
#ifdef KISAK_SP
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    int makeCurrent[32];
    memset(makeCurrent, 0, sizeof(makeCurrent));
    for (int i = 0; i < count; ++i)
    {
        const int index = Scr_GetInt(i, SCRIPTINSTANCE_SERVER);
        if (static_cast<unsigned int>(index) >= 32)
            Scr_Error(va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
        makeCurrent[index] = 1;
    }
    char config[1025];
    for (int i = 0; i < 32; ++i)
    {
        SV_GetConfigstring(CS_SP_OBJECTIVES + i, config, 1024);
        const char *value = Info_ValueForKey(config, "state");
        const int state = value[0] ? atoi(value) : 0;
        if (makeCurrent[i] ? state != OBJST_CURRENT : state == OBJST_CURRENT)
        {
            Info_SetValueForKey(config, "state", va("%i", makeCurrent[i] ? OBJST_CURRENT : OBJST_ACTIVE));
            SV_SetConfigstring(CS_SP_OBJECTIVES + i, config);
        }
    }
    G_SetCurrentObjectives_SP();
    G_SendObjectiveNotification_SP(scr_const.current, "GAME_OBJECTIVESUPDATED", -1);
#else
    const char *v0;
    objective_t *obj;
    int numParam;
    int makeCurrent[32];
    int i;
    int objNum;

    numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    memset(makeCurrent, 0, sizeof(makeCurrent));
    for ( i = 0; i < numParam; ++i )
    {
        objNum = Scr_GetInt(i, SCRIPTINSTANCE_SERVER);
        if ( (unsigned int)objNum >= 0x20 )
        {
            v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
            Scr_ParamError(i, v0, SCRIPTINSTANCE_SERVER);
        }
        makeCurrent[objNum] = 1;
    }
    for ( objNum = 0; objNum < 32; ++objNum )
    {
        obj = &level.objectives[objNum];
        if ( makeCurrent[objNum] )
        {
            obj->state = OBJST_CURRENT;
        }
        else if ( obj->state == OBJST_CURRENT )
        {
            obj->state = OBJST_ACTIVE;
        }
    }
#endif
}

#ifdef KISAK_SP
static void Scr_Objective_AdditionalCurrent_SP()
{
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    int makeCurrent[32];
    memset(makeCurrent, 0, sizeof(makeCurrent));
    for (int i = 0; i < count; ++i)
    {
        const int index = Scr_GetInt(i, SCRIPTINSTANCE_SERVER);
        if (static_cast<unsigned int>(index) >= 32)
            Scr_Error(va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", index, 31), SCRIPTINSTANCE_SERVER);
        makeCurrent[index] = 1;
    }
    char config[1025];
    for (int i = 0; i < 32; ++i)
    {
        SV_GetConfigstring(CS_SP_OBJECTIVES + i, config, 1024);
        const char *value = Info_ValueForKey(config, "state");
        const int state = value[0] ? atoi(value) : 0;
        if (makeCurrent[i] && state != OBJST_CURRENT)
        {
            Info_SetValueForKey(config, "state", va("%i", OBJST_CURRENT));
            SV_SetConfigstring(CS_SP_OBJECTIVES + i, config);
        }
    }
    G_SendObjectiveNotification_SP(scr_const.current, "GAME_OBJECTIVESUPDATED", -1);
}
#endif

void Scr_Objective_SetVisibleToPlayer()
{
    const char *v0; // eax
    int objNum; // [esp+0h] [ebp-Ch]
    gentity_s *player; // [esp+8h] [ebp-4h]

    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objNum >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    player = Scr_GetEntity(1u);
    if ( !player->client
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    5293,
                    0,
                    "%s",
                    "player->client") )
    {
        __debugbreak();
    }
    level.objectivesClientMask[objNum][player->s.number >> 5] &= ~(1 << (player->s.number & 0x1F));
}

void Scr_Objective_SetInvisibleToPlayer()
{
    const char *v0; // eax
    int objNum; // [esp+0h] [ebp-Ch]
    gentity_s *player; // [esp+8h] [ebp-4h]

    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objNum >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    player = Scr_GetEntity(1u);
    if ( !player->client
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    5320,
                    0,
                    "%s",
                    "player->client") )
    {
        __debugbreak();
    }
    level.objectivesClientMask[objNum][player->s.number >> 5] |= 1 << (player->s.number & 0x1F);
}

void  Scr_Objective_SetVisibleToAll()
{
    const char *v1; // eax
    int objNum; // [esp+0h] [ebp-4h]

    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ((unsigned int)objNum >= 0x20)
    {
        v1 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
    level.objectivesClientMask[objNum][0] = 0;
    level.objectivesClientMask[objNum][1] = 0;
}

void Scr_Objective_SetInvisibleToAll()
{
    char *v1; // eax
    int objNum; // [esp+0h] [ebp-4h]

    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ((unsigned int)objNum >= 0x20)
    {
        v1 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
    level.objectivesClientMask[objNum][0] = -1;
    level.objectivesClientMask[objNum][1] = -1;
}

void Scr_Objective_SetSize()
{
    const char *v0; // eax
    float Float; // [esp+4h] [ebp-20h]
    float v2; // [esp+8h] [ebp-1Ch]
    float height; // [esp+14h] [ebp-10h]
    gentity_s *ent; // [esp+18h] [ebp-Ch]
    objective_t *objective; // [esp+1Ch] [ebp-8h]
    int objectiveIndex; // [esp+20h] [ebp-4h]

    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 2 )
        Scr_Error("Objective_Size() called with wrong params.\n", 0);
    objectiveIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objectiveIndex >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objectiveIndex, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    objective = &level.objectives[objectiveIndex];
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        ent = Scr_GetEntity(1u);
        if ( ent )
        {
            height = ent->r.maxs[1] - ent->r.mins[1];
            objective->size[0] = ent->r.maxs[0] - ent->r.mins[0];
            objective->size[1] = height;
        }
        else
        {
            Scr_ParamError(1u, "Illegal entity parameter for Objective_SetSize.", SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        Float = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
        v2 = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
        objective->size[0] = Float;
        objective->size[1] = v2;
    }
}

void Scr_Objective_SetColor()
{
    const char *v0; // eax
    int objectiveIndex; // [esp+80h] [ebp-14h]
    float color[4]; // [esp+84h] [ebp-10h] BYREF

    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 4 )
        Scr_Error("Objective_Size() called with wrong params.\n", 0);
    objectiveIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objectiveIndex >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objectiveIndex, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    color[0] = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    color[1] = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    color[2] = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    color[3] = 1.0f;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 5 )
        color[3] = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
    Byte4PackRgba(color, (unsigned __int8 *)&level.objectives[objectiveIndex].color);   // nx-port: was through objectivesClientMask at x86 offsets
}

void GScr_Objective_Team()
{
    const char *v0; // eax
    char *v1; // eax
    const char *v2; // eax
    objective_t *obj; // [esp+0h] [ebp-Ch]
    unsigned __int16 team; // [esp+4h] [ebp-8h]
    int objNum; // [esp+8h] [ebp-4h]

    objNum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)objNum >= 0x20 )
    {
        v0 = va("index %i is an illegal objective index. Valid indexes are 0 to %i\n", objNum, 31);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    obj = &level.objectives[objNum];
    team = (unsigned __int16)Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    if ( team == scr_const.allies )
    {
        obj->teamNum = 2;
    }
    else if ( team == scr_const.axis )
    {
        obj->teamNum = 1;
    }
    else if ( team == scr_const.none )
    {
        obj->teamNum = 0;
    }
    else
    {
        v1 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v2 = va("Illegal team string '%s'. Must be allies, axis, or none.", v1);
        Scr_ParamError(1u, v2, SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GetNormalised2DMapPosition(float *inPos, float *outPos)
{
    float v2; // [esp+0h] [ebp-20h]
    float v3; // [esp+4h] [ebp-1Ch]
    float v4; // [esp+8h] [ebp-18h]
    float v5; // [esp+Ch] [ebp-14h]
    float v6; // [esp+14h] [ebp-Ch]
    float posDelta; // [esp+18h] [ebp-8h]
    float posDelta_4; // [esp+1Ch] [ebp-4h]

    *outPos = *inPos;
    outPos[1] = inPos[1];
    posDelta = *outPos - level.compassMapUpperLeft[0];
    posDelta_4 = outPos[1] - level.compassMapUpperLeft[1];
    *outPos = (float)(level.compassNorth[1] * posDelta) - (float)(level.compassNorth[0] * posDelta_4);
    outPos[1] = (float)((-(level.compassNorth[1])) * posDelta_4)
                        - (float)(level.compassNorth[0] * posDelta);
    if ( level.compassMapWorldSize[0] != 0.0 && level.compassMapWorldSize[1] != 0.0 )
    {
        *outPos = *outPos / level.compassMapWorldSize[0];
        outPos[1] = outPos[1] / level.compassMapWorldSize[1];
    }
    if ( (float)(*outPos - 1.0) < 0.0 )
        v6 = *outPos;
    else
        v6 = 1.0f;
    if ( (float)(0.0 - *outPos) < 0.0 )
        v3 = v6;
    else
        v3 = 0.0f;
    *outPos = v3;
    v4 = outPos[1];
    if ( (float)(v4 - 1.0) < 0.0 )
        v5 = outPos[1];
    else
        v5 = 1.0f;
    if ( (float)(0.0 - v4) < 0.0 )
        v2 = v5;
    else
        v2 = 0.0f;
    outPos[1] = v2;
}

void __cdecl SetArtilleryIconLocation()
{
    char *v0; // eax
    const char *v1; // eax
    int activeBits; // [esp+0h] [ebp-48h]
    playerState_s *ps; // [esp+4h] [ebp-44h]
    gentity_s *player; // [esp+8h] [ebp-40h]
    int i; // [esp+Ch] [ebp-3Ch]
    float outPos[2]; // [esp+10h] [ebp-38h] BYREF
    unsigned __int16 team; // [esp+18h] [ebp-30h]
    int yPos; // [esp+1Ch] [ebp-2Ch]
    float vPos[3]; // [esp+20h] [ebp-28h] BYREF
    int xPos; // [esp+2Ch] [ebp-1Ch]
    int teamNum; // [esp+30h] [ebp-18h]
    int isActive; // [esp+34h] [ebp-14h]
    int isMortar; // [esp+38h] [ebp-10h]
    int artilleryIconLocation; // [esp+3Ch] [ebp-Ch]
    int numParams; // [esp+40h] [ebp-8h]
    int clientNum; // [esp+44h] [ebp-4h]

    numParams = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( numParams < 2 || numParams > 5 )
        Scr_Error("Incorrect number of parameters to artilleryiconlocation\n", 0);
    isMortar = 0;
    clientNum = -1;
    if ( numParams > 4 )
        clientNum = Scr_GetInt(4u, SCRIPTINSTANCE_SERVER);
    if ( numParams > 3 )
        isMortar = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
    isActive = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    teamNum = 0;
    artilleryIconLocation = 0;
    if ( isActive )
    {
        Scr_GetVector(0, vPos, SCRIPTINSTANCE_SERVER);
        vPos[0] = (float)(int)vPos[0];
        vPos[1] = (float)(int)vPos[1];
        vPos[2] = (float)(int)vPos[2];
        GetNormalised2DMapPosition(vPos, outPos);
        team = (unsigned __int16)Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
        if ( team == scr_const.allies )
        {
            teamNum = 2;
        }
        else if ( team == scr_const.axis )
        {
            teamNum = 1;
        }
        else if ( team == scr_const.none )
        {
            teamNum = 0;
        }
        else
        {
            v0 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
            v1 = va("Illegal team string '%s'. Must be allies, axis, or none.", v0);
            Scr_ParamError(1u, v1, SCRIPTINSTANCE_SERVER);
        }
        xPos = (unsigned __int8)(int)(float)(outPos[0] * 255.0);
        yPos = (unsigned __int8)(int)(float)(outPos[1] * 255.0);
        artilleryIconLocation = xPos | (yPos << 8);
    }
    for ( i = 0; i < com_maxclients->current.integer; ++i )
    {
        player = &g_entities[i];
        if ( player->client )
        {
            ps = &player->client->ps;
            if ( isActive )
            {
                if ( (teamNum || clientNum == ps->clientNum) && player->client->sess.cs.team == teamNum )
                    activeBits = 0x10000;
                else
                    activeBits = 0x20000;
                if ( isMortar )
                    artilleryIconLocation |= 0x40000u;
                ps->artilleryInboundIconLocation = activeBits | artilleryIconLocation;
            }
            else
            {
                ps->artilleryInboundIconLocation &= 0xFFF8FFFF;
            }
        }
    }
}

void GScr_LogPrint()
{
    unsigned int v0; // [esp+0h] [ebp-428h]
    int iStringLen; // [esp+10h] [ebp-418h]
    char string[1024]; // [esp+18h] [ebp-410h] BYREF
    int iNumParms; // [esp+41Ch] [ebp-Ch]
    int i; // [esp+420h] [ebp-8h]
    const char *pszToken; // [esp+424h] [ebp-4h]

    string[0] = 0;
    iStringLen = 0;
    iNumParms = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    for ( i = 0; i < iNumParms; ++i )
    {
        pszToken = Scr_GetString(i, SCRIPTINSTANCE_SERVER);
        v0 = strlen(pszToken);
        if ( (int)(v0 + iStringLen) >= 1024 )
            break;
        I_strncat(string, 1024, pszToken);
        iStringLen += v0;
    }
    G_LogPrintf(string);
}

void GScr_WorldEntNumber()
{
    Scr_AddInt(1022, SCRIPTINSTANCE_SERVER);
}

void GScr_Obituary()
{
    gentity_s *pOtherEnt; // [esp+0h] [ebp-18h]
    char *pszWeapon; // [esp+4h] [ebp-14h]
    const WeaponDef *weapondef; // [esp+8h] [ebp-10h]
    unsigned int iWeaponNum; // [esp+Ch] [ebp-Ch]
    gentity_s *pEnt; // [esp+10h] [ebp-8h]
    int iMODNum; // [esp+14h] [ebp-4h]

    pszWeapon = Scr_GetString(2u, SCRIPTINSTANCE_SERVER);
    iWeaponNum = G_GetWeaponIndexForName(pszWeapon);
    weapondef = BG_GetWeaponDef(iWeaponNum);
    iMODNum = G_MeansOfDeathFromScriptParam(3u);
    pOtherEnt = Scr_GetEntity(0);
    pEnt = G_TempEntity(vec3_origin, EV_OBITUARY);
    pEnt->s.otherEntityNum = pOtherEnt->s.number;
    if (Scr_GetType(1u, SCRIPTINSTANCE_SERVER) == 1 && Scr_GetPointerType(1u, SCRIPTINSTANCE_SERVER) == 19)
        pEnt->s.attackerEntityNum = Scr_GetEntity(1u)->s.number;
    else
        pEnt->s.attackerEntityNum = 1022;
    pEnt->r.svFlags = 8;
    if (iMODNum == 16 && weapondef->impactType != IMPACT_TYPE_BLADE
        || iMODNum == 7
        || iMODNum == 9
        || iMODNum == 13
        || iMODNum == 12
        || iMODNum == 10)
    {
        pEnt->s.eventParm = iMODNum;
        pEnt->s.weaponModel = 0;
    }
    else
    {
        pEnt->s.eventParm = iWeaponNum;
        pEnt->s.weaponModel = 1;
    }
}

void __cdecl GScr_ReviveObituary()
{
    const gentity_s *player_entity; // [esp+0h] [ebp-14h]
    int entity_index; // [esp+4h] [ebp-10h]
    gentity_s *temporary_entity; // [esp+8h] [ebp-Ch]
    unsigned __int16 client_index; // [esp+Ch] [ebp-8h]
    int client_indexa; // [esp+Ch] [ebp-8h]
    gentity_s *victim_entity; // [esp+10h] [ebp-4h]

    victim_entity = Scr_GetEntity(0);
    if ( victim_entity->s.eType == 1 )
    {
        temporary_entity = G_TempEntity(vec3_origin, EV_REVIVE_OBITUARY);
        client_index = victim_entity->client->ps.clientNum;
        temporary_entity->r.clientMask[0] = -1;
        temporary_entity->s.eventParms[0] = client_index;
        for ( entity_index = 0; entity_index < level.num_entities; ++entity_index )
        {
            player_entity = &g_entities[entity_index];
            if ( player_entity->client )
            {
                if ( player_entity->client->sess.cs.team == victim_entity->client->sess.cs.team )
                {
                    client_indexa = player_entity->client->ps.clientNum;
                    temporary_entity->r.clientMask[client_indexa >> 5] &= ~(1 << (client_indexa & 0x1F));
                }
            }
        }
    }
    else
    {
        Scr_Error("Can only call ReviveObituary on player entities", 0);
    }
}

void GScr_LerpFloat()
{
    float v0; // [esp+8h] [ebp-18h]
    float v1; // [esp+Ch] [ebp-14h]
    float from; // [esp+10h] [ebp-10h]
    float time; // [esp+14h] [ebp-Ch]
    float to; // [esp+18h] [ebp-8h]

    from = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    to = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    time = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    if ( (float)(time - 1.0) < 0.0 )
        v1 = time;
    else
        v1 = 1.0f;
    if ( (float)(0.0 - time) < 0.0 )
        v0 = v1;
    else
        v0 = 0.0f;
    Scr_AddFloat((float)((float)(to - from) * v0) + from, SCRIPTINSTANCE_SERVER);
}

void GScr_LerpVector()
{
    float v0; // [esp+0h] [ebp-30h]
    float v1; // [esp+4h] [ebp-2Ch]
    float from[3]; // [esp+8h] [ebp-28h] BYREF
    float time; // [esp+14h] [ebp-1Ch]
    float to[3]; // [esp+18h] [ebp-18h] BYREF
    float retVal[3]; // [esp+24h] [ebp-Ch] BYREF

    Scr_GetVector(0, from, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, to, SCRIPTINSTANCE_SERVER);
    time = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    if ( (float)(time - 1.0) < 0.0 )
        v1 = time;
    else
        v1 = 1.0f;
    if ( (float)(0.0 - time) < 0.0 )
        v0 = v1;
    else
        v0 = 0.0f;
    time = v0;
    retVal[0] = (float)((float)(to[0] - from[0]) * v0) + from[0];
    retVal[1] = (float)((float)(to[1] - from[1]) * v0) + from[1];
    retVal[2] = (float)((float)(to[2] - from[2]) * v0) + from[2];
    Scr_AddVector(retVal, SCRIPTINSTANCE_SERVER);
}

void GScr_AddDemoBookmark()
{
    int time; // [esp+0h] [ebp-10h]
    int clientNum1; // [esp+4h] [ebp-Ch]
    VariableUnion clientNum2; // [esp+8h] [ebp-8h]
    int type; // [esp+Ch] [ebp-4h]

    if ( Demo_IsEnabled() && Demo_IsRecording() )
    {
        clientNum2.intValue = 255;
        type = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        time = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        clientNum1 = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
        if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 4 )
            clientNum2.intValue = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
        if ( clientNum2.intValue > 32 )
            clientNum2.intValue = 255;
        Demo_AddBookmark(type, time, clientNum1, clientNum2.intValue);
    }
}

void __cdecl Scr_UpdateSpawnPoints()
{
    unsigned __int16 callback; // [esp+0h] [ebp-4h]

#ifdef KISAK_SP
    // Consumer half of the maps/mp/gametypes/_spawning guard in GScr_LoadScripts: on SP the
    // handle is always 0 because the script does not ship, so do not hand 0 to Scr_ExecThread.
    if ( !g_scr_data.updatespawnpoints )
        return;
#endif
    callback = Scr_ExecThread(SCRIPTINSTANCE_SERVER, g_scr_data.updatespawnpoints, 0);
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void GScr_positionWouldTelefrag()
{
    float mins[3]; // [esp+0h] [ebp-24h] BYREF
    float maxs[3]; // [esp+Ch] [ebp-18h] BYREF
    float vPos[3]; // [esp+18h] [ebp-Ch] BYREF

    Scr_GetVector(0, vPos, SCRIPTINSTANCE_SERVER);
    mins[0] = vPos[0] + -15.0;
    mins[1] = vPos[1] + -15.0;
    mins[2] = vPos[2] + 0.0;
    maxs[0] = vPos[0] + 15.0;
    maxs[1] = vPos[1] + 15.0;
    maxs[2] = vPos[2] + 70.0;
    Scr_BoundsWouldTelefrag(mins, maxs);
}

void __cdecl Scr_BoundsWouldTelefrag(float *mins, float *maxs)
{
    int pm_type; // [esp+0h] [ebp-101Ch]
    int entityList[1025]; // [esp+Ch] [ebp-1010h] BYREF
    gentity_s *v4; // [esp+1010h] [ebp-Ch]
    int v5; // [esp+1014h] [ebp-8h]
    int i; // [esp+1018h] [ebp-4h]

    entityList[1024] = 0x2800000;
    v5 = CM_AreaEntities(mins, maxs, entityList, 1024, 0x2800000);
    for ( i = 0; i < v5; ++i )
    {
        v4 = &g_entities[entityList[i]];
        if ( v4->client )
            pm_type = v4->client->ps.pm_type;
        else
            pm_type = 9;
        if ( pm_type < 9 || v4->s.eType == 14 )
        {
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
            return;
        }
    }
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}

void GScr_BoundsWouldTelefrag()
{
    float mins[3]; // [esp+0h] [ebp-18h] BYREF
    float maxs[3]; // [esp+Ch] [ebp-Ch] BYREF

    Scr_GetVector(0, mins, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, maxs, SCRIPTINSTANCE_SERVER);
    Scr_BoundsWouldTelefrag(mins, maxs);
}

int __cdecl GScr_ReadTeamForSpawnPoints(unsigned int index)
{
    char *String; // eax
    const char *v2; // eax
    unsigned __int16 teamName; // [esp+0h] [ebp-8h]

    teamName = (unsigned __int16)Scr_GetConstString(index, SCRIPTINSTANCE_SERVER);
    if ( teamName == scr_const.allies )
        return 2;
    if ( teamName == scr_const.axis )
        return 1;
    if ( teamName != scr_const.free )
    {
        String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        v2 = va("team \"%s\" should be \"allies\", \"axis\", or \"free\"", String);
        Scr_ParamError(0, v2, SCRIPTINSTANCE_SERVER);
    }
    return 0;
}

void GScr_RecordUsedSpawnPoint()
{
    float origin[3]; // [esp+0h] [ebp-14h] BYREF
    int point_team; // [esp+Ch] [ebp-8h]
    gentity_s *player; // [esp+10h] [ebp-4h]

    player = Scr_GetEntity(0);
    point_team = GScr_ReadTeamForSpawnPoints(1u);
    Scr_GetVector(2u, origin, SCRIPTINSTANCE_SERVER);
    //BLOPS_NULLSUB();
}

void GScr_getStartTime()
{
    Scr_AddInt(level.startTime, SCRIPTINSTANCE_SERVER);
}

void GScr_PrecacheMenu()
{
    const char *v0; // eax
    char *pszNewMenu; // [esp+0h] [ebp-410h]
    int iConfigNum; // [esp+4h] [ebp-40Ch]
    int iConfigNuma; // [esp+4h] [ebp-40Ch]
    char szConfigString[1028]; // [esp+8h] [ebp-408h] BYREF

    pszNewMenu = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    for ( iConfigNum = 0; iConfigNum < 32; ++iConfigNum )
    {
        SV_GetConfigstring(iConfigNum + 2548, szConfigString, 1024);
        if ( !I_stricmp(szConfigString, pszNewMenu) )
        {
            Com_DPrintf(24, "Script tried to precache the menu '%s' more than once\n", pszNewMenu);
            return;
        }
    }
    for ( iConfigNuma = 0; iConfigNuma < 32; ++iConfigNuma )
    {
        SV_GetConfigstring(iConfigNuma + 2548, szConfigString, 1024);
        if ( !szConfigString[0] )
            break;
    }
    if ( iConfigNuma == 32 )
    {
        v0 = va("Too many menus precached. Max allowed menus is %i", 32);
        Scr_Error(v0, 0);
    }
    SV_SetConfigstring(iConfigNuma + 2548, pszNewMenu);
}

int __cdecl GScr_GetScriptMenuIndex(const char *pszMenu)
{
    const char *v2; // eax
    int iConfigNum; // [esp+0h] [ebp-40Ch]
    char szConfigString[1028]; // [esp+4h] [ebp-408h] BYREF

    for ( iConfigNum = 0; iConfigNum < 32; ++iConfigNum )
    {
        SV_GetConfigstring(iConfigNum + 2548, szConfigString, 1024);
        if ( !I_stricmp(szConfigString, pszMenu) )
            return iConfigNum;
    }
    v2 = va("Menu '%s' was not precached\n", pszMenu);
    Scr_Error(v2, 0);
    return 0;
}

void GScr_PrecacheStatusIcon()
{
    const char *v0; // eax
    char *pszNewIcon; // [esp+0h] [ebp-410h]
    int iConfigNum; // [esp+4h] [ebp-40Ch]
    int iConfigNuma; // [esp+4h] [ebp-40Ch]
    char szConfigString[1028]; // [esp+8h] [ebp-408h] BYREF

    pszNewIcon = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    for ( iConfigNum = 0; iConfigNum < 8; ++iConfigNum )
    {
        SV_GetConfigstring(iConfigNum + 3092, szConfigString, 1024);
        if ( !I_stricmp(szConfigString, pszNewIcon) )
        {
            Com_DPrintf(24, "Script tried to precache the player status icon '%s' more than once\n", pszNewIcon);
            return;
        }
    }
    for ( iConfigNuma = 0; iConfigNuma < 8; ++iConfigNuma )
    {
        SV_GetConfigstring(iConfigNuma + 3092, szConfigString, 1024);
        if ( !szConfigString[0] )
            break;
    }
    if ( iConfigNuma == 8 )
    {
        v0 = va("Too many player status icons precached. Max allowed is %i", 8);
        Scr_Error(v0, 0);
    }
    SV_SetConfigstring(iConfigNuma + 3092, pszNewIcon);
}

int __cdecl GScr_GetStatusIconIndex(const char *pszIcon)
{
    const char *v2; // eax
    int iConfigNum; // [esp+0h] [ebp-40Ch]
    char szConfigString[1028]; // [esp+4h] [ebp-408h] BYREF

    if ( !*pszIcon )
        return 0;
    for ( iConfigNum = 0; iConfigNum < 8; ++iConfigNum )
    {
        SV_GetConfigstring(iConfigNum + 3092, szConfigString, 1024);
        if ( !I_stricmp(szConfigString, pszIcon) )
            return iConfigNum + 1;
    }
    v2 = va("Status icon '%s' was not precached\n", pszIcon);
    Scr_Error(v2, 0);
    return 0;
}

void GScr_PrecacheHeadIcon()
{
    const char *v0; // eax
    char *pszNewIcon; // [esp+0h] [ebp-410h]
    int iConfigNum; // [esp+4h] [ebp-40Ch]
    int iConfigNuma; // [esp+4h] [ebp-40Ch]
    char szConfigString[1028]; // [esp+8h] [ebp-408h] BYREF

    pszNewIcon = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    for ( iConfigNum = 0; iConfigNum < 15; ++iConfigNum )
    {
        SV_GetConfigstring(iConfigNum + 3100, szConfigString, 1024);
        if ( !I_stricmp(szConfigString, pszNewIcon) )
        {
            Com_DPrintf(24, "Script tried to precache the player head icon '%s' more than once\n", pszNewIcon);
            return;
        }
    }
    for ( iConfigNuma = 0; iConfigNuma < 15; ++iConfigNuma )
    {
        SV_GetConfigstring(iConfigNuma + 3100, szConfigString, 1024);
        if ( !szConfigString[0] )
            break;
    }
    if ( iConfigNuma == 15 )
    {
        v0 = va("Too many player head icons precached. Max allowed is %i", 15);
        Scr_Error(v0, 0);
    }
    SV_SetConfigstring(iConfigNuma + 3100, pszNewIcon);
}

int __cdecl GScr_GetHeadIconIndex(const char *pszIcon)
{
    const char *v2; // eax
    int iConfigNum; // [esp+0h] [ebp-40Ch]
    char szConfigString[1028]; // [esp+4h] [ebp-408h] BYREF

    if ( !*pszIcon )
        return 0;
    for ( iConfigNum = 0; iConfigNum < 15; ++iConfigNum )
    {
        SV_GetConfigstring(iConfigNum + 3100, szConfigString, 1024);
        if ( !I_stricmp(szConfigString, pszIcon) )
            return iConfigNum + 1;
    }
    v2 = va("Head icon '%s' was not precached\n", pszIcon);
    Scr_Error(v2, 0);
    return 0;
}

void GScr_WeaponClipSize()
{
    int ClipSize; // eax
    char *weaponName; // [esp+18h] [ebp-8h]
    unsigned int weaponIndex; // [esp+1Ch] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    ClipSize = BG_GetClipSize(weaponIndex);
    Scr_AddInt(ClipSize, SCRIPTINSTANCE_SERVER);
}

void GScr_WeaponIsSemiAuto()
{
    char *weaponName; // [esp+0h] [ebp-Ch]
    unsigned int weaponIndex; // [esp+4h] [ebp-8h]
    const WeaponDef *weapDef; // [esp+8h] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    weapDef = BG_GetWeaponDef(weaponIndex);
    Scr_AddInt(weapDef->fireType == WEAPON_FIRETYPE_SINGLESHOT, SCRIPTINSTANCE_SERVER);
}

void GScr_WeaponIsBoltAction()
{
    char *weaponName; // [esp+0h] [ebp-Ch]
    unsigned int weaponIndex; // [esp+4h] [ebp-8h]
    const WeaponDef *weapDef; // [esp+8h] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    weapDef = BG_GetWeaponDef(weaponIndex);
    Scr_AddInt(weapDef->bBoltAction, SCRIPTINSTANCE_SERVER);
}

void GScr_WeaponType()
{
    char *WeaponTypeName; // eax
    char *weaponName; // [esp+0h] [ebp-Ch]
    unsigned int weaponIndex; // [esp+4h] [ebp-8h]
    const WeaponDef *weapDef; // [esp+8h] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    weapDef = BG_GetWeaponDef(weaponIndex);
    WeaponTypeName = (char *)BG_GetWeaponTypeName(weapDef->weapType);
    Scr_AddString(WeaponTypeName, SCRIPTINSTANCE_SERVER);
}

void GScr_WeaponClass()
{
    char *WeaponClassName; // eax
    char *weaponName; // [esp+0h] [ebp-Ch]
    unsigned int weaponIndex; // [esp+4h] [ebp-8h]
    const WeaponDef *weapDef; // [esp+8h] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    if ( weaponIndex )
    {
        weapDef = BG_GetWeaponDef(weaponIndex);
        WeaponClassName = (char *)BG_GetWeaponClassName(weapDef->weapClass);
        Scr_AddString(WeaponClassName, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddString("none", SCRIPTINSTANCE_SERVER);
    }
}

void GScr_WeaponIsMountable()
{
    char *weaponName; // [esp+0h] [ebp-Ch]
    unsigned int weaponIndex; // [esp+4h] [ebp-8h]
    const WeaponDef *weapDef; // [esp+8h] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    weapDef = BG_GetWeaponDef(weaponIndex);
    Scr_AddInt(weapDef->mountableWeapon, SCRIPTINSTANCE_SERVER);
}

void GScr_WeaponInventoryType()
{
    char *WeaponInventoryTypeName; // eax
    char *weaponName; // [esp+0h] [ebp-Ch]
    unsigned int weaponIndex; // [esp+4h] [ebp-8h]
    const WeaponDef *weapDef; // [esp+8h] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    weapDef = BG_GetWeaponDef(weaponIndex);
    WeaponInventoryTypeName = (char *)BG_GetWeaponInventoryTypeName(weapDef->inventoryType);
    Scr_AddString(WeaponInventoryTypeName, SCRIPTINSTANCE_SERVER);
}

void GScr_WeaponStartAmmo()
{
    int StartAmmo; // eax
    char *weaponName; // [esp+1Ch] [ebp-8h]
    unsigned int weaponIndex; // [esp+20h] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    StartAmmo = BG_GetStartAmmo(weaponIndex);
    Scr_AddInt(StartAmmo, SCRIPTINSTANCE_SERVER);
}

void GScr_WeaponMaxAmmo()
{
    int MaxAmmo; // eax
    char *weaponName; // [esp+1Ch] [ebp-8h]
    unsigned int weaponIndex; // [esp+20h] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    MaxAmmo = BG_GetMaxAmmo(weaponIndex);
    Scr_AddInt(MaxAmmo, SCRIPTINSTANCE_SERVER);
}

void GScr_WeaponAltWeaponName()
{
    char *v0; // eax
    char *weaponName; // [esp+0h] [ebp-10h]
    int altWeaponIndex; // [esp+4h] [ebp-Ch]
    unsigned int weaponIndex; // [esp+Ch] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    altWeaponIndex = BG_GetWeaponVariantDef(weaponIndex)->altWeaponIndex;
    if ( altWeaponIndex )
    {
        v0 = (char *)BG_WeaponName(altWeaponIndex);
        Scr_AddString(v0, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddConstString(scr_const.none, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_GetWatcherWeapons()
{
    char *v0; // eax
    unsigned int i; // [esp+0h] [ebp-4h]

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for ( i = 0; i < bg_lastParsedWeaponIndex; ++i )
    {
        if ( BG_GetWeaponDef(i)->bDieOnRespawn )
        {
            v0 = (char *)BG_WeaponName(i);
            Scr_AddString(v0, SCRIPTINSTANCE_SERVER);
            Scr_AddArray(SCRIPTINSTANCE_SERVER);
        }
    }
}

void GScr_GetRetrievableWeapons()
{
    char *v0; // eax
    unsigned int i; // [esp+0h] [ebp-4h]

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for ( i = 0; i < bg_lastParsedWeaponIndex; ++i )
    {
        if ( BG_GetWeaponDef(i)->bRetrievable )
        {
            v0 = (char *)BG_WeaponName(i);
            Scr_AddString(v0, SCRIPTINSTANCE_SERVER);
            Scr_AddArray(SCRIPTINSTANCE_SERVER);
        }
    }
}

void GScr_GetWeaponIndexFromName()
{
    char *weaponName; // [esp+0h] [ebp-8h]
    int weaponIndex; // [esp+4h] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_AddInt(weaponIndex, SCRIPTINSTANCE_SERVER);
}

void GScr_GetWeaponFireSound()
{
    char *weaponFireSound; // [esp+0h] [ebp-8h]
    int weaponIndex; // [esp+4h] [ebp-4h]

    weaponFireSound = (char *)"";
    weaponIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( weaponIndex && weaponIndex != -1 )
        weaponFireSound = (char *)BG_GetWeaponDef(weaponIndex)->fireSound;
    Scr_AddString(weaponFireSound, SCRIPTINSTANCE_SERVER);
}

void GScr_GetWeaponFireSoundPlayer()
{
    int weaponIndex; // [esp+0h] [ebp-8h]
    char *weaponFireSoundPlayer; // [esp+4h] [ebp-4h]

    weaponFireSoundPlayer = (char *)"";
    weaponIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( weaponIndex && weaponIndex != -1 )
        weaponFireSoundPlayer = (char *)BG_GetWeaponDef(weaponIndex)->fireSoundPlayer;
    Scr_AddString(weaponFireSoundPlayer, SCRIPTINSTANCE_SERVER);
}

void GScr_GetWeaponPickupSound()
{
    int weaponIndex; // [esp+0h] [ebp-8h]
    char *weaponPickupSound; // [esp+4h] [ebp-4h]

    weaponPickupSound = (char *)"";
    weaponIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( weaponIndex && weaponIndex != -1 )
        weaponPickupSound = (char *)BG_GetWeaponDef(weaponIndex)->pickupSound;
    Scr_AddString(weaponPickupSound, SCRIPTINSTANCE_SERVER);
}

void GScr_GetWeaponPickupSoundPlayer()
{
    int weaponIndex; // [esp+0h] [ebp-8h]
    char *weaponPickupSoundPlayer; // [esp+4h] [ebp-4h]

    weaponPickupSoundPlayer = (char *)"";
    weaponIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( weaponIndex && weaponIndex != -1 )
        weaponPickupSoundPlayer = (char *)BG_GetWeaponDef(weaponIndex)->pickupSoundPlayer;
    Scr_AddString(weaponPickupSoundPlayer, SCRIPTINSTANCE_SERVER);
}

void GScr_IsTurretFiring()
{
    char *v0; // eax
    const char *v1; // eax
    gentity_s *ent; // [esp+0h] [ebp-8h]
    bool firing; // [esp+7h] [ebp-1h]

    ent = Scr_GetEntity(0);
    if ( !ent->pTurretInfo )
    {
        v0 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v1 = va("entity type '%s' is not a turret", v0);
        Scr_Error(v1, 0);
    }
    if ( (ent->s.lerp.eFlags & 0x40) != 0 )
    {
        firing = 1;
    }
    else if ( ent->pTurretInfo->state )
    {
        firing = 1;
    }
    else
    {
        firing = (ent->pTurretInfo->flags & 4) != 0;
    }
    Scr_AddBool(firing, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetDefaultDropPitch(scr_entref_t entref)
{
    float pitch; // [esp+Ch] [ebp-8h]
    gentity_s *ent; // [esp+10h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        ent = GetEntity(entref);
        if ( ent->pTurretInfo )
        {
            pitch = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
            turret_SetDefaultDropPitch(ent, pitch);
        }
        else
        {
            Scr_Error("entity is not a turret", 0);
        }
    }
    else
    {
        Scr_Error("illegal call to setdefaultdroppitch()\n", 0);
    }
}

void __cdecl GScr_SetScanningPitch(scr_entref_t entref)
{
    float pitch; // [esp+Ch] [ebp-8h]
    gentity_s *ent; // [esp+10h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        ent = GetEntity(entref);
        if ( ent->pTurretInfo )
        {
            pitch = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
            turret_SetScanningPitch(ent, pitch);
        }
        else
        {
            Scr_Error("entity is not a turret", 0);
        }
    }
    else
    {
        Scr_Error("illegal call to setdefaultdroppitch()\n", 0);
    }
}

void GScr_WeaponFireTime()
{
    float value; // xmm0_4
    unsigned int iWeaponIndex; // [esp+Ch] [ebp-8h]
    char *pszWeaponName; // [esp+10h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName(pszWeaponName);
    if ( iWeaponIndex )
    {
        value = (float)BG_GetWeaponDef(iWeaponIndex)->iFireTime * 0.001;
        Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddFloat(0.0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_WeaponReloadTime()
{
    float value; // xmm0_4
    char *weaponName; // [esp+Ch] [ebp-8h]
    unsigned int weaponIndex; // [esp+10h] [ebp-4h]

    weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    if ( weaponIndex )
    {
        value = (float)BG_GetWeaponDef(weaponIndex)->iReloadAddTime * 0.001;
        Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddFloat(0.0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_IsWeaponClipOnly()
{
    bool IsClipOnly; // eax
    char *weapName; // [esp+0h] [ebp-8h]
    unsigned int weapIdx; // [esp+4h] [ebp-4h]

    weapName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weapIdx = G_GetWeaponIndexForName(weapName);
    if ( weapIdx )
    {
        IsClipOnly = BG_WeaponIsClipOnly(weapIdx);
        Scr_AddBool(IsClipOnly, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddBool(0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_IsWeaponDetonationTimed()
{
    char *weapName; // [esp+0h] [ebp-Ch]
    unsigned int weapIdx; // [esp+4h] [ebp-8h]
    const WeaponDef *weapDef; // [esp+8h] [ebp-4h]

    weapName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weapIdx = G_GetWeaponIndexForName(weapName);
    if ( weapIdx )
    {
        weapDef = BG_GetWeaponDef(weapIdx);
        Scr_AddBool(weapDef->timedDetonation, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddBool(0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_PrecacheLocationSelector()
{
    const char *v0; // eax
    int iConfigNum; // [esp+0h] [ebp-40Ch]
    int iConfigNuma; // [esp+0h] [ebp-40Ch]
    char szConfigString[1024]; // [esp+4h] [ebp-408h] BYREF
    const char *pszNewMtl; // [esp+408h] [ebp-4h]

    pszNewMtl = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    for ( iConfigNum = 0; iConfigNum < 15; ++iConfigNum )
    {
        SV_GetConfigstring(iConfigNum + 1553, szConfigString, 1024);
        if ( !I_stricmp(szConfigString, pszNewMtl) )
        {
            Com_DPrintf(24, "Script tried to precache the location selector '%s' more than once\n", pszNewMtl);
            return;
        }
    }
    for ( iConfigNuma = 0; iConfigNuma < 15; ++iConfigNuma )
    {
        SV_GetConfigstring(iConfigNuma + 1553, szConfigString, 1024);
        if ( !szConfigString[0] )
            break;
    }
    if ( iConfigNuma == 15 )
    {
        v0 = va("Too many location selectors precached. Max allowed is %i", 15);
        Scr_Error(v0, 0);
    }
    SV_SetConfigstring(iConfigNuma + 1553, (char *)pszNewMtl);
}

int __cdecl GScr_GetLocSelIndex(const char *mtlName)
{
    const char *v2; // eax
    int iConfigNum; // [esp+0h] [ebp-40Ch]
    char szConfigString[1028]; // [esp+4h] [ebp-408h] BYREF

    if ( !mtlName
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 6760, 0, "%s", "mtlName") )
    {
        __debugbreak();
    }
    if ( !*mtlName )
        return 0;
    for ( iConfigNum = 0; iConfigNum < 15; ++iConfigNum )
    {
        SV_GetConfigstring(iConfigNum + 1553, szConfigString, 1024);
        if ( !I_stricmp(szConfigString, mtlName) )
            return iConfigNum + 1;
    }
    v2 = va("Location selector '%s' was not precached\n", mtlName);
    Scr_Error(v2, 0);
    return 0;
}

void Scr_BulletTrace()
{
    char *value; // eax
    unsigned intresult; // eax
    float vNorm[3]; // [esp+1Ch] [ebp-90h] BYREF
    int bIgnoreWater; // [esp+28h] [ebp-84h]
    float vEnd[3]; // [esp+2Ch] [ebp-80h] BYREF
    int bIgnoreGlass; // [esp+38h] [ebp-74h]
    gentity_s *pIgnoreEnt; // [esp+3Ch] [ebp-70h]
    int iClipMask; // [esp+40h] [ebp-6Ch]
    trace_t trace; // [esp+48h] [ebp-64h] BYREF
    float endpos[3]; // [esp+84h] [ebp-28h] BYREF
    int iIgnoreEntNum; // [esp+94h] [ebp-18h]
    int iSurfaceTypeIndex; // [esp+98h] [ebp-14h]
    float vStart[3]; // [esp+9Ch] [ebp-10h] BYREF
    unsigned __int16 hitEntId; // [esp+A8h] [ebp-4h]

    PROF_SCOPED("Scr_BulletTrace");

    pIgnoreEnt = 0;
    iIgnoreEntNum = 1023;
#ifdef KISAK_SP
    iClipMask = 0x280E833;
#else
    iClipMask = 0x280E033;
#endif
    Scr_GetVector(0, vStart, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, vEnd, SCRIPTINSTANCE_SERVER);
    if ( !Scr_GetInt(2u, SCRIPTINSTANCE_SERVER) )
#ifdef KISAK_SP
        iClipMask = 0x802833;
#else
        iClipMask &= 0xFDFF7FFF;
#endif
    if ( Scr_GetType(3u, SCRIPTINSTANCE_SERVER) == 1 && Scr_GetPointerType(3u, SCRIPTINSTANCE_SERVER) == 19 )
    {
        pIgnoreEnt = Scr_GetEntity(3u);
        iIgnoreEntNum = pIgnoreEnt->s.number;
    }
    bIgnoreWater = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 5 )
        bIgnoreWater = Scr_GetInt(4u, SCRIPTINSTANCE_SERVER);
    if ( bIgnoreWater )
        iClipMask &= ~0x20u;
    bIgnoreGlass = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 6 )
        bIgnoreGlass = Scr_GetInt(5u, SCRIPTINSTANCE_SERVER);
    if ( bIgnoreGlass )
        iClipMask &= ~0x10u;
    G_LocationalTrace(&trace, vStart, vEnd, iIgnoreEntNum, iClipMask, 0, 0);
    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    Scr_AddFloat(trace.fraction, SCRIPTINSTANCE_SERVER);
    Scr_AddArrayStringIndexed(scr_const.fraction, SCRIPTINSTANCE_SERVER);
    Vec3Lerp(vStart, vEnd, trace.fraction, endpos);
    Scr_AddVector(endpos, SCRIPTINSTANCE_SERVER);
    Scr_AddArrayStringIndexed(scr_const.position, SCRIPTINSTANCE_SERVER);
    hitEntId = Trace_GetEntityHitId(&trace);
    if ( hitEntId == 1023 || hitEntId == 1022 )
        Scr_AddUndefined(SCRIPTINSTANCE_SERVER);
    else
        Scr_AddEntity(&g_entities[hitEntId], SCRIPTINSTANCE_SERVER);
    Scr_AddArrayStringIndexed(scr_const.entity, SCRIPTINSTANCE_SERVER);
    if ( trace.fraction >= 1.0 )
    {
        vNorm[0] = vEnd[0] - vStart[0];
        vNorm[1] = vEnd[1] - vStart[1];
        vNorm[2] = vEnd[2] - vStart[2];
        Vec3Normalize(vNorm);
        Scr_AddVector(vNorm, SCRIPTINSTANCE_SERVER);
        Scr_AddArrayStringIndexed(scr_const.normal, SCRIPTINSTANCE_SERVER);
        Scr_AddConstString(scr_const.none, SCRIPTINSTANCE_SERVER);
        Scr_AddArrayStringIndexed(scr_const.surfacetype, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddVector(trace.normal.vec.v, SCRIPTINSTANCE_SERVER);
        Scr_AddArrayStringIndexed(scr_const.normal, SCRIPTINSTANCE_SERVER);
        iSurfaceTypeIndex = (unsigned __int8)((int)(0x3F00000 & trace.sflags) >> 20);
        value = (char *)Com_SurfaceTypeToName(iSurfaceTypeIndex);
        Scr_AddString(value, SCRIPTINSTANCE_SERVER);
        Scr_AddArrayStringIndexed(scr_const.surfacetype, SCRIPTINSTANCE_SERVER);
    }
}

#ifdef KISAK_SP
// Retail SP server-function table entry 0x00B766D8 pairs "groundtrace" with
// handler 0x00807C00. Its argument parsing and result struct are identical to
// Scr_BulletTrace, but the two content masks are SP-specific: 0x0280ECB3 when
// parameter 2 is true and 0x00802CB3 when false.
static void __cdecl Scr_GroundTrace_SP()
{
    float start[3];
    float end[3];
    float endpos[3];
    float normal[3];
    trace_t trace = {};
    int ignoreEntNum = ENTITYNUM_NONE;
    int clipMask;

    Scr_GetVector(0, start, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1, end, SCRIPTINSTANCE_SERVER);
    clipMask = Scr_GetInt(2, SCRIPTINSTANCE_SERVER) ? 0x0280ECB3 : 0x00802CB3;

    if ( Scr_GetType(3, SCRIPTINSTANCE_SERVER) == VAR_POINTER
        && Scr_GetPointerType(3, SCRIPTINSTANCE_SERVER) == VAR_ENTITY )
    {
        ignoreEntNum = Scr_GetEntity(3)->s.number;
    }

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 5 && Scr_GetInt(4, SCRIPTINSTANCE_SERVER) )
        clipMask &= ~0x20;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 6 && Scr_GetInt(5, SCRIPTINSTANCE_SERVER) )
        clipMask &= ~0x10;

    G_LocationalTrace(&trace, start, end, ignoreEntNum, clipMask, 0, 0);

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    Scr_AddFloat(trace.fraction, SCRIPTINSTANCE_SERVER);
    Scr_AddArrayStringIndexed(scr_const.fraction, SCRIPTINSTANCE_SERVER);

    Vec3Lerp(start, end, trace.fraction, endpos);
    Scr_AddVector(endpos, SCRIPTINSTANCE_SERVER);
    Scr_AddArrayStringIndexed(scr_const.position, SCRIPTINSTANCE_SERVER);

    const unsigned __int16 hitEntId = Trace_GetEntityHitId(&trace);
    if ( hitEntId == ENTITYNUM_NONE || hitEntId == ENTITYNUM_WORLD )
        Scr_AddUndefined(SCRIPTINSTANCE_SERVER);
    else
        Scr_AddEntity(&g_entities[hitEntId], SCRIPTINSTANCE_SERVER);
    Scr_AddArrayStringIndexed(scr_const.entity, SCRIPTINSTANCE_SERVER);

    if ( trace.fraction < 1.0f )
    {
        Scr_AddVector(trace.normal.vec.v, SCRIPTINSTANCE_SERVER);
        Scr_AddArrayStringIndexed(scr_const.normal, SCRIPTINSTANCE_SERVER);
        const int surfaceTypeIndex = static_cast<unsigned char>((trace.sflags & 0x3F00000) >> 20);
        Scr_AddString((char *)Com_SurfaceTypeToName(surfaceTypeIndex), SCRIPTINSTANCE_SERVER);
    }
    else
    {
        normal[0] = end[0] - start[0];
        normal[1] = end[1] - start[1];
        normal[2] = end[2] - start[2];
        Vec3Normalize(normal);
        Scr_AddVector(normal, SCRIPTINSTANCE_SERVER);
        Scr_AddArrayStringIndexed(scr_const.normal, SCRIPTINSTANCE_SERVER);
        Scr_AddConstString(scr_const.none, SCRIPTINSTANCE_SERVER);
    }
    Scr_AddArrayStringIndexed(scr_const.surfacetype, SCRIPTINSTANCE_SERVER);
}

// Local implementation name only: the retail C identifier is unattested.
// Retail SP's server-function record at 0x00B76B1C pairs "ropesetflag" with
// handler 0x007FDD90.  Its body independently proves the complete command
// contract: exactly three arguments, six accepted flag strings, booleanized
// argument 2, and broadcast rope command '# S <id> <mask> <onoff>'.
static void __cdecl Scr_CreateRope_SP()
{
    int argc = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (argc < 3)
        Scr_Error("Incorrect number of parameters", false);
    float start[3], end[3];
    Scr_GetVector(0, start, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1, end, SCRIPTINSTANCE_SERVER);
    float length = Scr_GetFloat(2, SCRIPTINSTANCE_SERVER);
    if (length < 0.0f)
        Scr_ParamError(1, "length is negative", SCRIPTINSTANCE_SERVER);
    if (g_totalRopeCount >= 300)
    {
        Scr_AddInt(g_totalRopeCount, SCRIPTINSTANCE_SERVER);
        return;
    }
    int entity_index = ENTITYNUM_NONE;
    unsigned int bone_name = 0;
    int health = 0;
    if (argc > 3)
    {
        gentity_s *entity = Scr_GetEntity(3);
        entity_index = entity->s.number;
        if (argc > 4)
            bone_name = Scr_GetConstLowercaseString(4, SCRIPTINSTANCE_SERVER);
        int i;
        for (i = 0; i < g_entsWithRopesCount; ++i)
        {
            if (g_entsWithRopes[i] == entity->s.number)
                break;
        }
        if (i == g_entsWithRopesCount)
        {
            if (g_entsWithRopesCount < 64)
                g_entsWithRopes[g_entsWithRopesCount++] = entity->s.number;
            else
                Com_Error(ERR_DROP, "Too many entity rope to entity anchors.\n");
        }
    }
    if (argc > 5)
        health = Scr_GetInt(5, SCRIPTINSTANCE_SERVER);
    if (argc > 6)
    {
        Scr_GetEntity(6);
        if (argc > 7)
            Scr_GetConstLowercaseString(7, SCRIPTINSTANCE_SERVER);
    }
    const int integer_length = length >= -2147483648.0f && length < 2147483648.0f ? (int)length : INT_MIN;
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE,
        va("%c %d %d %d %1.1f %1.1f %1.1f %1.1f %1.1f %1.1f %d %d %d",
            0x23, 0x4e, g_totalRopeCount, integer_length,
            start[0], start[1], start[2], end[0], end[1], end[2], entity_index, bone_name, health));
    Scr_AddInt(g_totalRopeCount, SCRIPTINSTANCE_SERVER);
    ++g_totalRopeCount;
}

static void __cdecl Scr_DeleteRope_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1)
        Scr_Error("Incorrect number of parameters.", false);
    int rope_index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (rope_index < 0)
        Scr_ParamError(1, "Bad rope index", SCRIPTINSTANCE_SERVER);
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c %d %d", 0x23, 0x51, rope_index));
}

static void __cdecl Scr_RopeSetParam_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3)
        Scr_Error("Incorrect number of parameters", false);
    int rope_index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    const char *parameter = Scr_GetString(1, SCRIPTINSTANCE_SERVER);
    if (!I_stricmp(parameter, "width"))
    {
        float width = Scr_GetFloat(2, SCRIPTINSTANCE_SERVER);
        SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c %d %d %d %1.1f", 0x23, 0x54, rope_index, 1, width));
    }
}

static void __cdecl Scr_RopeAddEntityAnchor_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 4)
        Scr_Error("Incorrect number of parameters", false);
    int rope_index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    float fraction = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    if (fraction < 0.0f || fraction > 1.0f)
        Scr_Error("The fraction must be between 0 and 1", false);
    gentity_s *entity = Scr_GetEntity(2);
    if (!entity || !entity->r.inuse)
        Scr_Error("Bad entity specified", false);
    float position[3];
    Scr_GetVector(3, position, SCRIPTINSTANCE_SERVER);
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE,
        va("%c %d %d %1.1f, %d, %1.1f, %1.1f, %1.1f", 0x23, 0x55, rope_index,
            fraction, entity->s.number, position[0], position[1], position[2]));
}

static void __cdecl Scr_RopeAddWorldAnchor_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3)
        Scr_Error("Incorrect number of parameters", false);
    int rope_index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    float fraction = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    if (fraction < 0.0f || fraction > 1.0f)
        Scr_Error("The fraction must be between 0 and 1", false);
    float position[3];
    Scr_GetVector(2, position, SCRIPTINSTANCE_SERVER);
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE,
        va("%c %d %d %1.1f, %d, %1.1f, %1.1f, %1.1f", 0x23, 0x55, rope_index,
            fraction, -1, position[0], position[1], position[2]));
}

static void __cdecl Scr_RopeRemoveAnchor_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2)
        Scr_Error("Incorrect number of parameters.", false);
    int rope_index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    float fraction = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    if (fraction < 0.0f || fraction > 1.0f)
        Scr_Error("The fraction must be between 0 and 1", false);
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c %d %d %1.1f", 0x23, 0x56, rope_index, fraction));
}

static void __cdecl Scr_RopeMoveAnchor_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3)
        Scr_Error("Incorrect number of parameters", false);
    int rope_index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    float fraction = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    if (fraction < 0.0f || fraction > 1.0f)
        Scr_Error("The fraction must be between 0 and 1", false);
    float position[3];
    Scr_GetVector(2, position, SCRIPTINSTANCE_SERVER);
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE,
        va("%c %d %d %1.1f, %1.1f, %1.1f, %1.1f", 0x23, 0x57, rope_index,
            fraction, position[0], position[1], position[2]));
}

static void __cdecl Scr_RopeGetPosition_SP()
{
    if (onlinegame->current.enabled)
        Scr_Error("ropegetposition doesnt support online games.", false);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2)
        Scr_Error("Incorrect number of parameters", false);
    int rope_index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    float fraction = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    if (fraction < 0.0f || fraction > 1.0f)
        Scr_Error("The fraction must be between 0 and 1", false);
    float position[3];
    Rope_GetPosition(rope_index, fraction, position);
    Scr_AddVector(position, SCRIPTINSTANCE_SERVER);
}

static void __cdecl Scr_GetRope_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1)
        Scr_Error("Incorrect number of parameters.", false);
    unsigned int targetname = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(Rope_GetIndex(targetname), SCRIPTINSTANCE_SERVER);
}

static void __cdecl Scr_RopeSetFlag_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3 )
        Scr_Error("Incorrect number of parameters", false);

    const int ropeId = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    const char *flagName = Scr_GetString(1, SCRIPTINSTANCE_SERVER);
    int flag;

    if ( !I_stricmp(flagName, "keep_ent_anchors") )
        flag = 0x10;
    else if ( !I_stricmp(flagName, "collide") )
        flag = 0x01;
    else if ( !I_stricmp(flagName, "detach_opposite_anchor") )
        flag = 0x20;
    else if ( !I_stricmp(flagName, "force_update") )
        flag = 0x40;
    else if ( !I_stricmp(flagName, "no_wind") )
        flag = 0x80;
    else if ( !I_stricmp(flagName, "no_lod") )
        flag = 0x100;
    else
        return;

    const int onoff = Scr_GetInt(2, SCRIPTINSTANCE_SERVER) != 0;
    SV_GameSendServerCommand(
        -1,
        SV_CMD_RELIABLE,
        va("%c %d %d %d %d", 0x23, 0x53, ropeId, flag, onoff));
}
#endif

void Scr_BulletTracePassed()
{
    bool v0; // eax
    int hitnum; // [esp+0h] [ebp-54h] BYREF
    float vEnd[3]; // [esp+2Ch] [ebp-28h] BYREF
    gentity_s *pIgnoreEnt; // [esp+38h] [ebp-1Ch]
    int iClipMask; // [esp+3Ch] [ebp-18h]
    int iIgnoreEntNum; // [esp+44h] [ebp-10h]
    float vStart[3]; // [esp+48h] [ebp-Ch] BYREF

    pIgnoreEnt = 0;
    iIgnoreEntNum = 1023;
#ifdef KISAK_SP
    iClipMask = 0x280E833;
#else
    iClipMask = 0x280E033;
#endif
    Scr_GetVector(0, vStart, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, vEnd, SCRIPTINSTANCE_SERVER);
    if ( !Scr_GetInt(2u, SCRIPTINSTANCE_SERVER) )
#ifdef KISAK_SP
        iClipMask = 0x802833;
#else
        iClipMask &= 0xFDFF7FFF;
#endif
    if ( Scr_GetType(3u, SCRIPTINSTANCE_SERVER) == 1 && Scr_GetPointerType(3u, SCRIPTINSTANCE_SERVER) == 19 )
    {
        pIgnoreEnt = Scr_GetEntity(3u);
        iIgnoreEntNum = pIgnoreEnt->s.number;
    }

    //col_context_t::col_context_t(&context, iClipMask);
    col_context_t context(iClipMask); // [esp+4h] [ebp-50h] BYREF
    //col_context_t::init_locational(&context, iIgnoreEntNum);
    context.init_locational(iIgnoreEntNum);
    hitnum = -1;
    v0 = SV_SightTracePoint(&hitnum, vStart, vEnd, &context);
    Scr_AddBool(v0, SCRIPTINSTANCE_SERVER);
}

#ifdef KISAK_SP
void __cdecl Scr_SightTracePassed()
{
    float vStart[3];
    float vEnd[3];
    int iIgnoreEntNum = ENTITYNUM_NONE;
    int iClipMask = 0x280D803;
    int hitNum;

    Scr_GetVector(0u, vStart, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, vEnd, SCRIPTINSTANCE_SERVER);
    if ( !Scr_GetInt(2u, SCRIPTINSTANCE_SERVER) )
        iClipMask = 0x801803;
    if ( Scr_GetType(3u, SCRIPTINSTANCE_SERVER) == VAR_POINTER
        && Scr_GetPointerType(3u, SCRIPTINSTANCE_SERVER) == VAR_ENTITY )
    {
        iIgnoreEntNum = Scr_GetEntity(3u)->s.number;
    }

    col_context_t context(iClipMask);
    context.passEntityNum0 = iIgnoreEntNum;
    SV_SightTracePoint(&hitNum, vStart, vEnd, &context);
    if ( hitNum == 0
        && FX_GetClientVisibility(fx_serverVisClient, vStart, vEnd) < 0.2f )
    {
        hitNum = 1;
    }
    Scr_AddBool(hitNum == 0, SCRIPTINSTANCE_SERVER);
}
#else
void __cdecl Scr_SightTracePassed()
{
    col_context_t context; // [esp+0h] [ebp-54h] BYREF
    float vEnd[3]; // [esp+28h] [ebp-2Ch] BYREF
    gentity_s *pIgnoreEnt; // [esp+34h] [ebp-20h]
    int iClipMask; // [esp+38h] [ebp-1Ch]
    int iIgnoreEntNum; // [esp+40h] [ebp-14h]
    int hitNum; // [esp+44h] [ebp-10h] BYREF
    float vStart[3]; // [esp+48h] [ebp-Ch] BYREF

    pIgnoreEnt = 0;
    iIgnoreEntNum = 1023;
    iClipMask = 0x2809803;
    Scr_GetVector(0, vStart, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, vEnd, SCRIPTINSTANCE_SERVER);
    if ( !Scr_GetInt(2u, SCRIPTINSTANCE_SERVER) )
        iClipMask &= 0xFDFF7FFF;
    if ( Scr_GetType(3u, SCRIPTINSTANCE_SERVER) == 1 && Scr_GetPointerType(3u, SCRIPTINSTANCE_SERVER) == 19 )
    {
        pIgnoreEnt = Scr_GetEntity(3u);
        iIgnoreEntNum = pIgnoreEnt->s.number;
    }
    //col_context_t::col_context_t(&context, iClipMask);
    context.passEntityNum0 = iIgnoreEntNum;
    SV_SightTracePoint(&hitNum, vStart, vEnd, &context);
    Scr_AddBool(hitNum == 0, SCRIPTINSTANCE_SERVER);
}
#endif

#ifdef KISAK_SP
void Scr_PhysicsTrace()
{
    float start[3];
    float end[3];
    float mins[3] = { 0.0f, 0.0f, 0.0f };
    float maxs[3] = { 0.0f, 0.0f, 0.0f };
    float endpos[3];
    trace_t trace;
    int iIgnoreEntNum = ENTITYNUM_NONE;

    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
    case 5:
        if ( Scr_GetType(5u, SCRIPTINSTANCE_SERVER) == VAR_POINTER
            && Scr_GetPointerType(5u, SCRIPTINSTANCE_SERVER) == VAR_ENTITY )
        {
            iIgnoreEntNum = Scr_GetEntity(5u)->s.number;
        }
        [[fallthrough]];
    case 4:
        Scr_GetVector(3u, maxs, SCRIPTINSTANCE_SERVER);
        [[fallthrough]];
    case 3:
        Scr_GetVector(2u, mins, SCRIPTINSTANCE_SERVER);
        [[fallthrough]];
    case 2:
        Scr_GetVector(0u, start, SCRIPTINSTANCE_SERVER);
        Scr_GetVector(1u, end, SCRIPTINSTANCE_SERVER);
        break;
    default:
        Scr_Error("illegal call to PhysicsTrace()", 0);
        break;
    }

    col_context_t context;
    G_TraceCapsule(&trace, start, mins, maxs, end, iIgnoreEntNum, 0x820011, &context);
    Vec3Lerp(start, end, trace.fraction, endpos);
    Scr_AddVector(endpos, SCRIPTINSTANCE_SERVER);
}
#else
const int scriptMaskToPhysicsMask[3] = { 42003603, 529, 32 };

void Scr_PhysicsTrace()
{
    char *v0; // eax
    float vNorm[3]; // [esp+18h] [ebp-C8h] BYREF
    int i; // [esp+24h] [ebp-BCh]
    col_context_t context; // [esp+28h] [ebp-B8h] BYREF
    float mins[3]; // [esp+50h] [ebp-90h] BYREF
    float start[3]; // [esp+5Ch] [ebp-84h] BYREF
    float end[3]; // [esp+68h] [ebp-78h] BYREF
    int maskType; // [esp+74h] [ebp-6Ch]
    gentity_s *pIgnoreEnt; // [esp+78h] [ebp-68h]
    float endpos[3]; // [esp+7Ch] [ebp-64h] BYREF
    trace_t trace; // [esp+88h] [ebp-58h] BYREF
    float maxs[3]; // [esp+C4h] [ebp-1Ch] BYREF
    int mask; // [esp+D0h] [ebp-10h]
    int iIgnoreEntNum; // [esp+D4h] [ebp-Ch]
    int iSurfaceTypeIndex; // [esp+D8h] [ebp-8h]
    unsigned __int16 hitEntId; // [esp+DCh] [ebp-4h]

    maskType = 1;
    pIgnoreEnt = 0;
    iIgnoreEntNum = 1023;
    Scr_GetVector(0, start, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, end, SCRIPTINSTANCE_SERVER);
    memset(mins, 0, sizeof(mins));
    memset(maxs, 0, sizeof(maxs));
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 2:
            goto $LN11_33;
        case 3:
            goto $LN12_24;
        case 4:
            goto $LN14_18;
        case 5:
            goto $LN15_19;
        case 6:
            maskType = Scr_GetInt(5u, SCRIPTINSTANCE_SERVER);
$LN15_19:
            if ( Scr_GetType(4u, SCRIPTINSTANCE_SERVER) == 1 && Scr_GetPointerType(4u, SCRIPTINSTANCE_SERVER) == 19 )
            {
                pIgnoreEnt = Scr_GetEntity(4u);
                iIgnoreEntNum = pIgnoreEnt->s.number;
            }
$LN14_18:
            Scr_GetVector(3u, maxs, SCRIPTINSTANCE_SERVER);
$LN12_24:
            Scr_GetVector(2u, mins, SCRIPTINSTANCE_SERVER);
$LN11_33:
            Scr_GetVector(0, start, SCRIPTINSTANCE_SERVER);
            Scr_GetVector(1u, end, SCRIPTINSTANCE_SERVER);
            break;
        default:
            Scr_Error("illegal call to PhysicsTrace()", 0);
            break;
    }
    mask = 0;
    for ( i = 0; (unsigned int)i < 3; ++i )
    {
        if ( (maskType & (1 << i)) != 0 )
            mask |= scriptMaskToPhysicsMask[i];
    }
    //col_context_t::col_context_t(&context);
    G_TraceCapsule(&trace, start, mins, maxs, end, iIgnoreEntNum, mask, &context);
    Vec3Lerp(start, end, trace.fraction, endpos);
    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    Scr_AddFloat(trace.fraction, SCRIPTINSTANCE_SERVER);
    Scr_AddArrayStringIndexed(scr_const.fraction, SCRIPTINSTANCE_SERVER);
    Scr_AddVector(endpos, SCRIPTINSTANCE_SERVER);
    Scr_AddArrayStringIndexed(scr_const.position, SCRIPTINSTANCE_SERVER);
    hitEntId = Trace_GetEntityHitId(&trace);
    if ( hitEntId == 1023 || hitEntId == 1022 )
        Scr_AddUndefined(SCRIPTINSTANCE_SERVER);
    else
        Scr_AddEntity(&g_entities[hitEntId], SCRIPTINSTANCE_SERVER);
    Scr_AddArrayStringIndexed(scr_const.entity, SCRIPTINSTANCE_SERVER);
    if ( trace.fraction >= 1.0 )
    {
        vNorm[0] = end[0] - start[0];
        vNorm[1] = end[1] - start[1];
        vNorm[2] = end[2] - start[2];
        Vec3Normalize(vNorm);
        Scr_AddVector(vNorm, SCRIPTINSTANCE_SERVER);
        Scr_AddArrayStringIndexed(scr_const.normal, SCRIPTINSTANCE_SERVER);
        Scr_AddConstString(scr_const.none, SCRIPTINSTANCE_SERVER);
        Scr_AddArrayStringIndexed(scr_const.surfacetype, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddVector(trace.normal.vec.v, SCRIPTINSTANCE_SERVER);
        Scr_AddArrayStringIndexed(scr_const.normal, SCRIPTINSTANCE_SERVER);
        iSurfaceTypeIndex = (unsigned __int8)((int)(0x3F00000 & trace.sflags) >> 20);
        v0 = (char *)Com_SurfaceTypeToName(iSurfaceTypeIndex);
        Scr_AddString(v0, SCRIPTINSTANCE_SERVER);
        Scr_AddArrayStringIndexed(scr_const.surfacetype, SCRIPTINSTANCE_SERVER);
    }
}
#endif

// LWSS ADD
void Scr_PlayerBulletTrace()
{
    iassert(0); // KISAKTODO :)
}
// LWSS END

void Scr_PlayerPhysicsTrace()
{
    col_context_t context; // [esp+8h] [ebp-88h] BYREF
    float start[3]; // [esp+30h] [ebp-60h] BYREF
    float end[3]; // [esp+3Ch] [ebp-54h] BYREF
    float endpos[3]; // [esp+48h] [ebp-48h] BYREF
    trace_t trace; // [esp+54h] [ebp-3Ch] BYREF

    Scr_GetVector(0, start, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, end, SCRIPTINSTANCE_SERVER);
    //col_context_t::col_context_t(&context);
    G_TraceCapsule(&trace, start, playerMins, playerMaxs, end, 1023, 8519697, &context);
    Vec3Lerp(start, end, trace.fraction, endpos);
    Scr_AddVector(endpos, SCRIPTINSTANCE_SERVER);
}

void Scr_RandomInt()
{
    int v0; // eax
    int iMax; // [esp+0h] [ebp-4h]

    iMax = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( iMax > 0 )
    {
        v0 = G_irand(0, iMax);
        Scr_AddInt(v0, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Com_Printf(24, "RandomInt parm: %d    ", iMax);
        Scr_Error("RandomInt parm must be positive integer.\n", 0);
    }
}

void Scr_RandomFloat()
{
    float max; // [esp+4h] [ebp-Ch]
    float fMax; // [esp+Ch] [ebp-4h]

    fMax = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    max = G_flrand(0.0, fMax);
    Scr_AddFloat(max, SCRIPTINSTANCE_SERVER);
}

void Scr_RandomIntRange()
{
    int v0; // eax
    int iMax; // [esp+0h] [ebp-8h]
    int iMin; // [esp+4h] [ebp-4h]

    iMin = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    iMax = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    if ( iMax <= iMin )
    {
        Com_Printf(24, "RandomIntRange parms: %d %d ", iMin, iMax);
        Scr_Error("RandomIntRange range must be positive integer.\n", 0);
    }
    v0 = G_irand(iMin, iMax);
    Scr_AddInt(v0, SCRIPTINSTANCE_SERVER);
}

void Scr_RandomFloatRange()
{
    float max; // [esp+8h] [ebp-10h]
    float fMin; // [esp+10h] [ebp-8h]
    float fMax; // [esp+14h] [ebp-4h]

    fMin = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
    const double maxValue = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    fMax = static_cast<float>(maxValue);
    if (static_cast<double>(fMin) >= maxValue)
#else
    fMax = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    if ( fMin > fMax )
#endif
    {
        Com_Printf(24, "Scr_RandomFloatRange parms: %f %f ", fMin, fMax);
#ifdef KISAK_SP
        Scr_Error("Scr_RandomFloatRange's second parameter must be greater than the first.\n", 0);
#else
        Scr_Error("Scr_RandomFloatRange range must be positive float.\n", 0);
#endif
    }
    max = G_flrand(fMin, fMax);
    Scr_AddFloat(max, SCRIPTINSTANCE_SERVER);
}

void GScr_log()
{
#if 0
    double value; // xmm0_8
    long double v1; // [esp+4h] [ebp-8h]
    scriptInstance_t v2; // [esp+4h] [ebp-8h]

    *((float *)&v1 + 1) = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    LODWORD(v1) = 0;
    value = *((float *)&v1 + 1);
    __libm_sse2_log(v1);
    *(float *)&value = value;
    Scr_AddFloat(*(float *)&value, v2);
#endif

    float value = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    value = log(value);
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void GScr_sin()
{
#ifdef KISAK_SP
    const double degrees = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    const double radians = degrees * static_cast<double>(0.01745329238474369f);
    Scr_AddFloat(static_cast<float>(sin(radians)), SCRIPTINSTANCE_SERVER);
#else
#if 0
    double value; // xmm0_8
    long double v1; // [esp+4h] [ebp-8h]
    scriptInstance_t v2; // [esp+4h] [ebp-8h]

    *((float *)&v1 + 1) = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER) * 0.017453292;
    LODWORD(v1) = 0;
    value = *((float *)&v1 + 1);
    __libm_sse2_sin(v1);
    *(float *)&value = value;
    Scr_AddFloat(*(float *)&value, v2);
#endif

    float value = DEG2RAD(Scr_GetFloat(0, SCRIPTINSTANCE_SERVER));
    value = sin(value);
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
#endif
}

void GScr_cos()
{
#ifdef KISAK_SP
    const double degrees = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    const double radians = degrees * static_cast<double>(0.01745329238474369f);
    Scr_AddFloat(static_cast<float>(cos(radians)), SCRIPTINSTANCE_SERVER);
#else
#if 0
    double value; // xmm0_8
    long double v1; // [esp+4h] [ebp-8h]
    scriptInstance_t v2; // [esp+4h] [ebp-8h]

    *((float *)&v1 + 1) = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER) * 0.017453292;
    LODWORD(v1) = 0;
    value = *((float *)&v1 + 1);
    __libm_sse2_cos(v1);
    *(float *)&value = value;
    Scr_AddFloat(*(float *)&value, v2);
#endif

    float value = DEG2RAD(Scr_GetFloat(0, SCRIPTINSTANCE_SERVER));
    value = cos(value);
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
#endif
}

void GScr_tan()
{
    float v0; // [esp+10h] [ebp-Ch]
    float sinT; // [esp+14h] [ebp-8h]
    float cosT; // [esp+18h] [ebp-4h]

    v0 = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER) * 0.017453292;
    cosT = cos(v0);
    sinT = sin(v0);
    if ( cosT == 0.0 )
        Scr_Error("divide by 0", 0);
    Scr_AddFloat(sinT / cosT, SCRIPTINSTANCE_SERVER);
}

void GScr_asin()
{
#if 0
    const char *v0; // eax
    double v1; // xmm0_8
    long double v2; // [esp+8h] [ebp-8h]
    scriptInstance_t v3; // [esp+8h] [ebp-8h]

    *((float *)&v2 + 1) = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( *((float *)&v2 + 1) < -1.0 || *((float *)&v2 + 1) > 1.0 )
    {
        v0 = va("%g out of range", *((float *)&v2 + 1));
        Scr_Error(v0, 0);
    }
    LODWORD(v2) = 0;
    v1 = *((float *)&v2 + 1);
    __libm_sse2_asin(v2);
    *(float *)&v1 = v1;
    Scr_AddFloat(*(float *)&v1 * 57.295776, v3);
#endif

    float value = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    value = asin(value);
    Scr_AddFloat(RAD2DEG(value), SCRIPTINSTANCE_SERVER);
}

void GScr_acos()
{
#ifdef KISAK_SP
    const double value = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    const float argument = static_cast<float>(value);
    if (value < -1.0 || argument > 1.0f)
        Scr_Error(va("%g out of range", static_cast<double>(argument)), 0);

    volatile float radians = static_cast<float>(acos(static_cast<double>(argument)));
    Scr_AddFloat(radians * 57.2957763671875f, SCRIPTINSTANCE_SERVER);
#else
#if 0
    const char *v0; // eax
    double v1; // xmm0_8
    long double v2; // [esp+8h] [ebp-8h]
    scriptInstance_t v3; // [esp+8h] [ebp-8h]

    *((float *)&v2 + 1) = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( *((float *)&v2 + 1) < -1.0 || *((float *)&v2 + 1) > 1.0 )
    {
        v0 = va("%g out of range", *((float *)&v2 + 1));
        Scr_Error(v0, 0);
    }
    LODWORD(v2) = 0;
    v1 = *((float *)&v2 + 1);
    __libm_sse2_acos(v2);
    *(float *)&v1 = v1;
    Scr_AddFloat(*(float *)&v1 * 57.295776, v3);
#endif
    float value = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    value = acos(value);
    Scr_AddFloat(RAD2DEG(value), SCRIPTINSTANCE_SERVER);
#endif
}

void GScr_atan()
{
#if 0
    double v0; // xmm0_8
    long double v1; // [esp+4h] [ebp-8h]
    scriptInstance_t v2; // [esp+4h] [ebp-8h]

    *((float *)&v1 + 1) = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    LODWORD(v1) = 0;
    v0 = *((float *)&v1 + 1);
    __libm_sse2_atan(v1);
    *(float *)&v0 = v0;
    Scr_AddFloat(*(float *)&v0 * 57.295776, v2);
#endif
    float value = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    value = atan(value);
    Scr_AddFloat(RAD2DEG(value), SCRIPTINSTANCE_SERVER);
}

void GScr_abs()
{
    float Float; // [esp+8h] [ebp-4h]

    Float = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    Scr_AddFloat(fabs(Float), SCRIPTINSTANCE_SERVER);
}

void GScr_min()
{
    float value; // [esp+8h] [ebp-Ch]
    float Float; // [esp+Ch] [ebp-8h]
    float v2; // [esp+10h] [ebp-4h]

    Float = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    v2 = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    if ( (float)(v2 - Float) < 0.0 )
        value = v2;
    else
        value = Float;
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void GScr_max()
{
    float value; // [esp+8h] [ebp-Ch]
    float Float; // [esp+Ch] [ebp-8h]
    float v2; // [esp+10h] [ebp-4h]

    Float = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    v2 = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    if ( (float)(Float - v2) < 0.0 )
        value = v2;
    else
        value = Float;
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void GScr_floor()
{
    float v0; // [esp+Ch] [ebp-8h]
    float Float; // [esp+10h] [ebp-4h]

    Float = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    v0 = floor(Float);
    Scr_AddFloat(v0, SCRIPTINSTANCE_SERVER);
}

void GScr_ceil()
{
    float v0; // [esp+Ch] [ebp-8h]
    float Float; // [esp+10h] [ebp-4h]

    Float = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    v0 = ceil(Float);
    Scr_AddFloat(v0, SCRIPTINSTANCE_SERVER);
}

void GScr_sqrt()
{
    float Float; // [esp+8h] [ebp-4h]

    Float = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    Scr_AddFloat(sqrtf(Float), SCRIPTINSTANCE_SERVER);
}

void GScr_CastInt()
{
    VariableUnion v0; // eax
    double Float; // st7
    char *String; // eax
    int v3; // eax
    const char *TypeName; // eax
    const char *v5; // eax
    int Type; // [esp+0h] [ebp-4h]

    Type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    switch ( Type )
    {
        case 2:
            String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
            v3 = atoi(String);
            Scr_AddInt(v3, SCRIPTINSTANCE_SERVER);
            break;
        case 5:
            Float = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
            Scr_AddInt((int)Float, SCRIPTINSTANCE_SERVER);
            break;
        case 6:
            v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            Scr_AddInt(v0.intValue, SCRIPTINSTANCE_SERVER);
            break;
        default:
            TypeName = Scr_GetTypeName(0, SCRIPTINSTANCE_SERVER);
            v5 = va("cannot cast %s to int", TypeName);
            Scr_ParamError(0, v5, SCRIPTINSTANCE_SERVER);
            break;
    }
}

void GScr_CastFloat()
{
    float intValue; // xmm0_4
    char *String; // eax
    const char *TypeName; // eax
    const char *v3; // eax
    float value; // [esp+0h] [ebp-10h]
    float v5; // [esp+8h] [ebp-8h]
    int Type; // [esp+Ch] [ebp-4h]

    Type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    switch ( Type )
    {
        case 2:
            String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
            v5 = atof(String);
            Scr_AddFloat(v5, SCRIPTINSTANCE_SERVER);
            break;
        case 5:
            value = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
            Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
            break;
        case 6:
            intValue = (float)Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            Scr_AddFloat(intValue, SCRIPTINSTANCE_SERVER);
            break;
        default:
            TypeName = Scr_GetTypeName(0, SCRIPTINSTANCE_SERVER);
            v3 = va("cannot cast %s to float", TypeName);
            Scr_ParamError(0, v3, SCRIPTINSTANCE_SERVER);
            break;
    }
}

void GScr_VectorFromLineToPoint()
{
    float segmentB[3]; // [esp+4h] [ebp-50h] BYREF
    float result[3]; // [esp+10h] [ebp-44h] BYREF
    float BA[3]; // [esp+1Ch] [ebp-38h]
    float PA[3]; // [esp+28h] [ebp-2Ch]
    float fraction; // [esp+34h] [ebp-20h]
    float segmentLengthSq; // [esp+38h] [ebp-1Ch]
    float segmentA[3]; // [esp+3Ch] [ebp-18h] BYREF
    float P[3]; // [esp+48h] [ebp-Ch] BYREF

    Scr_GetVector(0, segmentA, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, segmentB, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2u, P, SCRIPTINSTANCE_SERVER);
    BA[0] = segmentB[0] - segmentA[0];
    BA[1] = segmentB[1] - segmentA[1];
    BA[2] = segmentB[2] - segmentA[2];
    segmentLengthSq = (float)((float)((float)(segmentB[0] - segmentA[0]) * (float)(segmentB[0] - segmentA[0]))
                                                    + (float)((float)(segmentB[1] - segmentA[1]) * (float)(segmentB[1] - segmentA[1])))
                                    + (float)((float)(segmentB[2] - segmentA[2]) * (float)(segmentB[2] - segmentA[2]));
    if ( segmentLengthSq == 0.0 )
        Scr_ParamError(0, "The two points on the line must be different from each other", SCRIPTINSTANCE_SERVER);
    PA[0] = P[0] - segmentA[0];
    PA[1] = P[1] - segmentA[1];
    PA[2] = P[2] - segmentA[2];
    fraction = (float)((float)((float)(BA[0] * (float)(P[0] - segmentA[0])) + (float)(BA[1] * (float)(P[1] - segmentA[1])))
                                     + (float)(BA[2] * (float)(P[2] - segmentA[2])))
                     / segmentLengthSq;
    result[0] = (float)((-(fraction)) * BA[0]) + (float)(P[0] - segmentA[0]);
    result[1] = (float)((-(fraction)) * BA[1]) + (float)(P[1] - segmentA[1]);
    result[2] = (float)((-(fraction)) * BA[2]) + (float)(P[2] - segmentA[2]);
    Scr_AddVector(result, SCRIPTINSTANCE_SERVER);
}

void GScr_PointOnSegmentNearestToPoint()
{
    float segmentB[3]; // [esp+0h] [ebp-50h] BYREF
    float BA[3]; // [esp+Ch] [ebp-44h]
    float PA[3]; // [esp+18h] [ebp-38h]
    float fraction; // [esp+24h] [ebp-2Ch]
    float segmentLengthSq; // [esp+28h] [ebp-28h]
    float segmentA[3]; // [esp+2Ch] [ebp-24h] BYREF
    float P[3]; // [esp+38h] [ebp-18h] BYREF
    float nearPoint[3]; // [esp+44h] [ebp-Ch] BYREF

    Scr_GetVector(0, segmentA, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, segmentB, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2u, P, SCRIPTINSTANCE_SERVER);
    BA[0] = segmentB[0] - segmentA[0];
    BA[1] = segmentB[1] - segmentA[1];
    BA[2] = segmentB[2] - segmentA[2];
    segmentLengthSq = (float)((float)((float)(segmentB[0] - segmentA[0]) * (float)(segmentB[0] - segmentA[0]))
                                                    + (float)((float)(segmentB[1] - segmentA[1]) * (float)(segmentB[1] - segmentA[1])))
                                    + (float)((float)(segmentB[2] - segmentA[2]) * (float)(segmentB[2] - segmentA[2]));
    if ( segmentLengthSq == 0.0 )
        Scr_ParamError(0, "Line segment must not have zero length", SCRIPTINSTANCE_SERVER);
    PA[0] = P[0] - segmentA[0];
    PA[1] = P[1] - segmentA[1];
    PA[2] = P[2] - segmentA[2];
    fraction = (float)((float)((float)(BA[0] * (float)(P[0] - segmentA[0])) + (float)(BA[1] * (float)(P[1] - segmentA[1])))
                                     + (float)(BA[2] * (float)(P[2] - segmentA[2])))
                     / segmentLengthSq;
    if ( fraction >= 0.0 )
    {
        if ( fraction <= 1.0 )
        {
            nearPoint[0] = (float)(fraction * BA[0]) + segmentA[0];
            nearPoint[1] = (float)(fraction * BA[1]) + segmentA[1];
            nearPoint[2] = (float)(fraction * BA[2]) + segmentA[2];
            Scr_AddVector(nearPoint, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_AddVector(segmentB, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        Scr_AddVector(segmentA, SCRIPTINSTANCE_SERVER);
    }
}

void Scr_Distance()
{
    float value; // [esp+0h] [ebp-30h]
    float v0[3]; // [esp+18h] [ebp-18h] BYREF
    float v1[3]; // [esp+24h] [ebp-Ch] BYREF

    Scr_GetVector(0, v0, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, v1, SCRIPTINSTANCE_SERVER);
    value = Vec3Distance(v0, v1);
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void Scr_Distance2D()
{
    float value; // [esp+0h] [ebp-2Ch]
    float v[2]; // [esp+Ch] [ebp-20h] BYREF
    float v0[3]; // [esp+14h] [ebp-18h] BYREF
    float v1[3]; // [esp+20h] [ebp-Ch] BYREF

    Scr_GetVector(0, v0, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, v1, SCRIPTINSTANCE_SERVER);
    v[0] = v1[0] - v0[0];
    v[1] = v1[1] - v0[1];
    value = Vec2Length(v);
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void Scr_DistanceSquared()
{
    float value; // [esp+0h] [ebp-2Ch]
    float v0[3]; // [esp+14h] [ebp-18h] BYREF
    float v1[3]; // [esp+20h] [ebp-Ch] BYREF

    Scr_GetVector(0, v0, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, v1, SCRIPTINSTANCE_SERVER);
    value = Vec3DistanceSq(v0, v1);
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void Scr_Length()
{
    float value; // [esp+0h] [ebp-18h]
    float v[3]; // [esp+Ch] [ebp-Ch] BYREF

    Scr_GetVector(0, v, SCRIPTINSTANCE_SERVER);
    value = Vec3Length(v);
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void Scr_LengthSquared()
{
    float v[3]; // [esp+8h] [ebp-Ch] BYREF

    Scr_GetVector(0, v, SCRIPTINSTANCE_SERVER);
    Scr_AddFloat((float)((float)(v[0] * v[0]) + (float)(v[1] * v[1])) + (float)(v[2] * v[2]), SCRIPTINSTANCE_SERVER);
}

void Scr_Closer()
{
    float fDistBSqrd; // [esp+1Ch] [ebp-2Ch]
    float vB[3]; // [esp+20h] [ebp-28h] BYREF
    float fDistASqrd; // [esp+2Ch] [ebp-1Ch]
    float vRef[3]; // [esp+30h] [ebp-18h] BYREF
    float vA[3]; // [esp+3Ch] [ebp-Ch] BYREF

    Scr_GetVector(0, vRef, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, vA, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2u, vB, SCRIPTINSTANCE_SERVER);
    fDistASqrd = Vec3DistanceSq(vA, vRef);
    fDistBSqrd = Vec3DistanceSq(vB, vRef);
    Scr_AddInt(fDistBSqrd > fDistASqrd, SCRIPTINSTANCE_SERVER);
}

void Scr_VectorDot()
{
    float b[3]; // [esp+8h] [ebp-18h] BYREF
    float a[3]; // [esp+14h] [ebp-Ch] BYREF

    Scr_GetVector(0, a, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, b, SCRIPTINSTANCE_SERVER);
    Scr_AddFloat((float)((float)(a[0] * b[0]) + (float)(a[1] * b[1])) + (float)(a[2] * b[2]), SCRIPTINSTANCE_SERVER);
}

void Scr_VectorCross()
{
    float b[3]; // [esp+0h] [ebp-24h] BYREF
    float tempVec[3]; // [esp+Ch] [ebp-18h] BYREF
    float a[3]; // [esp+18h] [ebp-Ch] BYREF

    Scr_GetVector(0, a, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, b, SCRIPTINSTANCE_SERVER);
    Vec3Cross(a, b, tempVec);
    Scr_AddVector(tempVec, SCRIPTINSTANCE_SERVER);
}

void Scr_VectorNormalize()
{
    float b[3]; // [esp+Ch] [ebp-18h] BYREF
    float a[3]; // [esp+18h] [ebp-Ch] BYREF

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("wrong number of arguments to vectornormalize!", 0);
    Scr_GetVector(0, a, SCRIPTINSTANCE_SERVER);
    b[0] = a[0];
    b[1] = a[1];
    b[2] = a[2];
    Vec3Normalize(b);
    Scr_AddVector(b, SCRIPTINSTANCE_SERVER);
}

void Scr_VectorToAngles()
{
    float angles[3]; // [esp+0h] [ebp-18h] BYREF
    float vec[3]; // [esp+Ch] [ebp-Ch] BYREF

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("wrong number of arguments to vectortoangle!", 0);
    Scr_GetVector(0, vec, SCRIPTINSTANCE_SERVER);
    vectoangles(vec, angles);
    Scr_AddVector(angles, SCRIPTINSTANCE_SERVER);
}

void Scr_VectorLerp()
{
    float from[3]; // [esp+8h] [ebp-28h] BYREF
    float result[3]; // [esp+14h] [ebp-1Ch] BYREF
    float fraction; // [esp+20h] [ebp-10h]
    float to[3]; // [esp+24h] [ebp-Ch] BYREF

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3 )
        Scr_Error("wrong number of arguments to vectorlerp", 0);
    Scr_GetVector(0, from, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, to, SCRIPTINSTANCE_SERVER);
    fraction = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    Vec3Lerp(from, to, fraction, result);
    Scr_AddVector(result, SCRIPTINSTANCE_SERVER);
}

void Scr_AnglesToUp()
{
    float angles[3]; // [esp+0h] [ebp-18h] BYREF
    float up[3]; // [esp+Ch] [ebp-Ch] BYREF

    Scr_GetVector(0, angles, SCRIPTINSTANCE_SERVER);
    AngleVectors(angles, 0, 0, up);
    Scr_AddVector(up, SCRIPTINSTANCE_SERVER);
}

void Scr_AnglesToRight()
{
    float right[3]; // [esp+0h] [ebp-18h] BYREF
    float angles[3]; // [esp+Ch] [ebp-Ch] BYREF

    Scr_GetVector(0, angles, SCRIPTINSTANCE_SERVER);
    AngleVectors(angles, 0, right, 0);
    Scr_AddVector(right, SCRIPTINSTANCE_SERVER);
}

void Scr_AnglesToForward()
{
    float forward[3]; // [esp+0h] [ebp-18h] BYREF
    float angles[3]; // [esp+Ch] [ebp-Ch] BYREF

    Scr_GetVector(0, angles, SCRIPTINSTANCE_SERVER);
    AngleVectors(angles, forward, 0, 0);
    Scr_AddVector(forward, SCRIPTINSTANCE_SERVER);
}

void Scr_CombineAngles()
{
    float anglesfinal[3]; // [esp+0h] [ebp-90h] BYREF
    float axisB[3][3]; // [esp+Ch] [ebp-84h] BYREF
    float anglesA[3]; // [esp+30h] [ebp-60h] BYREF
    float axisA[3][3]; // [esp+3Ch] [ebp-54h] BYREF
    float anglesB[3]; // [esp+60h] [ebp-30h] BYREF
    float combinedaxis[3][3]; // [esp+6Ch] [ebp-24h] BYREF

    Scr_GetVector(0, anglesA, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, anglesB, SCRIPTINSTANCE_SERVER);
    AnglesToAxis(anglesA, axisA);
    AnglesToAxis(anglesB, axisB);
    MatrixMultiply(axisB, axisA, combinedaxis);
    AxisToAngles(combinedaxis, anglesfinal);
    Scr_AddVector(anglesfinal, SCRIPTINSTANCE_SERVER);
}

void Scr_ClampAngle180()
{
    float v0; // [esp+8h] [ebp-Ch]
    float anglea; // [esp+Ch] [ebp-8h]
    float angle; // [esp+Ch] [ebp-8h]

    anglea = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    v0 = floor((float)(anglea / 360.0));
    angle = ((float)(anglea / 360.0) - v0) * 360.0;
    if ( angle <= 180.0 )
        Scr_AddFloat(angle, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddFloat(angle - 360.0, SCRIPTINSTANCE_SERVER);
}

void Scr_AbsAngleClamp180()
{
    float v0; // [esp+8h] [ebp-Ch]
    float anglea; // [esp+Ch] [ebp-8h]
    float angle; // [esp+Ch] [ebp-8h]

    anglea = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    v0 = floor((float)(anglea / 360.0));
    angle = ((float)(anglea / 360.0) - v0) * 360.0;
    if ( angle <= 180.0 )
        Scr_AddFloat(angle, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddFloat(360.0 - angle, SCRIPTINSTANCE_SERVER);
}

void Scr_RotatePoint()
{
    float result[3]; // [esp+24h] [ebp-34h] BYREF
    float quat[4]; // [esp+30h] [ebp-28h] BYREF
    float angles[3]; // [esp+40h] [ebp-18h] BYREF
    float point[3]; // [esp+4Ch] [ebp-Ch] BYREF

    Scr_GetVector(0, point, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, angles, SCRIPTINSTANCE_SERVER);
    AnglesToQuat(angles, quat);
    RotatePoint(point, quat, result);
    Scr_AddVector(result, SCRIPTINSTANCE_SERVER);
}

void Scr_IsSubStr()
{
    char *v0; // eax
    char *v1; // eax
    char *String; // [esp-8h] [ebp-8h]

    String = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    v0 = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    v1 = strstr(v0, String);
    Scr_AddBool(v1 != 0, SCRIPTINSTANCE_SERVER);
}

void Scr_GetSubStr()
{
    VariableUnion v0; // [esp+0h] [ebp-424h]
    int source; // [esp+4h] [ebp-420h]
    char c; // [esp+Bh] [ebp-419h]
    char tempString[1028]; // [esp+Ch] [ebp-418h] BYREF
    int start; // [esp+414h] [ebp-10h]
    int end; // [esp+418h] [ebp-Ch]
    int dest; // [esp+41Ch] [ebp-8h]
    const char *s; // [esp+420h] [ebp-4h]

    s = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    start = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 3 )
        v0.intValue = 0x7FFFFFFF;
    else
        v0.intValue = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    end = v0.intValue;
    source = start;
    for ( dest = 0; source < end; ++dest )
    {
        if ( dest >= 1024 )
            Scr_Error("string too long", 0);
        c = s[source];
        if ( !c )
            break;
        tempString[dest] = c;
        ++source;
    }
    tempString[dest] = 0;
    Scr_AddString(tempString, SCRIPTINSTANCE_SERVER);
}

void Scr_ToLower()
{
    char v0; // al
    char tempString[1028]; // [esp+4h] [ebp-410h] BYREF
    const char *s; // [esp+40Ch] [ebp-8h]
    int i; // [esp+410h] [ebp-4h]

    s = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    i = 0;
    while ( i < 1024 )
    {
        v0 = tolower(*s);
        tempString[i] = v0;
        if ( !v0 )
        {
            Scr_AddString(tempString, SCRIPTINSTANCE_SERVER);
            return;
        }
        ++i;
        ++s;
    }
    Scr_Error("string too long", 0);
}

void Scr_StrTok()
{
    int source; // [esp+10h] [ebp-42Ch]
    char c; // [esp+17h] [ebp-425h]
    unsigned int delimId; // [esp+18h] [ebp-424h]
    char tempString[1028]; // [esp+1Ch] [ebp-420h] BYREF
    const char *delim; // [esp+424h] [ebp-18h]
    int dest; // [esp+428h] [ebp-14h]
    const char *s; // [esp+42Ch] [ebp-10h]
    int i; // [esp+430h] [ebp-Ch]
    int delimLen; // [esp+434h] [ebp-8h]
    unsigned int sId; // [esp+438h] [ebp-4h]

    sId = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    delimId = Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    s = SL_ConvertToString(sId, SCRIPTINSTANCE_SERVER);
    delim = SL_ConvertToString(delimId, SCRIPTINSTANCE_SERVER);
    SL_AddRefToString(sId, SCRIPTINSTANCE_SERVER);
    SL_AddRefToString(delimId, SCRIPTINSTANCE_SERVER);
    delimLen = strlen(delim);
    dest = 0;
    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for ( source = 0; ; ++source )
    {
        c = s[source];
        if ( !c )
            break;
        for ( i = 0; i < delimLen; ++i )
        {
            if ( c == delim[i] )
            {
                if ( dest )
                {
                    tempString[dest] = 0;
                    Scr_AddString(tempString, SCRIPTINSTANCE_SERVER);
                    Scr_AddArray(SCRIPTINSTANCE_SERVER);
                    dest = 0;
                }
                goto LABEL_2;
            }
        }
        tempString[dest++] = c;
        if ( dest >= 1024 )
        {
            SL_RemoveRefToString(SCRIPTINSTANCE_SERVER, sId);
            SL_RemoveRefToString(SCRIPTINSTANCE_SERVER, delimId);
            Scr_Error("string too long", 0);
        }
LABEL_2:
        ;
    }
    if ( dest )
    {
        tempString[dest] = 0;
        Scr_AddString(tempString, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
        dest = 0;
    }
    SL_RemoveRefToString(SCRIPTINSTANCE_SERVER, sId);
    SL_RemoveRefToString(SCRIPTINSTANCE_SERVER, delimId);
}

void __cdecl GScr_NeedsRevive(scr_entref_t entref)
{
    gentity_s *pEnt; // [esp+8h] [ebp-8h]
    int needsRevive; // [esp+Ch] [ebp-4h]

    needsRevive = 0;
    pEnt = GetEntity(entref);
    if ( !pEnt->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 7727, 0, "%s", "pEnt->client") )
    {
        __debugbreak();
    }
    if ( Scr_GetInt(0, SCRIPTINSTANCE_SERVER) )
        needsRevive = 1;
    if ( pEnt->client->ps.clientNum >= 0x20u
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    7736,
                    0,
                    "pEnt->client->ps.clientNum doesn't index MAX_CLIENTS\n\t%i not in [0, %i)",
                    pEnt->client->ps.clientNum,
                    32) )
    {
        __debugbreak();
    }
    bgs->clientinfo[pEnt->client->ps.clientNum].needsRevive = needsRevive;
    G_GetClientState(pEnt->client->ps.clientNum)->needsRevive = needsRevive;
}

void __cdecl GScr_IsInSecondChance(scr_entref_t entref)
{
    clientState_s *client; // [esp+0h] [ebp-8h]
    gentity_s *pEnt; // [esp+4h] [ebp-4h]

    pEnt = GetEntity(entref);
    if ( !pEnt->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 7763, 0, "%s", "pEnt->client") )
    {
        __debugbreak();
    }
    if ( pEnt->client->ps.clientNum >= 0x20u
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    7765,
                    0,
                    "pEnt->client->ps.clientNum doesn't index MAX_CLIENTS\n\t%i not in [0, %i)",
                    pEnt->client->ps.clientNum,
                    32) )
    {
        __debugbreak();
    }
    client = G_GetClientState(pEnt->client->ps.clientNum);
    Scr_AddBool(client->needsRevive, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetBurn(scr_entref_t entref)
{
    const char *v1; // eax
    gentity_s *ent; // [esp+4h] [ebp-Ch]
    int clientNum; // [esp+8h] [ebp-8h]
    float burnTime; // [esp+Ch] [ebp-4h]

    clientNum = -1;
    burnTime = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( burnTime < 0.0 )
        Scr_ParamError(1u, "Time must be positive", SCRIPTINSTANCE_SERVER);
    ent = GetEntity(entref);
    if ( ent && ent->r.inuse && ent->client )
        clientNum = ent->s.number;
    else
        Scr_Error("setburn() called on an invalid client entity.\n", 0);
    v1 = va("%c %i", 87, (int)(float)(burnTime * 1000.0));
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_SetElectrified(scr_entref_t entref)
{
    const char *v1; // eax
    float effectTime; // [esp+4h] [ebp-Ch]
    gentity_s *ent; // [esp+8h] [ebp-8h]
    int clientNum; // [esp+Ch] [ebp-4h]

    clientNum = -1;
    effectTime = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( effectTime < 0.0 )
        Scr_ParamError(1u, "Time must be positive", SCRIPTINSTANCE_SERVER);
    ent = GetEntity(entref);
    if ( ent && ent->r.inuse && ent->client )
        clientNum = ent->s.number;
    else
        Scr_Error("setelectrified() called on an invalid client entity.\n", 0);
    v1 = va("%c %i", 40, (int)(float)(effectTime * 1000.0));
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_StartTanning(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->client )
        ent->client->ps.eFlags2 |= 0x200000u;
    else
        ent->s.lerp.eFlags2 |= 0x200000u;
}

// LWSS ADD
void __cdecl GScr_SetWaterDrops(scr_entref_t entref)
{
    gentity_s *pSelf;
    int count;
    const char *cmd;

    count = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (count < 0)
        Scr_ParamError(1, "Count must be either zero, or a positive number", SCRIPTINSTANCE_SERVER);

    pSelf = GetEntity(entref);

    if (!pSelf->r.linked || !pSelf->client)
    {
        Scr_Error("setwaterdrops() called on an invalid client entity.\n", SCRIPTINSTANCE_SERVER);
        return;
    }

    cmd = va("%c %i", '0', count); // KISAKTODO: I am confused how this command relates to water drops
    SV_GameSendServerCommand(pSelf->s.number, SV_CMD_RELIABLE, cmd);
}

// LWSS END
void __cdecl GScr_StopBurning(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->client )
        ent->client->ps.eFlags2 &= ~0x200000u;
    else
        ent->s.lerp.eFlags2 &= ~0x200000u;
}

void __cdecl GScr_SpawnNapalmGroundFlame(scr_entref_t entref)
{
    char *weaponName; // [esp+0h] [ebp-2Ch]
    float origin[3]; // [esp+4h] [ebp-28h] BYREF
    int time; // [esp+10h] [ebp-1Ch]
    int weaponIndex; // [esp+14h] [ebp-18h]
    gentity_s *ent; // [esp+18h] [ebp-14h]
    float direction[3]; // [esp+1Ch] [ebp-10h] BYREF
    const WeaponDef *weapDef; // [esp+28h] [ebp-4h]
    int savedregs; // [esp+2Ch] [ebp+0h] BYREF

    ent = GetEntity(entref);
    time = 10;
    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    weaponName = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2u, direction, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 3 )
        time = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
    weaponIndex = G_GetWeaponIndexForName(weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, weaponName);
    weapDef = BG_GetWeaponDef(weaponIndex);
    G_SetOrigin(ent, origin);
    G_SetAngle(ent, direction);
    ent->s.weapon = weaponIndex;
    ent->s.eType = ET_GENERAL;
    ent->s.lerp.eFlags |= 0x20u;
    ent->s.weapon = ent->s.weapon;
    G_BroadcastEntity(ent);
    G_AddEvent(ent, 0x42u, 0);
    ent->s.lerp.pos.trBase[0] = (float)(int)ent->s.lerp.pos.trBase[0];
    ent->s.lerp.pos.trBase[1] = (float)(int)ent->s.lerp.pos.trBase[1];
    ent->s.lerp.pos.trBase[2] = (float)(int)ent->s.lerp.pos.trBase[2];
    G_SetOrigin(ent, ent->s.lerp.pos.trBase);
    G_SetAngle(ent, ent->s.lerp.apos.trBase);
    ent->s.lerp.eFlags |= 0x4000u;
    ent->s.lerp.u.actor.actorNum = level.time;
    ent->s.time2 = level.time + 1000 * time;
    ent->s.lerp.eFlags |= 0x10u;
    ent->s.lerp.eFlags2 |= 1u;
    ent->handler = 11;
    ent->nextthink = level.time + 1;
    SV_LinkEntity(ent);
}

void __cdecl GScr_RestoreDefaultDropPitch(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        Scr_Error("illegal call to restoredefaultdroppitch()\n", 0);
    }
    else
    {
        ent = GetEntity(entref);
        if ( ent->pTurretInfo )
            turret_RestoreDefaultDropPitch(ent);
        else
            Scr_Error("entity is not a turret", 0);
    }
}

void __cdecl GScr_clearCenterPopups(scr_entref_t entref)
{
    const char *v1; // eax
    int clientNum; // [esp+4h] [ebp-4h]

    clientNum = GetEntity(entref)->s.number;
    v1 = va("%c %c", 91, 110);
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_clearPopups(scr_entref_t entref)
{
    const char *v1; // eax
    int clientNum; // [esp+4h] [ebp-4h]

    clientNum = GetEntity(entref)->s.number;
    v1 = va("%c %c", 91, 105);
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_DisplayGameModeMessage(scr_entref_t entref)
{
    const char *v1; // eax
    int sentToClientNum; // [esp+0h] [ebp-94h]
    char *sound; // [esp+4h] [ebp-90h]
    char gameModeMessage[132]; // [esp+Ch] [ebp-88h] BYREF

    sound = (char *)"";
    sentToClientNum = GetEntity(entref)->s.number;
    Scr_ConstructMessageString(0, 0, "Game Message", gameModeMessage, 0x80u);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
        sound = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    v1 = va("%c %c %s %s", 91, 99, gameModeMessage, sound);
    SV_GameSendServerCommand(sentToClientNum, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_DisplayTeamMessage(scr_entref_t entref)
{
    const char *v1; // eax
    int configStringIndex; // [esp+0h] [ebp-9Ch]
    char message[132]; // [esp+4h] [ebp-98h] BYREF
    gentity_s *playerEnt; // [esp+8Ch] [ebp-10h]
    const char *sound; // [esp+90h] [ebp-Ch]
    gentity_s *sendToEntity; // [esp+94h] [ebp-8h]
    int clientNum; // [esp+98h] [ebp-4h]

    sendToEntity = GetEntity(entref);
    playerEnt = Scr_GetEntity(1u);
    sound = "";
    Scr_ConstructMessageString(0, 0, "Team Message String", message, 0x80u);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 3 )
        sound = Scr_GetString(2u, SCRIPTINSTANCE_SERVER);
    configStringIndex = G_LocalizedStringIndex(message);
    clientNum = sendToEntity->s.number;
    v1 = va("%c %c %i %i %s", 91, 104, configStringIndex, playerEnt->client->ps.clientNum, sound);
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_DisplayMedal(scr_entref_t entref)
{
    const char *v1; // eax
    char *sound; // [esp+Ch] [ebp-18h]
    float xpScale; // [esp+10h] [ebp-14h]
    gentity_s *player_entity; // [esp+14h] [ebp-10h]
    int teamBased; // [esp+18h] [ebp-Ch]
    int clientNum; // [esp+1Ch] [ebp-8h]
    int medalIndex; // [esp+20h] [ebp-4h]

    player_entity = GetEntity(entref);
    medalIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    teamBased = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    xpScale = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    sound = Scr_GetString(3u, SCRIPTINSTANCE_SERVER);
    clientNum = player_entity->s.number;
    v1 = va("%c %c %i %i %.2f %s", 91, 102, medalIndex, teamBased, xpScale, sound);
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_DisplayContract(scr_entref_t entref)
{
    const char *v1; // eax
    int contractIndex; // [esp+0h] [ebp-14h]
    int passed; // [esp+4h] [ebp-10h]
    char *sound; // [esp+8h] [ebp-Ch]
    gentity_s *player_entity; // [esp+Ch] [ebp-8h]
    int clientNum; // [esp+10h] [ebp-4h]

    player_entity = GetEntity(entref);
    contractIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    sound = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    passed = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 3 )
        passed = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    clientNum = player_entity->s.number;
    v1 = va("%c %c %i %s %i", 91, 98, contractIndex, sound, passed);
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_DisplayChallengeComplete(scr_entref_t entref)
{
    const char *v1; // eax
    int tier; // [esp+14h] [ebp-20h]
    char *sound; // [esp+18h] [ebp-1Ch]
    float xpScale; // [esp+1Ch] [ebp-18h]
    int index; // [esp+20h] [ebp-14h]
    int weaponIndex; // [esp+24h] [ebp-10h]
    char *type; // [esp+28h] [ebp-Ch]
    int clientNum; // [esp+30h] [ebp-4h]

    clientNum = GetEntity(entref)->s.number;
    tier = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    index = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    xpScale = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    weaponIndex = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
    sound = Scr_GetString(4u, SCRIPTINSTANCE_SERVER);
    type = Scr_GetString(5u, SCRIPTINSTANCE_SERVER);
    v1 = va("%c %c %i %i %.2f %i %s %s", 91, 100, tier, index, xpScale, weaponIndex, type, sound);
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_DisplayEndGameMilestoneComplete(scr_entref_t entref)
{
    const char *v1; // eax
    int tier; // [esp+0h] [ebp-1Ch]
    int slot; // [esp+4h] [ebp-18h]
    int index; // [esp+8h] [ebp-14h]
    int weaponIndex; // [esp+Ch] [ebp-10h]
    char *type; // [esp+10h] [ebp-Ch]
    int clientNum; // [esp+18h] [ebp-4h]

    clientNum = GetEntity(entref)->s.number;
    Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
    Scr_GetString(4u, SCRIPTINSTANCE_SERVER);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 5
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    8125,
                    0,
                    "%s",
                    "Scr_GetNumParam() == 5") )
    {
        __debugbreak();
    }
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 5 )
    {
        slot = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        tier = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        index = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
        weaponIndex = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
        type = Scr_GetString(4u, SCRIPTINSTANCE_SERVER);
        v1 = va("%c %c %i %i %i %i %s", 91, 108, slot, tier, index, weaponIndex, type);
        SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
    }
}

void __cdecl GScr_DisplayEndGame(scr_entref_t entref)
{
    const char *v1; // eax
    int challengeIndex2; // [esp+0h] [ebp-18h]
    int challengeIndex0; // [esp+4h] [ebp-14h]
    int promoted; // [esp+8h] [ebp-10h]
    int challengeIndex1; // [esp+Ch] [ebp-Ch]
    int clientNum; // [esp+14h] [ebp-4h]

    clientNum = GetEntity(entref)->s.number;
    promoted = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    challengeIndex0 = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    challengeIndex1 = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    challengeIndex2 = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 4
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    8160,
                    0,
                    "%s",
                    "Scr_GetNumParam() == 4") )
    {
        __debugbreak();
    }
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 4 )
    {
        v1 = va("%c %c %i %i %i %i", 91, 109, promoted, challengeIndex0, challengeIndex1, challengeIndex2);
        SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
    }
}

void __cdecl GScr_ClearEndGameComplete(scr_entref_t entref)
{
    const char *v1; // eax
    int clientNum; // [esp+4h] [ebp-4h]

    clientNum = GetEntity(entref)->s.number;
    v1 = va("%c %c", 91, 107);
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_DisplayKillstreak(scr_entref_t entref)
{
    const char *v1; // eax
    int streakCount; // [esp+0h] [ebp-10h]
    int killstreakTableNumber; // [esp+4h] [ebp-Ch]
    int clientNum; // [esp+8h] [ebp-8h]
    gentity_s *player_entity; // [esp+Ch] [ebp-4h]

    player_entity = GetEntity(entref);
    if ( player_entity->s.eType == 1 )
    {
        streakCount = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        killstreakTableNumber = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        clientNum = player_entity->s.number;
        v1 = va("%c %c %i %i", 91, 101, streakCount, killstreakTableNumber);
        SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
    }
}

void __cdecl GScr_DisplayRankUp(scr_entref_t entref)
{
    const char *v1; // eax
    int prestige; // [esp+0h] [ebp-14h]
    int rank; // [esp+4h] [ebp-10h]
    char *sound; // [esp+8h] [ebp-Ch]
    int clientNum; // [esp+Ch] [ebp-8h]
    gentity_s *player_entity; // [esp+10h] [ebp-4h]

    player_entity = GetEntity(entref);
    if ( player_entity->s.eType == 1 )
    {
        rank = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        prestige = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        if ( prestige < 0 )
            prestige = 0;
        sound = Scr_GetString(2u, SCRIPTINSTANCE_SERVER);
        clientNum = player_entity->s.number;
        v1 = va("%c %c %i %i %s", 91, 103, rank, prestige, sound);
        SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
    }
}

void __cdecl GScr_DisplayWagerPopup(scr_entref_t entref)
{
    const char *v1; // eax
    const char *v2; // eax
    char *subMessageString; // [esp+0h] [ebp-1Ch]
    int subMessageStringIndex; // [esp+4h] [ebp-18h]
    int messageStringIndex; // [esp+8h] [ebp-14h]
    int points; // [esp+Ch] [ebp-10h]
    char *messageString; // [esp+10h] [ebp-Ch]
    int clientNum; // [esp+14h] [ebp-8h]
    gentity_s *player_entity; // [esp+18h] [ebp-4h]

    player_entity = GetEntity(entref);
    if ( player_entity->s.eType == 1 )
    {
        messageString = Scr_GetIString(0, SCRIPTINSTANCE_SERVER);
        messageStringIndex = G_FindConfigstringIndex(messageString, 515, 1023, 0, "WAGER POPUP ERROR:");
        if ( messageStringIndex )
        {
            points = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
            clientNum = player_entity->s.number;
            if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 3 )
            {
                subMessageString = Scr_GetIString(2u, SCRIPTINSTANCE_SERVER);
                subMessageStringIndex = G_FindConfigstringIndex(subMessageString, 515, 1023, 0, "WAGER POPUP ERROR:");
                if ( subMessageStringIndex )
                {
                    v1 = va("%c %c %i %i %i", 91, 106, messageStringIndex, points, subMessageStringIndex);
                    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
                }
                else
                {
                    Scr_Error("Error displaying wager popup: sub message not precached", 0);
                }
            }
            else
            {
                v2 = va("%c %c %i %i", 91, 106, messageStringIndex, points);
                SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v2);
            }
        }
        else
        {
            Scr_Error("Error displaying wager popup: message not precached", 0);
        }
    }
}

void __cdecl GScr_DisplayHudAnim(scr_entref_t entref)
{
    const char *v1; // eax
    char *hudAnimName; // [esp+0h] [ebp-10h]
    char *hudMenuName; // [esp+4h] [ebp-Ch]
    int clientNum; // [esp+8h] [ebp-8h]
    gentity_s *player_entity; // [esp+Ch] [ebp-4h]

    player_entity = GetEntity(entref);
    if ( player_entity->s.eType == 1 )
    {
        clientNum = player_entity->client->ps.clientNum;
        hudAnimName = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        hudMenuName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        v1 = va("%c %c %s %s", 91, 97, hudMenuName, hudAnimName);
        SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, v1);
    }
}

void __cdecl GScr_IsFiringTurret(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    bool IsFiring; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    IsFiring = turret_IsFiring(ent);
    Scr_AddBool(IsFiring, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_IsTurretLockedOn(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("entity type '%s' is not a turret", v1);
        Scr_Error(v2, 0);
    }
    if ( ent->pTurretInfo->state == 1 )
        Scr_AddBool(1u, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddBool(0, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_ShootUp(scr_entref_t entref)
{
    float velocity_8; // [esp+20h] [ebp-10h]
    trajectory_t *trajectory; // [esp+28h] [ebp-8h]
    gentity_s *pEnt; // [esp+2Ch] [ebp-4h]

    pEnt = GetEntity(entref);
    velocity_8 = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    trajectory = &pEnt->s.lerp.pos;
    pEnt->s.lerp.pos.trTime = level.time;
    pEnt->s.lerp.pos.trDuration = (int)(float)(1000.0 * 1000.0);
    pEnt->s.lerp.pos.trBase[0] = pEnt->r.currentOrigin[0];
    pEnt->s.lerp.pos.trBase[1] = pEnt->r.currentOrigin[1];
    pEnt->s.lerp.pos.trBase[2] = pEnt->r.currentOrigin[2];
    pEnt->s.lerp.pos.trDelta[0] = 0.0f;
    pEnt->s.lerp.pos.trDelta[1] = 0.0f;
    pEnt->s.lerp.pos.trDelta[2] = velocity_8;
    if ( ((LODWORD(pEnt->s.lerp.pos.trDelta[0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(pEnt->s.lerp.pos.trDelta[1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(pEnt->s.lerp.pos.trDelta[2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    8395,
                    0,
                    "%s",
                    "!IS_NAN((trajectory->trDelta)[0]) && !IS_NAN((trajectory->trDelta)[1]) && !IS_NAN((trajectory->trDelta)[2])") )
    {
        __debugbreak();
    }
    trajectory->trType = 6;
    BG_EvaluateTrajectory(trajectory, level.time, pEnt->r.currentOrigin);
}

void __cdecl GScr_GetWaterHeight()
{
    float pos[3]; // [esp+8h] [ebp-10h] BYREF
    float height; // [esp+14h] [ebp-4h]

    Scr_GetVector(0, pos, SCRIPTINSTANCE_SERVER);
    height = CM_GetWaterHeight(pos, 200.0, -200.0);
    Scr_AddFloat(height, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_DepthInWater(scr_entref_t entref)
{
    float v1; // [esp+8h] [ebp-Ch]
    float waterHeight; // [esp+Ch] [ebp-8h]
    gentity_s *ent; // [esp+10h] [ebp-4h]

    ent = GetEntity(entref);
    waterHeight = CM_GetWaterHeight(ent->r.currentOrigin, 200.0, -200.0) - ent->r.currentOrigin[2];
    if ( (float)(waterHeight - 0.0) < 0.0 )
        v1 = 0.0f;
    else
        v1 = waterHeight;
    Scr_AddFloat(v1, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_DepthOfPlayerInWater(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 8459, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    Scr_AddInt(ent->client->ps.waterlevel, SCRIPTINSTANCE_SERVER);
}

void Scr_SoundFade()
{
    const char *v0; // eax
    float fTargetVol; // [esp+Ch] [ebp-8h]
    int iFadeTime; // [esp+10h] [ebp-4h]

    fTargetVol = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 1 )
        iFadeTime = 0;
    else
        iFadeTime = (int)(Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER) * 1000.0);
    v0 = va("%c %f %i\n", 113, fTargetVol, iFadeTime);
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, v0);
}

void Scr_PrecacheModel()
{
    char *modelName; // [esp+4h] [ebp-4h]

    if (!level.initializing)
        Scr_Error("precacheModel must be called before any wait statements in the gametype or level script\n", 0);
    modelName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if (!*modelName)
        Scr_ParamError(0, "Model name string is empty", SCRIPTINSTANCE_SERVER);
#ifndef KISAK_SP
    // Retail SP Scr_PrecacheModel (0x007FA850) goes directly from the empty-name
    // check to G_ModelIndex.  The MP-only default-asset rejection turns the
    // shipped Zombies sentinel precachemodel("fx") into a fatal script error.
    if (useFastFile->current.enabled)
        Scr_ErrorOnDefaultAsset(ASSET_TYPE_XMODEL, modelName);
#endif
    G_ModelIndex(modelName);
}

void __cdecl Scr_ErrorOnDefaultAsset(XAssetType type, char *assetName)
{
    const char *XAssetTypeName; // eax
    const char *v3; // eax

    DB_FindXAssetHeader(type, assetName, 1, -1);
    if ( DB_IsXAssetDefault(type, assetName) )
    {
        XAssetTypeName = DB_GetXAssetTypeName(type);
        v3 = va("precache %s '%s' failed", XAssetTypeName, assetName);
        Scr_NeverTerminalError(v3, SCRIPTINSTANCE_SERVER);
    }
}

void Scr_PrecacheShellShock()
{
    shellshock_parms_t *ShellshockParms; // eax
    char *shellshockName; // [esp+0h] [ebp-8h]
    unsigned int index; // [esp+4h] [ebp-4h]

    if ( !level.initializing )
        Scr_Error("precacheShellShock must be called before any wait statements in the gametype or level script\n", 0);
    shellshockName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    index = G_ShellShockIndex(shellshockName);
    if ( !BG_LoadShellShockDvars(shellshockName) )
        Com_Error(ERR_DROP, "couldn't find shell shock %s -- see console", shellshockName);
    ShellshockParms = BG_GetShellshockParms(index);
    BG_SetShellShockParmsFromDvars(ShellshockParms);
}

void Scr_PrecacheItem()
{
    const char *v0; // eax
    char *pszItemName; // [esp+4h] [ebp-4h]

    if ( !level.initializing )
        Scr_Error("precacheItem must be called before any wait statements in the gametype or level script\n", 0);
    pszItemName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( !G_FindItem(pszItemName) )
    {
        v0 = va("unknown item '%s'", pszItemName);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
}

void Scr_PrecacheShader()
{
    char *shaderName; // [esp+0h] [ebp-4h]

    if (!level.initializing)
        Scr_Error("precacheShader must be called before any wait statements in the gametype or level script\n", 0);
    shaderName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if (!*shaderName)
        Scr_ParamError(0, "Shader name string is empty", SCRIPTINSTANCE_SERVER);
    G_MaterialIndex( shaderName);
}

#ifdef KISAK_SP
static void __cdecl GScr_MakeLocalizedString_SP()
{
    char message[1024];
    char localized[1024];
    Scr_ConstructMessageString(0, Scr_GetNumParam(SCRIPTINSTANCE_SERVER) - 1,
        "Script localized string", message, sizeof(message));
    SEH_LocalizeTextMessageWithBindings(0, message, "Script localized string", localized, sizeof(localized));
    Scr_AddString(localized, SCRIPTINSTANCE_SERVER);
}
#endif

void Scr_PrecacheString()
{
    char *stringName; // [esp+0h] [ebp-4h]

    if (!level.initializing)
        Scr_Error("precacheString must be called before any wait statements in the gametype or level script\n", 0);
#ifdef KISAK_SP
    char message[1024];
    Scr_ConstructMessageString(0, 0, "Precache String", message, sizeof(message));
    stringName = message;
#else
    stringName = Scr_GetIString(0, SCRIPTINSTANCE_SERVER);
#endif
    if (*stringName)
        G_LocalizedStringIndex( stringName);
}

void Scr_GrenadeExplosionEffect()
{
    unsigned __int8 v0; // al
    col_context_t context; // [esp+0h] [ebp-98h] BYREF
    float vDir[3]; // [esp+28h] [ebp-70h] BYREF
    float vOrg[3]; // [esp+34h] [ebp-64h] BYREF
    float vEnd[3]; // [esp+40h] [ebp-58h] BYREF
    trace_t trace; // [esp+4Ch] [ebp-4Ch] BYREF
    gentity_s *pEnt; // [esp+88h] [ebp-10h]
    float vPos[3]; // [esp+8Ch] [ebp-Ch] BYREF

    //col_context_t::col_context_t(&context);
    Scr_GetVector(0, vOrg, SCRIPTINSTANCE_SERVER);
    vPos[0] = vOrg[0];
    vPos[1] = vOrg[1];
    vPos[2] = vOrg[2] + 1.0;
    pEnt = G_TempEntity(vPos, EV_GRENADE_EXPLODE);
    vDir[0] = 0.0f;
    vDir[1] = 0.0f;
    vDir[2] = 1.0f;
    v0 = DirToByte(vDir);
    pEnt->s.eventParm = v0;
    vEnd[0] = vPos[0];
    vEnd[1] = vPos[1];
    vEnd[2] = vPos[2] - 17.0;
    G_TraceCapsule(&trace, vPos, vec3_origin, vec3_origin, vEnd, 1023, 2065, &context);
    pEnt->s.surfType = (trace.sflags & 0x3F00000) >> 20;
}

void GScr_RadiusDamage()
{
    GScr_RadiusDamageInternal(0);
}

void __cdecl GScr_RadiusDamageInternal(gentity_s *inflictor)
{
    char *String; // eax
    gentity_s *attacker; // [esp+20h] [ebp-24h]
    meansOfDeath_t mod; // [esp+24h] [ebp-20h]
    float max_damage; // [esp+28h] [ebp-1Ch]
    float origin[3]; // [esp+2Ch] [ebp-18h] BYREF
    float range; // [esp+38h] [ebp-Ch]
    int weapon; // [esp+3Ch] [ebp-8h]
    float min_damage; // [esp+40h] [ebp-4h]

    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    range = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    max_damage = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    min_damage = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
    if (range < 0.0f)
        Scr_Error(SCRIPTINSTANCE_SERVER, va("RadiusDamage range negative %g", range), false);
    if (max_damage < 0.0f)
        Scr_Error(SCRIPTINSTANCE_SERVER, va("max_damage negative %g", max_damage), false);
    if (min_damage < 0.0f)
        Scr_Error(SCRIPTINSTANCE_SERVER, va("min_damage negative %g", min_damage), false);
#endif
    attacker = &g_entities[1022];
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 4 && Scr_GetType(4u, SCRIPTINSTANCE_SERVER) )
        attacker = Scr_GetEntity(4u);
    mod = MOD_EXPLOSIVE;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 5 && Scr_GetType(5u, SCRIPTINSTANCE_SERVER) )
        mod = (meansOfDeath_t)G_MeansOfDeathFromScriptParam(5u);
    weapon = -1;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 6 && Scr_GetType(6u, SCRIPTINSTANCE_SERVER) )
    {
        String = Scr_GetString(6u, SCRIPTINSTANCE_SERVER);
        weapon = G_GetWeaponIndexForName(String);
    }
#ifdef KISAK_SP
    if (range > 65535.0f)
        Scr_Error(SCRIPTINSTANCE_SERVER, va("RadiusDamage range too large %g > %d.", range, 65535), false);
#endif
    level.bPlayerIgnoreRadiusDamage = level.bPlayerIgnoreRadiusDamageLatched;
    G_RadiusDamage(origin, inflictor, attacker, max_damage, min_damage, range, 1.0, 0, inflictor, mod, weapon);
    level.bPlayerIgnoreRadiusDamage = 0;
}

void __cdecl GScr_EntityRadiusDamage(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    GScr_RadiusDamageInternal(ent);
}

void GScr_GlassRadiusDamage()
{
    meansOfDeath_t mod; // [esp+18h] [ebp-1Ch]
    float max_damage; // [esp+1Ch] [ebp-18h]
    float origin[3]; // [esp+20h] [ebp-14h] BYREF
    float range; // [esp+2Ch] [ebp-8h]
    float min_damage; // [esp+30h] [ebp-4h]

    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    range = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    max_damage = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    min_damage = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    mod = MOD_EXPLOSIVE;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 4 && Scr_GetType(4u, SCRIPTINSTANCE_SERVER) )
        mod = (meansOfDeath_t)G_MeansOfDeathFromScriptParam(4u);
    GlassSv_RadiusDamage(origin, range, 1.0, 0, max_damage, min_damage, mod);
}

void __cdecl GScr_Detonate(scr_entref_t entref)
{
    gentity_s *ent; // [esp+0h] [ebp-Ch]
    const WeaponDef *weapDef; // [esp+4h] [ebp-8h]
    gentity_s *player; // [esp+8h] [ebp-4h]
    int savedregs; // [esp+Ch] [ebp+0h] BYREF

    ent = GetEntity(entref);
    weapDef = BG_GetWeaponDef(ent->s.weapon);
    if ( ent->s.eType != 4
        || !weapDef
        || weapDef->weapType != WEAPTYPE_GRENADE && weapDef->weapType != WEAPTYPE_PROJECTILE )
    {
        Scr_ObjectError("entity is not a grenade or projectile", SCRIPTINSTANCE_SERVER);
    }
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) )
        {
            player = Scr_GetEntity(0);
            if ( !player->client )
                Scr_ParamError(0, "Entity is not a player", SCRIPTINSTANCE_SERVER);
            ent->parent.setEnt(player);
        }
        else
        {
            ent->parent.setEnt(&g_entities[1022]);
        }
    }
    G_ExplodeMissile(ent);
}

void GScr_SetPlayerIgnoreRadiusDamage()
{
    level.bPlayerIgnoreRadiusDamageLatched = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_DamageConeTrace(scr_entref_t entref)
{
    GScr_DamageConeTraceInternal(entref, 8396819);
}

void __cdecl GScr_DamageConeTraceInternal(scr_entref_t entref, int contentMask)
{
#ifdef KISAK_SP
    gentity_s *target = GetEntity(entref);
    gentity_s *ignoreEnt = Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 0 ? Scr_GetEntity(1) : nullptr;
    float damageOrigin[3];
    Scr_GetVector(0, damageOrigin, SCRIPTINSTANCE_SERVER);
    Scr_AddFloat(G_CanDamage_SP(target, target->r.currentOrigin, ignoreEnt, damageOrigin,
                              3.4028234663852886e38f, 1.0f, nullptr, 0.0f, true, contentMask) ? 1.0f : 0.0f,
                 SCRIPTINSTANCE_SERVER);
#else
    double v2; // xmm0_8
    unsigned int NumParam; // eax
    long double v4; // [esp+Ch] [ebp-38h]
    float v5; // [esp+Ch] [ebp-38h]
    gentity_s *Entity; // [esp+14h] [ebp-30h]
    float damageAngles[3]; // [esp+18h] [ebp-2Ch] BYREF
    float coneAngleDegrees; // [esp+24h] [ebp-20h]
    gentity_s *target; // [esp+28h] [ebp-1Ch]
    float damageOrigin[3]; // [esp+2Ch] [ebp-18h] BYREF
    gentity_s *ignoreEnt; // [esp+38h] [ebp-Ch]
    float damageAmount; // [esp+3Ch] [ebp-8h]
    float coneAngleCos; // [esp+40h] [ebp-4h]

    target = GetEntity(entref);
    Scr_GetVector(0, damageOrigin, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 1 )
        Entity = 0;
    else
        Entity = Scr_GetEntity(1u);
    ignoreEnt = Entity;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 2 )
        Scr_GetVector(2u, damageAngles, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 3 )
        coneAngleDegrees = 65.0f;
    else
        coneAngleDegrees = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    //coneAngleDegrees = *((float *)&v4 + 1);
    //v2 = (float)(*((float *)&v4 + 1) * 0.017453292);
    //__libm_sse2_cos(v4);
    //*(float *)&v2 = v2;
    coneAngleCos = cos(coneAngleDegrees * 0.017453292);// * //*(float *)&v2;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 2 )
        v5 = 1.0f;
    else
        v5 = coneAngleCos;
    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    damageAmount = CanDamage(target, ignoreEnt, damageOrigin, v5, NumParam > 2 ? damageAngles : 0, contentMask);
    Scr_AddFloat(damageAmount, SCRIPTINSTANCE_SERVER);
#endif
}

void __cdecl GScr_SightConeTrace(scr_entref_t entref)
{
    GScr_DamageConeTraceInternal(entref, 14337);
}

void __cdecl GScr_PlayerSightTrace(scr_entref_t entref)
{
    gclient_s *client; // ecx
    float dist; // [esp+4Ch] [ebp-44h]
    int distance; // [esp+50h] [ebp-40h]
    float viewdir[3]; // [esp+54h] [ebp-3Ch] BYREF
    gentity_s *ent; // [esp+60h] [ebp-30h]
    int hitNum; // [esp+64h] [ebp-2Ch] BYREF
    float itemPosition[3]; // [esp+68h] [ebp-28h] BYREF
    float dot; // [esp+74h] [ebp-1Ch]
    float playerEyes[3]; // [esp+78h] [ebp-18h] BYREF
    float objdir[3]; // [esp+84h] [ebp-Ch] BYREF

    ent = GetEntity(entref);
    client = ent->client;
    playerEyes[0] = client->ps.origin[0];
    playerEyes[1] = client->ps.origin[1];
    playerEyes[2] = client->ps.origin[2];
    playerEyes[2] = playerEyes[2] + ent->client->ps.viewHeightCurrent;
    Scr_GetVector(0, itemPosition, SCRIPTINSTANCE_SERVER);
    distance = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    hitNum = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    AngleVectors(ent->r.currentAngles, viewdir, 0, 0);
    objdir[0] = itemPosition[0] - playerEyes[0];
    objdir[1] = itemPosition[1] - playerEyes[1];
    objdir[2] = itemPosition[2] - playerEyes[2];
    dist = Vec3Length(objdir);
    objdir[0] = (float)(1.0 / dist) * objdir[0];
    objdir[1] = (float)(1.0 / dist) * objdir[1];
    objdir[2] = (float)(1.0 / dist) * objdir[2];
    Vec3Normalize(viewdir);
    Vec3Normalize(objdir);
    dot = (float)((float)(viewdir[0] * objdir[0]) + (float)(viewdir[1] * objdir[1])) + (float)(viewdir[2] * objdir[2]);
    if ( dot >= 0.70700002 && dist <= (float)distance || dist < 100.0 && dot > 0.0 )
    {
        //col_context_t::col_context_t(&context, (int)&loc_806823);
        col_context_t context(0x806823); // [esp+24h] [ebp-6Ch] BYREF
        context.passEntityNum0 = ent->s.number;
        SV_SightTracePoint(&hitNum, playerEyes, itemPosition, &context);
        Scr_AddInt(hitNum, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_HeliTurretSightTrace(scr_entref_t entref)
{
    gclient_s *client; // edx
    gentity_s *copterEnt; // [esp+2Ch] [ebp-24h]
    float turretPosition[3]; // [esp+30h] [ebp-20h] BYREF
    int hitNum; // [esp+3Ch] [ebp-14h] BYREF
    gentity_s *player; // [esp+40h] [ebp-10h]
    float playerEyes[3]; // [esp+44h] [ebp-Ch] BYREF

    copterEnt = GetEntity(entref);
    Scr_GetVector(0, turretPosition, SCRIPTINSTANCE_SERVER);
    player = Scr_GetEntity(1u);
    client = player->client;
    playerEyes[0] = client->ps.origin[0];
    playerEyes[1] = client->ps.origin[1];
    playerEyes[2] = client->ps.origin[2];
    playerEyes[2] = playerEyes[2] + player->client->ps.viewHeightCurrent;
    hitNum = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    //col_context_t::col_context_t(&context, 14337);
    col_context_t context(0x3801); // [esp+4h] [ebp-4Ch] BYREF
    context.passEntityNum0 = copterEnt->s.number;
    SV_SightTracePoint(&hitNum, playerEyes, turretPosition, &context);
    Scr_AddInt(hitNum, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_HeliTurretDogTrace(scr_entref_t entref)
{
    gentity_s *dog; // [esp+4h] [ebp-4Ch]
    gentity_s *copterEnt; // [esp+30h] [ebp-20h]
    float turretPosition[3]; // [esp+34h] [ebp-1Ch] BYREF
    int hitNum; // [esp+40h] [ebp-10h] BYREF
    float dogEyes[3]; // [esp+44h] [ebp-Ch] BYREF

    copterEnt = GetEntity(entref);
    Scr_GetVector(0, turretPosition, SCRIPTINSTANCE_SERVER);
    dog = Scr_GetEntity(1u);
    dogEyes[0] = dog->r.currentOrigin[0];
    dogEyes[1] = dog->r.currentOrigin[1];
    dogEyes[2] = dog->r.currentOrigin[2];
    dogEyes[2] = dogEyes[2] + 24.0;
    hitNum = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    //col_context_t::col_context_t(&context, 14337);
    col_context_t context(0x3801); // [esp+8h] [ebp-48h] BYREF
    context.passEntityNum0 = copterEnt->s.number;
    SV_SightTracePoint(&hitNum, dogEyes, turretPosition, &context);
    Scr_AddInt(hitNum, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_VisionSetLerpRatio(scr_entref_t entref)
{
    float v1; // [esp+0h] [ebp-10h]
    float v2; // [esp+4h] [ebp-Ch]
    float visionSetLerpRatio; // [esp+8h] [ebp-8h]
    gentity_s *ent; // [esp+Ch] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("USAGE: Must be called on a client\n", 0);
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 8943, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    visionSetLerpRatio = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( (float)(visionSetLerpRatio - 1.0) < 0.0 )
        v2 = visionSetLerpRatio;
    else
        v2 = 1.0f;
    if ( (float)(0.0 - visionSetLerpRatio) < 0.0 )
        v1 = v2;
    else
        v1 = 0.0f;
    ent->client->ps.visionSetLerpRatio = v1;
}

void __cdecl GScr_DirectionalHitIndicator(scr_entref_t entref)
{
    gentity_s *pSelf; // [esp+0h] [ebp-20h]
    unsigned __int16 hitEntBitArray1; // [esp+4h] [ebp-1Ch]
    float zero_vec[3]; // [esp+8h] [ebp-18h] BYREF
    gentity_s *temporary_entity; // [esp+14h] [ebp-Ch]
    int hitEntBitArray0; // [esp+18h] [ebp-8h]
    int client_index; // [esp+1Ch] [ebp-4h]

    pSelf = GetEntity(entref);
    hitEntBitArray1 = 0;
    hitEntBitArray0 = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 2 || !Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
        Scr_Error("USAGE: DirectionalHitIndicator ( <victims0>, <victims1> )\n", 0);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
        hitEntBitArray1 = (unsigned __int16)Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    memset(zero_vec, 0, sizeof(zero_vec));
    temporary_entity = G_TempEntity(zero_vec, EV_DIRECTIONAL_HIT_INDICATOR);
    temporary_entity->r.clientMask[0] = -1;
    temporary_entity->s.eventParms[0] = hitEntBitArray0;
    temporary_entity->s.eventParms[1] = hitEntBitArray1;
    client_index = pSelf->client->ps.clientNum;
    temporary_entity->r.clientMask[client_index >> 5] &= ~(1 << (client_index & 0x1F));
}

void __cdecl GScr_DoCowardsWayAnims(scr_entref_t entref)
{
    float value; // xmm0_4
    gentity_s *ent; // [esp+Ch] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("USAGE: Must be called on a client\n", 0);
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 9014, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    value = (float)(BG_AnimScriptEvent(&g_pmove[ent->client->ps.clientNum], ANIM_ET_LASTSTAND_SUICIDE, 0, 1) - 100)
                / 1000.0;
    Scr_AddFloat(value, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_StartPoisoning(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("USAGE: Must be called on a client\n", 0);
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 9039, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    ent->client->ps.poisoned = 1;
}

void __cdecl GScr_StopPoisoning(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("USAGE: Must be called on a client\n", 0);
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 9060, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    ent->client->ps.poisoned = 0;
}

void __cdecl GScr_StartBinocs(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("USAGE: Must be called on a client\n", 0);
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 9081, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    ent->client->ps.binoculars = 1;
}

void __cdecl GScr_StopBinocs(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("USAGE: Must be called on a client\n", 0);
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 9102, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    ent->client->ps.binoculars = 0;
}

void __cdecl GScr_IsFlared(scr_entref_t entref)
{
    gentity_s *ent; // [esp+0h] [ebp-8h]
    unsigned int isFlared; // [esp+4h] [ebp-4h]

    isFlared = 0;
    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("USAGE: Must be called on a client\n", 0);
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 9124, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    if ( ent->client->ps.visionSetLerpRatio > 0.0 )
        isFlared = 1;
    Scr_AddBool(isFlared, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_IsPoisoned(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("USAGE: Must be called on a client\n", 0);
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 9147, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    Scr_AddBool(ent->client->ps.poisoned, SCRIPTINSTANCE_SERVER);
}

void GScr_GetMoveDelta()
{
    const XAnim_s *Anims; // eax
    unsigned int index; // [esp-Ch] [ebp-3Ch]
    float time1; // [esp+0h] [ebp-30h]
    float time2; // [esp+4h] [ebp-2Ch]
    int NumParam; // [esp+8h] [ebp-28h]
    float trans[3]; // [esp+10h] [ebp-20h] BYREF
    float endTime; // [esp+1Ch] [ebp-14h]
    float startTime; // [esp+20h] [ebp-10h]
    float rot[2]; // [esp+24h] [ebp-Ch] BYREF
    scr_anim_s anim; // [esp+2Ch] [ebp-4h]

    startTime = 0.0f;
    endTime = 1.0f;
    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( NumParam != 1 )
    {
        if ( NumParam != 2 )
        {
            endTime = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
            if ( endTime < 0.0 || endTime > 1.0 )
                Scr_ParamError(2u, "end time must be between 0 and 1", SCRIPTINSTANCE_SERVER);
        }
        startTime = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
        if ( startTime < 0.0 || startTime > 1.0 )
            Scr_ParamError(1u, "start time must be between 0 and 1", SCRIPTINSTANCE_SERVER);
    }
    anim = Scr_GetAnim(0, 0, SCRIPTINSTANCE_SERVER);
    time2 = endTime;
    time1 = startTime;
    index = anim.index;
    Anims = Scr_GetAnims(anim.tree, SCRIPTINSTANCE_SERVER);
    XAnimGetRelDelta(Anims, index, rot, trans, time1, time2);
    Scr_AddVector(trans, SCRIPTINSTANCE_SERVER);
}

void GScr_GetAngleDelta()
{
    const XAnim_s *Anims; // eax
    unsigned int index; // [esp-Ch] [ebp-3Ch]
    float time1; // [esp+0h] [ebp-30h]
    float time1a; // [esp+0h] [ebp-30h]
    float time2; // [esp+4h] [ebp-2Ch]
    int NumParam; // [esp+8h] [ebp-28h]
    float trans[3]; // [esp+10h] [ebp-20h] BYREF
    float endTime; // [esp+1Ch] [ebp-14h]
    float startTime; // [esp+20h] [ebp-10h]
    float rot[2]; // [esp+24h] [ebp-Ch] BYREF
    scr_anim_s anim; // [esp+2Ch] [ebp-4h]

    startTime = 0.0f;
    endTime = 1.0f;
    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( NumParam != 1 )
    {
        if ( NumParam != 2 )
        {
            endTime = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
            if ( endTime < 0.0 || endTime > 1.0 )
                Scr_ParamError(2u, "end time must be between 0 and 1", SCRIPTINSTANCE_SERVER);
        }
        startTime = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
        if ( startTime < 0.0 || startTime > 1.0 )
            Scr_ParamError(1u, "start time must be between 0 and 1", SCRIPTINSTANCE_SERVER);
    }
    anim = Scr_GetAnim(0, 0, SCRIPTINSTANCE_SERVER);
    time2 = endTime;
    time1 = startTime;
    index = anim.index;
    Anims = Scr_GetAnims(anim.tree, SCRIPTINSTANCE_SERVER);
    XAnimGetRelDelta(Anims, index, rot, trans, time1, time2);
    time1a = RotationToYaw(rot);
    Scr_AddFloat(time1a, SCRIPTINSTANCE_SERVER);
}

void GScr_GetNorthYaw()
{
    char northYawString[32]; // [esp+Ch] [ebp-24h] BYREF

    SV_GetConfigstring(CS_NORTHYAW, northYawString, 32);
    Scr_AddFloat(atof(northYawString), SCRIPTINSTANCE_SERVER);
}

#ifdef KISAK_SP
static void GScr_SetNorthYaw_SP()
{
    char *northYawString = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    SV_SetConfigstring(CS_NORTHYAW, northYawString);
    const float radians = atof(northYawString) * 0.017453292f;
    level.compassNorth[0] = cos(radians);
    level.compassNorth[1] = sin(radians);
}
#endif

void Scr_LoadFX()
{
    char *filename; // [esp+0h] [ebp-8h]
    int id; // [esp+4h] [ebp-4h]

    filename = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( !I_strncmp(filename, "fx/", 3) )
        Scr_ParamError(0, "effect name should start after the 'fx' folder.", SCRIPTINSTANCE_SERVER);
    id = G_EffectIndex(filename);
    if ( !id && !level.initializing )
        Scr_Error(
            "loadFx must be called before any wait statements in the level script, or on an already loaded effect\n",
            0);
    Scr_AddInt(id, SCRIPTINSTANCE_SERVER);
}

void Scr_PlayFX()
{
    float pos[3]; // [esp+18h] [ebp-40h] BYREF
    int numParams; // [esp+24h] [ebp-34h]
    int fxId; // [esp+28h] [ebp-30h]
    gentity_s *ent; // [esp+2Ch] [ebp-2Ch]
    float axis[3][3]; // [esp+30h] [ebp-28h] BYREF
    float vecLength; // [esp+54h] [ebp-4h]

    numParams = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( numParams < 2 || numParams > 4 )
        Scr_Error("Incorrect number of parameters", 0);
    fxId = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, pos, SCRIPTINSTANCE_SERVER);
    ent = G_TempEntity(pos, EV_PLAY_FX);

    iassert(ent->s.lerp.apos.trType == TR_STATIONARY);

    ent->s.eventParm = (unsigned __int8)fxId;
    if ( numParams == 2 )
    {
        Scr_SetFxAngles(0, axis, ent->s.lerp.apos.trBase);
    }
    else
    {
        iassert(numParams == 3 || numParams == 4);

        Scr_GetVector(2u, axis[0], SCRIPTINSTANCE_SERVER);
        vecLength = Vec3Normalize(axis[0]);
        if ( vecLength == 0.0 )
            Scr_FxParamError(2u, "playFx called with (0 0 0) forward direction", fxId);
        if ( numParams == 3 )
        {
            Scr_SetFxAngles(1u, axis, ent->s.lerp.apos.trBase);
        }
        else
        {
            iassert(numParams == 4);

            Scr_GetVector(3u, axis[2], SCRIPTINSTANCE_SERVER);
            vecLength = Vec3Normalize(axis[2]);
            if ( vecLength == 0.0 )
                Scr_FxParamError(3u, "playFx called with (0 0 0) up direction", fxId);
            Scr_SetFxAngles(2u, axis, ent->s.lerp.apos.trBase);
        }
    }
}

void __cdecl Scr_SetFxAngles(unsigned int givenAxisCount, float (*axis)[3], float *angles)
{
    const char *v3; // eax
    float v4; // [esp+14h] [ebp-10h]

    if ( givenAxisCount > 2
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    9241,
                    0,
                    "givenAxisCount not in [0, 2]\n\t%i not in [%i, %i]",
                    givenAxisCount,
                    0,
                    2) )
    {
        __debugbreak();
    }
    if ( givenAxisCount == 1 )
    {
        vectoangles((const float *)axis, angles);
    }
    else if ( givenAxisCount == 2 )
    {
#ifdef KISAK_SP
        v4 = -((axis[2][2] * axis[0][2] + axis[2][1] * axis[0][1]) + axis[2][0] * axis[0][0]);
#else
        (v4) = -((float)((float)((*axis)[0] * (*axis)[6]) + (float)((*axis)[1] * (*axis)[7])) + (float)((*axis)[2] * (*axis)[8]));
#endif
#ifdef KISAK_SP
        axis[2][0] = v4 * axis[0][0] + axis[2][0];
        axis[2][1] = v4 * axis[0][1] + axis[2][1];
        axis[2][2] = v4 * axis[0][2] + axis[2][2];
#else
        (*axis)[6] = (float)(v4 * (*axis)[0]) + (*axis)[6];
        (*axis)[7] = (float)(v4 * (*axis)[1]) + (*axis)[7];
        (*axis)[8] = (float)(v4 * (*axis)[2]) + (*axis)[8];
#endif
#ifdef KISAK_SP
        const float length = std::sqrt((axis[2][0] * axis[2][0] + axis[2][1] * axis[2][1]) + axis[2][2] * axis[2][2]);
        const float inverse = 1.0f / (-length >= 0.0f ? 1.0f : length);
        for (int i = 0; i < 3; ++i)
            axis[2][i] *= inverse;
        if (length == 0.0f)
#else
        if ( Vec3Normalize(&(*axis)[6]) == 0.0 )
#endif
        {
            v3 = va("forward and up vectors are the same direction or exact opposite directions");
            Scr_Error(v3, 0);
        }
#ifdef KISAK_SP
        Vec3Cross(axis[2], axis[0], axis[1]);
#else
        Vec3Cross(&(*axis)[6], (const float *)axis, &(*axis)[3]);
#endif
        AxisToAngles(axis, angles);
    }
    else
    {
        *angles = 270.0f;
        angles[1] = 0.0f;
        angles[2] = 0.0f;
    }
}

void __cdecl Scr_FxParamError(unsigned int paramIndex, const char *errorString, int fxId)
{
    const char *v3; // eax
    char fxName[1028]; // [esp+0h] [ebp-408h] BYREF

    if ( !errorString
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 9285, 0, "%s", "errorString") )
    {
        __debugbreak();
    }
    if ( fxId )
        SV_GetConfigstring(fxId + 2080, fxName, 1024);
    else
        strcpy(fxName, "not successfully loaded");
    v3 = va("%s (effect = %s)\n", errorString, fxName);
    Scr_ParamError(paramIndex, v3, SCRIPTINSTANCE_SERVER);
}

void Scr_PlayFXOnTag()
{
    const char *v0; // eax
    char *v1; // eax
    char *v2; // eax
    unsigned int v3; // eax
    char *v4; // eax
    const char *v5; // eax
    char *v6; // eax
    char *v7; // eax
    char *v8; // [esp-8h] [ebp-18h]
    int fxId; // [esp+0h] [ebp-10h]
    gentity_s *ent; // [esp+4h] [ebp-Ch]
    unsigned int tag; // [esp+8h] [ebp-8h]
    signed int csIndex; // [esp+Ch] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3 )
        Scr_Error("Incorrect number of parameters", 0);
    fxId = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( fxId <= 0 || fxId >= 196 )
    {
        v0 = va("effect id %i is invalid\n", fxId);
        Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
    }
    ent = Scr_GetEntity(1u);
    if ( !ent->model )
        Scr_ParamError(1u, "cannot play fx on entity with no model", SCRIPTINSTANCE_SERVER);
    tag = Scr_GetConstLowercaseString(2u, SCRIPTINSTANCE_SERVER);
    v1 = SL_ConvertToString(tag, SCRIPTINSTANCE_SERVER);
    v2 = strchr(v1, 0x22u);
    if ( v2 )
        Scr_ParamError(2u, "cannot use \" characters in tag names\n", SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
    // Retail SP Scr_PlayFXOnTag (BlackOps.exe 0x007fd380) accepts tag_origin
    // unconditionally -- the bone-index check is guarded by
    // `if (tag != scr_const.tag_origin)`, same idiom as this tree's own
    // ScrCmd_PlaySoundOnTag above. Without the bypass, frontend.gsc:1498/1504
    // play_aligned_fx() threw "tag 'tag_origin' does not exist" on
    // p_int_battery / p_int_security_camera (models with no real tag_origin
    // bone). Whether retail MP also has the bypass is unverified (only the SP
    // binary is available) -- MP arm left untouched.
    if ( tag != scr_const.tag_origin && SV_DObjGetBoneIndex(ent, tag) < 0 )
#else
    if ( SV_DObjGetBoneIndex(ent, tag) < 0 )
#endif
    {
        SV_DObjDumpInfo(ent);
        v3 = G_ModelName(ent->model);
        v8 = SL_ConvertToString(v3, SCRIPTINSTANCE_SERVER);
        v4 = SL_ConvertToString(tag, SCRIPTINSTANCE_SERVER);
        v5 = va("tag '%s' does not exist on entity with model '%s'", v4, v8);
        Scr_ParamError(2u, v5, SCRIPTINSTANCE_SERVER);
    }
    v6 = SL_ConvertToString(tag, SCRIPTINSTANCE_SERVER);
    v7 = va("%03d%s", fxId, v6);
    csIndex = G_FindConfigstringIndex(v7, 2276, 256, 1, 0);
    if ( (csIndex <= 0 || csIndex >= 256)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    9378,
                    0,
                    "%s",
                    "csIndex > 0 && csIndex < MAX_EFFECT_TAGS") )
    {
        __debugbreak();
    }
    G_AddEvent(ent, 0x47u, csIndex);
}

#ifdef KISAK_SP
static float GScr_NormalizeFXDirection_SP(float *direction, bool forward)
{
    const float length = forward
        ? std::sqrt((direction[2] * direction[2] + direction[1] * direction[1]) + direction[0] * direction[0])
        : std::sqrt((direction[0] * direction[0] + direction[1] * direction[1]) + direction[2] * direction[2]);
    const float inverse = 1.0f / (-length >= 0.0f ? 1.0f : length);
    for (int i = 0; i < 3; ++i)
        direction[i] *= inverse;
    return length;
}


static void GScr_CodePlayLoopedFX_SP()
{
    if ((unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 3
        || (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 6)
        Scr_Error("Incorrect number of parameters", 0);

    float axis[3][3];
    int givenAxisCount = 0;
    float cullDist = 0.0f;
    const int fxId = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (count == 6)
    {
        ++givenAxisCount;
        Scr_GetVector(5, axis[2], SCRIPTINSTANCE_SERVER);
        if (GScr_NormalizeFXDirection_SP(axis[2], false) == 0.0f)
            Scr_FxParamError(5, "playLoopedFx called with (0 0 0) up direction", fxId);
    }
    if (count == 5 || count == 6)
    {
        Scr_GetVector(4, axis[0], SCRIPTINSTANCE_SERVER);
        if (GScr_NormalizeFXDirection_SP(axis[0], true) == 0.0f)
            Scr_FxParamError(4, "playLoopedFx called with (0 0 0) forward direction", fxId);
        ++givenAxisCount;
    }
    if (count >= 4 && count <= 6)
        cullDist = Scr_GetFloat(3, SCRIPTINSTANCE_SERVER);

    float position[3];
    Scr_GetVector(2, position, SCRIPTINSTANCE_SERVER);
    const float milliseconds = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER) * 1000.0f;
    const double rounded = std::nearbyint((double)milliseconds + 9.313225746154785e-10);
    const int repeat = rounded >= -2147483648.0 && rounded < 2147483648.0 ? (int)rounded : INT_MIN;
    if (repeat <= 0)
        Scr_FxParamError(1, "playLoopedFx called with repeat < 0.001 seconds", fxId);

    gentity_s *ent = G_Spawn();
    ent->s.eType = ET_LOOP_FX;
    ent->s.un1.scale = (unsigned char)fxId;
    G_SetOrigin(ent, position);
    Scr_SetFxAngles(givenAxisCount, axis, ent->s.lerp.apos.trBase);
    ent->s.lerp.u.loopFx.cullDist = cullDist;
    ent->s.lerp.u.loopFx.period = repeat;
    SV_LinkEntity(ent);
    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
}

#endif

void Scr_PlayLoopedFX()
{
    int NumParam; // [esp+0h] [ebp-70h]
    float v1; // [esp+4h] [ebp-6Ch]
    float pos[3]; // [esp+2Ch] [ebp-44h] BYREF
    int fxId; // [esp+38h] [ebp-38h]
    int repeat; // [esp+3Ch] [ebp-34h]
    gentity_s *ent; // [esp+40h] [ebp-30h]
    int givenAxisCount; // [esp+44h] [ebp-2Ch]
    float axis[3][3]; // [esp+48h] [ebp-28h] BYREF
    float cullDist; // [esp+6Ch] [ebp-4h]
    int savedregs; // [esp+70h] [ebp+0h] BYREF

    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 3
        || (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 6 )
    {
        Scr_Error("Incorrect number of parameters", 0);
    }
    givenAxisCount = 0;
    cullDist = 0.0f;
    fxId = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( NumParam != 4 )
    {
        if ( NumParam != 5 )
        {
            if ( NumParam != 6 )
                goto LABEL_13;
            ++givenAxisCount;
            Scr_GetVector(5u, axis[2], SCRIPTINSTANCE_SERVER);
            if ( Vec3Normalize(axis[2]) == 0.0 )
                Scr_FxParamError(5u, "playLoopedFx called with (0 0 0) up direction", fxId);
        }
        Scr_GetVector(4u, axis[0], SCRIPTINSTANCE_SERVER);
        if ( Vec3Normalize(axis[0]) == 0.0 )
            Scr_FxParamError(4u, "playLoopedFx called with (0 0 0) forward direction", fxId);
        ++givenAxisCount;
    }
    cullDist = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
LABEL_13:
    Scr_GetVector(2u, pos, SCRIPTINSTANCE_SERVER);
    v1 = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER) * 1000.0;
    repeat = (int)(v1 + 9.313225746154785e-10);
    if ( repeat <= 0 )
        Scr_FxParamError(1u, "playLoopedFx called with repeat < 0.001 seconds", fxId);
    ent = G_Spawn();
    ent->s.eType = ET_LOOP_FX;
    ent->r.svFlags |= 8u;
    ent->s.un1.scale = fxId;
    iassert(ent->s.un1.eventParm2 == fxId);

    G_SetOrigin(ent, pos);
    Scr_SetFxAngles(givenAxisCount, axis, ent->s.lerp.apos.trBase);
    ent->s.lerp.u.turret.gunAngles[0] = cullDist;
    ent->s.lerp.u.loopFx.period = repeat;
    SV_LinkEntity(ent);
    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
}

void Scr_SpawnFX()
{
#ifdef KISAK_SP
    if ((unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 2
        || (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 4)
    {
        Scr_Error("Incorrect number of parameters", 0);
    }

    int givenAxisCount = 0;
    const int fxId = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    const int numParams = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    float axis[3][3];
    if (numParams == 4)
    {
        Scr_GetVector(3, axis[2], SCRIPTINSTANCE_SERVER);
        const float length = std::sqrt((axis[2][0] * axis[2][0] + axis[2][1] * axis[2][1]) + axis[2][2] * axis[2][2]);
        const float inverse = 1.0f / (-length >= 0.0f ? 1.0f : length);
        for (int i = 0; i < 3; ++i)
            axis[2][i] *= inverse;
        if (length == 0.0f)
            Scr_FxParamError(3, "spawnFx called with (0 0 0) up direction", fxId);
        ++givenAxisCount;
    }
    if (numParams == 3 || numParams == 4)
    {
        Scr_GetVector(2, axis[0], SCRIPTINSTANCE_SERVER);
        const float length = std::sqrt((axis[0][2] * axis[0][2] + axis[0][0] * axis[0][0]) + axis[0][1] * axis[0][1]);
        const float inverse = 1.0f / (-length >= 0.0f ? 1.0f : length);
        for (int i = 0; i < 3; ++i)
            axis[0][i] *= inverse;
        if (length == 0.0f)
            Scr_FxParamError(2, "spawnFx called with (0 0 0) forward direction", fxId);
        ++givenAxisCount;
    }

    float pos[3];
    Scr_GetVector(1, pos, SCRIPTINSTANCE_SERVER);
    gentity_s *ent = G_Spawn();
    ent->s.eType = ET_FX;
    ent->s.un1.scale = static_cast<unsigned char>(fxId);
    G_SetOrigin(ent, pos);
    Scr_SetFxAngles(givenAxisCount, axis, ent->s.lerp.apos.trBase);
    SV_LinkEntity(ent);
    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
#else
    int NumParam; // [esp+0h] [ebp-58h]
    float pos[3]; // [esp+1Ch] [ebp-3Ch] BYREF
    int fxId; // [esp+28h] [ebp-30h]
    gentity_s *ent; // [esp+2Ch] [ebp-2Ch]
    int givenAxisCount; // [esp+30h] [ebp-28h]
    float axis[3][3]; // [esp+34h] [ebp-24h] BYREF
    int savedregs; // [esp+58h] [ebp+0h] BYREF

    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 2
        || (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 4 )
    {
        Scr_Error("Incorrect number of parameters", 0);
    }
    givenAxisCount = 0;
    fxId = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( NumParam != 3 )
    {
        if ( NumParam != 4 )
            goto LABEL_12;
        Scr_GetVector(3u, axis[2], SCRIPTINSTANCE_SERVER);
        if ( Vec3Normalize(axis[2]) == 0.0 )
            Scr_FxParamError(3u, "spawnFx called with (0 0 0) up direction", fxId);
        ++givenAxisCount;
    }
    Scr_GetVector(2u, axis[0], SCRIPTINSTANCE_SERVER);
    if ( Vec3Normalize(axis[0]) == 0.0 )
        Scr_FxParamError(2u, "spawnFx called with (0 0 0) forward direction", fxId);
    ++givenAxisCount;
LABEL_12:
    Scr_GetVector(1u, pos, SCRIPTINSTANCE_SERVER);
    ent = G_Spawn();
    ent->s.eType = ET_FX;
    ent->r.svFlags |= 8u;
    ent->s.un1.scale = fxId;
    if ( ent->s.un1.scale != fxId
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    9474,
                    0,
                    "ent->s.un1.eventParm2 == fxId\n\t%i, %i",
                    ent->s.un1.scale,
                    fxId) )
    {
        __debugbreak();
    }
    pos[0] = (float)(int)pos[0];
    pos[1] = (float)(int)pos[1];
    pos[2] = (float)(int)pos[2];
    G_SetOrigin(ent, pos);
    Scr_SetFxAngles(givenAxisCount, axis, ent->s.lerp.apos.trBase);
    iassert(ent->s.time2 == 0);

    SV_LinkEntity(ent);
    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
#endif
}

void Scr_TriggerFX()
{
    float v1; // [esp+4h] [ebp-14h]
    gentity_s *ent; // [esp+14h] [ebp-4h]

    if (!Scr_GetNumParam(SCRIPTINSTANCE_SERVER) || (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 2)
        Scr_Error("Incorrect number of parameters", 0);
    ent = Scr_GetEntity( 0);
    if (!ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 9496, 0, "%s", "ent"))
        __debugbreak();
    if (ent->s.eType != 8)
        Scr_ParamError(0, "entity wasn't created with 'newFx'", SCRIPTINSTANCE_SERVER);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2)
    {
        v1 = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER) * 1000.0;
        ent->s.time2 = (int)(v1 + 9.313225746154785e-10);
    }
    else
    {
        ent->s.time2 = level.time;
    }
}

void __cdecl ScrCmd_SpawnActor(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    double v3; // [esp+0h] [ebp-30h]
    double v4; // [esp+8h] [ebp-28h]
    double v5; // [esp+10h] [ebp-20h]
    const char *v6; // [esp+1Ch] [ebp-14h]
    gentity_s *guy; // [esp+20h] [ebp-10h]
    int noEnemyInfo; // [esp+24h] [ebp-Ch]
    VariableUnion targetname; // [esp+28h] [ebp-8h]
    gentity_s *ent; // [esp+2Ch] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->s.eType != 18 )
    {
        if ( ent->targetname )
            v6 = SL_ConvertToString(ent->targetname, SCRIPTINSTANCE_SERVER);
        else
            v6 = "<unnamed>";
        v5 = ent->r.currentOrigin[2];
        v4 = ent->r.currentOrigin[1];
        v3 = ent->r.currentOrigin[0];
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va(
                     "dospawn can only be called on actor spawners\n"
                     "attempted to call dospawn on entity with name '%s' of type '%s' at (%.0f %.0f %.0f)\n",
                     v6,
                     v1,
                     v3,
                     v4,
                     v5);
        Scr_Error(v2, 0);
    }
    noEnemyInfo = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
        noEnemyInfo = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    targetname.intValue = 0;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 2 )
        targetname.intValue = Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    guy = SpawnActor(ent, targetname.stringValue, FORCE_SPAWN, noEnemyInfo == 0);
    if ( guy )
    {
        ent->item[0].clipAmmoCount = level.time;
        Scr_AddEntity(guy, SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_CreateDynEntAndLaunch()
{
#ifdef KISAK_SP
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 5u)
        Scr_Error("CreateDynEntAndLaunch called with invalid params. CreateDynEntAndLaunch( <model>, <pos>, <angles>, <hitpos>, <force>, <fx> )", 0);

    int fxId = 0;
    int mature = 0;
    const int model = G_ModelIndex(Scr_GetString(0, SCRIPTINSTANCE_SERVER));
    float origin[3], angles[3], hitPos[3], force[3];
    Scr_GetVector(1, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2, angles, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(3, hitPos, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(4, force, SCRIPTINSTANCE_SERVER);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 5u)
        fxId = Scr_GetInt(5, SCRIPTINSTANCE_SERVER);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 6u)
        mature = Scr_GetInt(6, SCRIPTINSTANCE_SERVER);

    gentity_s *event = G_TempEntity(origin, EV_CREATE_DYNENT);
    event->s.lerp.pos.trDelta[0] = hitPos[0];
    event->s.lerp.pos.trDelta[1] = hitPos[1];
    event->s.lerp.pos.trDelta[2] = hitPos[2];
    event->s.lerp.apos.trBase[0] = angles[0];
    event->s.lerp.apos.trBase[1] = angles[1];
    event->s.lerp.apos.trBase[2] = angles[2];
    event->s.lerp.apos.trDelta[0] = force[0];
    event->s.lerp.apos.trDelta[1] = force[1];
    event->s.lerp.apos.trDelta[2] = force[2];
    event->s.lerp.u.createDynEnt.fxId = fxId;
    event->s.lerp.u.createDynEnt.mature = mature;
    event->s.lerp.u.createDynEnt.model = model;
#else
    char *String; // eax
    gentity_s *v1; // eax
    float *trDelta; // [esp+0h] [ebp-48h]
    float *trBase; // [esp+4h] [ebp-44h]
    float pos[3]; // [esp+Ch] [ebp-3Ch] BYREF
    float force[3]; // [esp+18h] [ebp-30h] BYREF
    float angles[3]; // [esp+24h] [ebp-24h] BYREF
    int fxId; // [esp+30h] [ebp-18h]
    float hitpos[3]; // [esp+34h] [ebp-14h] BYREF
    int modelIndex; // [esp+40h] [ebp-8h]
    gentity_s *tempent; // [esp+44h] [ebp-4h]

    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 5 )
        Scr_Error(
            "CreateDynEntAndLaunch called with invalid params. CreateDynEntAndLaunch( <model>, <pos>, <angles>, <hitpos>, <force>, <fx> )",
            0);
    fxId = 0;
    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    modelIndex = G_ModelIndex(String);
    Scr_GetVector(1u, pos, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2u, angles, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(3u, hitpos, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(4u, force, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 5 )
        fxId = Scr_GetInt(5u, SCRIPTINSTANCE_SERVER);
    v1 = G_TempEntity(pos, EV_CREATE_DYNENT);
    tempent = v1;
    v1->s.lerp.pos.trDelta[0] = hitpos[0];
    v1->s.lerp.pos.trDelta[1] = hitpos[1];
    v1->s.lerp.pos.trDelta[2] = hitpos[2];
    trBase = tempent->s.lerp.apos.trBase;
    tempent->s.lerp.apos.trBase[0] = angles[0];
    trBase[1] = angles[1];
    trBase[2] = angles[2];
    trDelta = tempent->s.lerp.apos.trDelta;
    tempent->s.lerp.apos.trDelta[0] = force[0];
    trDelta[1] = force[1];
    trDelta[2] = force[2];
    tempent->s.index.brushmodel = modelIndex;
    tempent->s.un1.scale = fxId;
#endif
}

#ifdef KISAK_SP
void GScr_SetCellVisibleAtPos_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 || Scr_GetType(0, SCRIPTINSTANCE_SERVER) != VAR_VECTOR)
    {
        Scr_Error("invalid call to SetCellVisibleAtPos. Check doc for correct usage!", 0);
        return;
    }
    float origin[3];
    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    if (!R_ClearCellForcedInvisible(R_CellForPoint(origin)))
        Scr_Error(va("SetCellVisibleAtPos failed. Pos not in valid cell (%.2f,%.2f,%.2f)\n",
            origin[0], origin[1], origin[2]), 0);
}

void GScr_SetCellInvisibleAtPos_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 || Scr_GetType(0, SCRIPTINSTANCE_SERVER) != VAR_VECTOR)
    {
        Scr_Error("invalid call to SetCellInvisibleAtPos. Check doc for correct usage!", 0);
        return;
    }
    float origin[3];
    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    if (!R_SetCellInvisible(R_CellForPoint(origin)))
        Scr_Error(va("SetCellInvisibleAtPos failed. Pos not in valid cell (%.2f,%.2f,%.2f)\n",
            origin[0], origin[1], origin[2]), 0);
}
#endif

void Scr_PhysicsExplosionSphere()
{
#ifdef KISAK_SP
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 4 || Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 6)
        Scr_Error("PhysicsExplosionSphere: Incorrect number of parameters", 0);
    float origin[3];
    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    gentity_s *ent = G_TempEntity(origin, EV_PHYS_EXPLOSION_SPHERE);
    ent->s.eventParm = (unsigned short)Scr_GetInt(1, SCRIPTINSTANCE_SERVER);
    LerpEntityStateExplosion &explosion = ent->s.lerp.u.explosion;
    explosion.innerRadius = Scr_GetFloat(2, SCRIPTINSTANCE_SERVER);
    explosion.innerDamage = 0.0f;
    explosion.outerDamage = 0.0f;
    if (explosion.innerRadius < 0.0f)
        Scr_ParamError(2, "PhysicsExplosionSphere: Radius is negative", SCRIPTINSTANCE_SERVER);
    if (explosion.innerRadius > (float)ent->s.eventParm)
        Scr_Error("PhysicsExplosionSphere: Inner radius is outside the outer radius", 0);
    explosion.magnitude = Scr_GetFloat(3, SCRIPTINSTANCE_SERVER);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 4)
    {
        explosion.outerDamage = Scr_GetFloat(4, SCRIPTINSTANCE_SERVER);
        if (explosion.outerDamage < 0.0f)
            Scr_Error("PhysicsExplosionSphere: outer damage is negative", 0);
        explosion.innerDamage = explosion.outerDamage;
    }
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 5)
    {
        explosion.innerDamage = Scr_GetFloat(5, SCRIPTINSTANCE_SERVER);
        if (explosion.innerDamage < 0.0f)
            Scr_Error("PhysicsExplosionSphere: inner damage is negative", 0);
        if (explosion.outerDamage > explosion.innerDamage)
            Scr_Error("PhysicsExplosionSphere: inner damage less than outer damage", 0);
    }
#else
    float pos[3];
    gentity_s *ent;

    if ((unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 4
        || (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 6)
    {
        Scr_Error("Incorrect number of parameters", 0);
    }
    Scr_GetVector(0, pos, SCRIPTINSTANCE_SERVER);
    ent = G_TempEntity(pos, EV_PHYS_EXPLOSION_SPHERE);
    ent->s.eventParm = (unsigned __int16)Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    ent->s.lerp.u.turret.gunAngles[0] = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    ent->s.lerp.u.actor.team = 0.0f;
    ent->s.lerp.u.destructibleHit.modelState3 = 0.0f;
    if (ent->s.lerp.u.turret.gunAngles[0] < 0.0)
        Scr_ParamError(2u, "Radius is negative", SCRIPTINSTANCE_SERVER);
    if (ent->s.lerp.u.turret.gunAngles[0] > (float)ent->s.eventParm)
        Scr_Error("Inner radius is outside the outer radius", 0);
    ent->s.lerp.u.turret.gunAngles[1] = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    if ((unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 4)
        ent->s.lerp.u.turret.heatVal = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
    if ((unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 5)
        ent->s.lerp.u.turret.gunAngles[2] = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
#endif
}

void Scr_CreateStreamerHint()
{
    float origin[3]; // [esp+0h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+Ch] [ebp-4h]
    int savedregs; // [esp+10h] [ebp+0h] BYREF

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("Incorrect number of parameters", 0);
    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    ent = G_Spawn();
    G_SetOrigin(ent, origin);
    ent->s.eType = ET_STREAMER_HINT;
    ent->flags |= FL_SUPPORTS_LINKTO;
    ent->s.lerp.pos.trType = 0;
    ent->s.lerp.u.turret.gunAngles[0] = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    if ( ent->s.lerp.u.turret.gunAngles[0] < 0.0 )
        Scr_ParamError(1u, "streamer hint factor is negative", SCRIPTINSTANCE_SERVER);
    SV_LinkEntity(ent);
    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
}

void Scr_PhysicsRadiusJolt()
{
    float pos[3]; // [esp+8h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+14h] [ebp-4h]

    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 4)
        Scr_Error("Incorrect number of parameters", 0);
    Scr_GetVector(0, pos, SCRIPTINSTANCE_SERVER);
    ent = G_TempEntity(pos, EV_PHYS_EXPLOSION_JOLT);
    ent->s.eventParm = (unsigned __int16)Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    ent->s.lerp.u.turret.gunAngles[0] = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    if (ent->s.lerp.u.turret.gunAngles[0] < 0.0)
        Scr_ParamError(2u, "Radius is negative", SCRIPTINSTANCE_SERVER);
    if (ent->s.lerp.u.turret.gunAngles[0] > (float)ent->s.eventParm)
        Scr_Error("Inner radius is outside the outer radius", 0);
    Scr_GetVector(3u, &ent->s.lerp.u.turret.gunAngles[1], SCRIPTINSTANCE_SERVER);
    if (0.0 == ent->s.lerp.u.turret.gunAngles[1]
        && ent->s.lerp.u.turret.gunAngles[2] == 0.0
        && ent->s.lerp.u.primaryLight.cosHalfFovOuter == 0.0)
    {
        ent->s.lerp.u.turret.gunAngles[1] = 1.1754944;// eN38;
    }
}

void Scr_PhysicsExplosionCylinder()
{
    float pos[3]; // [esp+0h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+Ch] [ebp-4h]

    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 4)
        Scr_Error("Incorrect number of parameters", 0);
    Scr_GetVector(0, pos, SCRIPTINSTANCE_SERVER);
    ent = G_TempEntity(pos, EV_PHYS_EXPLOSION_CYLINDER);
    ent->s.eventParm = (unsigned __int16)Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    ent->s.lerp.u.turret.gunAngles[0] = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    if (ent->s.lerp.u.turret.gunAngles[0] < 0.0)
        Scr_ParamError(2u, "Radius is negative", SCRIPTINSTANCE_SERVER);
    if (ent->s.lerp.u.turret.gunAngles[0] > (float)ent->s.eventParm)
        Scr_Error("Inner radius is outside the outer radius", 0);
    ent->s.lerp.u.turret.gunAngles[1] = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
}

void Scr_SetExponentialFog()
{
#ifdef KISAK_SP
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 6)
        Scr_Error(
            "Incorrect number of parameters\n"
            "USAGE: setExpFog(<startDist>, <halfwayDist>, <red>, <green>, <blue>, <transition time>)\n",
            SCRIPTINSTANCE_SERVER);
    const float start = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if (start < 0.0f)
        Scr_Error("setExpFog: startDist must be greater or equal to 0", SCRIPTINSTANCE_SERVER);
    const float distance = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    if (distance <= 0.0f)
        Scr_Error("setExpFog: halfwayDist must be greater than 0", SCRIPTINSTANCE_SERVER);
    const float red = Scr_GetFloat(2, SCRIPTINSTANCE_SERVER);
    const float green = Scr_GetFloat(3, SCRIPTINSTANCE_SERVER);
    const float blue = Scr_GetFloat(4, SCRIPTINSTANCE_SERVER);
    const float time = Scr_GetFloat(5, SCRIPTINSTANCE_SERVER);
    Scr_SetFog("setExpFog", start, 1.0f / distance, 0.0f, 0.0f, red, green, blue,
        time, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
#else
    float green; // [esp+48h] [ebp-1Ch]
    float startDist; // [esp+4Ch] [ebp-18h]
    float blue; // [esp+50h] [ebp-14h]
    float red; // [esp+54h] [ebp-10h]
    float density; // [esp+58h] [ebp-Ch]
    float time; // [esp+5Ch] [ebp-8h]
    float halfwayDist; // [esp+60h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 6 )
        Scr_Error(
            "Incorrect number of parameters\n"
            "USAGE: setExpFog(<startDist>, <halfwayDist>, <red>, <green>, <blue>, <transition time>)\n",
            0);
    startDist = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( startDist < 0.0 )
        Scr_Error("setExpFog: startDist must be greater or equal to 0", 0);
    halfwayDist = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    if ( halfwayDist <= 0.0 )
        Scr_Error("setExpFog: halfwayDist must be greater than 0", 0);
    density = 1.0 / halfwayDist;
    red = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    green = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    blue = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
    time = Scr_GetFloat(5u, SCRIPTINSTANCE_SERVER);
    Dvar_SetColor((dvar_s *)g_fogColorReadOnly, red, green, blue, 1.0);
    Dvar_SetFloat((dvar_s *)g_fogStartDistReadOnly, startDist);
    Dvar_SetFloat((dvar_s *)g_fogHalfDistReadOnly, halfwayDist);
    if ( ((float)(1.0 / halfwayDist) <= 0.0 || density > 1.0)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    9792,
                    0,
                    "%s\n\t(density) = %g",
                    "(density > 0 && density <= 1)",
                    density) )
    {
        __debugbreak();
    }
    Scr_SetFog(
        "setExpFog",
        startDist,
        density,
        0.0,
        0.0,
        red,
        green,
        blue,
        time,
        1.0,
        0.5,
        0.5,
        0.5,
        1.0,
        0.0,
        0.0,
        0.0,
        0.0,
        1.0);
#endif
}

#ifdef KISAK_SP
static void Scr_SetFogForClient_SP(
    int clientNum, const char *cmd, float start, float density, float heightDensity,
    float baseHeight, float r, float g, float b, float time, float colorScale,
    float sunColR, float sunColG, float sunColB, float sunDirX, float sunDirY,
    float sunDirZ, float sunStartAng, float sunEndAng, float maxFogOpacity)
{
    if (start < 0.0f)
        Scr_Error(va("%s: near distance must be >= 0", cmd), SCRIPTINSTANCE_SERVER);
    if (time < 0.0f)
        Scr_Error(va("%s: transition time must be >= 0 seconds", cmd), SCRIPTINSTANCE_SERVER);
    if (level.loading == 2)
        return;

    auto setFog = [&](int client)
    {
        G_setfog(client, va(
            "%g %g %g %g %g %g %g %.0f %g %g %g %g %g %g %g %g %g %g",
            start, density, heightDensity, baseHeight, r, g, b,
            (float)(time * 1000.0f), colorScale, sunColR, sunColG, sunColB,
            sunDirX, sunDirY, sunDirZ, sunStartAng, sunEndAng, maxFogOpacity));
    };
    if (clientNum >= 0)
        setFog(clientNum);
    else
        for (int i = 0; i < com_maxclients->current.integer; ++i)
            if (g_entities[i].client)
                setFog(i);
}

static void Scr_ReadVolumetricFog_SP(int clientNum, bool allClients)
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 8
        && Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 18)
        Scr_Error(
            "Incorrect number of parameters\n"
            "USAGE: setVolFog(<startDist>, <halfwayDist>, <halfwayHeight>, <baseHeight>, "
            "<red>, <green>, <blue>, <transition time>)\n"
            "OR:    SetVolFog(<startDist>, <halfwayDist>, <halfwayHeight>, <baseHeight>, "
            "<red>, <green>, <blue>, <fogColorScale>, <sunFogRed>, <sunFogGreen>, "
            "<sunFogBlue>, <sunFogDirX>, <sunFogDirY>, <sunFogDirZ>, <sunFogStartAng>, "
            "<sunFogEndAng>, <transition time>)\n", SCRIPTINSTANCE_SERVER);
    const float start = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if (start < 0.0f)
        Scr_Error("setExpFog: startDist must be greater or equal to 0", SCRIPTINSTANCE_SERVER);
    const float halfwayDist = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    if (halfwayDist <= 0.0f)
        Scr_Error("setVolFog: halfwayDist must be greater than 0", SCRIPTINSTANCE_SERVER);
    const float halfwayHeight = Scr_GetFloat(2, SCRIPTINSTANCE_SERVER);
    if (halfwayHeight < 0.0f)
        Scr_Error("setVolFog: halfwayHeight must be greater or equal to 0", SCRIPTINSTANCE_SERVER);
    const float baseHeight = Scr_GetFloat(3, SCRIPTINSTANCE_SERVER);
    const float density = 1.0f / halfwayDist;
    const float heightDensity = halfwayHeight >= 1.0f ? 1.0f / halfwayHeight : 0.0f;
    float red = Scr_GetFloat(4, SCRIPTINSTANCE_SERVER);
    float green = Scr_GetFloat(5, SCRIPTINSTANCE_SERVER);
    float blue = Scr_GetFloat(6, SCRIPTINSTANCE_SERVER);
    float scale;
    float sunColor[3] = {0.5f, 0.5f, 0.5f};
    float sunDir[3] = {1.0f, 0.0f, 0.0f};
    float sunStart = 0.0f;
    float sunEnd = 0.0f;
    float maxOpacity = 1.0f;
    float time;
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 18)
    {
        scale = Scr_GetFloat(7, SCRIPTINSTANCE_SERVER);
        sunColor[0] = Scr_GetFloat(8, SCRIPTINSTANCE_SERVER);
        sunColor[1] = Scr_GetFloat(9, SCRIPTINSTANCE_SERVER);
        sunColor[2] = Scr_GetFloat(10, SCRIPTINSTANCE_SERVER);
        sunDir[0] = Scr_GetFloat(11, SCRIPTINSTANCE_SERVER);
        sunDir[1] = Scr_GetFloat(12, SCRIPTINSTANCE_SERVER);
        sunDir[2] = Scr_GetFloat(13, SCRIPTINSTANCE_SERVER);
        sunStart = Scr_GetFloat(14, SCRIPTINSTANCE_SERVER);
        sunEnd = Scr_GetFloat(15, SCRIPTINSTANCE_SERVER);
        time = Scr_GetFloat(16, SCRIPTINSTANCE_SERVER);
        maxOpacity = Scr_GetFloat(17, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Com_Printf(1, "setVolFog: Old syntax used. Please update script.\n");
        scale = green > red ? green : red;
        if (blue > scale)
            scale = blue;
        const float inverseScale = 1.0f / scale;
        red *= inverseScale;
        green *= inverseScale;
        blue *= inverseScale;
        time = Scr_GetFloat(7, SCRIPTINSTANCE_SERVER);
    }
    if (allClients || clientNum >= 0)
        Scr_SetFogForClient_SP(clientNum, "setVolFog", start, density, heightDensity,
            baseHeight, red, green, blue, time, scale, sunColor[0], sunColor[1],
            sunColor[2], sunDir[0], sunDir[1], sunDir[2], sunStart, sunEnd, maxOpacity);
}
#endif

void __cdecl Scr_SetFog(
                const char *cmd,
                float start,
                float density,
                float heightDensity,
                float baseHeight,
                float r,
                float g,
                float b,
                float time,
                float colorScale,
                float sunColR,
                float sunColG,
                float sunColB,
                float sunDirX,
                float sunDirY,
                float sunDirZ,
                float sunStartAng,
                float sunEndAng,
                float maxFogOpacity)
{
#ifdef KISAK_SP
    Scr_SetFogForClient_SP(-1, cmd, start, density, heightDensity, baseHeight,
        r, g, b, time, colorScale, sunColR, sunColG, sunColB, sunDirX, sunDirY,
        sunDirZ, sunStartAng, sunEndAng, maxFogOpacity);
#else
    const char *v19; // eax
    const char *v20; // eax
    char *v21; // eax

    if ( start < 0.0 )
    {
        v19 = va("%s: near distance must be >= 0", cmd);
        Scr_Error(v19, 0);
    }
    if ( time < 0.0 )
    {
        v20 = va("%s: transition time must be >= 0 seconds", cmd);
        Scr_Error(v20, 0);
    }
    v21 = va(
                    "%g %g %g %g %g %g %g %.0f %g %g %g %g %g %g %g %g %g %g",
                    start,
                    density,
                    heightDensity,
                    baseHeight,
                    r,
                    g,
                    b,
                    (float)(time * 1000.0),
                    colorScale,
                    sunColR,
                    sunColG,
                    sunColB,
                    sunDirX,
                    sunDirY,
                    sunDirZ,
                    sunStartAng,
                    sunEndAng,
                    maxFogOpacity);
    G_setfog(v21);
#endif
}

void Scr_SetVolumetricFog()
{
#ifdef KISAK_SP
    Scr_ReadVolumetricFog_SP(-1, true);
#else
    float v0; // [esp+48h] [ebp-5Ch]
    float v1; // [esp+4Ch] [ebp-58h]
    float sunDirY; // [esp+54h] [ebp-50h]
    float green; // [esp+58h] [ebp-4Ch]
    float startDist; // [esp+5Ch] [ebp-48h]
    float baseHeight; // [esp+60h] [ebp-44h]
    float blue; // [esp+64h] [ebp-40h]
    float sunStartAng; // [esp+68h] [ebp-3Ch]
    float red; // [esp+6Ch] [ebp-38h]
    float fogColorScale; // [esp+70h] [ebp-34h]
    float maxFogOpacity; // [esp+74h] [ebp-30h]
    float density; // [esp+78h] [ebp-2Ch]
    float time; // [esp+7Ch] [ebp-28h]
    float sunColorB; // [esp+80h] [ebp-24h]
    float halfwayDist; // [esp+84h] [ebp-20h]
    float sunDirZ; // [esp+88h] [ebp-1Ch]
    float halfwayHeight; // [esp+8Ch] [ebp-18h]
    float sunStopAng; // [esp+90h] [ebp-14h]
    float sunColorR; // [esp+94h] [ebp-10h]
    float sunColorG; // [esp+98h] [ebp-Ch]
    float sunDirX; // [esp+9Ch] [ebp-8h]

    sunColorR = 0.5f;
    sunColorG = 0.5f;
    sunColorB = 0.5f;
    sunDirX = 1.0f;
    sunDirY = 0.0f;
    sunDirZ = 0.0f;
    sunStartAng = 0.0f;
    sunStopAng = 0.0f;
    maxFogOpacity = 1.0f;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 8 && Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 18 )
        Scr_Error(
            "Incorrect number of parameters\n"
            "USAGE: setVolFog(<startDist>, <halfwayDist>, <halfwayHeight>, <baseHeight>, <red>, <green>, <blue>, <transition ti"
            "me>)\n"
            "OR:        SetVolFog(<startDist>, <halfwayDist>, <halfwayHeight>, <baseHeight>, <red>, <green>, <blue>, <fogColorScale"
            ">, <sunFogRed>, <sunFogGreen>, <sunFogBlue>, <sunFogDirX>, <sunFogDirY>, <sunFogDirZ>, <sunFogStartAng>, <sunFogEn"
            "dAng>, <transition time>)\n",
            0);
    startDist = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( startDist < 0.0 )
        Scr_Error("setExpFog: startDist must be greater or equal to 0", 0);
    halfwayDist = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    if ( halfwayDist <= 0.0 )
        Scr_Error("setVolFog: halfwayDist must be greater than 0", 0);
    halfwayHeight = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    if ( halfwayHeight < 0.0 )
        Scr_Error("setVolFog: halfwayHeight must be greater or equal to 0", 0);
    baseHeight = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    density = 1.0 / halfwayDist;
    if ( halfwayHeight < 1.0 )
        v1 = 0.0f;
    else
        v1 = 1.0 / halfwayHeight;
    red = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
    green = Scr_GetFloat(5u, SCRIPTINSTANCE_SERVER);
    blue = Scr_GetFloat(6u, SCRIPTINSTANCE_SERVER);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 18 )
    {
        fogColorScale = Scr_GetFloat(7u, SCRIPTINSTANCE_SERVER);
        sunColorR = Scr_GetFloat(8u, SCRIPTINSTANCE_SERVER);
        sunColorG = Scr_GetFloat(9u, SCRIPTINSTANCE_SERVER);
        sunColorB = Scr_GetFloat(0xAu, SCRIPTINSTANCE_SERVER);
        sunDirX = Scr_GetFloat(0xBu, SCRIPTINSTANCE_SERVER);
        sunDirY = Scr_GetFloat(0xCu, SCRIPTINSTANCE_SERVER);
        sunDirZ = Scr_GetFloat(0xDu, SCRIPTINSTANCE_SERVER);
        sunStartAng = Scr_GetFloat(0xEu, SCRIPTINSTANCE_SERVER);
        sunStopAng = Scr_GetFloat(0xFu, SCRIPTINSTANCE_SERVER);
        time = Scr_GetFloat(0x10u, SCRIPTINSTANCE_SERVER);
        maxFogOpacity = Scr_GetFloat(0x11u, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Com_Printf(1, "setVolFog: Old syntax used. Please update script.\n");
        if ( green <= red )
            v0 = red;
        else
            v0 = green;
        fogColorScale = v0;
        if ( blue > v0 )
            fogColorScale = blue;
        red = red * (float)(1.0 / fogColorScale);
        green = green * (float)(1.0 / fogColorScale);
        blue = blue * (float)(1.0 / fogColorScale);
        time = Scr_GetFloat(7u, SCRIPTINSTANCE_SERVER);
    }
    if ( (density <= 0.0 || density > 1.0)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    9901,
                    0,
                    "%s\n\t(density) = %g",
                    "(density > 0 && density <= 1)",
                    density) )
    {
        __debugbreak();
    }
    if ( (v1 < 0.0 || v1 > 1.0)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    9902,
                    0,
                    "%s\n\t(heightDensity) = %g",
                    "(heightDensity >= 0 && heightDensity <= 1)",
                    v1) )
    {
        __debugbreak();
    }
    Scr_SetFog(
        "setVolFog",
        startDist,
        density,
        v1,
        baseHeight,
        red,
        green,
        blue,
        time,
        fogColorScale,
        sunColorR,
        sunColorG,
        sunColorB,
        sunDirX,
        sunDirY,
        sunDirZ,
        sunStartAng,
        sunStopAng,
        maxFogOpacity);
#endif
}

void Scr_SetCullDist()
{
    double Float; // st7
    char *v1; // eax

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("Incorrect number of parameters\n", 0);
    Float = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    v1 = va("%g", Float);
    SV_SetConfigstring(7, v1);
}

#ifdef KISAK_SP
static int GScr_VisionTransitionMsec_SP()
{
    const float milliseconds = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER) * 1000.0f;
    const double rounded = std::nearbyint((double)milliseconds + 9.313225746154785e-10);
    return rounded >= -2147483648.0 && rounded < 2147483648.0 ? (int)rounded : INT_MIN;
}
#endif

void Scr_VisionSetNaked()
{
#ifdef KISAK_SP
    char config[1024];
    SV_GetConfigstring(CS_VISIONSET_NAKED, config, sizeof(config));
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (count != 1 && count != 2)
        Scr_Error("USAGE: VisionSetNaked( <visionset name>, <transition time> )\n", SCRIPTINSTANCE_SERVER);
    const int duration = count == 2 ? GScr_VisionTransitionMsec_SP() : 1000;
    const char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    for (int i = 0; i < com_maxclients->current.integer; ++i)
    {
        const char *value = va("\"%s\" %i", name, duration);
        char *key = va("%i", i);
        Info_SetValueForKey(config, key, value);
    }
    SV_SetConfigstring(CS_VISIONSET_NAKED, config);
#else
    char *v0; // eax
    int NumParam; // [esp+0h] [ebp-1Ch]
    float v2; // [esp+4h] [ebp-18h]
    int duration; // [esp+14h] [ebp-8h]
    char *name; // [esp+18h] [ebp-4h]

    duration = 1000;
    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( NumParam == 1 )
        goto LABEL_4;
    if ( NumParam == 2 )
    {
        v2 = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER) * 1000.0;
        duration = (int)(v2 + 9.313225746154785e-10);
LABEL_4:
        name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        v0 = va("\"%s\" %i", name, duration);
        SV_SetConfigstring(1550, v0);
        return;
    }
    Scr_Error("USAGE: VisionSetNaked( <visionset name>, <transition time> )\n", 0);

#endif
}

void Scr_VisionSetNight()
{
#ifdef KISAK_SP
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (count != 1 && count != 2)
    {
        Scr_Error(SCRIPTINSTANCE_SERVER, "USAGE: VisionSetNight( <visionset name>, <transition time> )\n", false);
        return;
    }
    const int duration = count == 2 ? GScr_VisionTransitionMsec_SP() : 1000;
    const char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    SV_SetConfigstring(CS_VISIONSET_NIGHT, va("\"%s\" %i", name, duration));
#else
    char *v0; // eax
    int NumParam; // [esp+0h] [ebp-1Ch]
    float v2; // [esp+4h] [ebp-18h]
    int duration; // [esp+14h] [ebp-8h]
    char *name; // [esp+18h] [ebp-4h]

    duration = 1000;
    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( NumParam == 1 )
        goto LABEL_4;
    if ( NumParam == 2 )
    {
        v2 = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER) * 1000.0;
        duration = (int)(v2 + 9.313225746154785e-10);
LABEL_4:
        name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        v0 = va("\"%s\" %i", name, duration);
        SV_SetConfigstring(1551, v0);
        return;
    }
    Scr_Error("USAGE: VisionSetNight( <visionset name>, <transition time> )\n", 0);
#endif
}

void Scr_TableLookupRowNum()
{
    char *filename; // [esp+4h] [ebp-14h]
    char *stringValue; // [esp+8h] [ebp-10h]
    int returnValueRow; // [esp+Ch] [ebp-Ch]
    const StringTable *tablePtr; // [esp+10h] [ebp-8h] BYREF
    int comparisonColumn; // [esp+14h] [ebp-4h]

    if ( useFastFile->current.enabled )
    {
        if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 2 )
            Scr_Error("USAGE: tableLookupRowNum( filename, searchColumnNum, searchValue )\n", 0);
        filename = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        StringTable_GetAsset(filename, (XAssetHeader *)&tablePtr);
        comparisonColumn = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        stringValue = Scr_GetString(2u, SCRIPTINSTANCE_SERVER);
        returnValueRow = StringTable_LookupRowNumForValue(tablePtr, comparisonColumn, stringValue);
        Scr_AddInt(returnValueRow, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Com_Printf(16, "You cannot do table lookups without fastfiles.\n");
        Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
    }
}

void Scr_TableLookupColumnForRow()
{
    char *filename; // [esp+4h] [ebp-14h]
    char *returnValue; // [esp+8h] [ebp-10h]
    const StringTable *tablePtr; // [esp+Ch] [ebp-Ch] BYREF
    int row; // [esp+10h] [ebp-8h]
    int column; // [esp+14h] [ebp-4h]

    if ( useFastFile->current.enabled )
    {
        if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 2 )
            Scr_Error("USAGE: tableLookupColumnForRow( filename, row, column )\n", 0);
        filename = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        StringTable_GetAsset(filename, (XAssetHeader *)&tablePtr);
        row = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        column = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
        returnValue = (char *)StringTable_GetColumnValueForRow(tablePtr, row, column);
        Scr_AddString(returnValue, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Com_Printf(16, "You cannot do table lookups without fastfiles.\n");
        Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
    }
}

void Scr_TableLookup()
{
    char *stringValue; // [esp+4h] [ebp-18h]
    char *filename; // [esp+8h] [ebp-14h]
    char *returnValue; // [esp+Ch] [ebp-10h]
    const StringTable *tablePtr; // [esp+10h] [ebp-Ch] BYREF
    int returnValueColumn; // [esp+14h] [ebp-8h]
    int comparisonColumn; // [esp+18h] [ebp-4h]

    if ( useFastFile->current.enabled )
    {
        if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 3 )
            Scr_Error("USAGE: tableLookup( filename, searchColumnNum, searchValue, returnValueColumnNum )\n", 0);
        filename = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        StringTable_GetAsset(filename, (XAssetHeader *)&tablePtr);
        comparisonColumn = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        stringValue = Scr_GetString(2u, SCRIPTINSTANCE_SERVER);
        returnValueColumn = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
        returnValue = (char *)StringTable_Lookup(tablePtr, comparisonColumn, stringValue, returnValueColumn);
        Scr_AddString(returnValue, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Com_Printf(16, "You cannot do table lookups without fastfiles.\n");
        Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
    }
}

void Scr_TableLookupIString()
{
    char *stringValue; // [esp+4h] [ebp-18h]
    char *filename; // [esp+8h] [ebp-14h]
    char *returnValue; // [esp+Ch] [ebp-10h]
    const StringTable *tablePtr; // [esp+10h] [ebp-Ch] BYREF
    int returnValueColumn; // [esp+14h] [ebp-8h]
    int comparisonColumn; // [esp+18h] [ebp-4h]

    if ( useFastFile->current.enabled )
    {
        if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 3 )
            Scr_Error("USAGE: tableLookupIString( filename, searchColumnNum, searchValue, returnValueColumnNum )\n", 0);
        filename = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        StringTable_GetAsset(filename, (XAssetHeader *)&tablePtr);
        comparisonColumn = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        stringValue = Scr_GetString(2u, SCRIPTINSTANCE_SERVER);
        returnValueColumn = Scr_GetInt(3u, SCRIPTINSTANCE_SERVER);
        returnValue = (char *)StringTable_Lookup(tablePtr, comparisonColumn, stringValue, returnValueColumn);
        Scr_AddIString(returnValue, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Com_Printf(16, "You cannot do table lookups without fastfiles.\n");
        Scr_AddIString((char *)"", SCRIPTINSTANCE_SERVER);
    }
}

float locs[255][3];
void __cdecl Scr_GetReflectionLocs()
{
    unsigned int i; // [esp+0h] [ebp-8h]
    unsigned int count; // [esp+4h] [ebp-4h]

    count = R_GetDebugReflectionProbeLocs(locs, 0xFFu);
    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for ( i = 0; i < count; ++i )
    {
        Scr_AddVector(locs[i], SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl Scr_GetReflectionOrigin()
{
    float probePos[3]; // [esp+0h] [ebp-1Ch] BYREF
    float queryPos[3]; // [esp+Ch] [ebp-10h] BYREF
    unsigned int index; // [esp+18h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Com_Error(ERR_DROP, "Invalid origin");
    Scr_GetVector(0, queryPos, SCRIPTINSTANCE_SERVER);
    index = R_CalcReflectionProbeIndex(queryPos);
    R_GetReflectionProbePosition(index, probePos);
    Scr_AddVector(probePos, SCRIPTINSTANCE_SERVER);
}

void GScr_IsPlayer()
{
    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == 1
        && Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == 19
        && Scr_GetEntity(0)->client )
    {
        Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_IsPlayerNumber()
{
    if ( Scr_GetInt(0, SCRIPTINSTANCE_SERVER) < 0x20 )
        Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}

void GScr_SetWinningPlayer()
{
    char *v0; // eax
    char *pszWinner; // [esp+0h] [ebp-410h]
    int iWinner; // [esp+4h] [ebp-40Ch]
    char buffer[1024]; // [esp+8h] [ebp-408h] BYREF
    gentity_s *pEnt; // [esp+40Ch] [ebp-4h]

    pEnt = Scr_GetEntity(0);
    iWinner = pEnt->s.number + 1;
    SV_GetConfigstring(0x15u, buffer, 1024);
    pszWinner = va("%i", iWinner);
    v0 = Info_ValueForKey(buffer, "winner");
    if ( I_stricmp(v0, pszWinner) )
    {
        Info_SetValueForKey(buffer, "winner", pszWinner);
        SV_SetConfigstring(21, buffer);
    }
}

void GScr_SetWinningTeam()
{
    char *v0; // eax
    const char *v1; // eax
    char *v2; // eax
    char *pszWinner; // [esp+0h] [ebp-414h]
    unsigned __int16 team; // [esp+4h] [ebp-410h]
    int iWinner; // [esp+8h] [ebp-40Ch]
    char buffer[1028]; // [esp+Ch] [ebp-408h] BYREF

    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( team == scr_const.allies )
    {
        iWinner = -2;
    }
    else if ( team == scr_const.axis )
    {
        iWinner = -1;
    }
    else
    {
        if ( team != scr_const.none )
        {
            v0 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
            v1 = va("Illegal team string '%s'. Must be allies, axis, or none.", v0);
            Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
            return;
        }
        iWinner = 0;
    }
    SV_GetConfigstring(0x15u, buffer, 1024);
    pszWinner = va("%i", iWinner);
    v2 = Info_ValueForKey(buffer, "winner");
    if ( I_stricmp(v2, pszWinner) )
    {
        Info_SetValueForKey(buffer, "winner", pszWinner);
        SV_SetConfigstring(21, buffer);
    }
}

void GScr_Announcement()
{
    int NumParam; // eax
    int v1; // eax
    VariableUnion v2; // eax
    const char *v3; // eax
    char string[1028]; // [esp+0h] [ebp-408h] BYREF

    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    Scr_ConstructMessageString(0, NumParam - 2, "Announcement", string, 0x400u);
    v1 = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    v2.intValue = Scr_GetInt(v1 - 1, SCRIPTINSTANCE_SERVER);
    v3 = va("%c \"%s\" %i", 99, string, v2.intValue);
    SV_GameSendServerCommand(-1, SV_CMD_CAN_IGNORE, v3);
}

void GScr_ClientAnnouncement()
{
    int NumParam; // eax
    int v1; // eax
    VariableUnion v2; // eax
    const char *v3; // eax
    char string[1024]; // [esp+0h] [ebp-408h] BYREF
    gentity_s *pEnt; // [esp+404h] [ebp-4h]

    pEnt = Scr_GetEntity(0);
    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    Scr_ConstructMessageString(1, NumParam - 2, "Announcement", string, 0x400u);
    v1 = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    v2.intValue = Scr_GetInt(v1 - 1, SCRIPTINSTANCE_SERVER);
    v3 = va("%c \"%s\" %i", 99, string, v2.intValue);
    SV_GameSendServerCommand(pEnt->s.number, SV_CMD_CAN_IGNORE, v3);
}

void GScr_GetTeamScore()
{
    char *v0; // eax
    const char *v1; // eax
    unsigned __int16 team; // [esp+4h] [ebp-4h]

    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( team != scr_const.allies && team != scr_const.axis )
    {
        v0 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v1 = va("Illegal team string '%s'. Must be allies, or axis.", v0);
        Scr_Error(v1, 0);
    }
    if ( team == scr_const.allies )
        Scr_AddInt(level.teamScores[2], SCRIPTINSTANCE_SERVER);
    else
        Scr_AddInt(level.teamScores[1], SCRIPTINSTANCE_SERVER);
}

void GScr_SetTeamScore()
{
    char *v0; // eax
    const char *v1; // eax
    const char *v2; // eax
    unsigned __int16 team; // [esp+0h] [ebp-8h]
    int teamScore; // [esp+4h] [ebp-4h]

    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( team != scr_const.allies && team != scr_const.axis )
    {
        v0 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v1 = va("Illegal team string '%s'. Must be allies, or axis.", v0);
        Scr_Error(v1, 0);
    }
    teamScore = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    if ( team == scr_const.allies )
    {
        level.teamScores[2] = teamScore;
        v2 = va("%c %i", 72, teamScore);
    }
    else
    {
        level.teamScores[1] = teamScore;
        v2 = va("%c %i", 71, teamScore);
    }
    SV_GameSendServerCommand(-1, SV_CMD_CAN_IGNORE, v2);
    level.bUpdateScoresForIntermission = 1;
}

void GScr_SetClientNameMode()
{
    unsigned __int16 mode; // [esp+0h] [ebp-4h]

    mode = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( mode == scr_const.auto_change )
    {
        level.manualNameChange = 0;
    }
    else if ( mode == scr_const.manual_change )
    {
        level.manualNameChange = 1;
    }
    else
    {
        Scr_Error("Unknown mode", 0);
    }
}

void GScr_UpdateClientNames()
{
    gclient_s *clients; // [esp+14h] [ebp-2Ch]
    char oldname[32]; // [esp+18h] [ebp-28h] BYREF
    int i; // [esp+3Ch] [ebp-4h]

    if (!level.manualNameChange)
        Scr_Error("Only works in [manual_change] mode", 0);
    i = 0;
    clients = level.clients;
    while (i < level.maxclients)
    {
        if (clients->sess.connected == CON_CONNECTED)
        {
            if (strcmp(clients->sess.cs.name, clients->sess.newnetname))
            {
                I_strncpyz(oldname, clients->sess.cs.name, 32);
                I_strncpyz(clients->sess.cs.name, clients->sess.newnetname, 32);
                ClientUserinfoChanged(i);
            }
        }
        ++i;
        ++clients;
    }
}

void GScr_GetTeamPlayersAlive()
{
    char *v0; // eax
    const char *v1; // eax
    int iLivePlayers; // [esp+0h] [ebp-14h]
    unsigned __int16 team; // [esp+4h] [ebp-10h]
    int iTeamNum; // [esp+8h] [ebp-Ch]
    gentity_s *pEnt; // [esp+Ch] [ebp-8h]
    int i; // [esp+10h] [ebp-4h]

    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( team != scr_const.allies && team != scr_const.axis )
    {
        v0 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v1 = va("Illegal team string '%s'. Must be allies, or axis.", v0);
        Scr_Error(v1, 0);
    }
    if ( team == scr_const.allies )
        iTeamNum = 2;
    else
        iTeamNum = 1;
    iLivePlayers = 0;
    for ( i = 0; i < com_maxclients->current.integer; ++i )
    {
        pEnt = &g_entities[i];
        if ( pEnt->r.inuse && pEnt->client->sess.cs.team == iTeamNum && pEnt->health > 0 )
            ++iLivePlayers;
    }
    Scr_AddInt(iLivePlayers, SCRIPTINSTANCE_SERVER);
}

void GScr_GetDroppedWeapons()
{
    gentity_s *ent; // [esp+0h] [ebp-8h]
    int i; // [esp+4h] [ebp-4h]

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for (i = 0; i < 32; ++i)
    {
        if (level.droppedWeaponCue[i].isDefined())
        {
            //ent = EntHandle::ent(&level.droppedWeaponCue[i]);
            ent = level.droppedWeaponCue[i].ent();
            Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
            Scr_AddArray(SCRIPTINSTANCE_SERVER);
        }
    }
}

void __cdecl GScr_GetNumParts()
{
    char *String; // eax
    int v1; // eax
    XModel *model; // [esp+0h] [ebp-4h]

    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    model = SV_XModelGet(String);
    v1 = XModelNumBones(model);
    Scr_AddInt(v1, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_GetPartName()
{
    char *String; // eax
    const char *v1; // eax
    XModel *model; // [esp+0h] [ebp-10h]
    unsigned __int16 name; // [esp+4h] [ebp-Ch]
    unsigned int index; // [esp+8h] [ebp-8h]
    unsigned int numbones; // [esp+Ch] [ebp-4h]

    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    model = SV_XModelGet(String);
    index = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    numbones = XModelNumBones(model);
    if ( index >= numbones )
    {
        v1 = va("index out of range (0 - %d)", numbones - 1);
        Scr_ParamError(1u, v1, SCRIPTINSTANCE_SERVER);
    }
    name = XModelBoneNames(model)[index];
    if ( !name )
        Scr_ParamError(0, "bad model", SCRIPTINSTANCE_SERVER);
    Scr_AddConstString(name, SCRIPTINSTANCE_SERVER);
}

#ifdef KISAK_SP
void GScr_Earthquake()
{
    const float scale = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    const float milliseconds = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER) * 1000.0f;
    const double rounded = std::nearbyint((double)milliseconds + 9.313225746154785e-10);
    const int duration = rounded >= -2147483648.0 && rounded < 2147483648.0
        ? (int)rounded : (std::numeric_limits<int>::min)();
    float source[3];
    Scr_GetVector(2, source, SCRIPTINSTANCE_SERVER);
    const float radius = Scr_GetFloat(3, SCRIPTINSTANCE_SERVER);
    if (scale <= 0.0f)
        Scr_ParamError(0, "Scale must be greater than 0", SCRIPTINSTANCE_SERVER);
    if (duration <= 0)
        Scr_ParamError(1, "duration must be greater than 0", SCRIPTINSTANCE_SERVER);
    if (radius <= 0.0f)
        Scr_ParamError(3, "Radius must be greater than 0", SCRIPTINSTANCE_SERVER);
    gentity_s *target = (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 5
        ? nullptr : Scr_GetEntity(4);
    if (!g_earthquakeEnable->current.enabled)
        return;

    gentity_s *tent = G_TempEntity(source, EV_EARTHQUAKE);
    tent->s.lerp.u.earthquake.scale = scale;
    tent->s.lerp.u.earthquake.duration = duration;
    tent->s.lerp.u.earthquake.radius = radius;
    if (target && target->client)
    {
        tent->r.clientMask[0] = -1;
        const int clientNum = target->client->ps.clientNum;
        tent->r.clientMask[clientNum >> 5] &= ~(1 << (clientNum & 0x1F));
    }
}
#else
void GScr_Earthquake()
{
    float v1; // [esp+0h] [ebp-34h]
    int clientNum; // [esp+10h] [ebp-24h]
    float source[3]; // [esp+14h] [ebp-20h] BYREF
    gentity_s *tent; // [esp+20h] [ebp-14h]
    int duration; // [esp+24h] [ebp-10h]
    gentity_s *target; // [esp+28h] [ebp-Ch]
    float radius; // [esp+2Ch] [ebp-8h]
    float scale; // [esp+30h] [ebp-4h]

    scale = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    v1 = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER) * 1000.0;
    duration = (int)(v1 + 9.313225746154785e-10);
    Scr_GetVector(2u, source, SCRIPTINSTANCE_SERVER);
    radius = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    if (scale <= 0.0)
        Scr_ParamError(0, "Scale must be greater than 0", SCRIPTINSTANCE_SERVER);
    if (duration <= 0)
        Scr_ParamError(1u, "duration must be greater than 0", SCRIPTINSTANCE_SERVER);
    if (radius <= 0.0)
        Scr_ParamError(3u, "Radius must be greater than 0", SCRIPTINSTANCE_SERVER);
    if ((unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 5)
        target = 0;
    else
        target = Scr_GetEntity(4u);
    tent = G_TempEntity(source, EV_EARTHQUAKE);
    tent->s.lerp.u.turret.gunAngles[0] = scale;
    tent->s.lerp.u.actor.team = duration;
    tent->s.lerp.u.turret.gunAngles[1] = radius;
    if (target)
    {
        if (target->client)
        {
            tent->r.clientMask[0] = -1;
            clientNum = target->client->ps.clientNum;
            tent->r.clientMask[clientNum >> 5] &= ~(1 << (clientNum & 0x1F));
        }
    }
}
#endif

#ifdef KISAK_SP
void __cdecl GScr_ShellShock(scr_entref_t entref)
{
    gentity_s *ent = GetPlayerEntity(entref);
    if ((unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 2)
        Scr_Error("USAGE: <player> shellshock(<shellshockname>, <duration>)\n", 0);

    char *shock = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const float milliseconds = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER) * 1000.0f;
    const double rounded = std::nearbyint((double)milliseconds + 9.313225746154785e-10);
    const int duration = rounded >= -2147483648.0 && rounded < 2147483648.0
        ? (int)rounded : (std::numeric_limits<int>::min)();
    if (duration < 0 || duration > 60000)
        Scr_ParamError(1, va("duration %g should be >= 0 and <= 60",
            (float)duration * 0.001f), SCRIPTINSTANCE_SERVER);

    const playerState_s *ps = &ent->client->ps;
    if (ps->shellshockDuration > duration
        && ps->shellshockDuration > std::bit_cast<int>(
            (unsigned int)level.time - (unsigned int)ps->shellshockTime))
        return;

    char configstring[1024];
    for (int id = 1; id < 16; ++id)
    {
        SV_GetConfigstring(CS_SHELLSHOCKS + id, configstring, sizeof(configstring));
        if (!I_stricmp(configstring, shock))
        {
            ent->client->ps.shellshockIndex = id;
            ent->client->ps.shellshockTime = level.time;
            ent->client->ps.shellshockDuration = duration;
            ent->client->ps.pm_flags |= 0x10000u;
            return;
        }
    }
    Scr_Error(va("shellshock '%s' was not precached\n", shock), 0);
}
#else
void __cdecl GScr_ShellShock(scr_entref_t entref)
{
    const char *v1; // eax
    const char *v2; // eax
    float v3; // [esp+30h] [ebp-428h]
    int duration; // [esp+40h] [ebp-418h]
    char *shock; // [esp+44h] [ebp-414h]
    gentity_s *ent; // [esp+4Ch] [ebp-40Ch]
    char s[1024]; // [esp+50h] [ebp-408h] BYREF
    int id; // [esp+454h] [ebp-4h]

    PROF_SCOPED("GScr_ShellShock");

    SV_CheckThread();
    ent = GetPlayerEntity(entref);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("USAGE: <player> shellshock(<shellshockname>, <duration>)\n", 0);
    shock = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    for ( id = 1; ; ++id )
    {
        if ( id >= 16 )
        {
            v2 = va("shellshock '%s' was not precached\n", shock);
            Scr_Error(v2, 0);
            return;
        }
        SV_GetConfigstring(id + 2532, s, 1024);
        if ( !I_stricmp(s, shock) )
            break;
    }
    v3 = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER) * 1000.0;
    duration = (int)(v3 + 9.313225746154785e-10);
    if ( (unsigned int)duration > 0xEA60 )
    {
        v1 = va("duration %g should be >= 0 and <= 60", (float)((float)duration * 0.001));
        Scr_ParamError(1u, v1, SCRIPTINSTANCE_SERVER);
    }
    ent->client->ps.shellshockIndex = id;
    ent->client->ps.shellshockTime = level.time;
    ent->client->ps.shellshockDuration = duration;
    if ( (ent->client->ps.perks[1] & 0x2000) != 0 )
        ent->client->ps.shellshockDuration = (int)(float)((float)ent->client->ps.shellshockDuration
                                                                                                        * perk_shellShockReduction->current.value);
    if ( ent->health > 0 )
    {
        ent->client->ps.pm_flags |= 0x10000u;
        bgs = &level_bgs;
        iassert(bgs == &level_bgs);
    }
}
#endif

void __cdecl GScr_StopShellShock(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetPlayerEntity(entref);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
        Scr_Error("USAGE: <player> stopshellshock()\n", 0);
    ent->client->ps.shellshockIndex = 0;
    ent->client->ps.shellshockTime = 0;
    ent->client->ps.shellshockDuration = 0;
    ent->client->ps.pm_flags &= ~0x10000u;
}

void __cdecl GScr_GetTagOrigin(scr_entref_t entref)
{
    VariableUnion tagName; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    tagName.intValue = Scr_GetConstLowercaseString(0, SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
    if (!GScr_UpdateTagInternal(ent, tagName.stringValue, &level.cachedTagMat, 0))
        return;
#else
    GScr_UpdateTagInternal(ent, tagName.stringValue, &level.cachedTagMat, 1);
#endif
    Scr_AddVector(level.cachedTagMat.tagMat[3], SCRIPTINSTANCE_SERVER);
}

int __cdecl GScr_UpdateTagInternal(
                gentity_s *ent,
                unsigned int tagName,
                cached_tag_mat_t *cachedTag,
                int showScriptError)
{
    char *v4; // eax
    const char *v5; // eax
    unsigned int v7; // eax
    char *v8; // eax
    const char *v9; // eax
    char *v10; // [esp-8h] [ebp-8h]

    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 10708, 0, "%s", "ent") )
        __debugbreak();
    if ( ent->s.number == cachedTag->entnum && level.time == cachedTag->time && tagName == cachedTag->name )
        return 1;
    if ( !SV_DObjExists(ent) )
    {
        if ( showScriptError )
        {
            v4 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
            v5 = va("entity has no model defined (classname '%s')", v4);
            Scr_ObjectError(v5, SCRIPTINSTANCE_SERVER);
        }
        return 0;
    }
    if ( G_DObjGetWorldTagMatrix(ent, tagName, cachedTag->tagMat) )
    {
        cachedTag->entnum = ent->s.number;
        cachedTag->time = level.time;
        Scr_SetString(&cachedTag->name, tagName, SCRIPTINSTANCE_SERVER);
        return 1;
    }
    if ( showScriptError )
    {
        SV_DObjDumpInfo(ent);
        v7 = G_ModelName(ent->model);
        v10 = SL_ConvertToString(v7, SCRIPTINSTANCE_SERVER);
        v8 = SL_ConvertToString(tagName, SCRIPTINSTANCE_SERVER);
        v9 = va("tag '%s' does not exist in model '%s' (or any attached submodels)", v8, v10);
        Scr_ParamError(0, v9, SCRIPTINSTANCE_SERVER);
    }
    return 0;
}

void __cdecl GScr_GetTagAngles(scr_entref_t entref)
{
    VariableUnion tagName; // [esp+0h] [ebp-14h]
    float angles[3]; // [esp+4h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+10h] [ebp-4h]

    ent = GetEntity(entref);
    tagName.intValue = Scr_GetConstLowercaseString(0, SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
    if (!GScr_UpdateTagInternal(ent, tagName.stringValue, &level.cachedTagMat, 0))
        return;
#else
    GScr_UpdateTagInternal(ent, tagName.stringValue, &level.cachedTagMat, 1);
#endif
    AxisToAngles(level.cachedTagMat.tagMat, angles);
    Scr_AddVector(angles, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_GetEntnum(scr_entref_t entref)
{
    Scr_AddInt(entref.entnum, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetDepthOfField(scr_entref_t entref)
{
    const char *v1; // eax
    const char *v2; // eax
    float dofNearBlur; // [esp+14h] [ebp-1Ch]
    float dofFarBlur; // [esp+18h] [ebp-18h]
    float dofNearStart; // [esp+1Ch] [ebp-14h]
    float dofFarStart; // [esp+20h] [ebp-10h]
    gentity_s *ent; // [esp+24h] [ebp-Ch]
    float dofFarEnd; // [esp+28h] [ebp-8h]
    float dofNearEnd; // [esp+2Ch] [ebp-4h]

    ent = GetPlayerEntity(entref);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 6 )
        Scr_Error("Incorrect number of parameters\n", 0);
    dofNearStart = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    dofNearEnd = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    dofFarStart = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    dofFarEnd = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    dofNearBlur = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
    dofFarBlur = Scr_GetFloat(5u, SCRIPTINSTANCE_SERVER);
    if ( dofNearStart < 0.0 )
        Scr_ParamError(0, "near start must be >= 0", SCRIPTINSTANCE_SERVER);
    if ( dofNearEnd < 0.0 )
        Scr_ParamError(1u, "near end must be >= 0", SCRIPTINSTANCE_SERVER);
    if ( dofFarStart < 0.0 )
        Scr_ParamError(2u, "far start must be >= 0", SCRIPTINSTANCE_SERVER);
    if ( dofFarEnd < 0.0 )
        Scr_ParamError(3u, "far end must be >= 0", SCRIPTINSTANCE_SERVER);
#ifndef KISAK_SP
    if ( dofNearBlur < 4.0 || dofNearBlur > 10.0 )
    {
        v1 = va("near blur should be between %g and %g", 4.0, 10.0);
        Scr_ParamError(4u, v1, SCRIPTINSTANCE_SERVER);
    }
    if ( dofFarBlur < 0.0 || dofFarBlur > dofNearBlur )
    {
        v2 = va("far blur should be >= %g and <= near blur", 0.0);
        Scr_ParamError(5u, v2, SCRIPTINSTANCE_SERVER);
    }
#else
    (void)v1; (void)v2;
#endif
    if ( dofNearStart >= dofNearEnd )
    {
        dofNearStart = 0.0f;
        dofNearEnd = 0.0f;
    }
    if ( dofFarStart >= dofFarEnd || dofFarBlur == 0.0 )
    {
        dofFarStart = 0.0f;
        dofFarEnd = 0.0f;
    }
    else if ( dofNearEnd > dofFarStart )
    {
        Scr_ParamError(
            2u,
            "far start must be >= near end, or far depth of field should be disabled with far start >= far end or far blur == 0",
            SCRIPTINSTANCE_SERVER);
    }
    ent->client->ps.dofNearStart = dofNearStart;
    ent->client->ps.dofNearEnd = dofNearEnd;
    ent->client->ps.dofFarStart = dofFarStart;
    ent->client->ps.dofFarEnd = dofFarEnd;
    ent->client->ps.dofNearBlur = dofNearBlur;
    ent->client->ps.dofFarBlur = dofFarBlur;
}

#ifdef KISAK_SP
static void GScr_GetDepthOfField_NearStart(scr_entref_t entref)
{
    Scr_AddFloat(GetPlayerEntity(entref)->client->ps.dofNearStart, SCRIPTINSTANCE_SERVER);
}

static void GScr_GetDepthOfField_NearEnd(scr_entref_t entref)
{
    Scr_AddFloat(GetPlayerEntity(entref)->client->ps.dofNearEnd, SCRIPTINSTANCE_SERVER);
}

static void GScr_GetDepthOfField_FarStart(scr_entref_t entref)
{
    Scr_AddFloat(GetPlayerEntity(entref)->client->ps.dofFarStart, SCRIPTINSTANCE_SERVER);
}

static void GScr_GetDepthOfField_FarEnd(scr_entref_t entref)
{
    Scr_AddFloat(GetPlayerEntity(entref)->client->ps.dofFarEnd, SCRIPTINSTANCE_SERVER);
}

static void GScr_GetDepthOfField_NearBlur(scr_entref_t entref)
{
    Scr_AddFloat(GetPlayerEntity(entref)->client->ps.dofNearBlur, SCRIPTINSTANCE_SERVER);
}

static void GScr_GetDepthOfField_FarBlur(scr_entref_t entref)
{
    Scr_AddFloat(GetPlayerEntity(entref)->client->ps.dofFarBlur, SCRIPTINSTANCE_SERVER);
}

#endif

void __cdecl GScr_SetViewModelDepthOfField(scr_entref_t entref)
{
    gentity_s *ent; // [esp+0h] [ebp-Ch]
    float dofEnd; // [esp+4h] [ebp-8h]
    float dofStart; // [esp+8h] [ebp-4h]

    ent = GetPlayerEntity(entref);
    dofStart = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    dofEnd = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    if ( dofStart < 0.0 )
        Scr_ParamError(0, "start must be >= 0", SCRIPTINSTANCE_SERVER);
    if ( dofEnd < 0.0 )
        Scr_ParamError(1u, "end must be >= 0", SCRIPTINSTANCE_SERVER);
    if ( dofStart >= dofEnd )
    {
        dofStart = 0.0f;
        dofEnd = 0.0f;
    }
    ent->client->ps.dofViewmodelStart = dofStart;
    ent->client->ps.dofViewmodelEnd = dofEnd;
}

void __cdecl GScr_ViewKick(scr_entref_t entref)
{
    double Float; // st7
    const char *v2; // eax
    float *damage_from; // [esp+Ch] [ebp-18h]
    float *v4; // [esp+10h] [ebp-14h]
    float origin[3]; // [esp+14h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+20h] [ebp-4h]

    ent = GetPlayerEntity(entref);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("USAGE: <player> viewkick <force 0-127> <source position>\n", 0);
    ent->client->damage_blood = (ent->maxHealth * Scr_GetInt(0, SCRIPTINSTANCE_SERVER) + 50) / 100;
    if ( ent->client->damage_blood < 0 )
    {
        Float = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
        v2 = va("viewkick: damage %g < 0\n", Float);
        Scr_Error(v2, 0);
    }
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    damage_from = ent->client->damage_from;
    v4 = ent->client->ps.origin;
    *damage_from = *v4 - origin[0];
    damage_from[1] = v4[1] - origin[1];
    damage_from[2] = v4[2] - origin[2];
}

void __cdecl GScr_LocalToWorldCoords(scr_entref_t entref)
{
    float vLocal[3]; // [esp+4h] [ebp-40h] BYREF
    float vWorld[3]; // [esp+10h] [ebp-34h] BYREF
    gentity_s *ent; // [esp+1Ch] [ebp-28h]
    float axis[3][3]; // [esp+20h] [ebp-24h] BYREF

    ent = GetEntity(entref);
    Scr_GetVector(0, vLocal, SCRIPTINSTANCE_SERVER);
    AnglesToAxis(ent->r.currentAngles, axis);
    MatrixTransformVector(vLocal, axis, vWorld);
    vWorld[0] = vWorld[0] + ent->r.currentOrigin[0];
    vWorld[1] = vWorld[1] + ent->r.currentOrigin[1];
    vWorld[2] = vWorld[2] + ent->r.currentOrigin[2];
    Scr_AddVector(vWorld, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetRightArc(scr_entref_t entref)
{
    TurretInfo *pTurretInfo; // [esp+0h] [ebp-8h]

    pTurretInfo = GetEntity(entref)->pTurretInfo;
    if ( !pTurretInfo )
        Scr_Error("entity is not a turret", 0);
    pTurretInfo->arcmin[1] = -Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( pTurretInfo->arcmin[1] > 0.0 )
        pTurretInfo->arcmin[1] = 0.0f;
}

void __cdecl GScr_SetLeftArc(scr_entref_t entref)
{
    TurretInfo *pTurretInfo; // [esp+0h] [ebp-8h]

    pTurretInfo = GetEntity(entref)->pTurretInfo;
    if ( !pTurretInfo )
        Scr_Error("entity is not a turret", 0);
    pTurretInfo->arcmax[1] = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( pTurretInfo->arcmax[1] < 0.0 )
        pTurretInfo->arcmax[1] = 0.0f;
}

void __cdecl GScr_SetTopArc(scr_entref_t entref)
{
    TurretInfo *pTurretInfo; // [esp+0h] [ebp-8h]

    pTurretInfo = GetEntity(entref)->pTurretInfo;
    if ( !pTurretInfo )
        Scr_Error("entity is not a turret", 0);
    pTurretInfo->arcmin[0] = -Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( pTurretInfo->arcmin[0] > 0.0 )
        pTurretInfo->arcmin[0] = 0.0f;
}

void __cdecl GScr_SetBottomArc(scr_entref_t entref)
{
    TurretInfo *pTurretInfo; // [esp+0h] [ebp-8h]

    pTurretInfo = GetEntity(entref)->pTurretInfo;
    if ( !pTurretInfo )
        Scr_Error("entity is not a turret", 0);
    pTurretInfo->arcmax[0] = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( pTurretInfo->arcmax[0] < 0.0 )
        pTurretInfo->arcmax[0] = 0.0f;
}

void __cdecl GScr_PlaceSpawnPoint(scr_entref_t entref)
{
    __int16 EntityHitId; // ax
    col_context_t context; // [esp+10h] [ebp-80h] BYREF
    float vEnd[3]; // [esp+38h] [ebp-58h] BYREF
    trace_t trace; // [esp+44h] [ebp-4Ch] BYREF
    gentity_s *pEnt; // [esp+80h] [ebp-10h]
    float vStart[3]; // [esp+84h] [ebp-Ch] BYREF

#ifdef KISAK_SP
    // Retail SP GScr_PlaceSpawnPoint (BlackOps.exe 0x00806680) uses this
    // mask for each of its three capsule traces.  The extra 0x4000 bit is an
    // SP-only binary divergence; do not change MP spawn placement with it.
    constexpr int spawnTraceMask = 0x281c011;
#else
    constexpr int spawnTraceMask = 0x2818011;
#endif

    //col_context_t::col_context_t(&context);
    pEnt = GetEntity(entref);
    vStart[0] = pEnt->r.currentOrigin[0];
    vStart[1] = pEnt->r.currentOrigin[1];
    vStart[2] = pEnt->r.currentOrigin[2];
    vEnd[0] = pEnt->r.currentOrigin[0];
    vEnd[1] = pEnt->r.currentOrigin[1];
    vEnd[2] = pEnt->r.currentOrigin[2];
    vEnd[2] = vEnd[2] + 128.0;
    G_TraceCapsule(
        &trace,
        vStart,
        playerMins,
        playerMaxs,
        vEnd,
        pEnt->s.number,
        spawnTraceMask,
        &context);
    Vec3Lerp(vStart, vEnd, trace.fraction, vStart);
    vEnd[0] = vStart[0];
    vEnd[1] = vStart[1];
    vEnd[2] = vStart[2] - 262144.0;
    G_TraceCapsule(
        &trace,
        vStart,
        playerMins,
        playerMaxs,
        vEnd,
        pEnt->s.number,
        spawnTraceMask,
        &context);
    EntityHitId = Trace_GetEntityHitId(&trace);
    pEnt->s.groundEntityNum = EntityHitId;
    g_entities[pEnt->s.groundEntityNum].flags |= 0x100000u;
    Vec3Lerp(vStart, vEnd, trace.fraction, vStart);
    G_TraceCapsule(
        &trace,
        vStart,
        playerMins,
        playerMaxs,
        vStart,
        pEnt->s.number,
        spawnTraceMask,
        &context);
    if ( trace.allsolid )
        Com_PrintWarning(
            24,
            "WARNING: Spawn point entity %i is in solid at (%i, %i, %i)\n",
            pEnt->s.number,
            (int)pEnt->r.currentOrigin[0],
            (int)pEnt->r.currentOrigin[1],
            (int)pEnt->r.currentOrigin[2]);
    G_SetOrigin(pEnt, vStart);
}

void __cdecl ScrCmd_SendFaceEvent(scr_entref_t entref)
{
    unsigned __int16 face_event; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    face_event = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( face_event == scr_const.face_casual )
    {
        G_AddEvent(ent, 0xC5u, 0);
    }
    else if ( face_event == scr_const.face_alert )
    {
        G_AddEvent(ent, 0xC5u, 1u);
    }
    else if ( face_event == scr_const.face_cqb )
    {
        G_AddEvent(ent, 0xC5u, 2u);
    }
    else if ( face_event == scr_const.face_running )
    {
        G_AddEvent(ent, 0xC5u, 3u);
    }
    else if ( face_event == scr_const.face_shoot_single )
    {
        G_AddEvent(ent, 0xC5u, 4u);
    }
    else if ( face_event == scr_const.face_shoot_burst )
    {
        G_AddEvent(ent, 0xC5u, 5u);
    }
    else if ( face_event == scr_const.face_react )
    {
        G_AddEvent(ent, 0xC5u, 7u);
    }
    else if ( face_event == scr_const.face_talk )
    {
        G_AddEvent(ent, 0xC5u, 8u);
    }
    else if ( face_event == scr_const.face_talk_long )
    {
        G_AddEvent(ent, 0xC5u, 9u);
    }
    else if ( face_event == scr_const.face_pain )
    {
        G_AddEvent(ent, 0xC5u, 0xAu);
    }
    else if ( face_event == scr_const.face_death )
    {
        G_AddEvent(ent, 0xC5u, 0xBu);
    }
    else if ( face_event == scr_const.face_melee )
    {
        G_AddEvent(ent, 0xC5u, 6u);
    }
    else
    {
        Com_PrintError(19, "Invalid face state specified by script\n");
    }
}

void GScr_TestSpawnPoint()
{
    col_context_t context; // [esp+0h] [ebp-70h] BYREF
    trace_t trace; // [esp+28h] [ebp-48h] BYREF
    float point[3]; // [esp+64h] [ebp-Ch] BYREF

    //col_context_t::col_context_t(&context);
    Scr_GetVector(0, point, SCRIPTINSTANCE_SERVER);
    G_TraceCapsule(&trace, point, playerMins, playerMaxs, point, 1023, (int)0x810011, &context);
    if ( trace.startsolid || trace.allsolid )
        Scr_AddBool(0, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddBool(1u, SCRIPTINSTANCE_SERVER);
}

void GScr_MapRestart()
{
    if ( level.finished )
    {
        if ( level.finished == 1 )
            Scr_Error("map_restart already called", 0);
        else
            Scr_Error("exitlevel already called", 0);
    }
    level.finished = 1;
    level.savepersist = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
        level.savepersist = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    Cbuf_AddText(0, "fast_restart\n");
}

void GScr_LoadMap()
{
    const char *v0; // eax
    char *mapname; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        mapname = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        if ( SV_MapExists(mapname) )
        {
            if ( level.finished )
            {
                if ( level.finished == 2 )
                    Scr_Error("map already called", 0);
                else
                    Scr_Error("exitlevel already called", 0);
            }
            level.finished = 2;
            level.savepersist = 0;
            if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
                level.savepersist = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
            v0 = va("map %s\n", mapname);
            Cbuf_AddText(0, v0);
        }
    }
}

void GScr_ExitLevel()
{
    if ( level.finished )
    {
        if ( level.finished == 1 )
            Scr_Error("map_restart already called", 0);
        else
            Scr_Error("exitlevel already called", 0);
    }
    level.finished = 3;
    level.savepersist = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
        level.savepersist = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    SV_MatchEnd();
    ExitLevel();
}

void GScr_KillServer()
{
    level.finished = 3;
    level.savepersist = 0;
    SV_KillLocalServer();
}

void GScr_AddTestClient()
{
    gentity_s *ent; // [esp+0h] [ebp-4h]

    ent = SV_AddTestClient();
    if ( ent )
        Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
}

void GScr_MakeDvarServerInfo()
{
    const char *v0; // eax
    int NumParam; // eax
    char string[1024]; // [esp+0h] [ebp-818h] BYREF
    char outString[1028]; // [esp+400h] [ebp-418h] BYREF
    const char *dvarName; // [esp+808h] [ebp-10h]
    int type; // [esp+80Ch] [ebp-Ch]
    const dvar_s *dvar; // [esp+810h] [ebp-8h]
    const char *dvarValue; // [esp+814h] [ebp-4h]

    dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( !Dvar_IsValidName(dvarName) )
    {
        v0 = va("Dvar %s has an invalid dvar name", dvarName);
        Scr_Error(v0, 0);
    }
    dvar = Dvar_FindVar(dvarName);
    if ( dvar )
    {
        Dvar_AddFlags(dvar, 1280);
    }
    else
    {
        type = Scr_GetType(1u, SCRIPTINSTANCE_SERVER);
        if ( type == 3 )
        {
            NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
            Scr_ConstructMessageString(1, NumParam - 1, "Dvar Value", string, 0x400u);
            dvarValue = string;
        }
        else
        {
            dvarValue = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        }
        CleanDvarValue(dvarValue, outString, 1024);
        _Dvar_RegisterString(dvarName, (char *)dvarValue, 0x4500u, "Script defined user info dvar");
    }
}

void GScr_SetBombTimer()
{
    const char *v0; // eax
    char *name; // [esp+0h] [ebp-8h]
    int value; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        value = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        if ( I_stricmp("A", name) && I_stricmp("primary", name) )
        {
            if ( I_stricmp("B", name) && I_stricmp("secondary", name) )
            {
                v0 = va("GScr_SetBombTimer: Unknown bomb timer name %s", name);
                Scr_Error(v0, 0);
            }
            else
            {
                level.matchState.archivedState.bombTimer[1] = value;
            }
        }
        else
        {
            level.matchState.archivedState.bombTimer[0] = value;
        }
    }
}

void GScr_SetMatchTalkFlag()
{
    const char *v0; // eax
    char *flagName; // [esp+0h] [ebp-Ch]
    int flagBit; // [esp+4h] [ebp-8h]
    int value; // [esp+8h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        flagName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        value = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        if ( I_stricmp("EveryoneHearsEveryone", flagName) )
        {
            if ( I_stricmp("DeadChatWithDead", flagName) )
            {
                if ( I_stricmp("DeadChatWithTeam", flagName) )
                {
                    if ( I_stricmp("DeadHearTeamLiving", flagName) )
                    {
                        if ( I_stricmp("DeadHearAllLiving", flagName) )
                        {
                            v0 = va("GScr_SetMatchTalkFlag: Couldn't find a matching bit for flag name : %s ", flagName);
                            Scr_Error(v0, 0);
                            return;
                        }
                        flagBit = 16;
                    }
                    else
                    {
                        flagBit = 8;
                    }
                }
                else
                {
                    flagBit = 4;
                }
            }
            else
            {
                flagBit = 2;
            }
        }
        else
        {
            flagBit = 1;
        }
        if ( value )
            level.matchState.unarchivedState.talkFlags |= flagBit;
        else
            level.matchState.unarchivedState.talkFlags &= ~flagBit;
    }
}

void GScr_SetMatchFlag()
{
    const char *v0; // eax
    char *flagName; // [esp+0h] [ebp-10h]
    bool archived; // [esp+7h] [ebp-9h]
    char flagBit; // [esp+8h] [ebp-8h]
    int value; // [esp+Ch] [ebp-4h]

    archived = 1;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        flagName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        value = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        if ( I_stricmp("radar_allies", flagName) )
        {
            if ( I_stricmp("radar_axis", flagName) )
            {
                if ( I_stricmp("bomb_timer", flagName) )
                {
                    if ( I_stricmp("bomb_timer_a", flagName) )
                    {
                        if ( I_stricmp("bomb_timer_b", flagName) )
                        {
                            if ( I_stricmp("bomb_timer_b", flagName) )
                            {
                                if ( I_stricmp("ammocounterhide", flagName) )
                                {
                                    if ( I_stricmp("enable_popups", flagName) )
                                    {
                                        if ( I_stricmp("hud_hardcore", flagName) )
                                        {
                                            if ( I_stricmp("pregame", flagName) )
                                            {
                                                if ( I_stricmp("final_killcam", flagName) )
                                                {
                                                    if ( I_stricmp("round_end_killcam", flagName) )
                                                    {
                                                        if ( I_stricmp("cg_drawSpectatorMessages", flagName) )
                                                        {
                                                            if ( I_stricmp("disableIngameMenu", flagName) )
                                                            {
                                                                v0 = va("GScr_SetMatchFlag: Couldn't find a matching bit for flag name : %s ", flagName);
                                                                Scr_Error(v0, 0);
                                                                return;
                                                            }
                                                            flagBit = 16;
                                                            archived = 0;
                                                        }
                                                        else
                                                        {
                                                            flagBit = 15;
                                                            archived = 0;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        flagBit = 5;
                                                        archived = 0;
                                                    }
                                                }
                                                else
                                                {
                                                    flagBit = 4;
                                                    archived = 0;
                                                }
                                            }
                                            else
                                            {
                                                flagBit = 14;
                                                archived = 0;
                                            }
                                        }
                                        else
                                        {
                                            flagBit = 13;
                                            archived = 0;
                                        }
                                    }
                                    else
                                    {
                                        flagBit = 8;
                                        archived = 0;
                                    }
                                }
                                else
                                {
                                    flagBit = 12;
                                }
                            }
                            else
                            {
                                flagBit = 11;
                            }
                        }
                        else
                        {
                            flagBit = 11;
                        }
                    }
                    else
                    {
                        flagBit = 10;
                    }
                }
                else
                {
                    flagBit = 9;
                }
            }
            else
            {
                flagBit = 7;
            }
        }
        else
        {
            flagBit = 6;
        }
        if ( archived )
        {
            if ( value )
                level.matchState.archivedState.matchUIVisibilityFlags |= 1 << flagBit >> 4;
            else
                level.matchState.archivedState.matchUIVisibilityFlags &= ~(1 << flagBit >> 4);
        }
        else if ( value )
        {
            level.matchState.unarchivedState.matchUIVisibilityFlags |= 1 << flagBit >> 4;
        }
        else
        {
            level.matchState.unarchivedState.matchUIVisibilityFlags &= ~(1 << flagBit >> 4);
        }
    }
}

void GScr_AllClientsPrint()
{
    const char *v0; // eax
    char *string; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        string = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        v0 = va("%c \"%s\"", 101, string);
        SV_GameSendServerCommand(-1, SV_CMD_CAN_IGNORE, v0);
    }
}

void GScr_MapExists()
{
    char *mapname; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        mapname = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        if ( SV_MapExists(mapname) )
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_IsValidGameType()
{
    char *gametype; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        gametype = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        if ( Scr_IsValidGameType(gametype) )
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_SetVoteString()
{
    char *v0; // eax
    char *v1; // eax
    char *v2; // eax
    char *string; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        string = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        SV_SetConfigstring(16, string);
        v0 = va("%i", level.voteTime);
        SV_SetConfigstring(15, v0);
        v1 = va("%i", level.voteYes);
        SV_SetConfigstring(17, v1);
        v2 = va("%i", level.voteNo);
        SV_SetConfigstring(18, v2);
    }
}

void GScr_SetVoteTime()
{
    char *v0; // eax
    char *v1; // eax
    char *v2; // eax
    int time; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        time = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        v0 = va("%i", time);
        SV_SetConfigstring(15, v0);
        v1 = va("%i", level.voteYes);
        SV_SetConfigstring(17, v1);
        v2 = va("%i", level.voteNo);
        SV_SetConfigstring(18, v2);
    }
}

void GScr_SetVoteYesCount()
{
    char *v0; // eax
    char *v1; // eax
    int yes; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        yes = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        v0 = va("%i", yes);
        SV_SetConfigstring(17, v0);
        v1 = va("%i", level.voteNo);
        SV_SetConfigstring(18, v1);
    }
}

void GScr_SetVoteNoCount()
{
    char *v0; // eax
    int no; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        no = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        v0 = va("%i", no);
        SV_SetConfigstring(18, v0);
    }
}

void GScr_KickPlayer()
{
#if 0 // KISAKTODO: find out why the scripts are trying to kick
    const char *v0; // eax
    const char *v1; // eax
    int playernum; // [esp+0h] [ebp-20h]
    char *reason; // [esp+4h] [ebp-1Ch]
    char genericreason[20]; // [esp+8h] [ebp-18h] BYREF

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        playernum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        strcpy(genericreason, "No reason specified");
        if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
        {
            reason = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
            if ( !reason || !*reason )
                reason = genericreason;
            v0 = va("clientkick %i %s\n", playernum, reason);
            Cbuf_AddText(0, v0);
        }
        else
        {
            v1 = va("clientkick %i\n", playernum);
            Cbuf_AddText(0, v1);
        }
    }
#else
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
    {
        Com_DPrintf(15, "[Kisak] Scripts attempted to kick player %i", Scr_GetInt(0, SCRIPTINSTANCE_SERVER));
    }
#endif
}

void GScr_BanPlayer()
{
    const char *v0; // eax
    bool temp; // [esp+3h] [ebp-5h]
    int playernum; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        playernum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        temp = 0;
        if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
            temp = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER) != 0;
        if ( temp )
            v0 = va("tempBanClient %i\n", playernum);
        else
            v0 = va("banClient %i\n", playernum);
        Cbuf_AddText(0, v0);
    }
}

void GScr_ClientPrint()
{
    const char *v0; // eax
    char *string; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        ent = Scr_GetEntity(0);
        string = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        v0 = va("%c \"%s\"", 101, string);
        SV_GameSendServerCommand(ent - g_entities, SV_CMD_CAN_IGNORE, v0);
    }
}

void GScr_OpenFile()
{
    char *v1; // eax
    char *v2; // eax
    char *v3; // eax
    char *fullpathname; // [esp+3Ch] [ebp-20h]
    int filesize; // [esp+40h] [ebp-1Ch]
    char *filename; // [esp+44h] [ebp-18h]
    int tempFile; // [esp+48h] [ebp-14h] BYREF
    int *f; // [esp+4Ch] [ebp-10h]
    const char *mode; // [esp+50h] [ebp-Ch]
    int filenum; // [esp+58h] [ebp-4h]

    f = 0;
    if ((unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1)
    {
        filename = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        mode = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        for (filenum = 0; filenum < 1; ++filenum)
        {
            if (!level.openScriptIOFileHandles[filenum])
            {
                f = &level.openScriptIOFileHandles[filenum];
                break;
            }
        }
        if (!f)
        {
            Com_Printf(24, "OpenFile failed.  %i files already open\n", 1);
            Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
            return;
        }
        if (!strcmp(mode, "read"))
        {
            fullpathname = va("%s/%s", "scriptdata", filename);
            filesize = FS_FOpenFileByMode(fullpathname, &tempFile, FS_READ);
            if (filesize >= 0)
            {
                v1 = Z_VirtualAlloc(filesize + 1, "GScr_OpenFile", 11);
                level.openScriptIOFileBuffers[filenum] = v1;
                FS_Read((unsigned __int8 *)level.openScriptIOFileBuffers[filenum], filesize, tempFile);
                FS_FCloseFile(tempFile);
                level.openScriptIOFileBuffers[filenum][filesize] = 0;
                Com_BeginParseSession(filename);
                Com_SetCSV(1);
                level.currentScriptIOLineMark[filenum].lines = 0;
                Scr_AddInt(filenum, SCRIPTINSTANCE_SERVER);
            }
            else
            {
                Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
            }
            return;
        }
        if (!strcmp(mode, "write"))
        {
            v2 = va("%s/%s", "scriptdata", filename);
            *f = FS_FOpenTextFileWrite(v2);
            if (!*f)
                goto LABEL_15;
        }
        else
        {
            if (strcmp(mode, "append"))
            {
                Com_Printf(24, "Valid openfile modes are 'write', 'read', and 'append'\n");
                Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
                return;
            }
            v3 = va("%s/%s", "scriptdata", filename);
            if ((FS_FOpenFileByMode(v3, f, FS_APPEND) & 0x80000000) != 0)
            {
            LABEL_15:
                Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
                return;
            }
        }
        Scr_AddInt(filenum, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_CloseFile()
{
    int filenum; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        filenum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        if ( (unsigned int)filenum >= 2 )
        {
            Com_Printf(24, "CloseFile failed, invalid file number %i\n", filenum);
            Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
            return;
        }
        if ( level.openScriptIOFileHandles[filenum]
            && level.openScriptIOFileBuffers[filenum]
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        11897,
                        0,
                        "%s",
                        "!((level.openScriptIOFileHandles[filenum] != 0) && (level.openScriptIOFileBuffers[filenum] != NULL))") )
        {
            __debugbreak();
        }
        if ( level.openScriptIOFileHandles[filenum] )
        {
            FS_FCloseFile(level.openScriptIOFileHandles[filenum]);
            level.openScriptIOFileHandles[filenum] = 0;
        }
        else
        {
            if ( !level.openScriptIOFileBuffers[filenum] )
            {
                Com_Printf(24, "CloseFile failed, file number %i was not open\n", filenum);
                Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
                return;
            }
            Com_EndParseSession();
            Z_VirtualFree(level.openScriptIOFileBuffers[filenum], 11);
            level.openScriptIOFileBuffers[filenum] = 0;
        }
        Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_FPrintln()
{
    Scr_FPrint_internal(0);
}

void __cdecl Scr_FPrint_internal(bool commaBetweenFields)
{
    int NumParam; // eax
    char *s; // [esp+10h] [ebp-Ch]
    unsigned int arg; // [esp+14h] [ebp-8h]
    int filenum; // [esp+18h] [ebp-4h]

    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
    {
        filenum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        if ( (unsigned int)filenum < 2 )
        {
            if ( level.openScriptIOFileHandles[filenum] )
            {
                for ( arg = 1; arg < Scr_GetNumParam(SCRIPTINSTANCE_SERVER); ++arg )
                {
                    s = Scr_GetString(arg, SCRIPTINSTANCE_SERVER);
                    FS_Write(s, strlen(s), level.openScriptIOFileHandles[filenum]);
                    if ( commaBetweenFields )
                        FS_Write(",", 1u, level.openScriptIOFileHandles[filenum]);
                }
                FS_Write("\n", 1u, level.openScriptIOFileHandles[filenum]);
                NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
                Scr_AddInt(NumParam - 1, SCRIPTINSTANCE_SERVER);
            }
            else
            {
                Com_Printf(24, "FPrintln failed, file number %i was not open for writing\n", filenum);
                Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
            }
        }
        else
        {
            Com_Printf(24, "FPrintln failed, invalid file number %i\n", filenum);
            Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        Com_Printf(24, "fprintln requires at least 2 parameters (file, output)\n");
        Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_FPrintFields()
{
    Scr_FPrint_internal(1);
}

void GScr_FReadLn()
{
    int v0; // eax
    int ArgCountOnLine; // eax
    bool eof; // [esp+0h] [ebp-10h]
    const char *buf; // [esp+4h] [ebp-Ch] BYREF
    const char *token; // [esp+8h] [ebp-8h]
    int filenum; // [esp+Ch] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        filenum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        if ( (unsigned int)filenum < 2 )
        {
            if ( level.openScriptIOFileBuffers[filenum] )
            {
                buf = level.openScriptIOFileBuffers[filenum];
                if ( level.currentScriptIOLineMark[filenum].lines )
                {
                    Com_ParseReturnToMark(&buf, &level.currentScriptIOLineMark[filenum]);
                    Com_SkipRestOfLine(&buf);
                    Com_ParseSetMark(&buf, &level.currentScriptIOLineMark[filenum]);
                    token = (const char *)Com_Parse(&buf);
                    eof = *token == 0;
                    Com_ParseReturnToMark(&buf, &level.currentScriptIOLineMark[filenum]);
                    if ( eof )
                    {
                        Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
                    }
                    else
                    {
                        ArgCountOnLine = Com_GetArgCountOnLine(&buf);
                        Scr_AddInt(ArgCountOnLine, SCRIPTINSTANCE_SERVER);
                    }
                }
                else
                {
                    Com_ParseSetMark(&buf, &level.currentScriptIOLineMark[filenum]);
                    v0 = Com_GetArgCountOnLine(&buf);
                    Scr_AddInt(v0, SCRIPTINSTANCE_SERVER);
                }
            }
            else
            {
                Com_Printf(24, "freadln failed, file number %i was not open for reading\n", filenum);
                Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
            }
        }
        else
        {
            Com_Printf(24, "freadln failed, invalid file number %i\n", filenum);
            Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        Com_Printf(24, "freadln requires a parameter - the file to read from\n");
        Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_FGetArg()
{
    const char *buf; // [esp+0h] [ebp-14h] BYREF
    int arg; // [esp+4h] [ebp-10h]
    int i; // [esp+8h] [ebp-Ch]
    const char *token; // [esp+Ch] [ebp-8h]
    int filenum; // [esp+10h] [ebp-4h]

    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
    {
        filenum = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        arg = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        if ( (unsigned int)filenum < 2 )
        {
            if ( arg >= 0 )
            {
                if ( level.openScriptIOFileBuffers[filenum] )
                {
                    token = 0;
                    buf = level.openScriptIOFileBuffers[filenum];
                    Com_ParseReturnToMark(&buf, &level.currentScriptIOLineMark[filenum]);
                    for ( i = 0; i <= arg; ++i )
                    {
                        token = (const char *)Com_ParseOnLine(&buf);
                        if ( !*token )
                        {
                            Com_Printf(
                                24,
                                "freadline failed, there aren't %i arguments on this line, there are only %i arguments\n",
                                arg + 1,
                                i);
                            Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
                            return;
                        }
                    }
                    Scr_AddString((char *)token, SCRIPTINSTANCE_SERVER);
                }
                else
                {
                    Com_Printf(24, "freadline failed, file number %i was not open for reading\n", filenum);
                    Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
                }
            }
            else
            {
                Com_Printf(24, "freadline failed, invalid argument number %i\n", arg);
                Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
            }
        }
        else
        {
            Com_Printf(24, "freadline failed, invalid file number %i\n", filenum);
            Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        Com_Printf(24, "freadline requires at least 2 parameters (file, string)\n");
        Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
    }
}

void GScr_ExecDevgui()
{
    const char *v0; // eax
    char *filename; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        filename = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        v0 = va("exec %s\n", filename);
        Cbuf_AddText(0, v0);
    }
}

void Scr_IsGlobalStatsServer()
{
    int LicenseType; // eax

    LicenseType = SV_GetLicenseType();
    if ( SV_IsServerRanked(LicenseType) )
        Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}

void Scr_SetPlayerStatsForMatchRecording()
{
    unsigned intresult; // eax
    char *statName; // [esp+8h] [ebp-10h]
    unsigned int statValue; // [esp+Ch] [ebp-Ch]
    gentity_s *ent; // [esp+14h] [ebp-4h]

    PROF_SCOPED("GScr_SetPlayerStatsForMatchRecording");

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3 )
        Scr_ParamError(0, "recordPlayerStats [player] [statName] [value]", SCRIPTINSTANCE_SERVER);
    ent = Scr_GetEntity(0);
    if ( !ent )
        Scr_ParamError(0, "recordPlayerStats Error: param 0 is not an entity.", SCRIPTINSTANCE_SERVER);
    if ( !ent->client )
        Scr_ParamError(0, "recordPlayerStats Error: param 0 is not an player.", SCRIPTINSTANCE_SERVER);
    statName = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    if ( !statName )
        Scr_ParamError(1u, "recordPlayerStats Error: param 1 is not a string.", SCRIPTINSTANCE_SERVER);
    statValue = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
#ifdef KISAK_LIVE
    MatchRecordSetPlayerStat(ent->client, statName, statValue);
#endif
}

void GScr_SetPlayerFinalForMatchRecording()
{
    gentity_s *ent; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_ParamError(0, "recordplayermatchend [player]", SCRIPTINSTANCE_SERVER);
    ent = Scr_GetEntity(0);
    if ( !ent )
        Scr_ParamError(0, "recordplayermatchend Error: param 0 is not an entity.", SCRIPTINSTANCE_SERVER);
    if ( !ent->client )
        Scr_ParamError(0, "recordplayermatchend Error: param 0 is not an player.", SCRIPTINSTANCE_SERVER);
#ifdef KISAK_LIVE
    MatchRecordEnd(ent->client);
#endif
}

void GScr_SetBeginForMatchRecording()
{
#ifdef KISAK_LIVE
    MatchRecordBegin();
#endif
}

void GScr_GetAssignedTeam()
{
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SendLeaderboards(scr_entref_t entref)
{
    const char *v1; // eax
    gentity_s *playerEnt; // [esp+8h] [ebp-4h]

    playerEnt = GetEntity(entref);
    if ( !playerEnt->client )
        Scr_Error("sendleaderboards: entity must be a player entity", 0);
    v1 = va("%c", 80);
    SV_GameSendServerCommand(playerEnt->s.number, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_IsItemPurchased(scr_entref_t entref)
{
    bool IsItemLockedForChallenge; // al

    IsItemLockedForChallenge = GScr_IsItemLockedForChallenge(entref, 1);
    Scr_AddInt(!IsItemLockedForChallenge, SCRIPTINSTANCE_SERVER);
}

bool __cdecl GScr_IsItemLockedForChallenge(scr_entref_t entref, bool purchaseRequired)
{
    int ClientPrestige; // eax
    int ClientRank; // [esp-8h] [ebp-18h]
    gentity_s *playerEnt; // [esp+0h] [ebp-10h]
    bool isLocked; // [esp+7h] [ebp-9h]
    int itemIndex; // [esp+8h] [ebp-8h]

    isLocked = 0;
    playerEnt = GetEntity(entref);
    if ( !playerEnt || playerEnt->client )
    {
        if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
        {
            itemIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            if ( itemIndex && itemIndex < 256 && CL_LocalClient_GetActiveCount() )
            {
                ClientRank = G_GetClientRank(playerEnt->s.number);
                ClientPrestige = G_GetClientPrestige(playerEnt->s.number);
                isLocked = BG_UnlockablesIsItemLockedForRank(ClientPrestige, ClientRank, itemIndex);
            }
            if ( purchaseRequired && !isLocked )
                return !GScr_IsItemPurchasedForClientNum(playerEnt->s.number, itemIndex);
            return isLocked;
        }
        else
        {
            Scr_Error("isItemLocked: takes one parameter.", 0);
            return 1;
        }
    }
    else
    {
        Scr_Error("isItemLocked: entity must be a player entity", 0);
        return 1;
    }
}

bool __cdecl GScr_IsItemPurchasedForClientNum(unsigned int clientNum, unsigned int itemIndex)
{
    if ( clientNum >= 0x20
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    12614,
                    0,
                    "%s",
                    "( clientNum >= 0 ) && ( clientNum < MAX_CLIENTS )") )
    {
        __debugbreak();
    }
    if ( itemIndex >= 0x100
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    12615,
                    0,
                    "%s",
                    "( itemIndex >= 0 ) && ( itemIndex < MAX_UNLOCKABLE_ITEMS )") )
    {
        __debugbreak();
    }
    return clientNum < 0x20
            && itemIndex < 0x100
            && ((BG_UnlockablesHasFullAccess() && BG_UnlockablesIsItemValidNotNull(itemIndex))
                || Com_GameMode_IsGameMode(GAMEMODE_PRIVATE_MATCH)
                || ((1 << (itemIndex & 7)) & svs.clients[clientNum].purchasedItems[(int)itemIndex >> 3]) == 1 << (itemIndex & 7));
}

void __cdecl GScr_IsItemLocked(scr_entref_t entref)
{
    bool IsItemLockedForChallenge; // al

    IsItemLockedForChallenge = GScr_IsItemLockedForChallenge(entref, 0);
    Scr_AddInt(IsItemLockedForChallenge, SCRIPTINSTANCE_SERVER);
}

void GScr_GetRefFromItemIndex()
{
    VariableUnion itemIndex; // [esp+0h] [ebp-8h]
    char *itemRef; // [esp+4h] [ebp-4h]

    itemIndex.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    itemRef = (char *)BG_UnlockablesGetItemRef(itemIndex.intValue);
    if ( itemRef && *itemRef )
        Scr_AddString(itemRef, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
}

void GScr_GetItemGroupFromItemIndex()
{
    VariableUnion itemIndex; // [esp+0h] [ebp-8h]
    char *itemGroup; // [esp+4h] [ebp-4h]

    itemIndex.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    itemGroup = (char *)BG_UnlockablesGetItemGroup(itemIndex.intValue);
    if ( itemGroup && *itemGroup )
        Scr_AddString(itemGroup, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
}

void GScr_GetBaseWeaponItemIndex()
{
    int WeaponTableItemIndex; // eax
    const WeaponVariantDef *weapVarDef; // [esp+0h] [ebp-18h]
    const WeaponVariantDef *weapVariantDef; // [esp+8h] [ebp-10h]
    int weaponItemIndex; // [esp+Ch] [ebp-Ch]
    unsigned int iWeaponIndex; // [esp+10h] [ebp-8h]
    char *pszWeaponName; // [esp+14h] [ebp-4h]

    weaponItemIndex = 0;
    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName(pszWeaponName);
    if (iWeaponIndex)
    {
        weapVariantDef = BG_GetWeaponVariantDef(iWeaponIndex);
        if (weapVariantDef && weapVariantDef->weapDef)
            weaponItemIndex = weapVariantDef->weapDef->itemIndex;
        if (!weaponItemIndex)
        {
            weapVarDef = BG_GetWeaponVariantDef(iWeaponIndex);
            if (weapVarDef->iVariantCount >= 0)
                WeaponTableItemIndex = BG_GetWeaponTableItemIndex(iWeaponIndex);
            else
                WeaponTableItemIndex = BG_GetWeaponTableItemIndex(-weapVarDef->iVariantCount);
            weaponItemIndex = WeaponTableItemIndex;
        }
    }
    Scr_AddInt(weaponItemIndex, SCRIPTINSTANCE_SERVER);
}

const char *lbWagerGameModeEnum[5] =
{ "oic", "hlnd", "gun", "shrp", NULL };

const char *lbTypeEnum[17] =
{
  "tdm",
  "dm",
  "ctf",
  "dom",
  "sab",
  "sd",
  "koth",
  "dem",
  "hctdm",
  "hcdm",
  "hcctf",
  "hcdom",
  "hcsab",
  "hcsd",
  "hckoth",
  "hcdem",
  NULL
};


void GScr_GetGameTypeEnumFromName()
{
    const char *v0; // eax
    const char *v1; // [esp+0h] [ebp-1Ch]
    char *gameTypeName; // [esp+8h] [ebp-14h]
    int endIndex; // [esp+10h] [ebp-Ch]
    const char **gameModeEnum; // [esp+14h] [ebp-8h]
    int currGameMode; // [esp+18h] [ebp-4h]

    gameTypeName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( Scr_GetInt(1u, SCRIPTINSTANCE_SERVER) )
    {
        endIndex = 4;
        gameModeEnum = lbWagerGameModeEnum;
    }
    else
    {
#ifdef KISAK_MP
        // Private Gun Game still evaluates the AAR enum, although private AAR
        // setters discard it. Use the existing FFA index; never extend the
        // ranked statistics enum or enable wagers just to run this gametype.
        if (Dvar_GetBool("xblive_privatematch")
            && !Dvar_GetBool("xblive_rankedmatch")
            && !Dvar_GetBool("xblive_wagermatch")
            && !Dvar_GetBool("xblive_basictraining")
            && (!I_stricmp(gameTypeName, "gun") || !I_stricmp(gameTypeName, "hcgun")))
        {
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
            return;
        }
#endif
        endIndex = 16;
        gameModeEnum = lbTypeEnum;
    }
    for ( currGameMode = 0; currGameMode < endIndex; ++currGameMode )
    {
        if ( !I_stricmp(gameTypeName, gameModeEnum[currGameMode]) )
        {
            Scr_AddInt(currGameMode, SCRIPTINSTANCE_SERVER);
            return;
        }
    }
    if ( gameTypeName )
        v1 = gameTypeName;
    else
        v1 = "unknown";
    v0 = va("GetGameTypeEnumFromName: Invalid gametype parameter: '%s' supplied to GetGameTypeEnumFromName", v1);
    Scr_Error(v0, 0);
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}

void GScr_GetWagerGametypeList()
{
    int i; // [esp+0h] [ebp-4h]

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for ( i = 0; i < 4; ++i )
    {
        Scr_AddString((char *)lbWagerGameModeEnum[i], SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_GetLoadoutItemFromProfile(scr_entref_t entref)
{
    int item; // [esp+0h] [ebp-8h]

    if ( !GetEntity(entref)->client )
        Scr_Error("getloadoutitemfromprofile: entity must be a player entity", 0);
#if defined(KISAK_NX) || (defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH))
    // Offline games (private match, combat training) read every custom class
    // slot through this (_class.gsc getLoadoutItemFromDDLStats, when
    // !level.onlineGame); a stub returning 0 gave every class no weapons. It is
    // the read side of BG_UnlockablesEquipClassToSlot: the custom game mode's
    // classes when ui_useCustomClassInfo is set, else the profile's CacLoadouts
    // (<class>.<slot>, or a global slot when the class name is empty, as for
    // killstreaks). Custom game-mode classes use the shared host settings.
    const char *customClassName = "";
    const char *loadoutName;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 1 )
    {
        loadoutName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        loadoutName = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        customClassName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    }
    const int controllerIndex = 0;
    item = 0;
    if ( *customClassName && Dvar_GetInt(ui_useCustomClassInfo) )
    {
        item = GetCustomClassLoadoutItemForClassName(controllerIndex, customClassName, loadoutName);
    }
    else
    {
        ddlState_t searchState;
        const bool found = *customClassName
            ? DDL_MoveTo(LiveStats_GetCacDDLState(), &searchState, 2, customClassName, loadoutName)
            : DDL_MoveTo(LiveStats_GetCacDDLState(), &searchState, 1, loadoutName);
        if ( found )
        {
#if defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH)
            // Read this player's server copy, also used by getdstat and weapon
            // cosmetics. A remote player must never inherit the host's profile.
            item = SV_GetClientDIntStat(GetEntity(entref)->s.number, &searchState);
#else
            item = DDL_GetInt(&searchState, (char *)LiveStorage_GetStatsBuffer(controllerIndex, STATS_LOCATION_NORMAL, 1));
#endif
        }
        else
            DDL_PrintError("DDL: Error could not find loadout slot %s %s\n", customClassName, loadoutName);
    }
#else
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 1 )
    {
        Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    }
    item = RETURN_ZERO32();
#endif
    Scr_AddInt(item, SCRIPTINSTANCE_SERVER);
}


#if defined(KISAK_DEDICATED) && defined(OPENBLOPS_NO_STEAM_AUTH)
static gentity_s *GScr_OfflineDStatPath(scr_entref_t entref, bool write, ddlState_t *state)
{
    gentity_s *player = GetEntity(entref);
    if (!player->client) Scr_Error("dstat: entity must be a player", 0);
    if (!SV_OfflineStatsReady(player->s.number)) Scr_Error("dstat: player stats are not ready", 0);
    const unsigned int argc = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    const unsigned int depth = argc - (write ? 1 : 0);
    if (argc < (write ? 2u : 1u) || depth > 8)
        Scr_Error("dstat: expected 1 to 8 path elements and, for setdstat, one value", 0);
    *state = *LiveStats_GetRootDDLState();
    if (!state->ddl) Scr_Error("dstat: stats schema is unavailable", 0);
    for (unsigned int index = 0; index < depth; ++index)
    {
        const int type = Scr_GetType(index, SCRIPTINSTANCE_SERVER);
        if (type == 2)
        {
            if (state->member && state->member->arraySize > 1 && state->arrayIndex == -1
                && state->member->enumIndex == -1)
                Scr_Error("dstat: array index must be an integer", 0);
            const char *name = Scr_GetString(index, SCRIPTINSTANCE_SERVER);
            if (!DDL_MoveToName(state, state, name))
                Scr_Error(va("dstat: unknown member '%s'", name), 0);
        }
        else if (type == 6)
        {
            const int element = Scr_GetInt(index, SCRIPTINSTANCE_SERVER);
            if (!state->member || state->member->arraySize <= 1 || state->member->enumIndex != -1 || state->arrayIndex != -1
                || element < 0 || element >= state->member->arraySize)
                Scr_Error("dstat: invalid array index", 0);
            if (!DDL_MoveToIndex(state, state, element, 1))
                Scr_Error("dstat: unable to resolve array index", 0);
        }
        else Scr_Error("dstat: path elements must be strings or integers", 0);
    }
    SV_OfflineStatsBuffer(player->s.number, state); // Validate complete leaf and extent before IO.
    return player;
}

static unsigned __int64 GScr_OfflineDStatUInt64(const char *text)
{
    unsigned __int64 value = 0;
    if (!text || !*text) Scr_Error("setdstat: expected unsigned decimal 64-bit string", 0);
    for (; *text; ++text)
    {
        if (*text < '0' || *text > '9') Scr_Error("setdstat: invalid unsigned decimal 64-bit string", 0);
        const unsigned int digit = *text - '0';
        if (value > (0xffffffffffffffffULL - digit) / 10)
            Scr_Error("setdstat: unsigned 64-bit value overflow", 0);
        value = value * 10 + digit;
    }
    return value;
}
#endif

void __cdecl GScr_GetDStat(scr_entref_t entref)
{
#if defined(KISAK_DEDICATED) && defined(OPENBLOPS_NO_STEAM_AUTH)
    ddlState_t state;
    gentity_s *player = GScr_OfflineDStatPath(entref, false, &state);
    switch (state.member->type)
    {
    case 0: case 1: case 2:
        Scr_AddInt(SV_GetClientDIntStat(player->s.number, &state), SCRIPTINSTANCE_SERVER);
        break;
    case 3:
        Scr_AddString(va("%llu", SV_GetClientDInt64Stat(player->s.number, &state)), SCRIPTINSTANCE_SERVER);
        break;
    case 5:
        Scr_AddString(SV_GetClientDStringStat(player->s.number, &state), SCRIPTINSTANCE_SERVER);
        break;
    default: Scr_Error("getdstat: unsupported stat type", 0);
    }
#elif defined(KISAK_LIVE) || defined(KISAK_NX) || (defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH))
    // Service-free listen servers copy the local profile at gamestate creation.
    // Read that per-client buffer (SV_GetClientDIntStat). Online-flagged games -- private
    // match through the offline lobby menus -- read every custom class slot
    // with getdstat("CacLoadouts", ...); the "not implemented" stub below
    // ended that script thread and left the player with no loadout.
    char *String; // eax
    char *v2; // eax
    const char *v3; // eax
    VariableUnion v4; // eax
    unsigned int ClientDIntStat; // eax
    char *ClientDStringStat; // eax
    __int64 v7; // rax
    char *v8; // eax
    int Type; // [esp+4h] [ebp-20h]
    signed int i; // [esp+8h] [ebp-1Ch]
    gentity_s *playerEnt; // [esp+Ch] [ebp-18h]
    ddlState_t searchState; // [esp+10h] [ebp-14h] BYREF
    int argc; // [esp+20h] [ebp-4h]

    searchState = *LiveStats_GetRootDDLState();
    playerEnt = GetEntity(entref);
    if ( !playerEnt->client )
        Scr_Error("getdstat: entity must be a player entity", 0);
    argc = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( argc > 8 )
        Scr_Error("getdstat: path is too deep.", 0);
    for ( i = 0; i < argc; ++i )
    {
        Type = Scr_GetType(i, SCRIPTINSTANCE_SERVER);
        if ( Type == 2 )
        {
            if ( searchState.member
                && searchState.member->arraySize > 1
                && searchState.member->enumIndex == -1
                && searchState.arrayIndex == -1 )
            {
                Scr_Error("getdstat: array index (integer) expected. Received a string instead.", 0);
            }
            String = Scr_GetString(i, SCRIPTINSTANCE_SERVER);
            if ( !DDL_MoveToName(&searchState, &searchState, String) )
            {
                v2 = Scr_GetString(i, SCRIPTINSTANCE_SERVER);
                v3 = va("getdstat: Could not find member name %s.", v2);
                Scr_Error(v3, 0);
            }
        }
        else if ( Type == 6 )
        {
            if ( !searchState.member
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            12957,
                            0,
                            "%s",
                            "searchState.member") )
            {
                __debugbreak();
            }
            if ( searchState.member->arraySize == 1 )
                Scr_Error("getdstat: member name (string) expected. Received an integer instead.", 0);
            v4.intValue = Scr_GetInt(i, SCRIPTINSTANCE_SERVER);
            if ( !DDL_MoveToIndex(&searchState, &searchState, v4.intValue, 1) )
                Scr_Error("getdstat: Could not find member array index number.", 0);
        }
        else
        {
            Scr_Error("getdstat: Expected strings or integers only.", 0);
        }
    }
    if ( searchState.member )
    {
        switch ( searchState.member->type )
        {
            case 0:
            case 1:
            case 2:
                ClientDIntStat = SV_GetClientDIntStat(playerEnt->s.number, &searchState);
                Scr_AddInt(ClientDIntStat, SCRIPTINSTANCE_SERVER);
                break;
            case 3:
                v7 = SV_GetClientDInt64Stat(playerEnt->s.number, &searchState);
                v8 = va("%llu", v7);
                Scr_AddString(v8, SCRIPTINSTANCE_SERVER);
                break;
            case 5:
                ClientDStringStat = SV_GetClientDStringStat(playerEnt->s.number, &searchState);
                Scr_AddString(ClientDStringStat, SCRIPTINSTANCE_SERVER);
                break;
            default:
                Scr_Error("getdstat: stat type undefined", 0);
                Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
                break;
        }
    }
    else
    {
        Scr_Error("getdstat: could not find ddl member.", 0);
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
#else
    Scr_Error("Stats not implemented in Kisak Black.", 0);
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
#endif
}

void GScr_GetMaxActiveContracts()
{
#if defined(KISAK_DEDICATED) && defined(OPENBLOPS_NO_STEAM_AUTH)
    // Online contract slots are unavailable in the direct-connect server mode.
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
#else
    Scr_AddInt(3, SCRIPTINSTANCE_SERVER);
#endif
}

void __cdecl GScr_GetIndexForActiveContract(scr_entref_t entref)
{
    unsigned int IndexForActiveContract; // eax
    gentity_s *playerEnt; // [esp+0h] [ebp-8h]
    unsigned int activeContractIndex; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        playerEnt = GetEntity(entref);
        if ( playerEnt->client )
        {
            activeContractIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            IndexForActiveContract = LiveContracts_SV_GetIndexForActiveContract(playerEnt->s.number, activeContractIndex);
            Scr_AddInt(IndexForActiveContract, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_Error("GetIndexForActiveContract: Entity must be a client", 0);
        }
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetActiveContractsForStat", 0);
    }
}

void GScr_GetContractStatType()
{
    VariableUnion v0; // eax
    char *ContractStatType; // eax

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        ContractStatType = LiveContracts_GetContractStatType(v0.intValue);
        Scr_AddString(ContractStatType, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetContractStatType", 0);
    }
}

void GScr_GetContractStatName()
{
    VariableUnion v0; // eax
    char *ContractStatName; // eax

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        ContractStatName = LiveContracts_GetContractStatName(v0.intValue);
        Scr_AddString(ContractStatName, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetContractStatName", 0);
    }
}

void GScr_GetContractRewardXP()
{
    VariableUnion v0; // eax
    int ContractRewardXP; // eax

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        ContractRewardXP = LiveContracts_GetContractRewardXP(v0.intValue);
        Scr_AddInt(ContractRewardXP, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetContractRewardXP", 0);
    }
}

void GScr_GetContractRewardCP()
{
    VariableUnion v0; // eax
    int ContractRewardCP; // eax

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        ContractRewardCP = LiveContracts_GetContractRewardCP(v0.intValue);
        Scr_AddInt(ContractRewardCP, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetContractRewardCP", 0);
    }
}

void GScr_GetContractRequirements()
{
    char *reqData; // [esp+0h] [ebp-10h]
    char *reqType; // [esp+4h] [ebp-Ch]
    signed int i; // [esp+8h] [ebp-8h]
    int contractIndex; // [esp+Ch] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        contractIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        Scr_MakeArray(SCRIPTINSTANCE_SERVER);
        for ( i = 0; i < 5; ++i )
        {
            reqType = LiveContracts_GetRequirementType(contractIndex, i);
            if ( reqType )
            {
                Scr_AddString(reqType, SCRIPTINSTANCE_SERVER);
                Scr_AddArray(SCRIPTINSTANCE_SERVER);
                reqData = LiveContracts_GetRequirementData(contractIndex, i);
                if ( reqData )
                    Scr_AddString(reqData, SCRIPTINSTANCE_SERVER);
                else
                    Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
                Scr_AddArray(SCRIPTINSTANCE_SERVER);
            }
        }
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetContractRequirements", 0);
    }
}

void GScr_GetContractName()
{
    char *ContractName; // eax
    VariableUnion contractIndex; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        contractIndex.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        ContractName = LiveContracts_GetContractName(contractIndex.intValue);
        Scr_AddString(ContractName, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetContractName", 0);
    }
}

void GScr_GetContractRequiredCount()
{
    int ContractRequiredCount; // eax
    VariableUnion contractIndex; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        contractIndex.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        ContractRequiredCount = LiveContracts_GetContractRequiredCount(contractIndex.intValue);
        Scr_AddInt(ContractRequiredCount, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetContractRequiredCount", 0);
    }
}

void GScr_GetContractResetConditions()
{
    VariableUnion contractIndex; // [esp+0h] [ebp-8h]
    char *resetConditions; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        contractIndex.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        resetConditions = LiveContracts_GetResetConditions(contractIndex.intValue);
        if ( resetConditions )
            Scr_AddString(resetConditions, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetContractResetConditions", 0);
    }
}

void __cdecl GScr_GetActiveContractProgress(scr_entref_t entref)
{
    int ActiveContractProgress; // eax
    gentity_s *playerEnt; // [esp+0h] [ebp-8h]
    unsigned int activeContractIndex; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        playerEnt = GetEntity(entref);
        if ( playerEnt->client )
        {
            activeContractIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            ActiveContractProgress = LiveContracts_SV_GetActiveContractProgress(playerEnt->s.number, activeContractIndex);
            Scr_AddInt(ActiveContractProgress, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_Error("GetActiveContractProgress: Entity must be a client", 0);
        }
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetActiveContractProgress", 0);
    }
}

void __cdecl GScr_IncrementActiveContractProgress(scr_entref_t entref)
{
    int increment; // [esp+0h] [ebp-Ch]
    gentity_s *playerEnt; // [esp+4h] [ebp-8h]
    int activeContractIndex; // [esp+8h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        playerEnt = GetEntity(entref);
        if ( playerEnt->client )
        {
            activeContractIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            increment = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
            if ( increment > 0 )
                LiveContracts_SV_IncrementActiveContractProgress(playerEnt->s.number, activeContractIndex, increment);
            else
                Scr_Error("IncrementActiveContractProgress: <increment> must be >= 0", 0);
        }
        else
        {
            Scr_Error("IncrementActiveContractProgress: Entity must be a client", 0);
        }
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to IncrementActiveContractProgress", 0);
    }
}

void __cdecl GScr_ResetActiveContractProgress(scr_entref_t entref)
{
    gentity_s *playerEnt; // [esp+0h] [ebp-8h]
    unsigned int activeContractIndex; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        playerEnt = GetEntity(entref);
        if ( playerEnt->client )
        {
            activeContractIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            LiveContracts_SV_ResetActiveContractProgress(playerEnt->s.number, activeContractIndex);
        }
        else
        {
            Scr_Error("ResetActiveContractProgress: Entity must be a client", 0);
        }
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to ResetActiveContractProgress", 0);
    }
}

void __cdecl GScr_IncrementActiveContractTime(scr_entref_t entref)
{
    int increment; // [esp+0h] [ebp-Ch]
    gentity_s *playerEnt; // [esp+4h] [ebp-8h]
    int activeContractIndex; // [esp+8h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        playerEnt = GetEntity(entref);
        if ( playerEnt->client )
        {
            activeContractIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            increment = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
            if ( increment >= 0 )
                LiveContracts_SV_IncrementActiveContractTime(playerEnt->s.number, activeContractIndex, increment);
            else
                Scr_Error("IncrementActiveContractTime: <increment> must be >= 0", 0);
        }
        else
        {
            Scr_Error("IncrementActiveContractTime: Entity must be a client", 0);
        }
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to IncrementActiveContractTime", 0);
    }
}

void __cdecl GScr_IsActiveContractComplete(scr_entref_t entref)
{
    gentity_s *playerEnt; // [esp+0h] [ebp-8h]
    unsigned int activeContractIndex; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        playerEnt = GetEntity(entref);
        if ( playerEnt->client )
        {
            activeContractIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            if ( LiveContracts_SV_GetActiveContractStatus(playerEnt->s.number, activeContractIndex) == 2 )
                Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
            else
                Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_Error("IsActiveContractComplete: Entity must be a client", 0);
        }
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to IsActiveContractComplete", 0);
    }
}

void __cdecl GScr_HasActiveContractExpired(scr_entref_t entref)
{
    gentity_s *playerEnt; // [esp+0h] [ebp-8h]
    unsigned int activeContractIndex; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        playerEnt = GetEntity(entref);
        if ( playerEnt->client )
        {
            activeContractIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            if ( LiveContracts_SV_GetActiveContractStatus(playerEnt->s.number, activeContractIndex) == 3 )
                Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
            else
                Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_Error("HasActiveContractExpired: Entity must be a client", 0);
        }
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to HasActiveContractExpired", 0);
    }
}

void __cdecl GScr_GetActiveContractTimePassed(scr_entref_t entref)
{
    int CombatTimePassed; // eax
    gentity_s *playerEnt; // [esp+0h] [ebp-8h]
    unsigned int activeContractIndex; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        playerEnt = GetEntity(entref);
        if ( playerEnt->client )
        {
            activeContractIndex = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
            CombatTimePassed = LiveContracts_SV_GetCombatTimePassed(playerEnt->s.number, activeContractIndex);
            Scr_AddInt(CombatTimePassed, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_Error("GetActiveContractTimePassed: Entity must be a client", 0);
        }
    }
    else
    {
        Scr_Error("Invalid number of parameters supplied to GetActiveContractTimePassed", 0);
    }
}

void GScr_GetFogSettings()
{
    unsigned int i; // [esp+8h] [ebp-4Ch]
    float settings[18]; // [esp+Ch] [ebp-48h] BYREF

    R_GetFogSettings(settings);
    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for ( i = 0; i < 0x11; ++i )
    {
        Scr_AddFloat(settings[i], SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}

void GScr_EnableOccluder()
{
    char *occluderName; // [esp+0h] [ebp-8h]
    int enable; // [esp+4h] [ebp-4h]

    occluderName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    enable = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    R_EnableOccluder(occluderName, enable != 0);
}

void Gscr_GetCustomClassLoadoutItem()
{
    VariableUnion v0; // eax
    unsigned __int8 CustomClassLoadoutItemForSlot; // al
    char *String; // [esp-8h] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("GetCustomClassLoadoutItem usage: <classnum>, <itemname>", 0);
    String = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    CustomClassLoadoutItemForSlot = GetCustomClassLoadoutItemForSlot(0, v0.stringValue, String);
    Scr_AddInt(CustomClassLoadoutItemForSlot, SCRIPTINSTANCE_SERVER);
}

void Gscr_GetCustomClassLoadoutModifier()
{
    VariableUnion v0; // eax
    int CustomClassModifierForClass; // eax
    char *String; // [esp-8h] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("GetCustomClassLoadoutItem usage: <classnum>, <itemname>", 0);
    String = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    CustomClassModifierForClass = GetCustomClassModifierForClass(0, v0.stringValue, String);
    Scr_AddInt(CustomClassModifierForClass, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetDStat(scr_entref_t entref)
{
#if defined(KISAK_DEDICATED) && defined(OPENBLOPS_NO_STEAM_AUTH)
    ddlState_t state;
    gentity_s *player = GScr_OfflineDStatPath(entref, true, &state);
    const unsigned int valueIndex = Scr_GetNumParam(SCRIPTINSTANCE_SERVER) - 1;
    const int valueType = Scr_GetType(valueIndex, SCRIPTINSTANCE_SERVER);
    if (state.member->type <= 2 && valueType == 6)
        SV_SetClientDIntStat(player->s.number, &state, Scr_GetInt(valueIndex, SCRIPTINSTANCE_SERVER));
    else if (state.member->type == 3 && valueType == 2)
        SV_SetClientDInt64Stat(player->s.number, &state,
            GScr_OfflineDStatUInt64(Scr_GetString(valueIndex, SCRIPTINSTANCE_SERVER)));
    else if (state.member->type == 5 && valueType == 2)
        SV_SetClientDStringStat(player->s.number, &state, Scr_GetString(valueIndex, SCRIPTINSTANCE_SERVER));
    else Scr_Error("setdstat: value type does not match the DDL member", 0);
#else
    char *String; // eax
    char *v2; // eax
    const char *v3; // eax
    VariableUnion v4; // eax
    char *v5; // eax
    unsigned __int64 v6; // rax
    char *v7; // eax
    char *v8; // eax
    char *v9; // eax
    char *v10; // eax
    VariableUnion v11; // eax
    char *v12; // eax
    int v13; // [esp-8h] [ebp-2Ch]
    char *v14; // [esp-4h] [ebp-28h]
    VariableUnion v15; // [esp-4h] [ebp-28h]
    int v16; // [esp+0h] [ebp-24h]
    int Type; // [esp+4h] [ebp-20h]
    signed int i; // [esp+8h] [ebp-1Ch]
    gentity_s *playerEnt; // [esp+Ch] [ebp-18h]
    ddlState_t searchState; // [esp+10h] [ebp-14h] BYREF
    int argc; // [esp+20h] [ebp-4h]

    searchState = *LiveStats_GetRootDDLState();
    playerEnt = GetEntity(entref);
    if ( !playerEnt->client )
        Scr_Error("setdstat: entity must be a player entity", 0);
    argc = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( argc > 8 )
        Scr_Error("setdstat: path should be at most 8 element deep", 0);
    for ( i = 0; i < argc - 1; ++i )
    {
        Type = Scr_GetType(i, SCRIPTINSTANCE_SERVER);
        if ( Type == 2 )
        {
            if ( searchState.member
                && searchState.member->arraySize > 1
                && searchState.member->enumIndex == -1
                && searchState.arrayIndex == -1 )
            {
                Scr_Error("setdstat: array index (integer) expected. Received a string instead.", 0);
            }
            String = Scr_GetString(i, SCRIPTINSTANCE_SERVER);
            if ( !DDL_MoveToName(&searchState, &searchState, String) )
            {
                v2 = Scr_GetString(i, SCRIPTINSTANCE_SERVER);
                v3 = va("setdstat: Could not find member name %s.", v2);
                Scr_Error(v3, 0);
            }
        }
        else if ( Type == 6 )
        {
            if ( !searchState.member
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            13672,
                            0,
                            "%s",
                            "searchState.member") )
            {
                __debugbreak();
            }
            if ( searchState.member->arraySize == 1 )
                Scr_Error("setdstat: member name (string) expected. Received an integer instead.", 0);
            v4.intValue = Scr_GetInt(i, SCRIPTINSTANCE_SERVER);
            if ( !DDL_MoveToIndex(&searchState, &searchState, v4.intValue, 1) )
                Scr_Error("setdstat: Could not find member array index number.", 0);
        }
        else
        {
            Scr_Error("setdstat: Expected strings or integers only.", 0);
        }
    }
    if ( searchState.member && searchState.member->arraySize > 1 && searchState.arrayIndex == -1 )
        Scr_Error("setdstat: trying to set a non leaf member of the ddl", 0);
    v16 = Scr_GetType(argc - 1, SCRIPTINSTANCE_SERVER);
    if ( v16 == 2 )
    {
        if ( searchState.member && searchState.member->type == 3 )
        {
            v5 = Scr_GetString(argc - 1, SCRIPTINSTANCE_SERVER);
            v6 = I_atoi64(v5);
            SV_SetClientDInt64Stat(playerEnt->s.number, &searchState, v6);
            if ( debugStats && debugStats->current.enabled )
            {
                v7 = Scr_GetString(argc - 1, SCRIPTINSTANCE_SERVER);
                v13 = I_atoi64(v7);
                v8 = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
                Com_Printf(16, "setStat %i %s %i\n", playerEnt->s.number, v8, v13);
            }
        }
        else if ( searchState.member && searchState.member->type == 5 )
        {
            v9 = Scr_GetString(argc - 1, SCRIPTINSTANCE_SERVER);
            SV_SetClientDStringStat(playerEnt->s.number, &searchState, v9);
            if ( debugStats && debugStats->current.enabled )
            {
                v14 = Scr_GetString(argc - 1, SCRIPTINSTANCE_SERVER);
                v10 = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
                Com_Printf(16, "setStat %i %s %i\n", playerEnt->s.number, v10, v14);
            }
        }
        else
        {
            Scr_Error("setdstat: member expects a string or 64 bit integer only. For regular integers, don't use quotes.", 0);
        }
    }
    else if ( v16 == 6 )
    {
        if ( searchState.member && searchState.member->type <= 2u )
        {
            v11.intValue = Scr_GetInt(argc - 1, SCRIPTINSTANCE_SERVER);
            SV_SetClientDIntStat(playerEnt->s.number, &searchState, v11.stringValue);
            if ( debugStats && debugStats->current.enabled )
            {
                v15.intValue = Scr_GetInt(argc - 1, SCRIPTINSTANCE_SERVER);
                v12 = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
                Com_Printf(16, "setStat %i %s %i\n", playerEnt->s.number, v12, v15.intValue);
            }
        }
        else
        {
            Scr_Error("setdstat: member expects a string or 64 bit integers. Pass the value in quotes.", 0);
        }
    }
    else
    {
        Scr_Error("setdstat: Only string or integer values are acceptable.", 0);
    }
#endif
}

void GScr_UploadStats()
{
    const gentity_s *playerEnt; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        playerEnt = Scr_GetEntity(0);
        if ( playerEnt->client )
        {
            if ( playerEnt->s.number >= com_maxclients->current.integer
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            13764,
                            0,
                            "%s",
                            "playerEnt->s.number < com_maxclients->current.integer") )
            {
                __debugbreak();
            }
            if ( playerEnt->s.number < com_maxclients->current.integer )
                SV_UploadStats(playerEnt->s.number);
        }
        else
        {
            Scr_Error("Non-player entity passed to UploadStats()", 0);
        }
    }
    else
    {
        SV_UploadStats();
    }
}

void GScr_GetItemAttachment()
{
    eAttachment ItemAttachment; // eax
    char *AttachmentName; // eax
    VariableUnion attachmentNum; // [esp+0h] [ebp-Ch]
    VariableUnion itemIndex; // [esp+4h] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("GetItemAttachment( <itemIndex>, <attachmentNum> ) takes 2 parameters", 0);
    itemIndex.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    attachmentNum.intValue = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    ItemAttachment = BG_UnlockablesGetItemAttachment(itemIndex.intValue, attachmentNum.intValue);
    AttachmentName = (char *)BG_GetAttachmentName(ItemAttachment);
    Scr_AddString(AttachmentName, SCRIPTINSTANCE_SERVER);
}

void GScr_GetDefaultClassSlot()
{
    char *DefaultClassSlotFromName; // eax
    char *className; // [esp+0h] [ebp-Ch]
    char *slotName; // [esp+4h] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("GetDefaultClassSlot( <classname>, <slot> ) takes 2 parameters", 0);
    className = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    slotName = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    DefaultClassSlotFromName = (char *)BG_UnlockablesGetDefaultClassSlotFromName(className, slotName);
    Scr_AddString(DefaultClassSlotFromName, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetTeamForTrigger(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    char *v3; // eax
    const char *v4; // eax
    char *v5; // [esp-10h] [ebp-18h]
    char *v6; // [esp-Ch] [ebp-14h]
    char *v7; // [esp-Ch] [ebp-14h]
    char *v8; // [esp-8h] [ebp-10h]
    char *v9; // [esp-8h] [ebp-10h]
    unsigned __int16 team; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->classname != scr_const.trigger_use
        && ent->classname != scr_const.trigger_use_touch
        && ent->classname != scr_const.trigger_radius
        && ent->classname != scr_const.trigger_radius_use )
    {
        v8 = SL_ConvertToString(scr_const.trigger_radius_use, SCRIPTINSTANCE_SERVER);
        v6 = SL_ConvertToString(scr_const.trigger_radius, SCRIPTINSTANCE_SERVER);
        v5 = SL_ConvertToString(scr_const.trigger_use_touch, SCRIPTINSTANCE_SERVER);
        v1 = SL_ConvertToString(scr_const.trigger_use, SCRIPTINSTANCE_SERVER);
        v2 = va("setteamfortrigger: trigger entity must be of type %s, %s, %s or %s", v1, v5, v6, v8);
        Scr_Error(v2, 0);
    }
    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( team == scr_const.allies )
    {
        ent->team = 2;
    }
    else if ( team == scr_const.axis )
    {
        ent->team = 1;
    }
    else if ( team == scr_const.none )
    {
        ent->team = 0;
    }
    else
    {
        v9 = SL_ConvertToString(scr_const.none, SCRIPTINSTANCE_SERVER);
        v7 = SL_ConvertToString(scr_const.axis, SCRIPTINSTANCE_SERVER);
        v3 = SL_ConvertToString(scr_const.allies, SCRIPTINSTANCE_SERVER);
        v4 = va("setteamfortrigger: invalid team used must be %s, %s or %s", v3, v7, v9);
        Scr_Error(v4, 0);
    }
}

void __cdecl GScr_SetPerkForTrigger(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    char *v4; // [esp-10h] [ebp-1Ch]
    char *v5; // [esp-Ch] [ebp-18h]
    char *v6; // [esp-8h] [ebp-14h]
    char *perkName; // [esp+0h] [ebp-Ch]
    unsigned int perkIndex; // [esp+4h] [ebp-8h]
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->classname != scr_const.trigger_use
        && ent->classname != scr_const.trigger_use_touch
        && ent->classname != scr_const.trigger_radius
        && ent->classname != scr_const.trigger_radius_use )
    {
        v6 = SL_ConvertToString(scr_const.trigger_radius_use, SCRIPTINSTANCE_SERVER);
        v5 = SL_ConvertToString(scr_const.trigger_radius, SCRIPTINSTANCE_SERVER);
        v4 = SL_ConvertToString(scr_const.trigger_use_touch, SCRIPTINSTANCE_SERVER);
        v1 = SL_ConvertToString(scr_const.trigger_use, SCRIPTINSTANCE_SERVER);
        v2 = va("setperkfortrigger: trigger entity must be of type %s, %s, %s or %s", v1, v4, v5, v6);
        Scr_Error(v2, 0);
    }
    perkName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    perkIndex = BG_GetPerkIndexForName(perkName);
    if ( perkIndex > 0xFF )
    {
        v3 = va("setperkfortrigger: perk index '%d' is out of bounds for perk '%s'", perkIndex, perkName);
        Scr_Error(v3, 0);
    }
    ent->trigger.perk = perkIndex;
}

void __cdecl GScr_SetIgnoreEntForTrigger(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->classname != scr_const.trigger_radius_use )
    {
        v1 = SL_ConvertToString(scr_const.trigger_radius_use, SCRIPTINSTANCE_SERVER);
        v2 = va("setperkfortrigger: trigger entity must be of type %s", v1);
        Scr_Error(v2, 0);
    }
    ent->s.otherEntityNum = Scr_GetEntity(0)->s.number;
}

void __cdecl GScr_ClientClaimTrigger(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    char *v3; // [esp-10h] [ebp-18h]
    char *v4; // [esp-Ch] [ebp-14h]
    char *v5; // [esp-8h] [ebp-10h]
    gentity_s *clientEnt; // [esp+0h] [ebp-8h]
    gentity_s *triggerEnt; // [esp+4h] [ebp-4h]

    clientEnt = GetEntity(entref);
    if ( !clientEnt->client )
        Scr_Error("clientclaimtrigger: claimer must be a client.", 0);
    triggerEnt = Scr_GetEntity(0);
    if ( triggerEnt->classname != scr_const.trigger_use
        && triggerEnt->classname != scr_const.trigger_use_touch
        && triggerEnt->classname != scr_const.trigger_radius
        && triggerEnt->classname != scr_const.trigger_radius_use )
    {
        v5 = SL_ConvertToString(scr_const.trigger_radius_use, SCRIPTINSTANCE_SERVER);
        v4 = SL_ConvertToString(scr_const.trigger_radius, SCRIPTINSTANCE_SERVER);
        v3 = SL_ConvertToString(scr_const.trigger_use_touch, SCRIPTINSTANCE_SERVER);
        v1 = SL_ConvertToString(scr_const.trigger_use, SCRIPTINSTANCE_SERVER);
        v2 = va("clientclaimtrigger: trigger entity must be of type %s or %s or %s or %s", v1, v3, v4, v5);
        Scr_Error(v2, 0);
    }
    if ( triggerEnt->item[1].ammoCount == 1023 || triggerEnt->item[1].ammoCount == clientEnt->client->ps.clientNum )
        triggerEnt->item[1].ammoCount = clientEnt->client->ps.clientNum;
}

void __cdecl GScr_ClientReleaseTrigger(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    char *v3; // [esp-10h] [ebp-18h]
    char *v4; // [esp-Ch] [ebp-14h]
    char *v5; // [esp-8h] [ebp-10h]
    gentity_s *clientEnt; // [esp+0h] [ebp-8h]
    gentity_s *triggerEnt; // [esp+4h] [ebp-4h]

    clientEnt = GetEntity(entref);
    if ( !clientEnt->client )
        Scr_Error("clientreleasetrigger: releaser must be a client.", 0);
    triggerEnt = Scr_GetEntity(0);
    if ( triggerEnt->classname != scr_const.trigger_use
        && triggerEnt->classname != scr_const.trigger_use_touch
        && triggerEnt->classname != scr_const.trigger_radius
        && triggerEnt->classname != scr_const.trigger_radius_use )
    {
        v5 = SL_ConvertToString(scr_const.trigger_radius_use, SCRIPTINSTANCE_SERVER);
        v4 = SL_ConvertToString(scr_const.trigger_radius, SCRIPTINSTANCE_SERVER);
        v3 = SL_ConvertToString(scr_const.trigger_use_touch, SCRIPTINSTANCE_SERVER);
        v1 = SL_ConvertToString(scr_const.trigger_use, SCRIPTINSTANCE_SERVER);
        v2 = va("clientreleasetrigger: trigger entity must be of type %s or %s or %s or %s", v1, v3, v4, v5);
        Scr_Error(v2, 0);
    }
    if ( triggerEnt->item[1].ammoCount == clientEnt->client->ps.clientNum )
        triggerEnt->item[1].ammoCount = 1023;
}

void __cdecl GScr_ReleaseClaimedTrigger(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    char *v3; // [esp+0h] [ebp-Ch]
    gentity_s *triggerEnt; // [esp+8h] [ebp-4h]

    triggerEnt = GetEntity(entref);
    if ( triggerEnt->classname != scr_const.trigger_use && triggerEnt->classname != scr_const.trigger_use_touch )
    {
        v3 = SL_ConvertToString(scr_const.trigger_use_touch, SCRIPTINSTANCE_SERVER);
        v1 = SL_ConvertToString(scr_const.trigger_use, SCRIPTINSTANCE_SERVER);
        v2 = va("releaseclaimedtrigger: trigger entity must be of type %s or %s", v1, v3);
        Scr_Error(v2, 0);
    }
    triggerEnt->item[1].ammoCount = 1023;
}

void GScr_SetMapCenter()
{
    float mapCenter[3]; // [esp+0h] [ebp-Ch] BYREF

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("Expected 1 argument to setMapCenter()", 0);
    Scr_GetVector(0, mapCenter, SCRIPTINSTANCE_SERVER);
    SV_SetMapCenter(mapCenter);
}

void GScr_SetDemoIntermissionPoint()
{
    float origin[3]; // [esp+0h] [ebp-18h] BYREF
    float angles[3]; // [esp+Ch] [ebp-Ch] BYREF

    if ( Demo_IsEnabled() && Demo_IsRecording() )
    {
        if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
            Scr_Error("Expected 2 argument to SetDemoIntermissionPoint()", 0);
        Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
        Scr_GetVector(1u, angles, SCRIPTINSTANCE_SERVER);
        Demo_SetIntermissionPoint(origin, angles);
    }
}

void GScr_StartDemoRecording()
{
    if ( Demo_IsEnabled() )
    {
        if ( Demo_IsIdle() )
            Cbuf_AddText(0, "demo_startrecord\n");
    }
}

void GScr_StopDemoRecording()
{
    if ( Demo_IsRecording() )
        Cbuf_AddText(0, "demo_stoprecord\n");
}

void GScr_IsDemoRecording()
{
    bool IsRecording; // al

    IsRecording = Demo_IsRecording();
    Scr_AddBool(IsRecording, SCRIPTINSTANCE_SERVER);
}

void isDemoEnabled()
{
    bool IsEnabled; // al

    IsEnabled = Demo_IsEnabled();
    Scr_AddBool(IsEnabled, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_isTestClient(scr_entref_t entref)
{
    bool IsTestClient; // al
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("istestclient: entity must be a player entity", 0);
    IsTestClient = SV_IsTestClient(ent->s.number);
    Scr_AddBool(IsTestClient, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_isDemoClient(scr_entref_t entref)
{
    bool IsDemoClient; // al
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("isdemoclient: entity must be a player entity", 0);
    IsDemoClient = SV_IsDemoClient(ent->s.number);
    Scr_AddBool(IsDemoClient, SCRIPTINSTANCE_SERVER);
}

void GScr_SetGameEndTime()
{
    VariableUnion v0; // eax

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("Expected 1 argument to setGameEndTime()", 0);
    v0.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    SV_SetGameEndTime(v0.intValue);
}

void GScr_SetTimeScale()
{
#ifdef KISAK_SP
    const int maxClients = com_maxclients->current.integer;
    int connected = 0;
    for (int client = 0; client < maxClients; ++client)
        if (svs.clients[client].header.state > CS_ZOMBIE)
            ++connected;
    if (connected <= 1)
        Dvar_SetFloat(const_cast<dvar_t *>(com_timescale), Scr_GetFloat(0, SCRIPTINSTANCE_SERVER));
#else
    float endTimeScale; // [esp+0h] [ebp-8h]
    int intValue; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("Expected 2 arguments to SetTimeScale()", 0);
    intValue = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    endTimeScale = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    SV_SetTimeScale(endTimeScale, intValue);
#endif
}

void GScr_SetMiniMap()
{
    char *v0; // eax
    float v1; // [esp+20h] [ebp-58h]
    float v2; // [esp+2Ch] [ebp-4Ch]
    float upperLeft; // [esp+38h] [ebp-40h]
    float upperLeft_4; // [esp+3Ch] [ebp-3Ch]
    char *material; // [esp+40h] [ebp-38h]
    char northYawString[32]; // [esp+44h] [ebp-34h] BYREF
    float north[2]; // [esp+68h] [ebp-10h]
    float lowerRight[2]; // [esp+70h] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 5 )
        Scr_Error("Expecting 5 arguments", 0);
    material = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    upperLeft = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    upperLeft_4 = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    lowerRight[0] = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    lowerRight[1] = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
    SV_GetConfigstring(0x60Cu, northYawString, 32);
    v1 = atof(northYawString);
    v2 = v1 * 0.017453292;
    north[0] = cos(v2);
    north[1] = sin(v2);
    level.compassMapWorldSize[0] = (float)((float)(lowerRight[0] - upperLeft) * north[1])
                                                             - (float)((float)(lowerRight[1] - upperLeft_4) * north[0]);
    level.compassMapWorldSize[1] = (float)((-(lowerRight[0] - upperLeft)) * north[0])
                                                             - (float)((float)(lowerRight[1] - upperLeft_4) * north[1]);
    if ( level.compassMapWorldSize[0] < 0.0 || level.compassMapWorldSize[1] < 0.0 )
        Scr_Error(
            "lower-right X and Y coordinates must be both south and east of upper-left X and Y coordinates in terms of the northyaw",
            0);
    level.compassMapUpperLeft[0] = upperLeft;
    level.compassMapUpperLeft[1] = upperLeft_4;
    v0 = va("\"%s\" %f %f %f %f", material, upperLeft, upperLeft_4, lowerRight[0], lowerRight[1]);
    SV_SetConfigstring(1549, v0);
}

void GScr_IncrementEscrow()
{
    const char *v0; // eax
    char *xuidString; // [esp+0h] [ebp-10h]
    int amount; // [esp+4h] [ebp-Ch]
    unsigned __int64 xuid; // [esp+8h] [ebp-8h] BYREF

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        xuidString = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        StringToXUID(xuidString, &xuid);
        amount = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
        v0 = va("%s %llu %d\n", "incrementescrow", xuid, amount);
        Cbuf_AddText(0, v0);
    }
    else
    {
        Scr_Error("Invalid number of parameters passed to IncrementEscrow", 0);
    }
}

void GScr_SetTeamSpyplane()
{
    char *v1; // eax
    char *v2; // eax
    unsigned __int16 team; // [esp+0h] [ebp-8h]
    unsigned int SpyplaneAvailable; // [esp+4h] [ebp-4h]

    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if (team != scr_const.allies && team != scr_const.axis && team != scr_const.none)
    {
        v1 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v2 = va("Illegal team string '%s'. Must be allies, axis, or none.", v1);
        Scr_ParamError(0, v2, SCRIPTINSTANCE_SERVER);
    }
    SpyplaneAvailable = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    if (team == scr_const.allies)
    {
        level.teamHasSpyplane[2] = SpyplaneAvailable;
    }
    else if (team == scr_const.axis)
    {
        level.teamHasSpyplane[1] = SpyplaneAvailable;
    }
    else
    {
        if (team != scr_const.none
            && !Assert_MyHandler(
                "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                14352,
                0,
                "%s",
                "team == scr_const.none"))
        {
            __debugbreak();
        }
        level.teamHasSpyplane[0] = SpyplaneAvailable;
    }
}

void GScr_GetTeamSpyplane()
{
    char *v0; // eax
    const char *v1; // eax
    unsigned __int16 team; // [esp+0h] [ebp-8h]

    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( team == scr_const.allies )
    {
        Scr_AddInt(level.teamHasSpyplane[2], SCRIPTINSTANCE_SERVER);
    }
    else if ( team == scr_const.axis )
    {
        Scr_AddInt(level.teamHasSpyplane[1], SCRIPTINSTANCE_SERVER);
    }
    else if ( team == scr_const.none )
    {
        Scr_AddInt(level.teamHasSpyplane[0], SCRIPTINSTANCE_SERVER);
    }
    else
    {
        v0 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v1 = va("Illegal team string '%s'. Must be allies, axis, or none.", v0);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_SetTeamSatellite()
{
    char *v1; // eax
    char *v2; // eax
    unsigned __int16 team; // [esp+0h] [ebp-8h]
    unsigned int SatelliteAvailable; // [esp+4h] [ebp-4h]

    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if (team != scr_const.allies && team != scr_const.axis && team != scr_const.none)
    {
        v1 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v2 = va("Illegal team string '%s'. Must be allies, axis, or none.", v1);
        Scr_ParamError(0, v2, SCRIPTINSTANCE_SERVER);
    }
    SatelliteAvailable = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    if (team == scr_const.allies)
    {
        level.teamHasSatellite[2] = SatelliteAvailable;
    }
    else if (team == scr_const.axis)
    {
        level.teamHasSatellite[1] = SatelliteAvailable;
    }
    else
    {
        if (team != scr_const.none
            && !Assert_MyHandler(
                "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                14431,
                0,
                "%s",
                "team == scr_const.none"))
        {
            __debugbreak();
        }
        level.teamHasSatellite[0] = SatelliteAvailable;
    }
}

void GScr_GetTeamSatellite()
{
    char *v0; // eax
    const char *v1; // eax
    unsigned __int16 team; // [esp+0h] [ebp-8h]

    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( team == scr_const.allies )
    {
        Scr_AddInt(level.teamHasSatellite[2], SCRIPTINSTANCE_SERVER);
    }
    else if ( team == scr_const.axis )
    {
        Scr_AddInt(level.teamHasSatellite[1], SCRIPTINSTANCE_SERVER);
    }
    else if ( team == scr_const.none )
    {
        Scr_AddInt(level.teamHasSatellite[0], SCRIPTINSTANCE_SERVER);
    }
    else
    {
        v0 = SL_ConvertToString(team, SCRIPTINSTANCE_SERVER);
        v1 = va("Illegal team string '%s'. Must be allies, axis, or none.", v0);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_GetArrayKeys()
{
    const char *TypeName; // eax
    const char *v1; // eax
    VariableUnion id; // [esp+0h] [ebp-4h]

    if ( Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) != 20 )
    {
        TypeName = Scr_GetTypeName(0, SCRIPTINSTANCE_SERVER);
        v1 = va("Parameter (%s) must be an array", TypeName);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
    id.intValue = Scr_GetObject(0, SCRIPTINSTANCE_SERVER);
    Scr_AddArrayKeys(id.stringValue, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_Launch(scr_entref_t entref)
{
    const char *v1; // eax
    float *v2; // [esp+28h] [ebp-48h]
    float *trDelta; // [esp+38h] [ebp-38h]
    float avelocity[3]; // [esp+54h] [ebp-1Ch] BYREF
    float velocity[3]; // [esp+60h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+6Ch] [ebp-4h]
    int savedregs; // [esp+70h] [ebp+0h] BYREF

    if ( !Scr_GetNumParam(SCRIPTINSTANCE_SERVER) || (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 2 )
        Scr_Error("Incorrect number of parameters\n", 0);
    ent = GetEntity(entref);
    Scr_GetVector(0, velocity, SCRIPTINSTANCE_SERVER);
    if ( (LODWORD(velocity[0]) & 0x7F800000) == 0x7F800000
        || (LODWORD(velocity[1]) & 0x7F800000) == 0x7F800000
        || (LODWORD(velocity[2]) & 0x7F800000) == 0x7F800000 )
    {
        v1 = va("invalid velocity parameter in launch command: %f %f %f", velocity[0], velocity[1], velocity[2]);
        Scr_Error(v1, 0);
    }
    if ( ((LODWORD(velocity[0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(velocity[1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(velocity[2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    14561,
                    0,
                    "%s",
                    "!IS_NAN((velocity)[0]) && !IS_NAN((velocity)[1]) && !IS_NAN((velocity)[2])") )
    {
        __debugbreak();
    }
    ent->s.lerp.pos.trType = 6;
    ent->s.lerp.pos.trTime = level.time;
    trDelta = ent->s.lerp.pos.trDelta;
    ent->s.lerp.pos.trDelta[0] = velocity[0];
    trDelta[1] = velocity[1];
    trDelta[2] = velocity[2];
    if ( ((LODWORD(ent->s.lerp.pos.trDelta[0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(ent->s.lerp.pos.trDelta[1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(ent->s.lerp.pos.trDelta[2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    14571,
                    0,
                    "%s",
                    "!IS_NAN((ent->s.lerp.pos.trDelta)[0]) && !IS_NAN((ent->s.lerp.pos.trDelta)[1]) && !IS_NAN((ent->s.lerp.pos.trDelta)[2])") )
    {
        __debugbreak();
    }
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
    {
        Scr_GetVector(1u, avelocity, SCRIPTINSTANCE_SERVER);
        ent->s.lerp.apos.trType = 3;
        ent->s.lerp.apos.trTime = level.time;
        v2 = ent->s.lerp.apos.trDelta;
        ent->s.lerp.apos.trDelta[0] = avelocity[0];
        v2[1] = avelocity[1];
        v2[2] = avelocity[2];
        if ( ((LODWORD(ent->s.lerp.apos.trDelta[0]) & 0x7F800000) == 0x7F800000
             || (LODWORD(ent->s.lerp.apos.trDelta[1]) & 0x7F800000) == 0x7F800000
             || (LODWORD(ent->s.lerp.apos.trDelta[2]) & 0x7F800000) == 0x7F800000)
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        14586,
                        0,
                        "%s",
                        "!IS_NAN((ent->s.lerp.apos.trDelta)[0]) && !IS_NAN((ent->s.lerp.apos.trDelta)[1]) && !IS_NAN((ent->s.lerp.apos.trDelta)[2])") )
        {
            __debugbreak();
        }
    }
    ent->physicsObject = 1;
    if ( ent->s.eType != 4 )
    {
        ent->r.contents = 0;
        SV_LinkEntity(ent);
    }
}

#ifdef KISAK_SP
void __cdecl GScr_MagicBullet()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 3)
        Scr_Error("MagicBullet weaponName sourceLoc destLoc.\n", 0);

    const char *weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const unsigned int weapon = G_GetWeaponIndexForName((char *)weaponName);
    if (!weapon)
        Scr_Error(va("MagicBullet called with unknown weapon name %s\n", weaponName), 0);

    float source[3], destination[3], direction[3];
    Scr_GetVector(1, source, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2, destination, SCRIPTINSTANCE_SERVER);
    weaponParms wp;
    Weapon_SetWeaponParamsWeapon(&wp, weapon);
    for (int axis = 0; axis != 3; ++axis)
    {
        wp.muzzleTrace[axis] = source[axis];
        direction[axis] = destination[axis] - source[axis];
    }

    const auto normalize = [](float *vector, bool zFirst)
    {
        volatile float xSquared = vector[0] * vector[0];
        volatile float ySquared = vector[1] * vector[1];
        volatile float zSquared = vector[2] * vector[2];
        volatile float partial = (zFirst ? zSquared : xSquared) + ySquared;
        volatile float squaredLength = partial + (zFirst ? xSquared : zSquared);
        const float length = sqrtf(squaredLength);
        const float divisor = -length >= 0.0f ? 1.0f : length;
        volatile float inverseLength = 1.0f / divisor;
        for (int axis = 0; axis != 3; ++axis)
            vector[axis] = vector[axis] * inverseLength;
    };
    normalize(direction, true);
    for (int axis = 0; axis != 3; ++axis)
        wp.forward[axis] = direction[axis];
    for (int axis = 0; axis != 3; ++axis)
        wp.right[axis] = wp.up[axis] = 0.0f;
    float targetOffset[3] = {0.0f, 0.0f, 0.0f};

    if (wp.weapDef->weapType == WEAPTYPE_GRENADE || wp.weapDef->weapType == WEAPTYPE_MINE)
        Scr_Error("MagicBullet() does not work with grenade-type weapons.\n", 0);

    gentity_s *attacker = &g_entities[ENTITYNUM_WORLD];
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 4 && Scr_GetType(3, SCRIPTINSTANCE_SERVER))
        attacker = Scr_GetEntity(3);
    gentity_s *target = nullptr;
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 5)
        target = Scr_GetEntity(4);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 6)
        Scr_GetVector(5, targetOffset, SCRIPTINSTANCE_SERVER);

    float spread = 0.0f;
    if (wp.weapDef->weapClass == WEAPCLASS_SPREAD)
    {
        spread = wp.weapDef->fHipSpreadStandMin;
        volatile float squares[3] = {
            wp.forward[0] * wp.forward[0],
            wp.forward[1] * wp.forward[1],
            wp.forward[2] * wp.forward[2]};
        int perpendicularAxis = squares[0] > squares[1] ? 1 : 0;
        if (squares[perpendicularAxis] > squares[2])
            perpendicularAxis = 2;
        const float scale = -wp.forward[perpendicularAxis];
        for (int axis = 0; axis != 3; ++axis)
            wp.up[axis] = scale * wp.forward[axis];
        wp.up[perpendicularAxis] = wp.up[perpendicularAxis] + 1.0f;
        normalize(wp.up, false);
        for (int axis = 0; axis != 3; ++axis)
        {
            const int next = (axis + 1) % 3;
            const int last = (axis + 2) % 3;
            volatile float first = wp.forward[next] * wp.up[last];
            volatile float second = wp.forward[last] * wp.up[next];
            wp.right[axis] = first - second;
        }
    }

    gentity_s *projectile = nullptr;
    switch (wp.weapDef->weapType)
    {
    case WEAPTYPE_BULLET:
        Bullet_Fire(attacker, spread, &wp, attacker, level.time);
        break;
    case WEAPTYPE_GAS:
        Weapon_Flamethrower_Fire(attacker, &wp);
        break;
    case WEAPTYPE_PROJECTILE:
        switch (wp.weapDef->weapClass)
        {
        case WEAPCLASS_GRENADE:
            projectile = Weapon_GrenadeLauncher_Fire(attacker, weapon, 0, &wp);
            break;
        case WEAPCLASS_PISTOL:
        case WEAPCLASS_ROCKETLAUNCHER:
        case WEAPCLASS_TURRET:
            projectile = Weapon_RocketLauncher_Fire(attacker, weapon, 0.0f, &wp,
                vec3_origin, target, targetOffset);
            break;
        default:
            Scr_Error("MagicBullet(): Unhandled projectile weapClass.\n", 0);
            break;
        }
        break;
    default:
        Scr_Error(va("MagicBullet(): Unhandled weapType \"%s\".\n",
            BG_GetWeaponTypeName(wp.weapDef->weapType)), 0);
        break;
    }

    gentity_s *temp = G_TempEntity(source, EV_FIRE_WEAPON);
    float angles[3];
    vectoangles(direction, angles);
    G_SetAngle(temp, angles);
    temp->s.weapon = static_cast<unsigned char>(weapon);
    temp->s.eventParms[temp->s.eventSequence & 3] = 0;
    if (projectile)
        Scr_AddEntity(projectile, SCRIPTINSTANCE_SERVER);
}
#else
void __cdecl GScr_MagicBullet()
{
    const char *WeaponTypeName; // eax
    weapType_t weapType; // [esp+14h] [ebp-BCh]
    float source[3]; // [esp+24h] [ebp-ACh] BYREF
    gentity_s *tempEnt; // [esp+30h] [ebp-A0h]
    gentity_s *attacker; // [esp+34h] [ebp-9Ch]
    float dir[3]; // [esp+38h] [ebp-98h] BYREF
    gentity_s *projectile; // [esp+44h] [ebp-8Ch]
    int weapon; // [esp+48h] [ebp-88h]
    const char *weapName; // [esp+4Ch] [ebp-84h]
    gentity_s *targetEnt; // [esp+50h] [ebp-80h]
    float angles[3]; // [esp+54h] [ebp-7Ch] BYREF
    weaponParms wp; // [esp+60h] [ebp-70h] BYREF
    float dest[3]; // [esp+ACh] [ebp-24h]
    float targetOffset[3]; // [esp+B8h] [ebp-18h] BYREF
    float vecIn[3]; // [esp+C4h] [ebp-Ch] BYREF
    int savedregs; // [esp+D0h] [ebp+0h] BYREF

    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 3 )
        Scr_Error("MagicBullet weaponName sourceLoc destLoc.\n", 0);

    weapName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weapon = G_GetWeaponIndexForName((char *)weapName);
    if ( !weapon )
    {
        Scr_Error(va("MagicBullet called with unknown weapon name %s\n", weapName), 0);
    }
    Scr_GetVector(1u, vecIn, SCRIPTINSTANCE_SERVER);
    source[0] = vecIn[0];
    source[1] = vecIn[1];
    source[2] = vecIn[2];
    Scr_GetVector(2u, vecIn, SCRIPTINSTANCE_SERVER);
    dest[0] = vecIn[0];
    dest[1] = vecIn[1];
    dest[2] = vecIn[2];
    Weapon_SetWeaponParamsWeapon(&wp, weapon);
    wp.muzzleTrace[0] = source[0];
    wp.muzzleTrace[1] = source[1];
    wp.muzzleTrace[2] = source[2];
    dir[0] = dest[0] - source[0];
    dir[1] = dest[1] - source[1];
    dir[2] = dest[2] - source[2];
    Vec3Normalize(dir);
    wp.forward[0] = dir[0];
    wp.forward[1] = dir[1];
    wp.forward[2] = dir[2];
    memset(wp.right, 0, 24);
    memset(targetOffset, 0, sizeof(targetOffset));
    if ( wp.weapDef->weapType == WEAPTYPE_GRENADE || wp.weapDef->weapType == WEAPTYPE_MINE )
        Scr_Error("MagicBullet() does not work with grenade-type weapons.\n", 0);
    attacker = &g_entities[ENTITYNUM_WORLD];
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 4 && Scr_GetType(3u, SCRIPTINSTANCE_SERVER) )
        attacker = Scr_GetEntity(3u);
    targetEnt = 0;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 5 )
        targetEnt = Scr_GetEntity(4u);
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 6 )
        Scr_GetVector(5u, targetOffset, SCRIPTINSTANCE_SERVER);
    projectile = 0;
    weapType = wp.weapDef->weapType;
    if ( weapType )
    {
        if ( weapType == WEAPTYPE_PROJECTILE )
        {
            switch ( wp.weapDef->weapClass )
            {
                case WEAPCLASS_GRENADE:
                    projectile = Weapon_GrenadeLauncher_Fire(attacker, weapon, 0, &wp);
                    break;
                case WEAPCLASS_ROCKETLAUNCHER:
                    goto LABEL_22;
                case WEAPCLASS_TURRET:
                    if ( wp.weapDef->weapType == WEAPTYPE_PROJECTILE )
                    {
LABEL_22:
                        projectile = Weapon_RocketLauncher_Fire(
                                                     attacker,
                                                     weapon,
                                                     0.0,
                                                     &wp,
                                                     vec3_origin,
                                                     targetEnt,
                                                     targetOffset);
                    }
                    else if ( wp.weapDef->weapType == WEAPTYPE_BULLET )
                    {
                        Bullet_Fire(attacker, 0.0, &wp, 0, level.time);
                    }
                    break;
                default:
                    Scr_Error("MagicBullet(): Unhandled projectile weapClass.\n", 0);
                    break;
            }
        }
        else
        {
            WeaponTypeName = BG_GetWeaponTypeName(wp.weapDef->weapType);
            Scr_Error(va("MagicBullet(): Unhandled weapType \"%s\".\n", WeaponTypeName), 0);
        }
    }
    else
    {
        Bullet_Fire(attacker, 0.0, &wp, 0, level.time);
    }
    tempEnt = G_TempEntity(source, EV_FIRE_WEAPON);
    vectoangles(dir, angles);
    G_SetAngle(tempEnt, angles);
    tempEnt->s.weapon = weapon;
    if ( tempEnt->s.weapon != weapon
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    14680,
                    0,
                    "%s",
                    "tempEnt->s.weapon == weapon") )
    {
        __debugbreak();
    }
    tempEnt->s.eventParms[tempEnt->s.eventSequence & 3] = 0;
    if ( attacker )
        tempEnt->s.eventParm = attacker->s.number;
    if ( projectile )
        Scr_AddEntity(projectile, SCRIPTINSTANCE_SERVER);
}
#endif

void __cdecl GScr_LaunchBomb(scr_entref_t entref)
{
    const char *v1; // eax
    const char *v2; // eax
    float velocity[3]; // [esp+Ch] [ebp-7Ch] BYREF
    float dir[3]; // [esp+18h] [ebp-70h] BYREF
    int iWeaponIndex; // [esp+24h] [ebp-64h]
    weaponParms wp; // [esp+28h] [ebp-60h] BYREF
    float targetOffset[3]; // [esp+70h] [ebp-18h] BYREF
    gentity_s *ent; // [esp+7Ch] [ebp-Ch]
    const char *pszWeaponName; // [esp+80h] [ebp-8h]
    gentity_s *player; // [esp+84h] [ebp-4h]

    player = GetEntity(entref);
    if ( !player->client )
        Scr_Error("LaunchBomb: entity must be a player entity", 0);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 3 )
    {
        pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
        if ( !iWeaponIndex )
        {
            if ( *pszWeaponName )
            {
                v1 = va("Invalid weapon name %s", pszWeaponName);
                Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
            }
            else
            {
                Scr_ParamError(0, "Invalid weapon name", SCRIPTINSTANCE_SERVER);
            }
        }
        Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
        if ( wp.weapDef->weapType
            && wp.weapDef->weapType != WEAPTYPE_PROJECTILE
            && wp.weapDef->weapType != WEAPTYPE_BOMB
            && wp.weapDef->weapType != WEAPTYPE_GAS )
        {
            v2 = va("LaunchBomb only support bullet, bomb, gas and projectile weapons\n");
            Scr_Error(v2, 0);
        }
        Scr_GetVector(1u, wp.muzzleTrace, SCRIPTINSTANCE_SERVER);
        Scr_GetVector(2u, velocity, SCRIPTINSTANCE_SERVER);
        memset(targetOffset, 0, sizeof(targetOffset));
        dir[0] = velocity[0];
        dir[1] = velocity[1];
        dir[2] = velocity[2];
        Vec3Normalize(dir);
        ent = G_DropBomb(player, iWeaponIndex, wp.muzzleTrace, dir, velocity, 0, targetOffset);
        Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_Error("illegal call to LaunchBomb(<weapon>, <position>, <velocity>)", 0);
    }
}

void __cdecl GScr_MakeGrenadeDud(scr_entref_t entref)
{
    gentity_s *grenade; // [esp+8h] [ebp-4h]

    grenade = GetEntity(entref);
    if ( grenade->s.eType == 4 )
    {
        if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
            Scr_Error("illegal call to MakeGrenadeDud()", 0);
        else
            grenade->s.lerp.u.turret.ownerNum = 1;
    }
    else
    {
        Scr_Error("MakeGrenadeDud: entity must be a grenade entity", 0);
    }
}

void __cdecl GScr_IsOnLadder(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("setstat: entity must be a player entity", 0);
    Scr_AddBool((ent->client->ps.pm_flags & 8) != 0, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_IsMantling(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_Error("setstat: entity must be a player entity", 0);
    Scr_AddBool((ent->client->ps.pm_flags & 4) != 0, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_StartDoorBreach(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_ObjectError("Not a player entity", SCRIPTINSTANCE_SERVER);
    ent->client->ps.pm_flags |= 0x1000000u;
}

void __cdecl GScr_StopDoorBreach(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->client )
        Scr_ObjectError("Not a player entity", SCRIPTINSTANCE_SERVER);
    ent->client->ps.pm_flags &= ~0x1000000u;
}

void __cdecl GScr_GetLightColor(scr_entref_t entref)
{
    gentity_s *ent; // [esp+0h] [ebp-14h]
    float unpackedColor[4]; // [esp+4h] [ebp-10h] BYREF

    ent = GScr_SetupLightEntity(entref);
    Byte4UnpackRgba((const unsigned __int8 *)&ent->s.lerp.u, unpackedColor);
    Scr_AddVector(unpackedColor, SCRIPTINSTANCE_SERVER);
}

gentity_s *__cdecl GScr_SetupLightEntity(scr_entref_t entref)
{
    char *v1; // eax
    const char *v2; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 14876, 0, "%s", "ent") )
        __debugbreak();
    if ( ent->s.eType != 10 )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("Function can only be called on a 'light' entity; actual classname is '%s'\n", v1);
        Scr_Error(v2, 0);
    }
    return ent;
}

void __cdecl GScr_SetLightColor(scr_entref_t entref)
{
    unsigned __int8 exponent; // [esp+7Fh] [ebp-15h]
    gentity_s *ent; // [esp+80h] [ebp-14h]
    float unpackedColor[4]; // [esp+84h] [ebp-10h] BYREF

    ent = GScr_SetupLightEntity(entref);
    Scr_GetVector(0, unpackedColor, SCRIPTINSTANCE_SERVER);
    unpackedColor[3] = 0.0f;
    exponent = ent->s.lerp.u.primaryLight.colorAndExp[3];
    Byte4PackRgba(unpackedColor, (unsigned __int8 *)&ent->s.lerp.u);
    ent->s.lerp.u.primaryLight.colorAndExp[3] = exponent;
}

void __cdecl GScr_GetLightIntensity(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GScr_SetupLightEntity(entref);
    Scr_AddFloat(ent->s.lerp.u.turret.gunAngles[1], SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetLightIntensity(scr_entref_t entref)
{
    int v1; // [esp+0h] [ebp-Ch]
    float intensity; // [esp+4h] [ebp-8h]
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GScr_SetupLightEntity(entref);
    intensity = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( intensity < -0.001 )
        Scr_ParamError(0, "intensity must be >= 0", SCRIPTINSTANCE_SERVER);
    if ( (float)(intensity - 0.0) < 0.0 )
        v1 = 0;
    else
        v1 = LODWORD(intensity);
    ent->s.lerp.u.loopFx.period = v1;
}

void __cdecl GScr_GetLightRadius(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GScr_SetupLightEntity(entref);
    Scr_AddFloat(ent->s.lerp.u.turret.gunAngles[2], SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetLightRadius(scr_entref_t entref)
{
    const char *v1; // eax
    int v2; // [esp+Ch] [ebp-18h]
    float v3; // [esp+14h] [ebp-10h]
    float radius; // [esp+18h] [ebp-Ch]
    gentity_s *ent; // [esp+1Ch] [ebp-8h]
    const ComPrimaryLight *refLight; // [esp+20h] [ebp-4h]

    ent = GScr_SetupLightEntity(entref);
    refLight = Com_GetPrimaryLight(ent->s.index.brushmodel);
    if ( !refLight
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 15013, 0, "%s", "refLight") )
    {
        __debugbreak();
    }
    radius = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( radius >= -0.001 )
    {
        if ( radius > (float)(refLight->radius + 0.001) )
        {
            v1 = va("radius must be less than the bsp radius (%g)", refLight->radius);
            Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        Scr_ParamError(0, "radius must be >= 0", SCRIPTINSTANCE_SERVER);
    }
    if ( (float)(radius - refLight->radius) < 0.0 )
        v3 = radius;
    else
        v3 = refLight->radius;
    if ( (float)(0.0 - radius) < 0.0 )
        v2 = LODWORD(v3);
    else
        v2 = 0;
    ent->s.lerp.u.actor.team = v2;
}

void __cdecl GScr_GetLightFovInner(scr_entref_t entref)
{
    gentity_s *ent = GScr_SetupLightEntity(entref);
    float innerCos = ent->s.lerp.u.primaryLight.cosHalfFovInner;
#ifdef KISAK_SP
    float fov = (float)acos((double)innerCos) * 2.0f;
#else
    float fov = acosf(innerCos) * 2.0f;
#endif

    Scr_AddFloat(fov, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_GetLightFovOuter(scr_entref_t entref)
{
    gentity_s *ent = GScr_SetupLightEntity(entref);
    float outerCos = ent->s.lerp.u.primaryLight.cosHalfFovOuter;
#ifdef KISAK_SP
    float fov = (float)acos((double)outerCos) * 2.0f;
#else
    float fov = acosf(outerCos) * 2.0f;
#endif

    Scr_AddFloat(fov, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetLightFovRange(scr_entref_t entref)
{
#ifdef KISAK_SP
    gentity_s *ent = GScr_SetupLightEntity(entref);
    const ComPrimaryLight *refLight = Com_GetPrimaryLight(ent->s.index.brushmodel);
    iassert(refLight);

    float fovOuter = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( fovOuter < 0.99900001f || fovOuter >= 136.00101f )
        Scr_ParamError(0, "outer fov must be in the range of 1 to 136", SCRIPTINSTANCE_SERVER);

    float halfOuter = (fovOuter * 0.017453292f) * 0.5f;
    float outerCos = (float)cos((double)halfOuter);
    if ( refLight->cosHalfFovOuter - 0.001f > outerCos )
        Scr_ParamError(0, "outer fov cannot be larger than the fov when the map was compiled", SCRIPTINSTANCE_SERVER);

    float limitedOuter = outerCos - 1.0f >= 0.0f ? 1.0f : outerCos;
    outerCos = refLight->cosHalfFovOuter - outerCos >= 0.0f ? refLight->cosHalfFovOuter : limitedOuter;
    float spacing = outerCos + 0.001f;
    float innerCos;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        float fovInner = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
        if ( fovInner < -0.001f || fovInner >= fovOuter + 0.001f )
            Scr_ParamError(1, "inner fov must be in the range of 0 to outer fov", SCRIPTINSTANCE_SERVER);
        float halfInner = (fovInner * 0.017453292f) * 0.5f;
        innerCos = (float)cos((double)halfInner);
        float limitedInner = innerCos - 1.0f >= 0.0f ? 1.0f : innerCos;
        innerCos = spacing - innerCos >= 0.0f ? spacing : limitedInner;
    }
    else
    {
        innerCos = refLight->cosHalfFovInner - spacing >= 0.0f ? spacing : refLight->cosHalfFovInner;
    }
    if ( outerCos <= 0.0 || innerCos <= outerCos || innerCos > 1.0 )
    {
        const char *v1 = va("%g, %g", outerCos, innerCos);
        if ( !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        15114,
                        0,
                        "%s\n\t%s",
                        "0.0f < cosHalfFovOuter && cosHalfFovOuter < cosHalfFovInner && cosHalfFovInner <= 1.0f",
                        v1) )
            __debugbreak();
    }
    ent->s.lerp.u.primaryLight.cosHalfFovOuter = outerCos;
    ent->s.lerp.u.primaryLight.cosHalfFovInner = innerCos;
#else
    const char *v1; // eax
    long double v2; // [esp+10h] [ebp-3Ch]
    long double v3; // [esp+10h] [ebp-3Ch]
    float v4; // [esp+10h] [ebp-3Ch]
    float v5; // [esp+14h] [ebp-38h]
    float v6; // [esp+18h] [ebp-34h]
    float v7; // [esp+28h] [ebp-24h]
    float v8; // [esp+30h] [ebp-1Ch]
    float cosHalfFovOuter; // [esp+34h] [ebp-18h]
    float fovInner; // [esp+38h] [ebp-14h]
    float fovOuter; // [esp+3Ch] [ebp-10h]
    gentity_s *ent; // [esp+40h] [ebp-Ch]
    float cosHalfFovInner; // [esp+44h] [ebp-8h]
    float cosHalfFovInnera; // [esp+44h] [ebp-8h]
    const ComPrimaryLight *refLight; // [esp+48h] [ebp-4h]

    ent = GScr_SetupLightEntity(entref);
    refLight = Com_GetPrimaryLight(ent->s.index.brushmodel);
    iassert(refLight);

    fovOuter = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);

    if ( fovOuter < 0.99900001 || fovOuter >= 136.00101 )
        Scr_ParamError(0, "outer fov must be in the range of 1 to 136", SCRIPTINSTANCE_SERVER);
    //__libm_sse2_cos(v2);
    //cosHalfFovOuter = (float)(fovOuter * 0.017453292) * 0.5;
    cosHalfFovOuter = (float)cos(fovOuter * 0.017453292) * 0.5;
    if ( (float)(refLight->cosHalfFovOuter - 0.001) > cosHalfFovOuter )
        Scr_ParamError(0, "outer fov cannot be larger than the fov when the map was compiled", SCRIPTINSTANCE_SERVER);
    if ( (float)(cosHalfFovOuter - 1.0) < 0.0 )
        v8 = (float)(fovOuter * 0.017453292) * 0.5;
    else
        v8 = 1.0f;
    if ( (float)(refLight->cosHalfFovOuter - cosHalfFovOuter) < 0.0 )
        v6 = v8;
    else
        v6 = refLight->cosHalfFovOuter;


    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        fovInner = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
        if ( fovInner < -0.001 || fovInner >= (float)(fovOuter + 0.001) )
            Scr_ParamError(1u, "inner fov must be in the range of 0 to outer fov", SCRIPTINSTANCE_SERVER);
        //__libm_sse2_cos(v3);
        //cosHalfFovInner = (float)(fovInner * 0.017453292) * 0.5;
        cosHalfFovInner = (float)cos(fovInner * 0.017453292) * 0.5;
        if ( (float)(cosHalfFovInner - 1.0) < 0.0 )
            v7 = (float)(fovInner * 0.017453292) * 0.5;
        else
            v7 = 1.0f;
        if ( (float)((float)(v6 + 0.001) - cosHalfFovInner) < 0.0 )
            v5 = v7;
        else
            v5 = v6 + 0.001;
        cosHalfFovInnera = v5;
    }
    else
    {
        if ( (float)(refLight->cosHalfFovInner - (float)(v6 + 0.001)) < 0.0 )
            v4 = refLight->cosHalfFovInner;
        else
            v4 = v6 + 0.001;
        cosHalfFovInnera = v4;
    }
    if ( v6 <= 0.0 || cosHalfFovInnera <= v6 || cosHalfFovInnera > 1.0 )
    {
        v1 = va("%g, %g", v6, cosHalfFovInnera);
        if ( !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        15114,
                        0,
                        "%s\n\t%s",
                        "0.0f < cosHalfFovOuter && cosHalfFovOuter < cosHalfFovInner && cosHalfFovInner <= 1.0f",
                        v1) )
            __debugbreak();
    }
    ent->s.lerp.u.primaryLight.cosHalfFovOuter = v6;
    ent->s.lerp.u.turret.heatVal = cosHalfFovInnera;
#endif
}

void __cdecl GScr_GetLightExponent(scr_entref_t entref)
{
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GScr_SetupLightEntity(entref);
    Scr_AddInt(ent->s.lerp.u.primaryLight.colorAndExp[3], SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetLightExponent(scr_entref_t entref)
{
    int exponent; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GScr_SetupLightEntity(entref);
    exponent = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)exponent > 0x64 )
        Scr_ParamError(0, "exponent must be in the range from 0 to 100", SCRIPTINSTANCE_SERVER);
    ent->s.lerp.u.primaryLight.colorAndExp[3] = exponent;
}

void __cdecl GScr_StartRagdoll(scr_entref_t entref)
{
#ifdef KISAK_SP
    gentity_s *ent = GetEntity(entref);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
        Scr_GetInt(0, SCRIPTINSTANCE_SERVER);

    if (ent->s.eType == ET_SCRIPTMOVER)
    {
        ent->r.svFlags = static_cast<unsigned __int16>((ent->r.svFlags & ~4u) | 2u);
        ent->r.mins[0] = -15.0f;
        ent->r.mins[1] = -15.0f;
        ent->r.mins[2] = 0.0f;
        ent->r.maxs[0] = 15.0f;
        ent->r.maxs[1] = 15.0f;
        ent->r.maxs[2] = 72.0f;
        ent->r.contents |= 0x8000;
        ent->s.surfType = 7;
    }

    switch (ent->s.lerp.pos.trType)
    {
    case TR_INTERPOLATE:
        ent->s.lerp.pos.trType = TR_RAGDOLL_INTERPOLATE;
        break;
    case TR_GRAVITY:
        ent->s.lerp.pos.trType = TR_RAGDOLL_GRAVITY;
        break;
    case TR_RAGDOLL:
    case TR_RAGDOLL_GRAVITY:
    case TR_RAGDOLL_INTERPOLATE:
        break;
    default:
        ent->s.lerp.pos.trType = TR_RAGDOLL;
        BG_EvaluateTrajectory(&ent->s.lerp.pos, level.time, ent->s.lerp.pos.trBase);
        Vec3Clear(ent->s.lerp.pos.trDelta);
        break;
    }

    switch (ent->s.lerp.apos.trType)
    {
    case TR_INTERPOLATE:
        ent->s.lerp.apos.trType = TR_RAGDOLL_INTERPOLATE;
        break;
    case TR_GRAVITY:
        ent->s.lerp.apos.trType = TR_RAGDOLL_GRAVITY;
        break;
    case TR_RAGDOLL:
    case TR_RAGDOLL_GRAVITY:
    case TR_RAGDOLL_INTERPOLATE:
        break;
    default:
        ent->s.lerp.apos.trType = TR_RAGDOLL;
        BG_EvaluateTrajectory(&ent->s.lerp.apos, level.time, ent->s.lerp.apos.trBase);
        Vec3Clear(ent->s.lerp.apos.trDelta);
        break;
    }
#else
    unsigned __int8 v1; // [esp+0h] [ebp-10h]
    unsigned __int8 trType; // [esp+4h] [ebp-Ch]
    gentity_s *ent; // [esp+Ch] [ebp-4h]

    ent = GetEntity(entref);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
        Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    trType = ent->s.lerp.pos.trType;
    if ( trType == 1 )
    {
        ent->s.lerp.pos.trType = 14;
    }
    else if ( trType == 6 )
    {
        ent->s.lerp.pos.trType = 13;
    }
    else
    {
        ent->s.lerp.pos.trType = 12;
    }
    v1 = ent->s.lerp.apos.trType;
    if ( v1 == 1 )
    {
        ent->s.lerp.apos.trType = 14;
    }
    else if ( v1 == 6 )
    {
        ent->s.lerp.apos.trType = 13;
    }
    else
    {
        ent->s.lerp.apos.trType = 12;
    }
#endif
}

void __cdecl GScr_IsRagdoll(scr_entref_t entref)
{
    bool IsRagdollTrajectory; // al
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetEntity(entref);
    IsRagdollTrajectory = Com_IsRagdollTrajectory(&ent->s.lerp.pos);
    Scr_AddInt(IsRagdollTrajectory, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_RagdollLaunch(scr_entref_t entref)
{
    unsigned __int16 floatValue; // ax
    gentity_s *tempent; // [esp+0h] [ebp-14h]
    float force[3]; // [esp+4h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+10h] [ebp-4h]

    ent = GetEntity(entref);
    if ( Com_IsRagdollTrajectory(&ent->s.lerp.pos) )
    {
        Scr_GetVector(0, force, SCRIPTINSTANCE_SERVER);
        tempent = G_TempEntity(force, EV_PHYS_LAUNCH);
        tempent->s.otherEntityNum = ent->s.number;
        if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 1 )
        {
            tempent->s.eventParm = 0;
        }
        else
        {
            floatValue = (unsigned __int16)Scr_GetConstLowercaseString(1u, SCRIPTINSTANCE_SERVER);
            tempent->s.eventParm = G_GetHitLocationIndexFromString(floatValue);
        }
    }
}

void __cdecl GScr_VehicleLaunch(scr_entref_t entref)
{
    float *currentOrigin; // [esp+8h] [ebp-1Ch]
    gentity_s *tempent; // [esp+10h] [ebp-14h]
    float force[3]; // [esp+14h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+20h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->scr_vehicle && ent->scr_vehicle->nitrousVehicle )
    {
        Scr_GetVector(0, force, SCRIPTINSTANCE_SERVER);
        tempent = G_TempEntity(force, EV_PHYS_LAUNCH);
        tempent->s.otherEntityNum = ent->s.number;
        tempent->s.eventParm = 0;
        if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 2 )
            tempent->s.eventParm = (unsigned __int16)Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
        if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 1 )
        {
            tempent->s.lerp.pos.trDelta[0] = 0.0f;
            tempent->s.lerp.pos.trDelta[1] = 0.0f;
            tempent->s.lerp.pos.trDelta[2] = 0.0f;
        }
        else
        {
            Scr_GetVector(1u, tempent->s.lerp.pos.trDelta, SCRIPTINSTANCE_SERVER);
            if ( !tempent->s.eventParm )
            {
                currentOrigin = ent->r.currentOrigin;
                tempent->s.lerp.pos.trDelta[0] = tempent->s.lerp.pos.trDelta[0] - ent->r.currentOrigin[0];
                tempent->s.lerp.pos.trDelta[1] = tempent->s.lerp.pos.trDelta[1] - currentOrigin[1];
                tempent->s.lerp.pos.trDelta[2] = tempent->s.lerp.pos.trDelta[2] - currentOrigin[2];
            }
        }
    }
}

void __cdecl GScr_GiveAchievement(scr_entref_t entref)
{
    const char *v1; // eax
    char *achievement; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_ParamError(0, "giveachievement [name]", SCRIPTINSTANCE_SERVER);
    achievement = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( !*achievement )
        Scr_Error("giveachievement: achievement must be named", 0);
    ent = GetEntity(entref);
    if ( !ent || !ent->client )
        Scr_Error("giveachievement: entity must be a player entity", 0);
    v1 = va("%c %s", 56, achievement);
    SV_GameSendServerCommand(ent->s.number, SV_CMD_RELIABLE, v1);
}

void __cdecl GScr_SetOwner(scr_entref_t entref)
{
    gentity_s *owner; // [esp+4h] [ebp-Ch]
    gentity_s *ent; // [esp+8h] [ebp-8h]
    unsigned int ownerIndex; // [esp+Ch] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->s.eType == 1 || ent->s.eType == 17 )
        Scr_Error("SetOwner can not be used for players or actors", 0);
    owner = Scr_GetEntity(0);
    if ( !owner->client
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    15353,
                    0,
                    "%s",
                    "owner->client") )
    {
        __debugbreak();
    }
    ownerIndex = owner->client - level.clients;
    if ( ownerIndex >= level.maxclients
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    15355,
                    0,
                    "ownerIndex doesn't index level.maxclients\n\t%i not in [0, %i)",
                    ownerIndex,
                    level.maxclients) )
    {
        __debugbreak();
    }
    ent->s.faction.iHeadIconTeam = ent->s.faction.iHeadIconTeam & 3 | (4 * ownerIndex);
}

void __cdecl GScr_SetTurretOwner(scr_entref_t entref)
{
    gentity_s *owner; // [esp+4h] [ebp-Ch]
    gentity_s *ent; // [esp+8h] [ebp-8h]
    unsigned int ownerIndex; // [esp+Ch] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->s.eType == 1 || ent->s.eType == 17 )
        Scr_Error("SetOwner can not be used for players or actors", 0);
    owner = Scr_GetEntity(0);
    if ( !owner->client
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    15383,
                    0,
                    "%s",
                    "owner->client") )
    {
        __debugbreak();
    }
    ownerIndex = owner->client - level.clients;
    if ( ownerIndex >= level.maxclients
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    15385,
                    0,
                    "ownerIndex doesn't index level.maxclients\n\t%i not in [0, %i)",
                    ownerIndex,
                    level.maxclients) )
    {
        __debugbreak();
    }
    ent->s.faction.iHeadIconTeam = ent->s.faction.iHeadIconTeam & 3 | (4 * ownerIndex);
    ent->s.lerp.u.turret.ownerNum = ownerIndex;
    Turret_SetTurretOwner(ent, owner);
}

void __cdecl GScr_SetTurretType(scr_entref_t entref)
{
    const char *v1; // eax
    char *pszType; // [esp+0h] [ebp-8h]
    gentity_s *ent; // [esp+4h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->pTurretInfo
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    15412,
                    0,
                    "%s",
                    "ent->pTurretInfo") )
    {
        __debugbreak();
    }
    pszType = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    ent->pTurretInfo->flags &= ~0x10000u;
    ent->pTurretInfo->flags &= ~0x20000u;
    ent->s.lerp.u.turret.flags &= ~1u;
    ent->s.lerp.u.turret.flags &= ~2u;
    if ( I_stricmp(pszType, "sentry") )
    {
        if ( I_stricmp(pszType, "tow") )
        {
            v1 = va("%s, Unknown turret type.", pszType);
            Scr_Error(SCRIPTINSTANCE_SERVER, v1, 0);
        }
        else
        {
            ent->pTurretInfo->flags |= 0x20000u;
            ent->s.lerp.u.turret.flags |= 2u;
        }
    }
    else
    {
        ent->pTurretInfo->flags |= 0x10000u;
        ent->s.lerp.u.turret.flags |= 1u;
    }
}

void __cdecl GScr_SetTeam(scr_entref_t entref)
{
    const char *v1; // eax
    char team; // [esp+0h] [ebp-10h]
    gentity_s *ent; // [esp+4h] [ebp-Ch]
    char *pszTeam; // [esp+Ch] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->s.eType == 1 || ent->s.eType == 17 )
        Scr_Error("SetTeam can not be used for players or actors", 0);
    pszTeam = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    team = 0;
    if ( I_stricmp(pszTeam, "axis") )
    {
        if ( I_stricmp(pszTeam, "allies") )
        {
            if ( I_stricmp(pszTeam, "spectator") )
            {
                if ( I_stricmp(pszTeam, "free") )
                {
                    v1 = va("unknown team '%s', should be axis, allies, or neutral\n", pszTeam);
                    Scr_Error(v1, 0);
                }
                else
                {
                    team = 0;
                }
            }
            else
            {
                team = 3;
            }
        }
        else
        {
            team = 2;
        }
    }
    else
    {
        team = 1;
    }
    ent->s.faction.iHeadIconTeam = team | (4 * ((int)ent->s.faction.iHeadIconTeam >> 2));
}

void __cdecl GScr_GetTeam(scr_entref_t entref)
{
    team_t team; // [esp+8h] [ebp-8h]
    gentity_s *ent; // [esp+Ch] [ebp-4h]

    team = TEAM_FREE;
    ent = GetEntity(entref);
    switch ( ent->s.eType )
    {
        case 1:
            team = ent->client->sess.cs.team;
            break;
        case 4:
            team = ent->missile.team;
            break;
        case 0xE:
            team = G_GetVehicleOccupantsTeam(ent);
            break;
        case 0x11:
            team = ent->sentient->eTeam;
            break;
        default:
            break;
    }
    switch ( team )
    {
        case TEAM_AXIS:
            Scr_AddString("axis", SCRIPTINSTANCE_SERVER);
            break;
        case TEAM_ALLIES:
            Scr_AddString("allies", SCRIPTINSTANCE_SERVER);
            break;
        case TEAM_SPECTATOR:
            Scr_AddString("spectator", SCRIPTINSTANCE_SERVER);
            break;
    }
}

void __cdecl GScr_GetCorpseAnim(scr_entref_t entref)
{
    const char *v1; // eax
    XAnim_s *treeAnims; // [esp+0h] [ebp-14h]
    gentity_s *ent; // [esp+8h] [ebp-Ch]
    scr_anim_s anim; // [esp+Ch] [ebp-8h]
    corpseInfo_t *corpseInfo; // [esp+10h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->s.eType == 2 )
    {
        corpseInfo = &g_scr_data.playerCorpseInfo[G_GetPlayerCorpseIndex(ent, "GScr_GetCorpseAnim")];
        anim.index = ent->s.animState.state & 0xFBFF;
        treeAnims = XAnimGetAnims(corpseInfo->tree);
        anim.tree = Scr_GetAnimsIndex(treeAnims, SCRIPTINSTANCE_SERVER);
        Scr_AddAnim(anim, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        v1 = va("Only valid on player corpses");
        Scr_Error(v1, 0);
    }
}

void __cdecl ScrCmd_ItemWeaponSetAmmo(scr_entref_t entref)
{
    const char *v1; // eax
    VariableUnion v2; // [esp+0h] [ebp-50h]
    int ClipSize; // [esp+4h] [ebp-4Ch]
    int reserveAmmo; // [esp+3Ch] [ebp-14h]
    int clipAmmo; // [esp+40h] [ebp-10h]
    unsigned int altIndex; // [esp+44h] [ebp-Ch]
    signed int weaponIndex; // [esp+48h] [ebp-8h]
    gentity_s *itemEnt; // [esp+4Ch] [ebp-4h]

    itemEnt = GetEntity(entref);
    if ( itemEnt->s.eType != 3 )
        Scr_Error("Entity is not an item.", 0);
    if ( bg_itemlist[itemEnt->s.un3.item].giType != IT_WEAPON )
        Scr_Error("Item entity is not a weapon.", 0);
    clipAmmo = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( clipAmmo < 0 )
        Scr_ParamError(0, "Ammo count must not be negative", SCRIPTINSTANCE_SERVER);
    reserveAmmo = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    if ( reserveAmmo < 0 )
        Scr_ParamError(1u, "Ammo count must not be negative", SCRIPTINSTANCE_SERVER);
    altIndex = 0;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 2 )
    {
        altIndex = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
        if ( altIndex >= 2 )
        {
            v1 = va("Value out of range.    Allowed values: 0 to %i", 2);
            Scr_ParamError(2u, v1, SCRIPTINSTANCE_SERVER);
        }
    }
    weaponIndex = itemEnt->item[altIndex].index % BG_ITEM_WEAPON_STRIDE;
    if ( weaponIndex > 0 )
    {
        if ( BG_GetClipSize(weaponIndex) < 0
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        15598,
                        0,
                        "%s",
                        "BG_GetClipSize( weaponIndex ) >= 0") )
        {
            __debugbreak();
        }
        ClipSize = BG_GetClipSize(weaponIndex);
        if ( ClipSize < clipAmmo )
            v2.intValue = ClipSize;
        else
            v2.intValue = clipAmmo;
        itemEnt->item[altIndex].ammoCount = reserveAmmo;
        itemEnt->item[altIndex].clipAmmoCount = v2.intValue;
    }
}

void __cdecl Scr_AddStruct()
{
    Scr_AddStruct(SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_ResetTimeout()
{
    Scr_ResetTimeout(SCRIPTINSTANCE_SERVER);
}

void GScr_ClientSysRegister()
{
    char *pRegSysName; // [esp+0h] [ebp-414h]
    char szConfigString[1024]; // [esp+4h] [ebp-410h] BYREF
    int i; // [esp+408h] [ebp-Ch]
    bool bSetString; // [esp+40Fh] [ebp-5h]
    const char *pSysName; // [esp+410h] [ebp-4h]

    pSysName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    bSetString = 0;
    for ( i = 0; i < 8; ++i )
    {
        SV_GetConfigstring(i + 1538, szConfigString, 1024);
        if ( szConfigString[0] )
        {
            pRegSysName = Info_ValueForKey(szConfigString, "n");
            if ( pRegSysName )
            {
                if ( !I_stricmp(pRegSysName, pSysName) )
                    bSetString = 1;
            }
        }
        else
        {
            bSetString = 1;
        }
        if ( bSetString )
        {
            Info_SetValueForKey(szConfigString, "n", pSysName);
            SV_SetConfigstring(i + 1538, szConfigString);
            Scr_AddInt(i, SCRIPTINSTANCE_SERVER);
            return;
        }
    }
    Scr_AddInt(-1, SCRIPTINSTANCE_SERVER);
}

void GScr_ClientSysSetState()
{
    const char *v0; // eax
    const char *v1; // eax
    const char *v2; // eax
    unsigned int j; // [esp+10h] [ebp-814h]
    char str[1028]; // [esp+14h] [ebp-810h] BYREF
    const char *pNewState; // [esp+418h] [ebp-40Ch]
    char szConfigString[1024]; // [esp+41Ch] [ebp-408h] BYREF
    int i; // [esp+820h] [ebp-4h]

    // Base 1538 here (and in the paired registration/PlayerCmd_ClientSysSetState
    // functions) vs retail SP's raw 0x5e1=1505 (Ghidra-verified 2026-08-22, same
    // pass that fixed the Info_SetValueForKey bug below) is the same class of
    // divergence as openmainmenu's 2548-vs-2503 base: this tree's own
    // "reconstruction" configstring layout, used self-consistently by every
    // ClientSys read/write site in this tree, not a retail address. Flagged
    // explicitly (unlike openmainmenu's case, this one previously had no
    // comment at all) so it isn't mistaken for an unnoticed bug.
    i = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)i <= 8 )
    {
        SV_GetConfigstring(i + 1538, szConfigString, 1024);
        if ( szConfigString[0] )
        {
            szConfigString[0] = 0;
            pNewState = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
            str[0] = 0;
            if ( pNewState )
            {
                I_strncpyz(str, pNewState, 1024);
                for ( j = 0; j < &str[strlen(str) + 1] - &str[1]; ++j )
                {
                    if ( str[j] == 32 )
                        str[j] = 33;
                }
            }
            // Retail SP 0x008048f0 sends the sanitized value as a bare token --
            // no Info_SetValueForKey wrapping (verified 2026-08-22: retail's
            // decompile has no such call). The client parser
            // (CG_ParseClientSystemStateChange, cg_servercmds_mp.cpp) only
            // desanitizes the raw string; it never unwraps an "s" info key, so
            // wrapping here sent every state change (including
            // maps/_music.gsc's setMusicState) as literal "\s\<value>" instead
            // of "<value>", which is why client-side exact-match dispatch on
            // the state name was failing.
            v2 = va("%c %i %s", 57, i, str);
            SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, v2);
        }
        else
        {
            v1 = va("ClientSysSetState - state index (%i) unregistered.    Use ClientSysRegister first.", i);
            Scr_Error(v1, 1);
        }
    }
    else
    {
        v0 = va("ClientSysSetState - state index (%i) out of bounds (0 - %i)", i, 8);
        Scr_Error(v0, 1);
    }
}

void GScr_IsAI()
{
    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == 1
        && Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == 19
        && Scr_GetEntity(0)->actor )
    {
        Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_GetAITriggerFlags()
{
    Scr_AddInt(7, SCRIPTINSTANCE_SERVER);
}

void GScr_IsVehicle()
{
    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == 1
        && Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == 19
        && Scr_GetEntity(0)->scr_vehicle )
    {
        Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl ScrCmd_GetShootAtPosition(scr_entref_t entref)
{
    float shootAtPos[3]; // [esp+0h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+Ch] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->sentient )
        Sentient_GetEyePosition(ent->sentient, shootAtPos);
    else
        G_EntityCentroid(ent, shootAtPos);
    Scr_AddVector(shootAtPos, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_GetMaxVehicles()
{
    Scr_AddInt(16, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_DisableDestructiblePieces()
{
    VariableUnion v0; // eax
    float zero_vec[3]; // [esp+0h] [ebp-10h] BYREF
    gentity_s *tempent; // [esp+Ch] [ebp-4h]

    v0.intValue = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    DisableDestructiblePiece(v0.intValue);
    memset(zero_vec, 0, sizeof(zero_vec));
    tempent = G_TempEntity(zero_vec, EV_DESTRUCTIBLE_DISABLE_PIECES);
    tempent->s.eventParm = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_EnableAllDestructiblePieces()
{
    float zero_vec[3]; // [esp+0h] [ebp-10h] BYREF
    gentity_s *tempent; // [esp+Ch] [ebp-4h]

    EnableAllDestructiblePieces();
    memset(zero_vec, 0, sizeof(zero_vec));
    tempent = G_TempEntity(zero_vec, EV_DESTRUCTIBLE_DISABLE_PIECES);
    tempent->s.eventParm = 0;
}

void __cdecl GScr_ClearSpawnPoints()
{
    SpawnSystem_ClearPoints();
}

void __cdecl GScr_SetSpawnPointRandomVariation()
{
    float variation; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("USAGE: setspawnpointrandomvariation( <variation> )\n", 0);
    variation = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    SpawnSystem_SetRandomVariation(variation);
}

void __cdecl GScr_ClearSpawnPointsBaseWeight()
{
    VariableUnion team_mask; // [esp+0h] [ebp-4h]

    team_mask.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    SpawnSystem_ClearPointsBaseWeight(team_mask.intValue);
}

void __cdecl GScr_SetSpawnPointsBaseWeight()
{
    float angle; // [esp+8h] [ebp-18h]
    VariableUnion team_mask; // [esp+Ch] [ebp-14h]
    float position[3]; // [esp+10h] [ebp-10h] BYREF
    float score; // [esp+1Ch] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 4 )
        Scr_Error("USAGE: setspawnpointsbaseweight( <team mask>, <objective position>, <angle>, <score> )\n", 0);
    team_mask.intValue = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, position, SCRIPTINSTANCE_SERVER);
    angle = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    score = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    SpawnSystem_SetPointsBaseWeight(team_mask.intValue, position, angle, score);
}

void __cdecl GScr_AddSpawnPoints()
{
    const char *v0; // eax
    const char *v1; // eax
    scr_entref_t v2; // [esp+0h] [ebp-2Ch] BYREF
    scr_entref_t v3; // [esp+Ah] [ebp-22h]
    VariableValueInternal *parentValue; // [esp+10h] [ebp-1Ch]
    int team; // [esp+14h] [ebp-18h]
    int script_array_size; // [esp+18h] [ebp-14h]
    int parent_id; // [esp+1Ch] [ebp-10h]
    int i; // [esp+20h] [ebp-Ch]
    VariableValueInternal *entry_value; // [esp+24h] [ebp-8h]
    int id; // [esp+28h] [ebp-4h]

    team = GScr_ReadTeamForSpawnPoints(0);
    parent_id = Scr_GetObject(1u, SCRIPTINSTANCE_SERVER);
    if ( !parent_id
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 16044, 0, "%s", "parent_id") )
    {
        __debugbreak();
    }
    if ( GetObjectType(SCRIPTINSTANCE_SERVER, parent_id) != 20
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    16045,
                    0,
                    "%s",
                    "GetObjectType(SCRIPTINSTANCE_SERVER, parent_id)==VAR_ARRAY") )
    {
        __debugbreak();
    }
    script_array_size = GetArraySize(SCRIPTINSTANCE_SERVER, parent_id);
    for ( i = 0; i < script_array_size; ++i )
    {
        id = GetArrayVariable(SCRIPTINSTANCE_SERVER, parent_id, i);
        if ( !id && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 16052, 0, "%s", "id") )
            __debugbreak();
        entry_value = &gScrVarGlob[0].variableList[id + 0x8000];
        parentValue = &gScrVarGlob[0].variableList[id + 1];
        if ( (entry_value->w.status & 0x60) == 0
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                        16055,
                        0,
                        "%s",
                        "(entry_value->w.status & VAR_STAT_MASK) != VAR_STAT_FREE") )
        {
            __debugbreak();
        }
        if ( (entry_value->w.status & 0x1F) != 1 || GetObjectType(SCRIPTINSTANCE_SERVER, entry_value->u.next) != 19 )
        {
            v1 = va("contents of spawnpoints array must be entities");
            Scr_ParamError(1u, v1, SCRIPTINSTANCE_SERVER);
            break;
        }
        v3 = Scr_GetEntityIdRef(SCRIPTINSTANCE_SERVER, entry_value->u.next);
        if ( !SpawnSystem_AddPoint(team, &g_entities[v3.entnum]) )
        {
            v0 = va("Adding to many spawn points to spawnpoint system.    Max: %i\n", 200);
            Scr_Error(v0, 0);
        }
    }
    SpawnSystem_SortPoints();
}

void __cdecl GScr_GetSortedSpawnPoints()
{
    const char *v0; // eax
    int SortedPointEntNum; // eax
    signed int i; // [esp+0h] [ebp-14h]
    unsigned int influencer_team; // [esp+4h] [ebp-10h]
    unsigned int point_team; // [esp+8h] [ebp-Ch]
    gentity_s *ent; // [esp+Ch] [ebp-8h]
    int count; // [esp+10h] [ebp-4h]

    point_team = GScr_ReadTeamForSpawnPoints(0);
    influencer_team = GScr_ReadTeamForSpawnPoints(1u);
    ent = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 3 )
    {
        ent = Scr_GetEntity(2u);
        if ( !ent->client )
        {
            v0 = va("getsortedspawnpoints: Entity must be a player");
            Scr_ParamError(2u, v0, SCRIPTINSTANCE_SERVER);
        }
    }
    if ( ent )
        count = SpawnSystem_UpdateSpawnPointsForPlayer(ent, point_team, influencer_team);
    else
        count = SpawnSystem_UpdateSpawnPointsForTeam(point_team, influencer_team);
    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for ( i = 0; i < count; ++i )
    {
        SortedPointEntNum = SpawnSystem_GetSortedPointEntNum(point_team, i);
        Scr_AddEntity(&level.gentities[SortedPointEntNum], SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_IsSpawnPointVisible()
{
    unsigned __int8 IsSpawnPointVisible; // al
    int team; // [esp+0h] [ebp-20h]
    float angles[3]; // [esp+4h] [ebp-1Ch] BYREF
    gentity_s *ent; // [esp+10h] [ebp-10h]
    float point[3]; // [esp+14h] [ebp-Ch] BYREF

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 4 )
        Scr_Error("USAGE: isspawnpointvisible( <point>, <angle>, <team>, <ignore player> )\n", 0);
    Scr_GetVector(0, point, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, angles, SCRIPTINSTANCE_SERVER);
    team = GScr_ReadTeamForSpawnPoints(2u);
    ent = Scr_GetEntity(3u);
    if ( !ent )
        Scr_Error("USAGE: isspawnpointvisible() ignore player is not a valid entity\n", 0);
    IsSpawnPointVisible = SpawnSystem_IsSpawnPointVisible(point, angles, team, ent);
    Scr_AddBool(IsSpawnPointVisible, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_PredictGrenade(scr_entref_t entref)
{
    int timeAtRest; // [esp+0h] [ebp-14h] BYREF
    float vLandPos[3]; // [esp+4h] [ebp-10h] BYREF
    gentity_s *ent; // [esp+10h] [ebp-4h]

    ent = GetEntity(entref);
    if ( !ent->r.inuse
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    16241,
                    0,
                    "%s",
                    "ent->r.inuse") )
    {
        __debugbreak();
    }
    if ( ent->s.eType != 4
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    16242,
                    0,
                    "%s",
                    "ent->s.eType == ET_MISSILE") )
    {
        __debugbreak();
    }
    if ( G_PredictMissile(ent, 3000, vLandPos, 1, &timeAtRest) )
        Scr_AddVector(vLandPos, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddVector((float *)vec3_origin, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_AddSphereInfluencer()
{
    const char *v0; // eax
    const char *v1; // eax
    unsigned int entNum; // [esp+1Ch] [ebp-54h]
    float time; // [esp+38h] [ebp-38h]
    int curve; // [esp+3Ch] [ebp-34h]
    int timeout; // [esp+40h] [ebp-30h]
    int influencer_index; // [esp+44h] [ebp-2Ch]
    float origin[3]; // [esp+48h] [ebp-28h] BYREF
    float radius; // [esp+54h] [ebp-1Ch]
    const char *description; // [esp+58h] [ebp-18h]
    int team_mask; // [esp+5Ch] [ebp-14h]
    int type; // [esp+64h] [ebp-Ch]
    gentity_s *ent; // [esp+68h] [ebp-8h]
    float score; // [esp+6Ch] [ebp-4h]

    PROF_SCOPED("GScr_AddSphereInfluencer");

    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 6
        || (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 9 )
    {
        Scr_Error(
            "USAGE: addsphereinfluencer( <type>, <origin>, <radius>, <score>, <team mask>, <description>, <curve>, <timeout>, <entity> )\n",
            0);
    }
    type = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)type > 6 )
    {
        v0 = va("USAGE: addsphereinfluencer type must be a value from %i to %i\n", 0, 6);
        Scr_Error(v0, 0);
    }
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    radius = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
    score = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
    team_mask = Scr_GetInt(4u, SCRIPTINSTANCE_SERVER);
    description = Scr_GetString(5u, SCRIPTINSTANCE_SERVER);
    curve = 0;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 7 )
    {
        curve = Scr_GetInt(6u, SCRIPTINSTANCE_SERVER);
        if ( (unsigned int)curve > 4 )
        {
            v1 = va("USAGE: addsphereinfluencer curve must be a value from %i to %i\n", 0, 4);
            Scr_Error(v1, 0);
        }
    }
    timeout = 0;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 8 )
    {
        time = Scr_GetFloat(7u, SCRIPTINSTANCE_SERVER);
        if ( time > 0.001 )
            timeout = (int)(float)(time * 1000.0);
    }
    ent = 0;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 9
        || (ent = Scr_GetEntity(8u)) != 0
        && (ent->s.eType == 1
         || ent->s.eType == 17
         || ent->s.eType == 14
         || ent->s.eType == 6
         || ent->s.eType == 4
         || ent->s.eType == 12) )
    {
        InfluencerTypeValidation(type, ent, "addsphereinfluencer");
        if ( ent )
            entNum = ent->s.number;
        else
            entNum = 1023;
        influencer_index = SpawnSystem_AddSphereInfluencer(
                                                 (eInfluencerType)type,
                                                 origin,
                                                 radius,
                                                 score,
                                                 (eInfluencerScoreCurve)curve,
                                                 team_mask,
                                                 entNum,
                                                 timeout);
        if ( influencer_index == -1 )
        {
            Scr_Error("USAGE: addsphereinfluencer could not create influencer \n", 0);
        }
        else
        {
            Scr_AddInt(influencer_index, SCRIPTINSTANCE_SERVER);
        }
    }
}

void __cdecl InfluencerTypeValidation(int type, gentity_s *ent, const char *function_name)
{
    const char *v3; // eax
    const char *v4; // eax
    const char *v5; // eax
    const char *v6; // eax
    const char *v7; // eax

    if ( type == 1 && (!ent || !ent->client) )
    {
        v3 = va("USAGE: %s type PLAYER can only be called on players\n", function_name);
        Scr_Error(v3, 0);
    }
    if ( type == 2 && (!ent || !ent->client) )
    {
        v4 = va("USAGE: %s type WEAPON can only be called on players\n", function_name);
        Scr_Error(v4, 0);
    }
    if ( type == 5 && (!ent || !ent->client) )
    {
        v5 = va("USAGE: %s type SQUAD can only be called on players\n", function_name);
        Scr_Error(v5, 0);
    }
    if ( type == 3 && (!ent || !ent->actor) )
    {
        v6 = va("USAGE: %s type DOG can only be called on dogs\n", function_name);
        Scr_Error(v6, 0);
    }
    if ( type == 4 && (!ent || !ent->scr_vehicle) )
    {
        v7 = va("USAGE: %s type VEHICLE can only be called on vehicles\n", function_name);
        Scr_Error(v7, 0);
    }
}

void __cdecl GScr_AddCylinderInfluencer()
{
    const char *v0; // eax
    const char *v1; // eax
    unsigned int entNum; // [esp+20h] [ebp-54h]
    float time; // [esp+24h] [ebp-50h]
    int curve; // [esp+28h] [ebp-4Ch]
    int timeout; // [esp+2Ch] [ebp-48h]
    int influencer_index; // [esp+30h] [ebp-44h]
    float origin[3]; // [esp+34h] [ebp-40h] BYREF
    float radius; // [esp+40h] [ebp-34h]
    const char *description; // [esp+44h] [ebp-30h]
    float forward[3]; // [esp+48h] [ebp-2Ch] BYREF
    int team_mask; // [esp+54h] [ebp-20h]
    int type; // [esp+58h] [ebp-1Ch]
    float up[3]; // [esp+5Ch] [ebp-18h] BYREF
    gentity_s *ent; // [esp+68h] [ebp-Ch]
    float length; // [esp+6Ch] [ebp-8h]
    float score; // [esp+70h] [ebp-4h]

    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 9
        || (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 0xC )
    {
        Scr_Error(
            "USAGE: addcylinderinfluencer( <type>, <origin>, <forward>, <up>, <radius>, <axis length>, <score>, <team mask>, <d"
            "escription>, <curve>, <timeout>, <entity> )\n",
            0);
    }
    type = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)type > 6 )
    {
        v0 = va("USAGE: addcylinderinfluencer type must be a value from %i to %i\n", 0, 6);
        Scr_Error(v0, 0);
    }
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2u, forward, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(3u, up, SCRIPTINSTANCE_SERVER);
    radius = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
    length = Scr_GetFloat(5u, SCRIPTINSTANCE_SERVER);
    score = Scr_GetFloat(6u, SCRIPTINSTANCE_SERVER);
    team_mask = Scr_GetInt(7u, SCRIPTINSTANCE_SERVER);
    description = Scr_GetString(8u, SCRIPTINSTANCE_SERVER);
    curve = 0;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 0xA )
    {
        curve = Scr_GetInt(9u, SCRIPTINSTANCE_SERVER);
        if ( (unsigned int)curve > 4 )
        {
            v1 = va("USAGE: addcylinderinfluencer curve must be a value from %i to %i\n", 0, 4);
            Scr_Error(v1, 0);
        }
    }
    timeout = 0;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 0xB )
    {
        time = Scr_GetFloat(0xAu, SCRIPTINSTANCE_SERVER);
        if ( time > 0.001 )
            timeout = (int)(float)(time * 1000.0);
    }
    ent = 0;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 0xC
        || (ent = Scr_GetEntity(0xBu)) != 0
        && (ent->s.eType == 1
         || ent->s.eType == 17
         || ent->s.eType == 14
         || ent->s.eType == 6
         || ent->s.eType == 4
         || ent->s.eType == 12) )
    {
        InfluencerTypeValidation(type, ent, "addcylinderinfluencer");
        if ( ent )
            entNum = ent->s.number;
        else
            entNum = 1023;
        influencer_index = SpawnSystem_AddCylinderInfluencer(
                                                 (eInfluencerType)type,
                                                 origin,
                                                 forward,
                                                 up,
                                                 radius,
                                                 length,
                                                 score,
                                                 (eInfluencerScoreCurve)curve,
                                                 team_mask,
                                                 entNum,
                                                 timeout);
        if ( influencer_index == -1 )
            Scr_Error("USAGE: addcylinderinfluencer could not create influencer \n", 0);
        else
            Scr_AddInt(influencer_index, SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_RemoveInfluencer()
{
    unsigned int influcencer_id; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("USAGE: removeinfluencer(<influencer id> )\n", 0);
    influcencer_id = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( !SpawnSystem_RemoveInfluencer(influcencer_id) )
        Scr_Error("Trying to remove influencer that does not exist\n", 0);
}

void __cdecl GScr_EnableInfluencer()
{
    unsigned int influcencer_id; // [esp+0h] [ebp-8h]
    int enable; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("USAGE: enableinfluencer(<influencer id>, <enable> )\n", 0);
    influcencer_id = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    enable = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    if ( !SpawnSystem_EnableInfluencer(influcencer_id, enable != 0) )
        Scr_Error("Trying to enable influencer that does not exist\n", 0);
}

void __cdecl GScr_SetInfluencerTeamMask()
{
    unsigned int influcencer_id; // [esp+0h] [ebp-8h]
    VariableUnion team_mask; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_Error("USAGE: setinfluencerteammask( <influencer id>, <team mask> )\n", 0);
    influcencer_id = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    team_mask.intValue = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
    if ( !SpawnSystem_SetInfluencerTeamMask(influcencer_id, team_mask.intValue) )
        Scr_Error("Trying to set influencer team mask on an influencer that does not exist\n", 0);
}

void __cdecl GScr_SetDebugSideSwitch()
{
    int enabled; // [esp+0h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("USAGE: setdebugsideswitch( <enabled> )\n", 0);
    enabled = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    SpawnSystem_DebugSideSwitch(enabled != 0);
}

void GScr_CollisionTestPointsInSphere()
{
    const char *v0; // eax
    const char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    const char *TypeName; // eax
    const char *v5; // eax
    VariableUnion v6; // eax
    float v7; // [esp+0h] [ebp-C38h]
    float collision_results[3]; // [esp+10h] [ebp-C28h] BYREF
    int point_index; // [esp+1Ch] [ebp-C1Ch]
    float points[256][3]; // [esp+20h] [ebp-C18h] BYREF
    float sphere_center[3]; // [esp+C20h] [ebp-18h] BYREF
    float sphere_radius; // [esp+C2Ch] [ebp-Ch]
    float rsquared; // [esp+C30h] [ebp-8h]
    int element_count; // [esp+C34h] [ebp-4h]

    if ( Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == 20 )
    {
        if ( Scr_GetType(1u, SCRIPTINSTANCE_SERVER) == 4 )
        {
            if ( Scr_GetType(2u, SCRIPTINSTANCE_SERVER) == 5 )
            {
                v6.intValue = Scr_GetObject(0, SCRIPTINSTANCE_SERVER);
                element_count = Scr_GetArrayValues_Vector(0, v6.stringValue, points, 256, "spawn points");
                Scr_GetVector(1u, sphere_center, SCRIPTINSTANCE_SERVER);
                sphere_radius = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
                rsquared = sphere_radius * sphere_radius;
                Scr_MakeArray(SCRIPTINSTANCE_SERVER);
                for ( point_index = 0; point_index < element_count; ++point_index )
                {
                    collision_results[1] = Vec3DistanceSq(points[point_index], sphere_center);
                    if ( rsquared <= collision_results[1] )
                        v7 = 0.0f;
                    else
                        v7 = 1.0f;
                    collision_results[0] = v7;
                    collision_results[2] = 0.0f;
                    Scr_AddVector(collision_results, SCRIPTINSTANCE_SERVER);
                    Scr_AddArray(SCRIPTINSTANCE_SERVER);
                }
            }
            else
            {
                TypeName = Scr_GetTypeName(2u, SCRIPTINSTANCE_SERVER);
                v5 = va("Parameter '%s' must be a floating point value (sphere radius)", TypeName);
                Scr_ParamError(2u, v5, SCRIPTINSTANCE_SERVER);
            }
        }
        else
        {
            v2 = Scr_GetTypeName(1u, SCRIPTINSTANCE_SERVER);
            v3 = va("Parameter '%s' must be a vector (3D point origin)", v2);
            Scr_ParamError(1u, v3, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        v0 = Scr_GetTypeName(0, SCRIPTINSTANCE_SERVER);
        v1 = va("Parameter '%s' must be an array", v0);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_CollisionTestPointsInCylinder()
{
    const char *v0; // eax
    const char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    const char *v4; // eax
    const char *v5; // eax
    const char *v6; // eax
    const char *v7; // eax
    const char *TypeName; // eax
    const char *v9; // eax
    VariableUnion v10; // eax
    double v11; // st7
    float dsquared; // [esp+34h] [ebp-C68h]
    float midpoint_axial_distance; // [esp+48h] [ebp-C54h]
    float collision_results[3]; // [esp+4Ch] [ebp-C50h] BYREF
    int point_index; // [esp+58h] [ebp-C44h]
    float half_cylinder_height; // [esp+5Ch] [ebp-C40h]
    float bounding_sphere_radius_squared; // [esp+60h] [ebp-C3Ch]
    float cylinder_base[3]; // [esp+64h] [ebp-C38h] BYREF
    float axis_midpoint[3]; // [esp+70h] [ebp-C2Ch] BYREF
    float points[256][3]; // [esp+7Ch] [ebp-C20h] BYREF
    float cylinder_height; // [esp+C7Ch] [ebp-20h]
    float cylinder_radius; // [esp+C80h] [ebp-1Ch]
    float cylinder_radius_squared; // [esp+C84h] [ebp-18h]
    float cylinder_height_unit_vector[3]; // [esp+C88h] [ebp-14h] BYREF
    float bounding_sphere_radius; // [esp+C94h] [ebp-8h]
    int element_count; // [esp+C98h] [ebp-4h]

    if ( Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == 20 )
    {
        if ( Scr_GetType(1u, SCRIPTINSTANCE_SERVER) == 4 )
        {
            if ( Scr_GetType(2u, SCRIPTINSTANCE_SERVER) == 5 )
            {
                if ( Scr_GetType(3u, SCRIPTINSTANCE_SERVER) == 5 )
                {
                    if ( Scr_GetType(4u, SCRIPTINSTANCE_SERVER) == 4 )
                    {
                        v10.intValue = Scr_GetObject(0, SCRIPTINSTANCE_SERVER);
                        element_count = Scr_GetArrayValues_Vector(0, v10.stringValue, points, 256, "spawn points");
                        Scr_GetVector(1u, cylinder_base, SCRIPTINSTANCE_SERVER);
                        cylinder_radius = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
                        cylinder_height = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
                        half_cylinder_height = 0.5 * cylinder_height;
                        cylinder_radius_squared = cylinder_radius * cylinder_radius;
                        bounding_sphere_radius = cylinder_radius + (float)(0.5 * cylinder_height);
                        bounding_sphere_radius_squared = bounding_sphere_radius * bounding_sphere_radius;
                        Scr_GetVector(4u, cylinder_height_unit_vector, SCRIPTINSTANCE_SERVER);
                        axis_midpoint[0] = (float)(cylinder_height_unit_vector[0] * half_cylinder_height) + cylinder_base[0];
                        axis_midpoint[1] = (float)(cylinder_height_unit_vector[1] * half_cylinder_height) + cylinder_base[1];
                        axis_midpoint[2] = (float)(cylinder_height_unit_vector[2] * half_cylinder_height) + cylinder_base[2];
                        Vec3Normalize(cylinder_height_unit_vector);
                        Scr_MakeArray(SCRIPTINSTANCE_SERVER);
                        for ( point_index = 0; point_index < element_count; ++point_index )
                        {
                            memset(collision_results, 0, sizeof(collision_results));
                            v11 = Vec3DistanceSq(points[point_index], axis_midpoint);
                            if ( bounding_sphere_radius_squared > v11 )
                            {
                                (midpoint_axial_distance) = fabs((float)((float)((float)(points[point_index][0] - axis_midpoint[0]) * cylinder_height_unit_vector[0]) 
                                    + (float)((float)(points[point_index][1] - axis_midpoint[1]) * cylinder_height_unit_vector[1]))
                                    + (float)((float)(points[point_index][2] - axis_midpoint[2]) * cylinder_height_unit_vector[2]))
                                                                                                 ;
                                if ( half_cylinder_height > midpoint_axial_distance )
                                {
                                    dsquared = Vec3DistanceSq(points[point_index], axis_midpoint);
                                    if ( cylinder_radius_squared > (float)(dsquared
                                                                                                             - (float)(midpoint_axial_distance * midpoint_axial_distance)) )
                                    {
                                        collision_results[0] = 1.0f;
                                        collision_results[1] = Vec3DistanceSq(cylinder_base, points[point_index]);
                                        collision_results[2] = dsquared - (float)(midpoint_axial_distance * midpoint_axial_distance);
                                    }
                                }
                            }
                            Scr_AddVector(collision_results, SCRIPTINSTANCE_SERVER);
                            Scr_AddArray(SCRIPTINSTANCE_SERVER);
                        }
                    }
                    else
                    {
                        TypeName = Scr_GetTypeName(4u, SCRIPTINSTANCE_SERVER);
                        v9 = va("Parameter '%s' must be a vector value (cylinder height unit vector)", TypeName);
                        Scr_ParamError(4u, v9, SCRIPTINSTANCE_SERVER);
                    }
                }
                else
                {
                    v6 = Scr_GetTypeName(3u, SCRIPTINSTANCE_SERVER);
                    v7 = va("Parameter '%s' must be a floating point value (cylinder height)", v6);
                    Scr_ParamError(3u, v7, SCRIPTINSTANCE_SERVER);
                }
            }
            else
            {
                v4 = Scr_GetTypeName(2u, SCRIPTINSTANCE_SERVER);
                v5 = va("Parameter '%s' must be a floating point value (cylinder radius)", v4);
                Scr_ParamError(2u, v5, SCRIPTINSTANCE_SERVER);
            }
        }
        else
        {
            v2 = Scr_GetTypeName(1u, SCRIPTINSTANCE_SERVER);
            v3 = va("Parameter '%s' must be a vector (3D point origin)", v2);
            Scr_ParamError(1u, v3, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        v0 = Scr_GetTypeName(0, SCRIPTINSTANCE_SERVER);
        v1 = va("Parameter '%s' must be an array", v0);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_CollisionTestPointsInPill()
{
    const char *v0; // eax
    const char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    const char *v4; // eax
    const char *v5; // eax
    const char *v6; // eax
    const char *v7; // eax
    const char *v8; // eax
    const char *v9; // eax
    const char *TypeName; // eax
    const char *v11; // eax
    VariableUnion v12; // eax
    double v13; // st7
    double v14; // st7
    double v15; // st7
    float distance_from_axis; // [esp+58h] [ebp-C90h]
    float dsquared; // [esp+68h] [ebp-C80h]
    float midpoint_axial_distance; // [esp+7Ch] [ebp-C6Ch]
    float collision_results[3]; // [esp+80h] [ebp-C68h] BYREF
    int point_index; // [esp+8Ch] [ebp-C5Ch]
    float half_cylinder_height; // [esp+90h] [ebp-C58h]
    float bounding_sphere_radius_squared; // [esp+94h] [ebp-C54h]
    float cylinder_radial_vector[3]; // [esp+98h] [ebp-C50h] BYREF
    float cylinder_base[3]; // [esp+A4h] [ebp-C44h] BYREF
    float axis_midpoint[3]; // [esp+B0h] [ebp-C38h] BYREF
    float cylinder_top[3]; // [esp+BCh] [ebp-C2Ch] BYREF
    float points[256][3]; // [esp+C8h] [ebp-C20h] BYREF
    float cylinder_height; // [esp+CC8h] [ebp-20h]
    float cylinder_radius; // [esp+CCCh] [ebp-1Ch]
    float cylinder_radius_squared; // [esp+CD0h] [ebp-18h]
    float cylinder_height_unit_vector[3]; // [esp+CD4h] [ebp-14h] BYREF
    float bounding_sphere_radius; // [esp+CE0h] [ebp-8h]
    int element_count; // [esp+CE4h] [ebp-4h]

    if ( Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == 20 )
    {
        if ( Scr_GetType(1u, SCRIPTINSTANCE_SERVER) == 4 )
        {
            if ( Scr_GetType(2u, SCRIPTINSTANCE_SERVER) == 5 )
            {
                if ( Scr_GetType(3u, SCRIPTINSTANCE_SERVER) == 5 )
                {
                    if ( Scr_GetType(4u, SCRIPTINSTANCE_SERVER) == 4 )
                    {
                        if ( Scr_GetType(5u, SCRIPTINSTANCE_SERVER) == 4 )
                        {
                            v12.intValue = Scr_GetObject(0, SCRIPTINSTANCE_SERVER);
                            element_count = Scr_GetArrayValues_Vector(0, v12.stringValue, points, 256, "spawn points");
                            Scr_GetVector(1u, cylinder_base, SCRIPTINSTANCE_SERVER);
                            cylinder_radius = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
                            cylinder_height = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
                            half_cylinder_height = 0.5 * cylinder_height;
                            cylinder_radius_squared = cylinder_radius * cylinder_radius;
                            bounding_sphere_radius = cylinder_radius + (float)(0.5 * cylinder_height);
                            bounding_sphere_radius_squared = bounding_sphere_radius * bounding_sphere_radius;
                            Scr_GetVector(4u, cylinder_height_unit_vector, SCRIPTINSTANCE_SERVER);
                            axis_midpoint[0] = (float)(cylinder_height_unit_vector[0] * half_cylinder_height) + cylinder_base[0];
                            axis_midpoint[1] = (float)(cylinder_height_unit_vector[1] * half_cylinder_height) + cylinder_base[1];
                            axis_midpoint[2] = (float)(cylinder_height_unit_vector[2] * half_cylinder_height) + cylinder_base[2];
                            cylinder_top[0] = (float)(cylinder_height_unit_vector[0] * cylinder_height) + cylinder_base[0];
                            cylinder_top[1] = (float)(cylinder_height_unit_vector[1] * cylinder_height) + cylinder_base[1];
                            cylinder_top[2] = (float)(cylinder_height_unit_vector[2] * cylinder_height) + cylinder_base[2];
                            Vec3Normalize(cylinder_height_unit_vector);
                            Scr_GetVector(5u, cylinder_radial_vector, SCRIPTINSTANCE_SERVER);
                            Vec3Normalize(cylinder_radial_vector);
                            Scr_MakeArray(SCRIPTINSTANCE_SERVER);
                            for ( point_index = 0; point_index < element_count; ++point_index )
                            {
                                memset(collision_results, 0, sizeof(collision_results));
                                v13 = Vec3DistanceSq(points[point_index], axis_midpoint);
                                if ( bounding_sphere_radius_squared > v13 )
                                {
                                    v14 = Vec3DistanceSq(points[point_index], cylinder_base);
                                    if ( cylinder_radius_squared <= v14 )
                                    {
                                        v15 = Vec3DistanceSq(points[point_index], cylinder_top);
                                        if ( cylinder_radius_squared <= v15 )
                                        {
                                            (midpoint_axial_distance) = fabs((float)((float)((float)(points[point_index][0] - axis_midpoint[0]) * cylinder_height_unit_vector[0])
                                                + (float)((float)(points[point_index][1] - axis_midpoint[1]) * cylinder_height_unit_vector[1]))
                                                + (float)((float)(points[point_index][2] - axis_midpoint[2]) * cylinder_height_unit_vector[2]));
                                            if ( half_cylinder_height > midpoint_axial_distance )
                                            {
                                                dsquared = Vec3DistanceSq(points[point_index], axis_midpoint);
                                                if ( cylinder_radius_squared > (float)(dsquared
                                                                                                                         - (float)(midpoint_axial_distance * midpoint_axial_distance)) )
                                                    collision_results[0] = 1.0f;
                                            }
                                        }
                                        else
                                        {
                                            collision_results[0] = 1.0f;
                                        }
                                    }
                                    else
                                    {
                                        collision_results[0] = 1.0f;
                                    }
                                }
                                if ( collision_results[0] > 0.0 )
                                {
                                    distance_from_axis = (float)((float)((float)(points[point_index][0] - cylinder_base[0])
                                                                                                         * cylinder_radial_vector[0])
                                                                                         + (float)((float)(points[point_index][1] - cylinder_base[1])
                                                                                                         * cylinder_radial_vector[1]))
                                                                         + (float)((float)(points[point_index][2] - cylinder_base[2])
                                                                                         * cylinder_radial_vector[2]);
                                    collision_results[1] = Vec3DistanceSq(cylinder_base, points[point_index]);
                                    collision_results[2] = distance_from_axis * distance_from_axis;
                                }
                                Scr_AddVector(collision_results, SCRIPTINSTANCE_SERVER);
                                Scr_AddArray(SCRIPTINSTANCE_SERVER);
                            }
                        }
                        else
                        {
                            TypeName = Scr_GetTypeName(5u, SCRIPTINSTANCE_SERVER);
                            v11 = va("Parameter '%s' must be a vector value (cylinder radial vector)", TypeName);
                            Scr_ParamError(5u, v11, SCRIPTINSTANCE_SERVER);
                        }
                    }
                    else
                    {
                        v8 = Scr_GetTypeName(4u, SCRIPTINSTANCE_SERVER);
                        v9 = va("Parameter '%s' must be a vector value (cylinder height unit vector)", v8);
                        Scr_ParamError(4u, v9, SCRIPTINSTANCE_SERVER);
                    }
                }
                else
                {
                    v6 = Scr_GetTypeName(3u, SCRIPTINSTANCE_SERVER);
                    v7 = va("Parameter '%s' must be a floating point value (cylinder height)", v6);
                    Scr_ParamError(3u, v7, SCRIPTINSTANCE_SERVER);
                }
            }
            else
            {
                v4 = Scr_GetTypeName(2u, SCRIPTINSTANCE_SERVER);
                v5 = va("Parameter '%s' must be a floating point value (cylinder radius)", v4);
                Scr_ParamError(2u, v5, SCRIPTINSTANCE_SERVER);
            }
        }
        else
        {
            v2 = Scr_GetTypeName(1u, SCRIPTINSTANCE_SERVER);
            v3 = va("Parameter '%s' must be a vector (3D point origin)", v2);
            Scr_ParamError(1u, v3, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        v0 = Scr_GetTypeName(0, SCRIPTINSTANCE_SERVER);
        v1 = va("Parameter '%s' must be an array", v0);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_CollisionTestPointsInCone()
{
    const char *v0; // eax
    const char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    const char *v4; // eax
    const char *v5; // eax
    const char *v6; // eax
    const char *v7; // eax
    const char *v8; // eax
    const char *v9; // eax
    const char *TypeName; // eax
    const char *v11; // eax
    VariableUnion v12; // eax
    double v13; // st7
    float v14; // [esp+0h] [ebp-D10h]
    float v15; // [esp+4h] [ebp-D0Ch]
    float distance_from_axis; // [esp+44h] [ebp-CCCh]
    float distance_from_origin_squared; // [esp+54h] [ebp-CBCh]
    float tp; // [esp+58h] [ebp-CB8h]
    float tp_4; // [esp+5Ch] [ebp-CB4h]
    float tp_8; // [esp+60h] [ebp-CB0h]
    float tp_dot_to; // [esp+64h] [ebp-CACh]
    float collision_results[3]; // [esp+7Ch] [ebp-C94h] BYREF
    int point_index; // [esp+88h] [ebp-C88h]
    float bounding_sphere_radius_squared; // [esp+8Ch] [ebp-C84h]
    float axis_midpoint[3]; // [esp+90h] [ebp-C80h] BYREF
    float te_dot_to; // [esp+9Ch] [ebp-C74h]
    float points[256][3]; // [esp+A0h] [ebp-C70h] BYREF
    float cone_height; // [esp+CA0h] [ebp-70h]
    float radial_edge[3]; // [esp+CA4h] [ebp-6Ch]
    float cone_base_radius; // [esp+CB0h] [ebp-60h]
    float half_cone_height; // [esp+CB4h] [ebp-5Ch]
    float te_magnitude_squared; // [esp+CB8h] [ebp-58h]
    float cone_tip[3]; // [esp+CBCh] [ebp-54h]
    float bounding_sphere_radius; // [esp+CC8h] [ebp-48h]
    float cone_base[3]; // [esp+CCCh] [ebp-44h] BYREF
    float cone_height_unit_vector[3]; // [esp+CD8h] [ebp-38h] BYREF
    float to[3]; // [esp+CE4h] [ebp-2Ch]
    float te[3]; // [esp+CF0h] [ebp-20h]
    float teto2_over_te; // [esp+CFCh] [ebp-14h]
    float cone_radial_unit_vector[3]; // [esp+D00h] [ebp-10h] BYREF
    int element_count; // [esp+D0Ch] [ebp-4h]

    if ( Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == 20 )
    {
        if ( Scr_GetType(1u, SCRIPTINSTANCE_SERVER) == 4 )
        {
            if ( Scr_GetType(2u, SCRIPTINSTANCE_SERVER) == 5 )
            {
                if ( Scr_GetType(3u, SCRIPTINSTANCE_SERVER) == 5 )
                {
                    if ( Scr_GetType(4u, SCRIPTINSTANCE_SERVER) == 4 )
                    {
                        if ( Scr_GetType(5u, SCRIPTINSTANCE_SERVER) == 4 )
                        {
                            v12.intValue = Scr_GetObject(0, SCRIPTINSTANCE_SERVER);
                            element_count = Scr_GetArrayValues_Vector(0, v12.stringValue, points, 256, "spawn points");
                            Scr_GetVector(1u, cone_base, SCRIPTINSTANCE_SERVER);
                            cone_base_radius = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
                            cone_height = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
                            Scr_GetVector(4u, cone_height_unit_vector, SCRIPTINSTANCE_SERVER);
                            Scr_GetVector(5u, cone_radial_unit_vector, SCRIPTINSTANCE_SERVER);
                            half_cone_height = 0.5 * cone_height;
                            bounding_sphere_radius = cone_base_radius + (float)(0.5 * cone_height);
                            bounding_sphere_radius_squared = bounding_sphere_radius * bounding_sphere_radius;
                            axis_midpoint[0] = (float)(cone_height_unit_vector[0] * (float)(0.5 * cone_height)) + cone_base[0];
                            axis_midpoint[1] = (float)(cone_height_unit_vector[1] * (float)(0.5 * cone_height)) + cone_base[1];
                            axis_midpoint[2] = (float)(cone_height_unit_vector[2] * (float)(0.5 * cone_height)) + cone_base[2];
                            cone_tip[0] = (float)(cone_height_unit_vector[0] * cone_height) + cone_base[0];
                            cone_tip[1] = (float)(cone_height_unit_vector[1] * cone_height) + cone_base[1];
                            cone_tip[2] = (float)(cone_height_unit_vector[2] * cone_height) + cone_base[2];
                            radial_edge[0] = (float)(cone_radial_unit_vector[0] * cone_base_radius) + cone_base[0];
                            radial_edge[1] = (float)(cone_radial_unit_vector[1] * cone_base_radius) + cone_base[1];
                            radial_edge[2] = (float)(cone_radial_unit_vector[2] * cone_base_radius) + cone_base[2];
                            Vec3Normalize(cone_height_unit_vector);
                            Vec3Normalize(cone_radial_unit_vector);
                            te[0] = radial_edge[0] - cone_tip[0];
                            te[1] = radial_edge[1] - cone_tip[1];
                            te[2] = radial_edge[2] - cone_tip[2];
                            to[0] = cone_base[0] - cone_tip[0];
                            to[1] = cone_base[1] - cone_tip[1];
                            to[2] = cone_base[2] - cone_tip[2];
                            te_dot_to = (float)((float)((float)(radial_edge[0] - cone_tip[0]) * (float)(cone_base[0] - cone_tip[0]))
                                                                + (float)((float)(radial_edge[1] - cone_tip[1]) * (float)(cone_base[1] - cone_tip[1])))
                                                + (float)((float)(radial_edge[2] - cone_tip[2]) * (float)(cone_base[2] - cone_tip[2]));
                            if ( (float)(0.0000099999997
                                                 - (float)((float)((float)((float)(radial_edge[0] - cone_tip[0])
                                                                                                 * (float)(radial_edge[0] - cone_tip[0]))
                                                                                 + (float)((float)(radial_edge[1] - cone_tip[1])
                                                                                                 * (float)(radial_edge[1] - cone_tip[1])))
                                                                 + (float)((float)(radial_edge[2] - cone_tip[2]) * (float)(radial_edge[2] - cone_tip[2])))) < 0.0 )
                                v15 = (float)((float)((float)(radial_edge[0] - cone_tip[0]) * (float)(radial_edge[0] - cone_tip[0]))
                                                        + (float)((float)(radial_edge[1] - cone_tip[1]) * (float)(radial_edge[1] - cone_tip[1])))
                                        + (float)((float)(radial_edge[2] - cone_tip[2]) * (float)(radial_edge[2] - cone_tip[2]));
                            else
                                v15 = 0.0000099999997f;
                            te_magnitude_squared = v15;
                            teto2_over_te = (float)(te_dot_to * te_dot_to) / v15;
                            Scr_MakeArray(SCRIPTINSTANCE_SERVER);
                            for ( point_index = 0; point_index < element_count; ++point_index )
                            {
                                memset(collision_results, 0, sizeof(collision_results));
                                v13 = Vec3DistanceSq(points[point_index], axis_midpoint);
                                if ( bounding_sphere_radius_squared > v13
                                    && half_cone_height > fabs((float)((float)((float)(points[point_index][0] - axis_midpoint[0])
                                                                                                                    * cone_height_unit_vector[0])
                                                                                                    + (float)((float)(points[point_index][1] - axis_midpoint[1])
                                                                                                                    * cone_height_unit_vector[1]))
                                                                                    + (float)((float)(points[point_index][2] - axis_midpoint[2])
                                                                                                    * cone_height_unit_vector[2]))
                                                                                 )
                                {
                                    tp = points[point_index][0] - cone_tip[0];
                                    tp_4 = points[point_index][1] - cone_tip[1];
                                    tp_8 = points[point_index][2] - cone_tip[2];
                                    if ( (float)(0.0000099999997
                                                         - (float)((float)((float)(tp * tp) + (float)(tp_4 * tp_4)) + (float)(tp_8 * tp_8))) < 0.0 )
                                        v14 = (float)((float)(tp * tp) + (float)(tp_4 * tp_4)) + (float)(tp_8 * tp_8);
                                    else
                                        v14 = 0.0000099999997;
                                    tp_dot_to = (float)((float)(tp * to[0]) + (float)(tp_4 * to[1])) + (float)(tp_8 * to[2]);
                                    if ( (float)((float)(tp_dot_to * tp_dot_to) / v14) > teto2_over_te )
                                    {
                                        distance_from_origin_squared = Vec3DistanceSq(cone_base, points[point_index]);
                                        distance_from_axis = (float)((float)((float)(points[point_index][0] - cone_base[0])
                                                                                                             * cone_radial_unit_vector[0])
                                                                                             + (float)((float)(points[point_index][1] - cone_base[1])
                                                                                                             * cone_radial_unit_vector[1]))
                                                                             + (float)((float)(points[point_index][2] - cone_base[2])
                                                                                             * cone_radial_unit_vector[2]);
                                        collision_results[0] = 1.0f;
                                        collision_results[1] = distance_from_origin_squared;
                                        collision_results[2] = distance_from_axis * distance_from_axis;
                                    }
                                }
                                Scr_AddVector(collision_results, SCRIPTINSTANCE_SERVER);
                                Scr_AddArray(SCRIPTINSTANCE_SERVER);
                            }
                        }
                        else
                        {
                            TypeName = Scr_GetTypeName(5u, SCRIPTINSTANCE_SERVER);
                            v11 = va("Parameter '%s' must be a vector value (cone radial vector)", TypeName);
                            Scr_ParamError(5u, v11, SCRIPTINSTANCE_SERVER);
                        }
                    }
                    else
                    {
                        v8 = Scr_GetTypeName(4u, SCRIPTINSTANCE_SERVER);
                        v9 = va("Parameter '%s' must be a vector value (cone height unit vector)", v8);
                        Scr_ParamError(4u, v9, SCRIPTINSTANCE_SERVER);
                    }
                }
                else
                {
                    v6 = Scr_GetTypeName(3u, SCRIPTINSTANCE_SERVER);
                    v7 = va("Parameter '%s' must be a floating point value (cone height)", v6);
                    Scr_ParamError(3u, v7, SCRIPTINSTANCE_SERVER);
                }
            }
            else
            {
                v4 = Scr_GetTypeName(2u, SCRIPTINSTANCE_SERVER);
                v5 = va("Parameter '%s' must be a floating point value (cone radius)", v4);
                Scr_ParamError(2u, v5, SCRIPTINSTANCE_SERVER);
            }
        }
        else
        {
            v2 = Scr_GetTypeName(1u, SCRIPTINSTANCE_SERVER);
            v3 = va("Parameter '%s' must be a vector (3D point origin)", v2);
            Scr_ParamError(1u, v3, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        v0 = Scr_GetTypeName(0, SCRIPTINSTANCE_SERVER);
        v1 = va("Parameter '%s' must be an array", v0);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_CollisionTestPointsInBox()
{
    const char *v0; // eax
    const char *v1; // eax
    const char *v2; // eax
    const char *v3; // eax
    const char *v4; // eax
    const char *v5; // eax
    const char *v6; // eax
    const char *v7; // eax
    const char *v8; // eax
    const char *v9; // eax
    const char *v10; // eax
    const char *v11; // eax
    const char *TypeName; // eax
    const char *v13; // eax
    VariableUnion v14; // eax
    float v15; // [esp+0h] [ebp-D2Ch]
    float v16; // [esp+34h] [ebp-CF8h]
    float collision_results[3]; // [esp+44h] [ebp-CE8h] BYREF
    float v18[3]; // [esp+50h] [ebp-CDCh]
    int point_index; // [esp+5Ch] [ebp-CD0h]
    float bounding_sphere_radius_squared; // [esp+60h] [ebp-CCCh]
    float box_depth; // [esp+64h] [ebp-CC8h]
    float half_box_height; // [esp+68h] [ebp-CC4h]
    float transform[4][3]; // [esp+6Ch] [ebp-CC0h] BYREF
    float points[256][3]; // [esp+9Ch] [ebp-C90h] BYREF
    float axes[3][3]; // [esp+C9Ch] [ebp-90h] BYREF
    float local_left[3]; // [esp+CC0h] [ebp-6Ch] BYREF
    float box_up[3]; // [esp+CCCh] [ebp-60h] BYREF
    float box_forward[3]; // [esp+CD8h] [ebp-54h] BYREF
    float box_height; // [esp+CE4h] [ebp-48h]
    float local_up[3]; // [esp+CE8h] [ebp-44h] BYREF
    float bounding_sphere_radius; // [esp+CF4h] [ebp-38h]
    float half_box_width; // [esp+CF8h] [ebp-34h]
    float box_width; // [esp+CFCh] [ebp-30h]
    float local_forward[3]; // [esp+D00h] [ebp-2Ch] BYREF
    float half_box_depth; // [esp+D0Ch] [ebp-20h]
    float box_origin[3]; // [esp+D10h] [ebp-1Ch] BYREF
    int element_count; // [esp+D1Ch] [ebp-10h]
    float box_left[3]; // [esp+D20h] [ebp-Ch] BYREF

    if ( Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == 20 )
    {
        if ( Scr_GetType(1u, SCRIPTINSTANCE_SERVER) == 4 )
        {
            if ( Scr_GetType(2u, SCRIPTINSTANCE_SERVER) == 5 )
            {
                if ( Scr_GetType(3u, SCRIPTINSTANCE_SERVER) == 5 )
                {
                    if ( Scr_GetType(4u, SCRIPTINSTANCE_SERVER) == 5 )
                    {
                        if ( Scr_GetType(5u, SCRIPTINSTANCE_SERVER) == 4 )
                        {
                            if ( Scr_GetType(6u, SCRIPTINSTANCE_SERVER) == 4 )
                            {
                                v14.intValue = Scr_GetObject(0, SCRIPTINSTANCE_SERVER);
                                element_count = Scr_GetArrayValues_Vector(0, v14.stringValue, points, 256, "spawn points");
                                Scr_GetVector(1u, box_origin, SCRIPTINSTANCE_SERVER);
                                box_width = Scr_GetFloat(2u, SCRIPTINSTANCE_SERVER);
                                box_height = Scr_GetFloat(3u, SCRIPTINSTANCE_SERVER);
                                box_depth = Scr_GetFloat(4u, SCRIPTINSTANCE_SERVER);
                                Scr_GetVector(5u, axes[0], SCRIPTINSTANCE_SERVER);
                                Scr_GetVector(6u, axes[1], SCRIPTINSTANCE_SERVER);
                                Vec3Cross(axes[0], axes[1], axes[2]);
                                if ( (float)(box_height - box_width) < 0.0 )
                                    v16 = box_height;
                                else
                                    v16 = box_width;
                                if ( (float)(box_depth - v16) < 0.0 )
                                    v15 = box_depth;
                                else
                                    v15 = v16;
                                bounding_sphere_radius = (float)((float)(box_width + box_height) + box_depth) - v15;
                                bounding_sphere_radius_squared = bounding_sphere_radius * bounding_sphere_radius;
                                half_box_width = 0.5 * box_width;
                                half_box_height = 0.5 * box_height;
                                half_box_depth = 0.5 * box_depth;
                                *(_QWORD *)&transform[0][0] = *(_QWORD *)&axes[0][0];
                                transform[0][2] = axes[0][2];
                                *(_QWORD *)&transform[1][0] = *(_QWORD *)&axes[1][0];
                                transform[1][2] = axes[1][2];
                                *(_QWORD *)&transform[2][0] = *(_QWORD *)&axes[2][0];
                                transform[2][2] = axes[2][2];
                                memset(transform[3], 0, sizeof(const float[3]));
                                local_forward[0] = box_width;
                                local_forward[1] = 0.0f;
                                local_forward[2] = 0.0f;
                                local_left[0] = 0.0f;
                                local_left[1] = box_height;
                                local_left[2] = 0.0f;
                                local_up[0] = 0.0f;
                                local_up[1] = 0.0f;
                                local_up[2] = box_depth;
                                MatrixTransformVector43(local_forward, transform, box_forward);
                                MatrixTransformVector43(local_up, transform, box_up);
                                MatrixTransformVector43(local_left, transform, box_left);
                                Vec3Normalize(box_forward);
                                Vec3Normalize(box_left);
                                Vec3Normalize(box_up);
                                Scr_MakeArray(SCRIPTINSTANCE_SERVER);
                                for ( point_index = 0; point_index < element_count; ++point_index )
                                {
                                    memset(collision_results, 0, sizeof(collision_results));
                                    v18[0] = points[point_index][0] - box_origin[0];
                                    v18[1] = points[point_index][1] - box_origin[1];
                                    v18[2] = points[point_index][2] - box_origin[2];
                                    if ( bounding_sphere_radius_squared > (float)((float)((float)(v18[0] * v18[0])
                                                                                                                                            + (float)(v18[1] * v18[1]))
                                                                                                                            + (float)(v18[2] * v18[2]))
                                        && half_box_width >  (fabs((float)((float)(v18[0] * box_forward[0]) + (float)(v18[1] * box_forward[1])) + (float)(v18[2] * box_forward[2])) )
                                        && half_box_height > (fabs((float)((float)(v18[0] * box_left[0]) + (float)(v18[1] * box_left[1])) + (float)(v18[2] * box_left[2])) )
                                        && half_box_depth >  (fabs((float)((float)(v18[0] * box_up[0]) + (float)(v18[1] * box_up[1])) + (float)(v18[2] * box_up[2]))) )
                                    {
                                        collision_results[0] = 1.0f;
                                        collision_results[1] = (float)((float)(v18[0] * v18[0]) + (float)(v18[1] * v18[1]))
                                                                                 + (float)(v18[2] * v18[2]);
                                    }
                                    Scr_AddVector(collision_results, SCRIPTINSTANCE_SERVER);
                                    Scr_AddArray(SCRIPTINSTANCE_SERVER);
                                }
                            }
                            else
                            {
                                TypeName = Scr_GetTypeName(6u, SCRIPTINSTANCE_SERVER);
                                v13 = va("Parameter '%s' must be a vector (up)", TypeName);
                                Scr_ParamError(6u, v13, SCRIPTINSTANCE_SERVER);
                            }
                        }
                        else
                        {
                            v10 = Scr_GetTypeName(5u, SCRIPTINSTANCE_SERVER);
                            v11 = va("Parameter '%s' must be a vector (forward)", v10);
                            Scr_ParamError(5u, v11, SCRIPTINSTANCE_SERVER);
                        }
                    }
                    else
                    {
                        v8 = Scr_GetTypeName(4u, SCRIPTINSTANCE_SERVER);
                        v9 = va("Parameter '%s' must be a floating point value (box depth)", v8);
                        Scr_ParamError(4u, v9, SCRIPTINSTANCE_SERVER);
                    }
                }
                else
                {
                    v6 = Scr_GetTypeName(3u, SCRIPTINSTANCE_SERVER);
                    v7 = va("Parameter '%s' must be a floating point value (box length)", v6);
                    Scr_ParamError(3u, v7, SCRIPTINSTANCE_SERVER);
                }
            }
            else
            {
                v4 = Scr_GetTypeName(2u, SCRIPTINSTANCE_SERVER);
                v5 = va("Parameter '%s' must be a floating point value (box width)", v4);
                Scr_ParamError(2u, v5, SCRIPTINSTANCE_SERVER);
            }
        }
        else
        {
            v2 = Scr_GetTypeName(1u, SCRIPTINSTANCE_SERVER);
            v3 = va("Parameter '%s' must be a vector (3D point origin)", v2);
            Scr_ParamError(1u, v3, SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        v0 = Scr_GetTypeName(0, SCRIPTINSTANCE_SERVER);
        v1 = va("Parameter '%s' must be an array", v0);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_QSortScoredSpawnPointArrayAscending()
{
    const char *TypeName; // eax
    const char *v1; // eax
    VariableUnion v2; // eax
    int value_index; // [esp+0h] [ebp-C0Ch]
    float scored_spawn_points[256][3]; // [esp+4h] [ebp-C08h] BYREF
    int element_count; // [esp+C08h] [ebp-4h]

    if ( Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == 20 )
    {
        v2.intValue = Scr_GetObject(0, SCRIPTINSTANCE_SERVER);
        element_count = Scr_GetArrayValues_Vector(0, v2.stringValue, scored_spawn_points, 256, "scored spawn points");
        Scr_MakeArray(SCRIPTINSTANCE_SERVER);
        if ( element_count > 0 )
            qsort(
                scored_spawn_points,
                element_count,
                0xCu,
                (int (__cdecl *)(const void *, const void *))sort_scored_spawn_point_vectors_ascending);
        for ( value_index = 0; value_index < element_count; ++value_index )
        {
            Scr_AddVector(scored_spawn_points[value_index], SCRIPTINSTANCE_SERVER);
            Scr_AddArray(SCRIPTINSTANCE_SERVER);
        }
    }
    else
    {
        TypeName = Scr_GetTypeName(0, SCRIPTINSTANCE_SERVER);
        v1 = va("Parameter '%s' must be an array", TypeName);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

int __cdecl sort_scored_spawn_point_vectors_ascending(float *a, float *b)
{
    if ( a[1] > b[1] )
        return 1;
    if ( b[1] <= a[1] )
        return 0;
    return -1;
}

void GScr_Matrix4x4TransformPoints()
{
    const char *v0; // eax
    const char *v1; // eax
    float transformed_point[3]; // [esp+0h] [ebp-670h] BYREF
    int point_index; // [esp+Ch] [ebp-664h]
    float points[128][3]; // [esp+10h] [ebp-660h] BYREF
    int point_count; // [esp+610h] [ebp-60h]
    int element_index; // [esp+614h] [ebp-5Ch]
    int matrix_element_size; // [esp+618h] [ebp-58h]
    int parent_id; // [esp+61Ch] [ebp-54h]
    float matrix_transform[16]; // [esp+620h] [ebp-50h]
    int parameter_index; // [esp+660h] [ebp-10h]
    int k_matrix_4x4_element_count; // [esp+664h] [ebp-Ch]
    VariableValueInternal *entry_value; // [esp+668h] [ebp-8h]
    int id; // [esp+66Ch] [ebp-4h]

    k_matrix_4x4_element_count = 16;
    parameter_index = 0;
    parent_id = Scr_GetObject(0, SCRIPTINSTANCE_SERVER);
    if ( GetObjectType(SCRIPTINSTANCE_SERVER, parent_id) != 20
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    17107,
                    0,
                    "%s",
                    "GetObjectType(SCRIPTINSTANCE_SERVER, parent_id)==VAR_ARRAY") )
    {
        __debugbreak();
    }
    matrix_element_size = GetArraySize(SCRIPTINSTANCE_SERVER, parent_id);
    if ( matrix_element_size == 16 )
    {
        for ( element_index = 0; element_index < 16; ++element_index )
        {
            id = GetArrayVariable(SCRIPTINSTANCE_SERVER, parent_id, element_index);
            if ( !id
                && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 17117, 0, "%s", "id") )
            {
                __debugbreak();
            }
            entry_value = &gScrVarGlob[0].variableList[id + 0x8000];
            if ( (entry_value->w.status & 0x60) == 0
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                            17119,
                            0,
                            "%s",
                            "(entry_value->w.status & VAR_STAT_MASK) != VAR_STAT_FREE") )
            {
                __debugbreak();
            }
            if ( (entry_value->w.status & 0x1F) == 5 )
            {
                matrix_transform[element_index] = entry_value->u.u.floatValue;
            }
            else
            {
                if ( (entry_value->w.status & 0x1F) != 6 )
                {
                    v0 = va("contents of transformation matrix array must be numbers");
                    Scr_ParamError(parameter_index, v0, SCRIPTINSTANCE_SERVER);
                    break;
                }
                matrix_transform[element_index] = (float)entry_value->u.u.intValue;
            }
        }
        if ( element_index == 16 )
        {
            parent_id = Scr_GetObject(++parameter_index, SCRIPTINSTANCE_SERVER);
            point_count = Scr_GetArrayValues_Vector(parameter_index, parent_id, points, 128, "debug geometry points");
            Scr_MakeArray(SCRIPTINSTANCE_SERVER);
            for ( point_index = 0; point_index < point_count; ++point_index )
            {
                transformed_point[0] = (float)((float)((float)(points[point_index][0] * matrix_transform[0])
                                                                                         + (float)(points[point_index][1] * matrix_transform[4]))
                                                                         + (float)(points[point_index][2] * matrix_transform[8]))
                                                         + matrix_transform[3];
                transformed_point[1] = (float)((float)((float)(points[point_index][0] * matrix_transform[1])
                                                                                         + (float)(points[point_index][1] * matrix_transform[5]))
                                                                         + (float)(points[point_index][2] * matrix_transform[9]))
                                                         + matrix_transform[7];
                transformed_point[2] = (float)((float)((float)(points[point_index][0] * matrix_transform[2])
                                                                                         + (float)(points[point_index][1] * matrix_transform[6]))
                                                                         + (float)(points[point_index][2] * matrix_transform[10]))
                                                         + matrix_transform[11];
                Scr_AddVector(transformed_point, SCRIPTINSTANCE_SERVER);
                Scr_AddArray(SCRIPTINSTANCE_SERVER);
            }
        }
    }
    else
    {
        v1 = va("first parameter passed to Matrix4x4TransformPoints must be float[16]");
        Scr_ParamError(parameter_index, v1, SCRIPTINSTANCE_SERVER);
    }
}

void GScr_GetNumGVRules()
{
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}

void GScr_GetGVRule()
{
    Scr_GameVariants_GetRule();
}

void GScr_GetWeaponMinDamageRange()
{
    const char *v0; // eax
    unsigned int iWeaponIndex; // [esp+8h] [ebp-54h]
    weaponParms wp; // [esp+Ch] [ebp-50h] BYREF
    const char *pszWeaponName; // [esp+58h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( !iWeaponIndex )
    {
        if ( *pszWeaponName )
        {
            v0 = va("Invalid weapon name %s", pszWeaponName);
            Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_ParamError(0, "Invalid weapon name", SCRIPTINSTANCE_SERVER);
        }
    }
    Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
    Scr_AddFloat(wp.weapDef->fMinDamageRange, SCRIPTINSTANCE_SERVER);
}

void GScr_GetWeaponMaxDamageRange()
{
    const char *v0; // eax
    unsigned int iWeaponIndex; // [esp+8h] [ebp-54h]
    weaponParms wp; // [esp+Ch] [ebp-50h] BYREF
    const char *pszWeaponName; // [esp+58h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( !iWeaponIndex )
    {
        if ( *pszWeaponName )
        {
            v0 = va("Invalid weapon name %s", pszWeaponName);
            Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_ParamError(0, "Invalid weapon name", SCRIPTINSTANCE_SERVER);
        }
    }
    Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
    Scr_AddFloat(wp.weapDef->fMaxDamageRange, SCRIPTINSTANCE_SERVER);
}

void GScr_GetWeaponMinDamage()
{
    const char *v0; // eax
    unsigned int iWeaponIndex; // [esp+8h] [ebp-54h]
    weaponParms wp; // [esp+Ch] [ebp-50h] BYREF
    const char *pszWeaponName; // [esp+58h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( !iWeaponIndex )
    {
        if ( *pszWeaponName )
        {
            v0 = va("Invalid weapon name %s", pszWeaponName);
            Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_ParamError(0, "Invalid weapon name", SCRIPTINSTANCE_SERVER);
        }
    }
    Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
    Scr_AddFloat((float)wp.weapDef->minDamage, SCRIPTINSTANCE_SERVER);
}

void GScr_GetWeaponMaxDamage()
{
    const char *v0; // eax
    unsigned int iWeaponIndex; // [esp+8h] [ebp-54h]
    weaponParms wp; // [esp+Ch] [ebp-50h] BYREF
    const char *pszWeaponName; // [esp+58h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( !iWeaponIndex )
    {
        if ( *pszWeaponName )
        {
            v0 = va("Invalid weapon name %s", pszWeaponName);
            Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_ParamError(0, "Invalid weapon name", SCRIPTINSTANCE_SERVER);
        }
    }
    Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
    Scr_AddFloat((float)wp.weapDef->damage, SCRIPTINSTANCE_SERVER);
}

void GScr_GetWeaponFuseTime()
{
    const char *v0; // eax
    unsigned int iWeaponIndex; // [esp+8h] [ebp-54h]
    weaponParms wp; // [esp+Ch] [ebp-50h] BYREF
    const char *pszWeaponName; // [esp+58h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( !iWeaponIndex )
    {
        if ( *pszWeaponName )
        {
            v0 = va("Invalid weapon name %s", pszWeaponName);
            Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_ParamError(0, "Invalid weapon name", SCRIPTINSTANCE_SERVER);
        }
    }
    Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
    Scr_AddFloat((float)wp.weapDef->fuseTime, SCRIPTINSTANCE_SERVER);
}

void GScr_GetWeaponProjExplosionSound()
{
    const char *v0; // eax
    unsigned int iWeaponIndex; // [esp+0h] [ebp-54h]
    weaponParms wp; // [esp+4h] [ebp-50h] BYREF
    const char *pszWeaponName; // [esp+50h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( !iWeaponIndex )
    {
        if ( *pszWeaponName )
        {
            v0 = va("Invalid weapon name %s", pszWeaponName);
            Scr_ParamError(0, v0, SCRIPTINSTANCE_SERVER);
        }
        else
        {
            Scr_ParamError(0, "Invalid weapon name", SCRIPTINSTANCE_SERVER);
        }
    }
    Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
    if ( wp.weapDef->projExplosionSound )
        Scr_AddString((char *)wp.weapDef->projExplosionSound, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
}

void GScr_IsWeaponSpecificUse()
{
    char *v0; // eax
    unsigned int iWeaponIndex; // [esp+0h] [ebp-54h]
    weaponParms wp; // [esp+4h] [ebp-50h] BYREF
    const char *pszWeaponName; // [esp+50h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( iWeaponIndex )
    {
        Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
        if ( wp.weapDef->offhandSlot == OFFHAND_SLOT_SPECIFIC_USE )
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        v0 = va("WARNING: unknown weapon '%s' passed to IsWeaponSpecificUse\n", pszWeaponName);
        Com_PrintWarning(24, v0);
    }
}

void GScr_IsWeaponEquipment()
{
    char *v0; // eax
    unsigned int iWeaponIndex; // [esp+0h] [ebp-54h]
    weaponParms wp; // [esp+4h] [ebp-50h] BYREF
    const char *pszWeaponName; // [esp+50h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( iWeaponIndex )
    {
        Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
        if ( wp.weapDef->offhandSlot == OFFHAND_SLOT_EQUIPMENT )
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        v0 = va("WARNING: unknown weapon '%s' passed to IsWeaponEquipment\n", pszWeaponName);
        Com_PrintWarning(24, v0);
    }
}

void GScr_IsWeaponPrimary()
{
    char *v0; // eax
    unsigned int iWeaponIndex; // [esp+0h] [ebp-54h]
    weaponParms wp; // [esp+4h] [ebp-50h] BYREF
    const char *pszWeaponName; // [esp+50h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( iWeaponIndex )
    {
        Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
        if ( wp.weapDef->inventoryType )
            Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        v0 = va("WARNING: unknown weapon '%s' passed to IsWeaponPrimary\n", pszWeaponName);
        Com_PrintWarning(24, v0);
    }
}

void GScr_IsWeaponScopeOverlay()
{
    char *v0; // eax
    unsigned int iWeaponIndex; // [esp+0h] [ebp-54h]
    weaponParms wp; // [esp+4h] [ebp-50h] BYREF
    const char *pszWeaponName; // [esp+50h] [ebp-4h]

    pszWeaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    iWeaponIndex = G_GetWeaponIndexForName((char *)pszWeaponName);
    if ( iWeaponIndex )
    {
        Weapon_SetWeaponParamsWeapon(&wp, iWeaponIndex);
        if ( wp.weapDef->overlayReticle )
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        v0 = va("WARNING: unknown weapon '%s' passed to IsWeaponEquipment\n", pszWeaponName);
        Com_PrintWarning(24, v0);
    }
}

void GScr_PCServerUpdatePlaylist()
{
    SV_FetchWADDeferred();
}

void __cdecl GScr_GetPregameTeam(scr_entref_t entref)
{
    team_t AssignedPregameTeam; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetPlayerEntity(entref);
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 17561, 0, "%s", "ent") )
        __debugbreak();
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 17562, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    AssignedPregameTeam = Pregame_GetAssignedPregameTeam(ent->s.number);
    Scr_AddInt(AssignedPregameTeam, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_GetPregameClass(scr_entref_t entref)
{
    char *AssignedPregameClass; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetPlayerEntity(entref);
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 17577, 0, "%s", "ent") )
        __debugbreak();
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 17578, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    if ( *Pregame_GetAssignedPregameClass(ent->s.number) )
    {
        AssignedPregameClass = Pregame_GetAssignedPregameClass(ent->s.number);
        Scr_AddString(AssignedPregameClass, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_AddUndefined(SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_SetPregameTeam(scr_entref_t entref)
{
    unsigned __int16 team; // [esp+4h] [ebp-8h]
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetPlayerEntity(entref);
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 17598, 0, "%s", "ent") )
        __debugbreak();
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 17599, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    team = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( team == scr_const.allies )
    {
        Pregame_SetAssignedPregameTeam(ent->s.number, TEAM_ALLIES);
    }
    else if ( team == scr_const.axis )
    {
        Pregame_SetAssignedPregameTeam(ent->s.number, TEAM_AXIS);
    }
    else
    {
        Pregame_SetAssignedPregameTeam(ent->s.number, TEAM_SPECTATOR);
    }
}

void __cdecl GScr_SetPregameClass(scr_entref_t entref)
{
    char *String; // eax
    gentity_s *ent; // [esp+8h] [ebp-4h]

    ent = GetPlayerEntity(entref);
    if ( !ent && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 17622, 0, "%s", "ent") )
        __debugbreak();
    if ( !ent->client
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 17623, 0, "%s", "ent->client") )
    {
        __debugbreak();
    }
    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    Pregame_SetAssignedPregameClass(ent->s.number, String);
}

void GScr_PixBeginEvent()
{
    char *String; // eax
    const char *v1; // eax

    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    v1 = va("SCRIPT: %s", String);
    //PIXBeginNamedEvent(0, v1); // KISAKTODO: Tracy Markers
}

void GScr_PixEndEvent()
{
    //char *String; // eax
    //const char *v1; // eax
    //
    //String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    //v1 = va("SCRIPT: %s", String);
    //PIXBeginNamedEvent(0, v1); // KISAKTODO: Tracy Markers
}

void GScr_PixMarker()
{
    char *String; // eax
    const char *v1; // eax

    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    v1 = va("SCRIPT: %s", String);
    //PIXSetMarker(0, v1);
}

void GScr_IncrementCounter()
{
    int LicenseType; // eax
    char *counterType; // [esp+0h] [ebp-10h]
    int counterId; // [esp+4h] [ebp-Ch]
    int increment; // [esp+8h] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
    {
        LicenseType = SV_GetLicenseType();
        if ( SV_IsServerRanked(LicenseType) )
        {
            counterType = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
            increment = Scr_GetInt(1u, SCRIPTINSTANCE_SERVER);
            if ( increment )
            {
                if ( increment < 0
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                                17746,
                                0,
                                "%s",
                                "increment >= 0") )
                {
                    __debugbreak();
                }
                if ( increment >= 0 )
                {
                    counterId = LiveCounter_CounterStringToID(counterType);
                    if ( counterId == -1 )
                        Com_PrintError(16, "Invalid counter string id: %s\n", counterType);
                    else
                        LiveCounter_IncrementCounterValue(counterId, increment);
                }
                else
                {
                    Com_PrintError(16, "Invalid param: <increment value> must be greater than or equal to zero\n");
                }
            }
        }
    }
    else
    {
        Com_PrintError(16, "Invalid param count. usage: incrementCounter( <counter id>, <increment value> )\n");
    }
}

void GScr_GetCounterTotal()
{
    __int64 v0; // rax
    char *v1; // eax
    char *counterType; // [esp+Ch] [ebp-Ch]
    int counterId; // [esp+10h] [ebp-8h]
    int numParam; // [esp+14h] [ebp-4h]

    numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    counterType = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( numParam == 1 )
    {
        counterId = LiveCounter_CounterStringToID(counterType);
        if ( counterId == -1 )
        {
            Com_PrintError(16, "Invalid counter string id: %s\n", counterType);
            Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
        }
        else
        {
            LODWORD(v0) = LiveCounter_GetCounterTotalValue(counterId);
            if ( v0 == -1 )
            {
                Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
            }
            else
            {
                v1 = va("%llu", v0);
                Scr_AddString(v1, SCRIPTINSTANCE_SERVER);
            }
        }
    }
    else
    {
        Com_PrintError(16, "Invalid param count. usage: <string returned> getCounterTotal( <counter id> ).\n");
        Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
    }
}

void GScr_SetScoreboardColumns()
{
    char *String; // eax
    char *v1; // eax
    signed int i; // [esp+0h] [ebp-8h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 4 )
    {
        for ( i = 0; i < 4; ++i )
        {
            String = Scr_GetString(i, SCRIPTINSTANCE_SERVER);
            level.teamScores[i - 24] = CScr_GetColumnTypeByName(String);
            if ( !level.teamScores[i - 24] )
            {
                v1 = Scr_GetString(i, SCRIPTINSTANCE_SERVER);
                Com_PrintError(16, "Invalid param: %s is not a valid column type.\n", v1);
            }
        }
    }
    else
    {
        Com_PrintError(
            16,
            "Invalid param count. usage: setScoreboardColumns( <column1 name> <column2 name> <column3 name> <column4 name> ).\n");
        Scr_AddString((char *)"", SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_SetNemesisXuid(scr_entref_t entref)
{
    const ddlState_t *RootDDLState; // eax
    unsigned __int64 nemesisXuid; // [esp+4h] [ebp-20h] BYREF
    gentity_s *playerEnt; // [esp+Ch] [ebp-18h]
    const char *nemesisXuidString; // [esp+10h] [ebp-14h]
    ddlState_t localState; // [esp+14h] [ebp-10h] BYREF

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 1 )
    {
        nemesisXuidString = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
        nemesisXuid = 0;
        playerEnt = GetEntity(entref);
        if ( playerEnt->client )
        {
            if ( nemesisXuidString )
            {
                RootDDLState = LiveStats_GetRootDDLState();
                DDL_MoveTo(RootDDLState, &localState, 2, "AfterActionReportStats", "nemesisXuid");
                StringToXUID(nemesisXuidString, &nemesisXuid);
                SV_SetClientDInt64Stat(playerEnt->s.number, &localState, nemesisXuid);
            }
        }
        else
        {
            Scr_Error("setnemesisxuid: entity must be a player entity", 0);
        }
    }
    else
    {
        Com_PrintError(16, "Invalid param count. usage: setnemesisxuid <xuid>.\n");
    }
}

void GScr_IsPregameEnabled()
{
    if ( Pregame_isEnabled() )
        Scr_AddBool(1u, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddBool(0, SCRIPTINSTANCE_SERVER);
}

void GScr_ResetPregameData()
{
    Pregame_ResetData();
}

void GScr_IsPregameGameStarted()
{
    if ( Pregame_isEnabled() && Pregame_GetState() == PREGAME_GAMESTARTED )
        Scr_AddBool(1u, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}

void GScr_PregameStartGame()
{
    Pregame_StartGame();
}

void print_0()
{
    const dvar_s *result; // eax
    char *DebugString; // eax
    int num; // [esp+0h] [ebp-8h]
    const dvar_s *i; // [esp+4h] [ebp-4h]

    if (!g_NoScriptSpam->current.enabled)
    {
        num = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
        for (i = 0; ; i = (const dvar_s *)((char *)i + 1))
        {
            result = i;
            if ((int)i >= num)
                break;
            DebugString = Scr_GetDebugString((unsigned int)i, SCRIPTINSTANCE_SERVER);
            Com_Printf(level.scriptPrintChannel, "%s", DebugString);
        }
    }
}

void println_0()
{
    if (!g_NoScriptSpam->current.enabled)
    {
        print_0();
        Com_Printf(level.scriptPrintChannel, "\n");
    }
}

void FUNCTION_NULLSUB()
{

}

void assertCmd_0()
{
    if (!Scr_GetInt(0, SCRIPTINSTANCE_SERVER))
        Scr_Error("assert fail", 0);
}

void assertexCmd_0()
{
    char *String; // eax
    char *v2; // eax

    if (!Scr_GetInt(0, SCRIPTINSTANCE_SERVER))
    {
        String = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
        v2 = va("assert fail: %s", String);
        Scr_Error(v2, 0);
    }
}

void assertmsgCmd_0()
{
    char *String; // eax
    char *v2; // eax

    String = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    v2 = va("assert fail: %s", String);
    Scr_Error(v2, 0);
}

void GScr_SetPlayerStatsForMatchRecording()
{
    DWORD result; // eax
    char *statName; // [esp+8h] [ebp-10h]
    unsigned int statValue; // [esp+Ch] [ebp-Ch]
    gentity_s *ent; // [esp+14h] [ebp-4h]

    PROF_SCOPED("GScr_SetPlayerStatsForMatchRecording");

    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3)
        Scr_ParamError(0, "recordPlayerStats [player] [statName] [value]", SCRIPTINSTANCE_SERVER);
    ent = Scr_GetEntity(0);
    if (!ent)
        Scr_ParamError(0, "recordPlayerStats Error: param 0 is not an entity.", SCRIPTINSTANCE_SERVER);
    if (!ent->client)
        Scr_ParamError(0, "recordPlayerStats Error: param 0 is not an player.", SCRIPTINSTANCE_SERVER);
    statName = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    if (!statName)
        Scr_ParamError(1u, "recordPlayerStats Error: param 1 is not a string.", SCRIPTINSTANCE_SERVER);
    statValue = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);

#ifdef KISAK_LIVE
    MatchRecordSetPlayerStat(ent->client, statName, statValue);
#endif
}

#ifdef KISAK_SP
// Pulled in only for the SP builtin bodies below (GScr_IsAssetLoaded_SP needs
// DB_FindXAssetEntry + the DB hash critical section). Deliberately inside the
// KISAK_SP guard so the MP translation unit is not perturbed at all.
#include <database/db_registry.h>
#include <win32/win_common.h>

// ===========================================================================
// TODO(SP-STUB) -- deliberate no-op stubs for retail-SP script builtins.
//
// WHAT THESE ARE: placeholders, NOT implementations. Every one of them does
// nothing at all except print a one-time warning naming itself. They exist
// solely so that the SP script set LINKS: LinkThread (cscr_compiler.cpp:271)
// raises a fatal "unknown function" CompileError on the first builtin name a
// compiled script references that Scr_GetFunction / Scr_GetMethod cannot
// resolve, so one missing name stops the whole SP boot dead. Registering the
// names lets the boot continue far enough to find out what is actually
// executed, and the one-shot warnings are the evidence that should drive which
// of these get real bodies first. Grep TODO(SP-STUB) to enumerate them all.
//
// WHY AN EMPTY BODY IS STACK-SAFE (verified, not assumed): the VM cleans up
// after a builtin itself. cscr_vm.cpp:5588-5599 pops and RemoveRefToValue()s
// any parameters the builtin did not consume (outparamcount is still set), and
// cscr_vm.cpp:5606-5611 pushes a fresh slot of type 0 == VAR_UNDEFINED
// (cscr_variable.h:19) whenever the builtin pushed no return value
// (inparamcount == 0). So a builtin that touches nothing leaves a correct
// stack and evaluates to undefined in value position -- which is what we want
// here: honest and visible, rather than a guessed value that would look like it
// worked. That is also why the existing FUNCTION_NULLSUB / METHOD_NULLSUB are
// safe; we still use one distinct symbol per name so each is individually
// identifiable in the console log and in a stack trace.
//
// THE type FIELD IS 0 ON EVERY ROW. 0 means "not a developer command". A 1
// there makes any use of the builtin in value position outside a /# ... #/
// block a hard CompileError (cscr_compiler.cpp:2844), so 0 is the permissive
// choice that cannot reintroduce the failure this pass is fixing. It is NOT a
// claim that retail SP marks each of these 0 (though all 10 rows re-read from
// the retail tables during this pass did carry 0).
//
// THE ADDRESSES in the row comments are the retail SP handler for that name,
// taken from a scan of SP's two builtin tables (server methods at 0x00A54218,
// server functions in the 0x00B76xxx block). 10 of the 92 were re-verified
// row-by-row against the binary during this pass and matched exactly; the
// other 82 were NOT re-verified here and should be sanity-checked before
// anyone decompiles from them.
// ===========================================================================

// Channel 24 == "parserscript" (con_channels.cpp:11, builtinChannels[24]) --
// the channel every other Com_PrintWarning in this file already uses, including
// GScr_SetTurretAccuracy's "no longer has any effect" warning, which is the
// same kind of message as this one.
static void GScr_SPStub_ReportOnce(const char *name, const char *spHandler, const char *retDesc, bool *pReported)
{
    if ( *pReported )
        return;
    *pReported = true;
    Com_PrintWarning(
        24,
        "WARNING: TODO(SP-STUB) script builtin '%s' (retail SP handler %s) was called "
        "but is an unimplemented stub: it does no work at all and evaluates to %s. "
        "This warning prints once per builtin name.\n",
        name,
        spHandler,
        retDesc);
}

// ===========================================================================
// TODO(SP-STUB) RETURN-SHAPE MACROS -- read this before adding a row.
//
// WHY THE PLAIN (undefined-returning) MACROS ARE NOT ALWAYS ENOUGH. A builtin
// that pushes nothing evaluates to VAR_UNDEFINED (cscr_vm.cpp:5606-5611), and
// the VM makes reading undefined in three very common positions a FATAL script
// error, not a soft failure:
//   * `x.size`  -> Scr_EvalSizeValue, cscr_variable.cpp:4249-4252,
//                  "size cannot be applied to undefined"
//   * `x[i]`    -> Scr_EvalArray,     cscr_variable.cpp:6048-6062,
//                  "undefined is not an array, string, or vector"
//   * `if (x)`  -> Scr_CastBool,      cscr_variable.cpp:4522-4529,
//                  "cannot cast undefined to bool"
// Those three killed the SP `frontend` boot (getaiarray/getspawnerarray read
// with .size from maps/_vehicle::setup_ai). The macros below fix the RETURN
// SHAPE only. They are still stubs: none of them does any work, and the value
// each returns is the value retail would produce for the empty/idle case, not
// a computed answer.
//
// THE STACK CONTRACT, verified rather than assumed: Scr_MakeArray /
// Scr_AddInt / Scr_AddFloat / Scr_AddString / Scr_AddVector all route through
// IncInParam (cscr_vm.cpp:3294-3305), which calls Scr_ClearOutParams to drop
// the builtin's arguments and then sets inparamcount = 1. The post-builtin
// epilogue at cscr_vm.cpp:5588-5611 therefore takes its `if (inparamcount)`
// branch and does NOT additionally push an undefined, so exactly one value is
// left on the stack -- the same path every already-working returning builtin
// in this file uses (e.g. GScr_GetWatcherWeapons above, which likewise calls
// Scr_MakeArray and may then add zero elements).
//
// AN EMPTY ARRAY IS A WELL-FORMED VALUE, not a special case: Scr_AllocArray
// (cscr_variable.cpp:2460-2487) sets the array's element count field
// (u.o.u.entnum) to 0, and Scr_EvalSizeValue reads exactly that field, so
// `arr.size` is 0 and `for (i = 0; i < arr.size; i++)` simply does not run --
// which is what retail does on a map with no AI, no spawners and no dynents.
//
// PER-NAME REASONING for every non-array value lives on that name's own row
// below. Where no value could be justified, the row deliberately still uses
// the plain undefined-returning macro and says so.
// ===========================================================================

#define SP_STUB_FUNCTION(symbol, gscName, spHandler)                             \
    static void __cdecl symbol()                                                 \
    {                                                                            \
        static bool s_reported = false;                                          \
        GScr_SPStub_ReportOnce(gscName, spHandler, "undefined", &s_reported);     \
    }

#define SP_STUB_METHOD(symbol, gscName, spHandler)                               \
    static void __cdecl symbol(scr_entref_t)                                     \
    {                                                                            \
        static bool s_reported = false;                                          \
        GScr_SPStub_ReportOnce(gscName, spHandler, "undefined", &s_reported);     \
    }

// Returns an EMPTY array. Still a stub -- it never enumerates anything.
#define SP_STUB_FUNCTION_ARRAY(symbol, gscName, spHandler)                       \
    static void __cdecl symbol()                                                 \
    {                                                                            \
        static bool s_reported = false;                                          \
        GScr_SPStub_ReportOnce(gscName, spHandler, "an EMPTY array", &s_reported);\
        Scr_MakeArray(SCRIPTINSTANCE_SERVER);                                    \
    }

// Returns a fixed integer. Still a stub -- nothing is measured or queried.
#define SP_STUB_FUNCTION_INT(symbol, gscName, spHandler, value)                  \
    static void __cdecl symbol()                                                 \
    {                                                                            \
        static bool s_reported = false;                                          \
        GScr_SPStub_ReportOnce(gscName, spHandler, "the fixed integer "           \
                               #value, &s_reported);                             \
        Scr_AddInt((value), SCRIPTINSTANCE_SERVER);                              \
    }

#define SP_STUB_METHOD_INT(symbol, gscName, spHandler, value)                    \
    static void __cdecl symbol(scr_entref_t)                                     \
    {                                                                            \
        static bool s_reported = false;                                          \
        GScr_SPStub_ReportOnce(gscName, spHandler, "the fixed integer "           \
                               #value, &s_reported);                             \
        Scr_AddInt((value), SCRIPTINSTANCE_SERVER);                              \
    }

// Returns a fixed string. Still a stub -- nothing is looked up.
#define SP_STUB_FUNCTION_STRING(symbol, gscName, spHandler, value)               \
    static void __cdecl symbol()                                                 \
    {                                                                            \
        static bool s_reported = false;                                          \
        GScr_SPStub_ReportOnce(gscName, spHandler, "the fixed string \"" value    \
                               "\"", &s_reported);                               \
        Scr_AddString((value), SCRIPTINSTANCE_SERVER);                           \
    }

#define SP_STUB_METHOD_STRING(symbol, gscName, spHandler, value)                 \
    static void __cdecl symbol(scr_entref_t)                                     \
    {                                                                            \
        static bool s_reported = false;                                          \
        GScr_SPStub_ReportOnce(gscName, spHandler, "the fixed string \"" value    \
                               "\"", &s_reported);                               \
        Scr_AddString((value), SCRIPTINSTANCE_SERVER);                           \
    }

// Returns the zero vector. Still a stub -- nothing is sampled.
#define SP_STUB_METHOD_ZEROVEC(symbol, gscName, spHandler)                       \
    static void __cdecl symbol(scr_entref_t)                                     \
    {                                                                            \
        static bool s_reported = false;                                          \
        float zero[3] = { 0.0f, 0.0f, 0.0f };                                    \
        GScr_SPStub_ReportOnce(gscName, spHandler, "the zero vector (0,0,0)",     \
                               &s_reported);                                     \
        Scr_AddVector(zero, SCRIPTINSTANCE_SERVER);                              \
    }

// --- real SP builtin implementations ---------------------------------------

void GScr_GetPlayers_SP()
{
    int teamNum = -1;
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
    {
        const unsigned __int16 team = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
        if (team == scr_const.allies)
            teamNum = 2;
        else if (team == scr_const.axis)
            teamNum = 1;
        else if (team == scr_const.neutral)
            teamNum = 3;
        else if (team == scr_const.spectator)
            teamNum = 4;
        else if (team != scr_const.all)
        {
            Scr_Error(va("Unknown team %s passed to getplayers()",
                         Scr_GetString(0, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);
            return;
        }
    }

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for (int i = 0; i < com_maxclients->current.integer; ++i)
    {
        gentity_s *ent = &g_entities[i];
        if (!ent->client || ent->client->sess.connected != CON_CONNECTED)
            continue;
        if (teamNum >= 0 && ent->client->sess.cs.team != teamNum)
            continue;
        Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}

static void __cdecl GScr_GetNumConnectedPlayers_SP()
{
    int count = 0;
    for (int i = 0; i < com_maxclients->current.integer; ++i)
    {
        const clientHeader_t *header = &svs.clients[i].header;
        if (header->state == CS_ACTIVE && header->connectionState_SP == CA_ACTIVE)
            ++count;
    }
    Scr_AddInt(count, SCRIPTINSTANCE_SERVER);
}

// getnumexpectedplayers() -- retail SP 0x005e6b20.
//
// During a running non-menu local server retail returns at least one even
// before the local client reaches CA_ACTIVE. That deliberate expected=1 /
// connected=0 mismatch makes maps/_callbackglobal.gsc wait for the client.
// The old paired zero stubs made the equality test succeed early and skipped
// the player-spawn synchronization. Retail's fallback counts server clients
// past their initial connection states; CS_CONNECTED is the semantic boundary
// after accounting for SP's one-step-smaller SignonState enum.
static void __cdecl GScr_GetNumExpectedPlayers_SP()
{
    int count = 0;

    if (IsDedicatedServer())
    {
        for (int i = 0; i < com_maxclients->current.integer; ++i)
            if (svs.clients[i].header.state >= CS_CONNECTED)
                ++count;
        // Keep the script's wait-for-players barrier closed until somebody joins.
        Scr_AddInt(count > 0 ? count : 1, SCRIPTINSTANCE_SERVER);
        return;
    }

    if ( !Com_IsMenuLevel(0) && com_sv_running &&
         com_sv_running->current.enabled )
    {
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
        count = CL_SplitGuestExpectedPlayers();
#else
        count = 1;
#endif
#if defined(OPENBLOPS_NO_STEAM_AUTH)
        // A full Zombies restart keeps the remote connections while their
        // clients reload. Include them in the next map's startup barrier.
        if (zombiemode && zombiemode->current.enabled && Dvar_GetBool("systemlink"))
        {
            for (int i = 0; i < com_maxclients->current.integer; ++i)
            {
                const clientHeader_t &header = svs.clients[i].header;
                if (header.state >= CS_CONNECTED && !NET_IsLocalAddress(header.netchan.remoteAddress))
                    ++count;
            }
        }
#endif
    }
    else
    {
        for ( int i = 0; i < com_maxclients->current.integer; ++i )
        {
            if ( svs.clients[i].header.state >= CS_CONNECTED )
                ++count;
        }
    }

    Scr_AddInt(count, SCRIPTINSTANCE_SERVER);
}

// ---------------------------------------------------------------------------
// getdifficulty() -- SP only. REAL BODY (this row used to be an undefined-
// returning TODO(SP-STUB)).
//
// EVIDENCE, all decompiled/read from BlackOps.exe this session:
//   * retail SP handler 0x007f07e0 is one statement:
//         Scr_AddString( nameTable[ g_gameskill->current.integer ], 0 )
//     -- the dvar pointer is the global at 0x02899cdc and +0x18 is dvar_s::current
//     (dvar_s in this tree: name 0x0, description 0x4, hash 0x8, flags 0xC,
//     type 0x10, current 0x18 -- so +0x18 == current.integer, matching).
//   * nameTable is the pointer array at 0x00b75ecc. Its four entries were read
//     out of the image byte-for-byte: 0x00a21a9c "easy", 0x00a3d5e8 "medium",
//     0x009debc4 "hard", 0x009c7324 "fu".
//   * the dvar is registered by SP's SV_Init (0x00698260, first registration in
//     the function) as Dvar_RegisterInt("g_gameskill", 1, 0, 3, 0x1064, "").
//     The 0..3 domain is exactly the table's length, so retail can never index
//     out of range and needs no bounds check -- and it has none.
//
// This replaces an `undefined` return, which was fatal in three positions
// (see the RETURN-SHAPE header above) -- e.g. maps/_vehicledrive.gsc:213
// switches on the result, and Scr_CastBool/switch on undefined throws.
static void __cdecl GScr_GetDifficulty_SP()
{
    // Same order as the retail table at 0x00b75ecc; index is g_gameskill.
    static const char *const s_difficultyNames[4] = { "easy", "medium", "hard", "fu" };
    Scr_AddString(s_difficultyNames[Dvar_GetInt("g_gameskill")], SCRIPTINSTANCE_SERVER);
}

// ---------------------------------------------------------------------------
// isassetloaded( <assetTypeName>, <assetName> ) -- SP only. REAL BODY (this row
// used to be a fixed-0 TODO(SP-STUB)).
//
// EVIDENCE: retail SP handler 0x007faad0 reads both parameters as const strings
// and tail-calls 0x00694550, which:
//   * linearly I_stricmp's the type name against the pointer table at
//     0x00b73bb0 with loop bound 0x2B == 43 -- the exact length of this tree's
//     g_assetNames[43] (db_assetnames.h), which is also what its own error text
//     names: "type %s is not a valid asset type: see g_assetNames in
//     db_assetnames.h\n", raised with Com_Error(4, ...). errorParm_t here has
//     ERR_SCRIPT == 4, so ERR_SCRIPT is the faithful level, not ERR_DROP.
//   * then, inside Sys_EnterCriticalSection(0x46) / Sys_LeaveCriticalSection,
//     calls 0x007a2a20 and returns (result != 0). 0x007a2a20 was decompiled: it
//     hashes the name, walks a 0x10-byte-stride pool bucket chain comparing the
//     entry's type word against the type and I_stricmp'ing the name -- i.e. it
//     is DB_FindXAssetEntry(type, name), which this tree has at
//     db_registry.cpp:1781.
// The critical-section ORDINAL differs between the two builds (retail SP 0x46,
// this tree's CRITSECT_DBHASH == 0x4A); the named constant is the port, since
// db_registry.cpp's own DB_FindXAssetEntry call sites take CRITSECT_DBHASH.
static void __cdecl GScr_IsAssetLoaded_SP()
{
    const char *typeName; // [esp+8h] [ebp-Ch]
    const char *assetName; // [esp+4h] [ebp-8h]
    int type; // [esp+0h] [ebp-4h]
    bool loaded;

    typeName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    assetName = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    for ( type = 0; type < 43; ++type )
    {
        if ( !I_stricmp(typeName, g_assetNames[type]) )
            break;
    }
    if ( type == 43 )
        Com_Error(ERR_SCRIPT, "type %s is not a valid asset type: see g_assetNames in db_assetnames.h\n", typeName);
    Sys_EnterCriticalSection(CRITSECT_DBHASH);
    loaded = DB_FindXAssetEntry((XAssetType)type, assetName) != 0;
    Sys_LeaveCriticalSection(CRITSECT_DBHASH);
    Scr_AddInt(loaded, SCRIPTINSTANCE_SERVER);
}

// ---------------------------------------------------------------------------
// setsaveddvar( <dvarName>, <value> ) -- SP only. REAL BODY (this row used to be
// a no-op TODO(SP-STUB)).
//
// EVIDENCE: retail SP handler 0x007f06a0, decompiled this session. It is the
// server-side twin of CScr_SetSavedDvar (0x007850a0, already named in the Ghidra
// project); the plate on 0x007f06a0 records the disambiguation. Structure:
//   name  = Scr_GetString(0)
//   value = (Scr_GetType(1) == 3 /* VAR_ISTRING */)
//             ? Scr_ConstructMessageString(1, numParam-1, "Dvar Value", buf, 1024)
//             : Scr_GetString(1)
//   copy value into a 1024-byte scratch, mapping '"' -> '\''
//   if (!Dvar_IsValidName(name))          Scr_Error(va("Dvar %s has an invalid dvar name", name))
//   else if (!(dvar = Dvar_FindVar(name))) Scr_Error(va("SetSavedDvar(): The dvar \"%s\" does not exist.", name))
//   else if (!(dvar->flags & 0x1000))      Scr_Error("SetSavedDvar can only be called on dvars with the SAVED flag set")
//   else Dvar_SetFromStringByNameFromSource(name, buf, 2, 0)
// dvar_s::flags is at +0xC in this tree, matching the decompile's
// `*(uint *)(dvar + 0xc) & 0x1000`; 0x1000 is the flag this tree's own
// Com_DvarDump prints as "V" (dvar_cmds.cpp:582) and is exactly the bit
// g_gameskill carries (0x1064). Source 2 == DVAR_SOURCE_SCRIPT (dvar.h:7).
//
// The copy is bounded to the real 1024-byte destination rather than reproducing
// retail's erroneous 0x4000 loop bound. All observable validation behavior is
// otherwise preserved; the SP dvar owners register the required saved dvars.
static void __cdecl GScr_SetSavedDvar_SP()
{
    const char *v0; // eax
    int numParam; // eax
    char messageString[1024]; // BYREF
    char cleaned[1024]; // BYREF
    const char *dvarName;
    const char *dvarValue;
    const dvar_s *dvar;
    int i;

    dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( Scr_GetType(1u, SCRIPTINSTANCE_SERVER) == 3 )
    {
        numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
        Scr_ConstructMessageString(1, numParam - 1, "Dvar Value", messageString, 0x400u);
        dvarValue = messageString;
    }
    else
    {
        dvarValue = Scr_GetString(1u, SCRIPTINSTANCE_SERVER);
    }

    memset(cleaned, 0, sizeof(cleaned));
    for ( i = 0; i < (int)sizeof(cleaned) - 1 && dvarValue[i]; ++i )
    {
        cleaned[i] = dvarValue[i];
        if ( cleaned[i] == '"' )
            cleaned[i] = '\'';
    }

    if ( !Dvar_IsValidName(dvarName) )
    {
        v0 = va("Dvar %s has an invalid dvar name", dvarName);
        Scr_Error(v0, 0);
        return;
    }
    dvar = Dvar_FindVar(dvarName);
    if ( !dvar )
    {
        v0 = va("SetSavedDvar(): The dvar \"%s\" does not exist.", dvarName);
        Scr_Error(v0, 0);
        return;
    }
    if ( (dvar->flags & 0x1000) == 0 )
    {
        Scr_Error("SetSavedDvar can only be called on dvars with the SAVED flag set", 0);
        return;
    }
    Dvar_SetFromStringByNameFromSource(dvarName, cleaned, DVAR_SOURCE_SCRIPT, 0);
}

// ---------------------------------------------------------------------------
// watersimenable( <bool> ) -- SP only. REAL BODY (this row used to be a no-op
// TODO(SP-STUB)).
//
// EVIDENCE: retail SP handler 0x007fa500 is three statements and was decompiled
// verbatim this session:
//   if (Scr_GetNumParam() != 1) Scr_Error("watersimenable() called with wrong params.\n");
//   Dvar_SetBoolByName("r_watersim_enabled", Scr_GetInt(0) != 0);
// The dvar exists in this reconstruction with that exact name, registered by
// r_water_sim.cpp:264 and read at r_water_sim.cpp:576/711/2121, so this body is
// fully wired up here rather than being a write into the void.
static void __cdecl GScr_WaterSimEnable_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("watersimenable() called with wrong params.\n", 0);
    Dvar_SetBoolByName("r_watersim_enabled", Scr_GetInt(0, SCRIPTINSTANCE_SERVER) != 0);
}

// ---------------------------------------------------------------------------
// start3dcinematic(<name>[, <looping>[, <inmemory>[, <startPaused>]]]) /
// stop3dcinematic() / pause3dcinematic(<bool>) -- SP only. REAL BODIES (these
// rows used to be no-op TODO(SP-STUB)s). Root cause of "bink video doesn't
// work": maps/frontend.gsc:597 calls Start3DCinematic("frontend",1,1) as
// literally the first thing that happens once a player is active in the SP
// main menu, and the stub silently dropped it.
//
// EVIDENCE (Ghidra-researched 2026-08-22): retail SP handlers are thin GSC
// wrappers, not a separate "3D" rendering subsystem:
//   - start3dcinematic  0x007fbd50: builds a playbackFlags byte and broadcasts
//     "%c %s %i" (0x3c='<', name, flags) via SV_GameSendServerCommand(-1, ...).
//   - stop3dcinematic   0x007fbf40: broadcasts "%c" (0x2e='.').
//   - pause3dcinematic  0x007fbef0: broadcasts "%c %i" (0x3b=';', pauseBool).
// The client's CG_DeployServerCommand (0x0078daf0, cases '<'/'.'/';') consumes
// these and drives the SAME single-instance fullscreen-Bink pipeline this
// reconstruction already has (cinematicGlob / R_Cinematic_* in r_cinematic.cpp)
// -- confirmed by decompiling the client-side handlers themselves:
//   case '.' -> R_Cinematic_StopPlayback(); Dvar_SetBool(cg_cinematicFullscreen, true);
//   case ';' -> atoi(Cmd_Argv(1))==0 ? resume(fromScript=0) : pause(fromScript=1)
//   case '<' -> R_Cinematic_StartPlayback_Internal(Cmd_Argv(1), atoi(Cmd_Argv(2)), 0);
//               Dvar_SetBool(cg_cinematicFullscreen, false);
// (the "3D" in the name just means "displayed via a material's cinematicSampler
// on in-world geometry", e.g. maps/frontend.gsc's TV-monitor shader constants --
// not a second concurrent-slot engine subsystem. See ORCHESTRATOR.md/
// SP_MAIN_MENU_BOOTCHAIN.md for the full research trail.)
//
// This reconstruction is a fully-integrated SP client+server process (SP has
// no dedicated-server mode), so the network round-trip is collapsed to a
// direct broadcast the local client always receives -- the client-side cases
// are added to cg_servercmds_mp.cpp's CG_DeployServerCommand switch exactly
// like the existing "openmainmenu"/case 0x62 precedent.
//
// start3dcinematic's exact flag computation (decompiled from 0x007fbd50):
// base flags = 0x42 (loop bit 0x02 + "useCustomSkipLogic" bit 0x40, both ON by
// default); numParam==1 leaves them as-is; numParam>=2 reads isLooping
// (Scr_GetInt(1)) and clears the loop bit if it's explicitly false; numParam>=3
// reads isInMemory (Scr_GetInt(2)) and ORs in bit 0x08 (R_Cinematic_BinkOpenPath
// already branches on this bit to load via DB_FindXAssetHeader instead of the
// filesystem); numParam==4 reads an undocumented start-paused bool
// (Scr_GetInt(3)) that, if true, hard-overwrites flags to 0x42|0x80 *before*
// the isLooping/isInMemory logic re-applies (retail's switch falls through
// case 4 -> 3 -> 2 in that order, so this ordering is exact, not approximated).
// Finally, if <name> case-insensitively matches one of a fixed retail mission
// whitelist, bit 0x01 is OR'd in (pairs with bit 0x40 to select
// "useCustomSkipLogic" timing in R_Cinematic_Advance -- neither of our SP
// main-menu call sites match this list, so it's inert for the menu boot chain,
// but is ported for fidelity since other maps may rely on it).
static const char *const g_start3DCinematicWhitelist[] = {
    "mid_cuba_1", "mid_cuba_3", "mid_vorkuta_2", "mid_vorkuta_3",
    "mid_flashpoint_1", "mid_flashpoint_2", "mid_hue_city_2", "mid_river_1",
    "wmd_load", "mid_rebirth_2", "int_hudson_explains",
    "int_reznov_disappearing_flashback",
};

static void __cdecl GScr_Start3DCinematic_SP()
{
    int numParam;
    unsigned int flags;
    const char *name;
    unsigned int i;

    numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    flags = 0x42u;
    switch ( numParam )
    {
        case 1:
            break;
        case 4:
            if ( Scr_GetInt(3, SCRIPTINSTANCE_SERVER) )
                flags = 0xC2u;
            // fallthrough
        case 3:
            if ( Scr_GetInt(2, SCRIPTINSTANCE_SERVER) )
                flags |= 8u;
            // fallthrough
        case 2:
            if ( !Scr_GetInt(1, SCRIPTINSTANCE_SERVER) )
                flags &= ~2u;
            break;
        default:
            Scr_Error("start3DCinematic takes one, two, or three parameters: start3DCinematic(<cinematic name>, <looping>, <inmemory>)", 0);
            return;
    }
    name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    for ( i = 0; i < ARRAY_COUNT(g_start3DCinematicWhitelist); ++i )
    {
        if ( !I_stricmp(name, g_start3DCinematicWhitelist[i]) )
        {
            flags |= 1u;
            break;
        }
    }
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c %s %i", '<', name, flags));
}

// Manual local adapter name, not a recovered retail symbol. Retail 0x00805340
// broadcasts '>' without reading script arguments; registration 0x00B77308.
static void __cdecl GScr_CleanupSpawnedDynEnts_SP()
{
    SV_SendServerCommand(0, SV_CMD_RELIABLE, "%c", '>');
}

static void __cdecl GScr_Stop3DCinematic_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 0 )
        Scr_Error("stop3DCinematic takes no parameters: stop3DCinematic()", 0);
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c", '.'));
}

static void __cdecl GScr_Pause3DCinematic_SP()
{
    int pause;

    pause = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c %i", ';', pause));
}

// Retail SP 0x007f0840 converts the script team strings to the bit flags used
// by Actor_FirstActor/Actor_NextActor.  TEAM_SPECTATOR is this reconstruction's
// semantic name for retail SP's neutral team value (3).
static int __cdecl GScr_GetAITeamFlag_SP(const char *team, const char *functionName)
{
    if ( !I_stricmp(team, "axis") )
        return 1 << TEAM_AXIS;
    if ( !I_stricmp(team, "allies") )
        return 1 << TEAM_ALLIES;
    if ( !I_stricmp(team, "neutral") )
        return 1 << TEAM_SPECTATOR;
    if ( !I_stricmp(team, "all") )
        return (1 << TEAM_AXIS) | (1 << TEAM_ALLIES) | (1 << TEAM_SPECTATOR);

    Scr_Error(
        va("unknown team '%s' in %s (should be axis, allies, or neutral)", team, functionName),
        SCRIPTINSTANCE_SERVER);
    return 0;
}

static int __cdecl GScr_GetAITeamFlags_SP(const char *functionName)
{
    const unsigned int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    int teamFlags = 0;

    for ( unsigned int i = 0; i < count; ++i )
        teamFlags |= GScr_GetAITeamFlag_SP(Scr_GetString(i, SCRIPTINSTANCE_SERVER), functionName);

    if ( !teamFlags )
        teamFlags = (1 << TEAM_AXIS) | (1 << TEAM_ALLIES) | (1 << TEAM_SPECTATOR);
    return teamFlags;
}

// Retail helper 0x005886b0 returns true for the two dog species (1 and 3).
static bool __cdecl GScr_IsDogSpecies_SP(AISpecies species)
{
    return species == AI_SPECIES_DOG || species == AI_SPECIES_ZOMBIE_DOG;
}

// Retail SP 0x007f0970. The old placeholder returned an empty array, so zombie
// scripts could not discover live actors for barrier, goal or cleanup work.
static void __cdecl GScr_GetAIArray_SP()
{
    const int teamFlags = GScr_GetAITeamFlags_SP("getaiarray");

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for ( actor_s *actor = Actor_FirstActor(teamFlags); actor; actor = Actor_NextActor(actor, teamFlags) )
    {
        if ( !actor->Physics.bIsAlive || actor->delayedDeath || GScr_IsDogSpecies_SP(actor->species) )
            continue;

        Scr_AddEntity(actor->ent, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}
// issentient(<value>) -- exact retail SP 0x007f00a0 body.  The former fixed
// false stub was observably wrong for players and actors and fired immediately
// before the frontend's late idle-camera branch.
static void __cdecl GScr_SPStubFn_issentient()
{
    int result = 0;
    gentity_s *ent = NULL;
    const int type = Scr_GetType(0, SCRIPTINSTANCE_SERVER);
    int pointerType = -1;
    if ( type == 1 )
    {
        pointerType = Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER);
        if ( pointerType == 19 )
        {
            ent = Scr_GetEntity(0);
            result = ent->sentient != NULL;
        }
    }
    Scr_AddInt(result, SCRIPTINSTANCE_SERVER);
}
    // getplayers: NO LONGER A STUB. The empty-array stub that used to sit here
    // deadlocked the whole SP script layer (see the deadlock note above the real
    // implementation, GScr_GetPlayers_SP, earlier in this file).
// ---------------------------------------------------------------------------
static void GScr_SetSunLight_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3 && Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 6)
        Scr_Error("Incorrect number of parameters\n", SCRIPTINSTANCE_SERVER);
    float diffuse[3];
    float specular[3];
    for (int i = 0; i < 3; ++i)
        diffuse[i] = Scr_GetFloat(i, SCRIPTINSTANCE_SERVER);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 6)
    {
        for (int i = 0; i < 3; ++i)
            specular[i] = Scr_GetFloat(i + 3, SCRIPTINSTANCE_SERVER);
    }
    else
        Vec3Copy(diffuse, specular);
    SV_SetConfigstring(CS_SUNLIGHT, va("%g %g %g %g %g %g",
        diffuse[0], diffuse[1], diffuse[2], specular[0], specular[1], specular[2]));
}

static void GScr_ResetSunLight_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
        Scr_Error("Incorrect number of parameters\n", SCRIPTINSTANCE_SERVER);
    SV_SetConfigstring(CS_SUNLIGHT, "");
}

static void GScr_NormalizeSunDirection_SP(float *direction)
{
    float length = std::sqrt((direction[0] * direction[0] + direction[1] * direction[1])
        + direction[2] * direction[2]);
    if (-length >= 0.0f)
        length = 1.0f;
    const float scale = 1.0f / length;
    for (int i = 0; i < 3; ++i)
        direction[i] *= scale;
}

static void GScr_GetMapSunDirection_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
        Scr_Error("Incorrect number of parameters\n", SCRIPTINSTANCE_SERVER);
    Scr_AddVector(level.mapSunDirection, SCRIPTINSTANCE_SERVER);
}

static void GScr_SetSunDirection_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1)
        Scr_Error("Incorrect number of parameters\n", SCRIPTINSTANCE_SERVER);
    float direction[3];
    Scr_GetVector(0, direction, SCRIPTINSTANCE_SERVER);
    GScr_NormalizeSunDirection_SP(direction);
    SV_SetConfigstring(CS_SUNDIR, va("%g %g %g", direction[0], direction[1], direction[2]));
}

static void GScr_LerpSunDirection_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3)
        Scr_Error("Incorrect number of parameters\n", SCRIPTINSTANCE_SERVER);
    float begin[3];
    float end[3];
    Scr_GetVector(0, begin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1, end, SCRIPTINSTANCE_SERVER);
    const float time = Scr_GetFloat(2, SCRIPTINSTANCE_SERVER);
    GScr_NormalizeSunDirection_SP(begin);
    GScr_NormalizeSunDirection_SP(end);
    if (time <= 0.001f)
        Scr_Error("Lerp time must be greater than 1 ms\n", SCRIPTINSTANCE_SERVER);
    const float endTime = time * 1000.0f + (float)level.time;
    const int endMilliseconds = endTime >= -2147483648.0f && endTime < 2147483648.0f ? (int)endTime : INT_MIN;
    SV_SetConfigstring(CS_SUNDIR, va("%g %g %g %g %g %g %d %d",
        begin[0], begin[1], begin[2], end[0], end[1], end[2], level.time, endMilliseconds));
}

static void GScr_ResetSunDirection_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
        Scr_Error("Incorrect number of parameters\n", SCRIPTINSTANCE_SERVER);
    SV_SetConfigstring(CS_SUNDIR, "");
}

static void GScr_WorldToLocalCoords_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    float world[3];
    float transform[4][3];
    float local[3];
    Scr_GetVector(0, world, SCRIPTINSTANCE_SERVER);
    AnglesToAxis(ent->r.currentAngles, transform);
    Vec3Copy(ent->r.currentOrigin, transform[3]);
    MatrixTransposeTransformVector43(world, transform, local);
    Scr_AddVector(local, SCRIPTINSTANCE_SERVER);
}

static void GScr_GetCycleOriginOffset_SP()
{
    float angles[3];
    float axis[3][3];
    float rotation[2];
    float start[3];
    float end[3];
    float delta[3];
    float result[3];

    Scr_GetVector(0, angles, SCRIPTINSTANCE_SERVER);
    const scr_anim_s anim = Scr_GetAnim(1, 0, SCRIPTINSTANCE_SERVER);
    AnglesToAxis(angles, axis);
    const XAnim_s *anims = Scr_GetAnims(anim.tree, SCRIPTINSTANCE_SERVER);
    XAnimGetAbsDelta(anims, anim.index, rotation, start, 0.0f);
    XAnimGetAbsDelta(anims, anim.index, rotation, end, 1.0f);
    Vec3Sub(end, start, delta);
    result[0] = (axis[2][0] * delta[2] + axis[0][0] * delta[0]) + axis[1][0] * delta[1];
    result[1] = (axis[2][1] * delta[2] + axis[0][1] * delta[0]) + axis[1][1] * delta[1];
    result[2] = (axis[2][2] * delta[2] + axis[1][2] * delta[1]) + axis[0][2] * delta[0];
    Scr_AddVector(result, SCRIPTINSTANCE_SERVER);
}

// (stack-slot-verified, not just decompiler heuristics) 2026-08-28.
// - `time` fed to XAnimGetAbsDelta is the hardcoded literal 0.0f (FLDZ).
// - AnglesToAxis writes the first three rows of a 4x3 transform. Retail then
//   copies Scr_GetVector(0)'s authored origin into the fourth row before
//   MatrixTransformVector43. The earlier audit mistook that fourth row for a
//   dead local and incorrectly reduced the result to world zero.
// - The shipped frontend idle-drift path calls this immediately before it
//   links the player to its temporary model. Returning world zero therefore
//   moved that model, and the camera, outside the interrogation room.
void GScr_GetStartOrigin_SP()
{
    float origin[3];
    float angles[3];
    float transform[4][3];
    float rotation[2];
    float translation[3];
    float result[3];

    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, angles, SCRIPTINSTANCE_SERVER);
    const scr_anim_s anim = Scr_GetAnim(2u, 0, SCRIPTINSTANCE_SERVER);
    const XAnim_s *anims = Scr_GetAnims(anim.tree, SCRIPTINSTANCE_SERVER);

    AnglesToAxis(angles, transform);
    transform[3][0] = origin[0];
    transform[3][1] = origin[1];
    transform[3][2] = origin[2];
    XAnimGetAbsDelta(anims, anim.index, rotation, translation, 0.0f);
    MatrixTransformVector43(translation, transform, result);

    Scr_AddVector(result, SCRIPTINSTANCE_SERVER);
}
// Retail SP 0x007f0b20. Unlike getaiarray, this accepts at most one team string
// followed by a species string and includes dog species when explicitly asked.
static void __cdecl GScr_GetAISpeciesArray_SP()
{
    const unsigned int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    int teamFlags = (1 << TEAM_AXIS) | (1 << TEAM_ALLIES) | (1 << TEAM_SPECTATOR);
    AISpecies species = AI_SPECIES_ALL;

    if ( count )
        teamFlags = GScr_GetAITeamFlag_SP(Scr_GetString(0, SCRIPTINSTANCE_SERVER), "getaiarray");

    if ( count >= 2 )
    {
        const unsigned __int16 speciesName = Scr_GetConstString(1, SCRIPTINSTANCE_SERVER);
        if ( speciesName != scr_const.all )
        {
            species = AI_SPECIES_FIRST;
            for ( ; species < MAX_AI_SPECIES; ++species )
            {
                if ( speciesName == *g_AISpeciesNames[species] )
                    break;
            }

            if ( species == MAX_AI_SPECIES )
            {
                Scr_ParamError(
                    1,
                    va(
                        "unknown species '%s' (should be human, dog, or all)",
                        SL_ConvertToString(speciesName, SCRIPTINSTANCE_SERVER)),
                    SCRIPTINSTANCE_SERVER);
                return;
            }
        }
    }

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for ( actor_s *actor = Actor_FirstActor(teamFlags); actor; actor = Actor_NextActor(actor, teamFlags) )
    {
        if ( !actor->Physics.bIsAlive || actor->delayedDeath )
            continue;
        if ( species != AI_SPECIES_ALL && actor->species != species )
            continue;

        Scr_AddEntity(actor->ent, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}
// ---------------------------------------------------------------------------
// WeaponFightDist( <weapon> ) / WeaponMaxDist( <weapon> ) -- SP only. REAL BODIES
// (both rows used to be undefined-returning TODO(SP-STUB)s).
//
// EVIDENCE: retail SP handlers 0x007fc870 and 0x007fc8c0, disassembled 2026-08-28. They are the
// same four statements and differ only in the member read:
//     name = Scr_GetString(0, 0);
//     w    = BG_GetWeaponIndexForName(name);            // 0x005c25c0
//     if ( !w )
//         Scr_Error("unknown weapon", 0);               // 0x00644900, string at 0x009a682c
//     Scr_AddFloat(BG_GetWeaponDef(w)-><+0x70c | +0x710>, 0);   // 0x00425770, 0x0065e540
//
// THE OFFSET MAPPING IS PROVEN, NOT INFERRED. Retail actor_s/WeaponDef offsets do not transfer
// to this tree in general, so +0x70c/+0x710 were not simply trusted:
//   * a temporary static_assert(offsetof(WeaponDef, fightDist) == 0x70c) and the matching one for
//     maxDist == 0x710 were compiled against this tree's WeaponDef and both passed;
//   * independently, retail's own weapon-def field table names these two: the row pointing at the
//     string "fightDist" (0x009cf640) carries offset 0x7f0 and type 7, and "maxDist" (0x00a0dc1c)
//     carries 0x7f4 and type 7 -- byte-identical to this tree's own rows in
//     bg_weapons_load_obj.cpp ({ "fightDist", 2032, 7 } and { "maxDist", 2036, 7 }).
// The two figures differ because the field table is indexed from the outer parsed struct while
// BG_GetWeaponDef returns the inner WeaponDef; the 0xE4 delta is consistent across both fields.
// The static_asserts are kept below so the coupling breaks loudly if WeaponDef is ever reordered.
//
// WHY THESE TWO MATTER MORE THAN THEIR CALL COUNT SUGGESTS. Two GSC references each, both in
// animscripts/init.gsc's SetWeaponDist -- but SetWeaponDist multiplies what they return:
//     primaryweapon_fightdist_min = WeaponFightDist(self.primaryweapon);
//     self.primaryweapon_fightdist_minSq = primaryweapon_fightdist_min * primaryweapon_fightdist_min;
// so an undefined return is not a soft miss, it is
//     pair 'undefined' and 'undefined' has unmatching types (animscripts/init.gsc:535)
// and it kills the thread. SetWeaponDist is reached from animscripts/shared::placeWeaponOn
// (shared.gsc:94, through call_overloaded_func), which animscripts/init.gsc main() calls at its
// line 186 -- so main() aborted there and every SP actor stopped before its animset and animtree
// setup. That is what made Actor_FinishSpawningAll's precache phase unshippable: with actors
// half-initialised, the first anim they played indexed a tree they did not own and tripped
// `animIndex < anims->size` in xanim. These two builtins are the upstream fix.
static void __cdecl GScr_WeaponFightDist_SP()
{
    const char *name; // [esp+0h] [ebp-8h]
    int weaponIndex; // [esp+4h] [ebp-4h]

    name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = BG_GetWeaponIndexForName(name);
    if ( !weaponIndex )
        Scr_Error("unknown weapon", 0);
    Scr_AddFloat(BG_GetWeaponDef(weaponIndex)->fightDist, SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_WeaponMaxDist_SP()
{
    const char *name; // [esp+0h] [ebp-8h]
    int weaponIndex; // [esp+4h] [ebp-4h]

    name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    weaponIndex = BG_GetWeaponIndexForName(name);
    if ( !weaponIndex )
        Scr_Error("unknown weapon", 0);
    Scr_AddFloat(BG_GetWeaponDef(weaponIndex)->maxDist, SCRIPTINSTANCE_SERVER);
}

#ifndef KISAK_NX // nx-port: x86 layout asserts; the fields are read by name
static_assert(offsetof(WeaponDef, fightDist) == 0x70c,
              "WeaponFightDist reads retail SP WeaponDef +0x70c. If fightDist has moved, this "
              "tree's WeaponDef no longer matches the layout that offset was verified against.");
static_assert(offsetof(WeaponDef, maxDist) == 0x710,
              "WeaponMaxDist reads retail SP WeaponDef +0x710. See the note above.");
#endif

// ---------------------------------------------------------------------------
// getspawnerarray() -- SP only. REAL BODY (this row used to be an empty-array
// TODO(SP-STUB), and the reasoning on that stub was wrong -- see below).
//
// EVIDENCE: retail SP handler 0x007f0be0, decompiled 2026-08-28. The whole body:
//     if ( Scr_GetNumParam(0) )
//         Scr_Error("cannot call getspawnerarray with parameters", 0);
//     Scr_MakeArray(0);
//     for ( i = 0; i < <level.num_entities, DAT_01c0314c>; ++i ) {
//         ent = &g_pLevelGentities[i];                  // stride 0x34c
//         if ( ent-><byte +0xdd> && ent-><short +0xbe> == 0x11 ) {
//             Scr_AddEntity(ent, 0);
//             Scr_AddArray(0);                          // FUN_004f1f00
//         }
//     }
// Offsets are mapped SEMANTICALLY, never arithmetically: +0xdd is the in-use
// flag (r.inuse, the same test GScr_GetPlayers_SP above makes) and +0xbe is
// s.eType. Retail's raw eType value is 0x11 while this tree's enum gives
// ET_ACTOR_SPAWNER == 0x12, because the MP eType enum inserts an extra entry --
// the SYMBOL is what SP_actor_spawner (actor_spawner.cpp:269) actually stores,
// so the symbol is what is compared here. That divergence is already documented
// on GScr_CodeSpawnerSpawn_Common below, which makes the identical comparison.
//
// The argument check must run BEFORE Scr_MakeArray, for the reason spelled out
// on GScr_GetPlayers_SP: Scr_MakeArray calls IncInParam, which calls
// Scr_ClearOutParams and pops the inparams.
//
// WHY THIS STOPPED BEING OPTIONAL. The retired stub's note read "frontend has no
// spawners -> empty". That is false, and measurably so: the frontend map carries
// actor_Hudson_int_silhoutte and actor_Mason_int_escape spawners, whose aitype
// scripts ship in frontend.ff. The empty array quietly disabled
// maps/_load_common.gsc's update_script_forcespawn_based_on_flags(), which is
//     spawners = GetSpawnerArray();
//     for (i = 0; i < spawners.size; i++)
//         if (spawners[i] has_spawnflag(level.SPAWNFLAG_ACTOR_SCRIPTFORCESPAWN))
//             spawners[i].script_forcespawn = 1;
// -- the ONLY writer of script_forcespawn on a map-placed spawner. With the loop
// never running, maps/_utility.gsc's spawn_ai() always took its DoSpawn
// (CHECK_SPAWN) branch instead of StalingradSpawn (FORCE_SPAWN), and SpawnActor
// then refused the spawn outright:
//     couldn't spawn from hudson because player can see spawnpoint (96 16 -127.875)
// which is unavoidable in the frontend, where the player is looking straight at
// the chair. So the interrogator never spawned, and every downstream
// maps/_anim.gsc exception followed from that. The refusal was invisible without
// developer 1 because SpawnActor reports it through Com_DPrintf on channel 18.
static void __cdecl GScr_GetSpawnerArray_SP()
{
    gentity_s *ent; // [esp+0h] [ebp-8h]
    int i; // [esp+4h] [ebp-4h]

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
        Scr_Error("cannot call getspawnerarray with parameters", 0);

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);

    for ( i = 0; i < level.num_entities; ++i )
    {
        ent = &g_entities[i];
        if ( !ent->r.inuse || ent->s.eType != ET_ACTOR_SPAWNER )
            continue;
        Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}

static void __cdecl GScr_GetItemArray_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
        Scr_Error("cannot call getitemarray with parameters", SCRIPTINSTANCE_SERVER);

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for (int i = 0; i < level.num_entities; ++i)
    {
        gentity_s *ent = &level.gentities[i];
        if (!ent->r.inuse || ent->s.eType != ET_ITEM)
            continue;
        Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}
// ---------------------------------------------------------------------------
// getstartangles( <origin>, <angles>, <anim> ) -- SP only. REAL BODY (this
// row used to be a no-op TODO(SP-STUB)).
//
// EVIDENCE: retail SP handler 0x004dcfc0, decompiled AND disassembled
// alongside getstartorigin (same evidence base: hardcoded time=0.0f, dead
// `origin` argument). GetStartAngles decodes the same time=0.0 delta's
// rotation half via an atan2 double-angle unpack, then composes it onto
// AnglesToAxis(anglesArg) and converts back to angles. At the identity
// rotation retail's own explicit guard substitutes on a raw-zero delta
// (`(rotXY==(0,0)) -> force (0,1)`, needed to avoid a divide-by-zero in the
// atan2 decode -- itself evidence that time=0 commonly yields exactly this
// identity case), the composed result is angles unchanged. Verified retail
// output for the common (multi-frame anim) case: argument 1 (angles) passed
// straight through. NOT reproduced: the single-frame (numframes==0) edge
// case, same caveat as GetStartOrigin.
void GScr_GetStartAngles_SP()
{
    float origin[3]; // read to match retail's call shape; proven dead store there too
    float angles[3];

    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, angles, SCRIPTINSTANCE_SERVER);
    Scr_GetAnim(2u, 0, SCRIPTINSTANCE_SERVER);
    Scr_AddVector(angles, SCRIPTINSTANCE_SERVER);
}
// Retail 0x007FBF20 / dispatch record 0x00B76CD8. Descriptive reconstruction name.
static void GScr_GetCinematicTimeRemaining_SP()
{
    if (!r_reflectionProbeGenerate->current.enabled)
        Scr_AddFloat(R_Cinematic_GetRemainingSeconds_SP(), SCRIPTINSTANCE_SERVER);
}
static void GScr_CheckCodeSpawnOrigin_SP(const float *origin, const char *kind)
{
    if (!BG_ValidateOrigin(origin, 18, 17, svs.mapCenter))
    {
        const float xy = float(1 << 16);
        const float z = float(1 << 15);
        Scr_Error(va("Trying to spawn %s at position (%0.2f, %0.2f, %0.2f) will place it outside of map ranges that can be sent across the network.\n Which given a map center (%0.2f, %0.2f, %0.2f) should be between (%0.2f, %0.2f, %0.2f) to (%0.2f, %0.2f, %0.2f)\n",
            kind, origin[0], origin[1], origin[2],
            svs.mapCenter[0], svs.mapCenter[1], svs.mapCenter[2],
            svs.mapCenter[0] - xy, svs.mapCenter[1] - xy, svs.mapCenter[2] - z,
            svs.mapCenter[0] + xy, svs.mapCenter[1] + xy, svs.mapCenter[2] + z),
            SCRIPTINSTANCE_SERVER);
    }
}

static void __cdecl GScr_CodeSpawnTurret_SP()
{
    const unsigned int classname = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    float origin[3];
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    const char *weapon = Scr_GetString(2u, SCRIPTINSTANCE_SERVER);
    GScr_CheckCodeSpawnOrigin_SP(origin, "entity");
    Scr_AddEntity(SpawnTurretInternal(classname, origin, weapon), SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_CodeSpawn_SP()
{
    char *v0; // eax
    const char *v1; // eax
    float *currentOrigin; // [esp+0h] [ebp-1Ch]
    float origin[3]; // [esp+4h] [ebp-18h] BYREF
    int iSpawnFlags; // [esp+10h] [ebp-Ch]
    unsigned __int16 classname; // [esp+14h] [ebp-8h]
    gentity_s *ent; // [esp+18h] [ebp-4h]

    classname = (unsigned __int16)Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1u, origin, SCRIPTINSTANCE_SERVER);
    GScr_CheckCodeSpawnOrigin_SP(origin, "entity");
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) <= 2 )
        iSpawnFlags = 0;
    else
        iSpawnFlags = Scr_GetInt(2u, SCRIPTINSTANCE_SERVER);
    G_RecordSpawnAttempt_SP(classname);
    ent = G_Spawn();
    Scr_SetString(&ent->classname, classname, SCRIPTINSTANCE_SERVER);
    currentOrigin = ent->r.currentOrigin;
    ent->r.currentOrigin[0] = origin[0];
    currentOrigin[1] = origin[1];
    currentOrigin[2] = origin[2];
    ent->spawnflags = iSpawnFlags;
    ent->noBaseline_SP = 1;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 5 )
    {
        char *destructibleName = SL_ConvertToString(
            Scr_GetConstString(5u, SCRIPTINSTANCE_SERVER), SCRIPTINSTANCE_SERVER);
        G_SetupDestructible(ent, destructibleName);
        ent->model = 1;
    }
    if ( G_CallSpawnEntity(ent) )
    {
        if ( ent->destructible )
            ent->model = G_ModelIndex((char *)ent->destructible->ddef->model->name);
        Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        v0 = SL_ConvertToString(classname, SCRIPTINSTANCE_SERVER);
        v1 = va("unable to spawn \"%s\" entity", v0);
        Scr_Error(v1, 0);
    }
}
// Retail SP 0x007F1180.  Fact 1 is the global-function table's literal
// name/handler pair.  Fact 2 is the body-level XAnim layout: it resolves the
// supplied animation, filters XAnimNotifyInfo records by their time-distance
// in seconds, calculates absolute root-motion at the notify fraction, and
// emits one four-element script array per match.
//
// This is gameplay-significant for Zombies.  zombie_init.gsc uses the result
// to populate cover-transition arrival/exit metadata; the former empty-array
// stub discarded every notetrack and left traversal/path state incomplete.
static void __cdecl GScr_GetNotetracksInDelta_SP()
{
    const scr_anim_s anim = Scr_GetAnim(0, 0, SCRIPTINSTANCE_SERVER);
    const float targetTime = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    const float maxDeltaSeconds = Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 3
        ? Scr_GetFloat(2, SCRIPTINSTANCE_SERVER)
        : 0.15f;

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);

    const XAnim_s *anims = Scr_GetAnims(anim.tree, SCRIPTINSTANCE_SERVER);
    const XAnimParts *parts = anims->entries[anim.index].parts;
    const XAnimNotifyInfo *notify = parts->notify;
    if ( !notify )
        return;

    const float animLength = static_cast<float>(parts->numframes) / parts->framerate;
    for ( unsigned int notifyIndex = 0; notifyIndex < parts->notifyCount; ++notifyIndex, ++notify )
    {
        if ( std::fabs(animLength * notify->time - animLength * targetTime) > maxDeltaSeconds )
            continue;

        float rotation[2];
        float translation[3];
        XAnimGetAbsDelta(anims, anim.index, rotation, translation, notify->time);

        Scr_MakeArray(SCRIPTINSTANCE_SERVER);
        Scr_AddString(parts->name, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
        Scr_AddConstString(notify->name, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
        Scr_AddFloat(notify->time, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
        Scr_AddVector(translation, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}
// getdifficulty / isassetloaded are IMPLEMENTED -- see GScr_GetDifficulty_SP and
// GScr_IsAssetLoaded_SP above. Their stub rows were removed from this block.
static void __cdecl GScr_GetTimeScale_SP()
{
    Scr_AddFloat(com_timescale->current.value, SCRIPTINSTANCE_SERVER);
}

static void GScr_GetDebugDvar_SP()
{
    const char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const dvar_s *dvar = Dvar_FindVar(name);
    Scr_AddString(dvar ? Dvar_DisplayableValue(dvar) : "", SCRIPTINSTANCE_SERVER);
}

static void GScr_GetDebugDvarInt_SP()
{
    const char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(atoi(Dvar_GetVariantString(name)), SCRIPTINSTANCE_SERVER);
}

static void GScr_GetDebugDvarFloat_SP()
{
    const char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    Scr_AddFloat((float)atof(Dvar_GetVariantString(name)), SCRIPTINSTANCE_SERVER);
}

static void GScr_IsGodMode_SP()
{
    int enabled = 0;
    if (Scr_GetType(0, SCRIPTINSTANCE_SERVER) == VAR_POINTER
        && Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == VAR_ENTITY)
    {
        enabled = (Scr_GetEntity(0)->flags & 3) != 0;
    }
    Scr_AddInt(enabled, SCRIPTINSTANCE_SERVER);
}

static void GScr_RecordDebug_SP()
{
}

static void GScr_Checkpoint_issaverecentlyloaded_SP(){Scr_AddInt(G_CheckpointRecentlyLoaded_SP()?1:0,SCRIPTINSTANCE_SERVER);}
SP_STUB_FUNCTION_INT(GScr_SPStubFn_numremoteclients, "numremoteclients", "0x00642850", 0)
    // RETURNS 0: EVIDENCE, not judgement: single-player has no remote clients. The corpus is
    //            written for exactly this -- maps/_utility.gsc:10433 wait_network_frame() is
    //            `if (NumRemoteClients()) { ...snapshot handshake... } else { wait(0.1); }`,
    //            and 0 takes the non-networked branch. This also makes the getsnapshotindexarray
    //            / snapshotacknowledged pair below unreachable.
    // TODO(SP-STUB) 4 GSC ref(s), e.g. maps/_load_common:2279 -- if(NumRemoteClients())
static void GScr_VisionSetLastStand_SP()
{
    char config[1024];
    SV_GetConfigstring(CS_SP_VISIONSET_LASTSTAND, config, sizeof(config));
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (count != 1 && count != 2)
    {
        Scr_Error(SCRIPTINSTANCE_SERVER, "USAGE: CS_VISIONSET_LASTSTAND( <visionset name>, <transition time> )\n", false);
        return;
    }
    const int duration = count == 2 ? GScr_VisionTransitionMsec_SP() : 1000;
    const char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    for (int client = 0; client < com_maxclients->current.integer; ++client)
    {
        const char *value = va("\"%s\" %i", name, duration);
        char *key = va("%i", client);
        Info_SetValueForKey(config, key, value);
    }
    SV_SetConfigstring(CS_SP_VISIONSET_LASTSTAND, config);
}
static void GScr_AbsAngleClamp360_SP()
{
    const float angle = static_cast<float>(Scr_GetFloat(0, SCRIPTINSTANCE_SERVER));
    const float normalized = static_cast<float>(AngleNormalize360(angle));
    Scr_AddFloat(normalized, SCRIPTINSTANCE_SERVER);
}
static void __cdecl GScr_AngleLerp_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3)
        Scr_Error("AngleLerp requires 3 parameters", false);

    const float from = static_cast<float>(Scr_GetFloat(0, SCRIPTINSTANCE_SERVER));
    const float to = static_cast<float>(Scr_GetFloat(1, SCRIPTINSTANCE_SERVER));
    const float lerp = static_cast<float>(Scr_GetFloat(2, SCRIPTINSTANCE_SERVER));
    volatile float delta = to - from;
    volatile float turns = delta * 0.0027777778450399637f;
    volatile float shifted = turns + 0.5f;
    const float signedMagic = std::copysign(8388608.0f, shifted);
    volatile float roundedSum = shifted + signedMagic;
    volatile float rounded = roundedSum - signedMagic;
    volatile float difference = rounded - shifted;
    if (!(difference <= std::copysign(0.0f, shifted)))
        rounded = rounded - 1.0f;
    volatile float fraction = turns - rounded;
    volatile float scaled = fraction * lerp;
    volatile float degrees = scaled * 360.0f;
    volatile float result = degrees + from;
    Scr_AddFloat(result, SCRIPTINSTANCE_SERVER);
}
static void GScr_SP_distance2dsquared()
{
    // Retail 0x007f92c0: two SERVER vectors, squared y/x deltas, no sqrt.
    // Zombie window melee compares this result before issuing DoDamage.
    float from[3], to[3];
    Scr_GetVector(0, from, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1, to, SCRIPTINSTANCE_SERVER);
    const float dx = to[0] - from[0];
    const float dy = to[1] - from[1];
    Scr_AddFloat(dy * dy + dx * dx, SCRIPTINSTANCE_SERVER);
}
static void GScr_FindPath_SP()
{
    float start[3];
    float goal[3];
    Scr_GetVector(0, start, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1, goal, SCRIPTINSTANCE_SERVER);
    if (!debugPath)
        debugPath = &debugPathBuf;
    Path_Begin(debugPath);
    Scr_AddInt(Path_FindPath(debugPath, TEAM_FREE, start, goal, 1), SCRIPTINSTANCE_SERVER);
}
static void GScr_HasCollectible_SP()
{
    Scr_AddInt(BG_HasCollectible(Scr_GetInt(0, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);
}
static void GScr_OkToSpawn_SP()
{
    Scr_AddInt(G_IsOkToSpawn_SP() ? 1 : 0, SCRIPTINSTANCE_SERVER);
}
static void GScr_Checkpoint_savegamenocommit_SP(){Scr_AddInt(G_CheckpointRequest_SP(Scr_GetString(0,SCRIPTINSTANCE_SERVER),false),SCRIPTINSTANCE_SERVER);}
static void GScr_CodeSpawnVehicle_SP()
{
    const unsigned int targetname = Scr_GetConstString(1, SCRIPTINSTANCE_SERVER);
    const unsigned int vehicleType = Scr_GetConstString(2, SCRIPTINSTANCE_SERVER);
    float origin[3];
    float angles[3];
    Scr_GetVector(3, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(4, angles, SCRIPTINSTANCE_SERVER);
    GScr_CheckCodeSpawnOrigin_SP(origin, "vehicle");
    G_RecordSpawnAttempt_SP(vehicleType);

    gentity_s *ent = G_Spawn();
    Scr_SetString(&ent->classname, scr_const.script_vehicle, SCRIPTINSTANCE_SERVER);
    if (Scr_GetType(0, SCRIPTINSTANCE_SERVER) == VAR_STRING)
        G_SetModel(ent, Scr_GetString(0, SCRIPTINSTANCE_SERVER));
    else
    {
        const int infoIndex = VEH_GetVehicleInfoFromName(
            SL_ConvertToString(vehicleType, SCRIPTINSTANCE_SERVER));
        G_SetModel(ent, (char *)BG_GetVehicleInfo(infoIndex)->model);
    }
    Scr_SetString(&ent->targetname, targetname, SCRIPTINSTANCE_SERVER);
    for (int i = 0; i < 3; ++i)
    {
        ent->r.currentOrigin[i] = origin[i];
        ent->r.currentAngles[i] = angles[i];
    }
    if ((unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 5)
        G_SetupDestructible(ent, SL_ConvertToString(
            Scr_GetConstString(5, SCRIPTINSTANCE_SERVER), SCRIPTINSTANCE_SERVER));
    G_SpawnVehicle(ent, SL_ConvertToString(vehicleType, SCRIPTINSTANCE_SERVER), 0);
    Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
}
static void GScr_GetDynModels_SP()
{
    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for (unsigned short i = 0; i < cm.dynEntCount[DYNENT_COLL_SERVER_MODEL]; ++i)
    {
        const XModel *model = cm.dynEntDefList[DYNENT_DRAW_MODEL][i].xModel;
        if (model)
        {
            Scr_AddString(model->name, SCRIPTINSTANCE_SERVER);
            Scr_AddArray(SCRIPTINSTANCE_SERVER);
        }
    }
}
static void __cdecl GScr_GetMiscModels_SP()
{
    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for (unsigned int i = 0; i < cm.numStaticModels; ++i)
    {
        XModel *model = cm.staticModelList[i].xmodel;
        if (model)
        {
            Scr_AddString((char *)model->name, SCRIPTINSTANCE_SERVER);
            Scr_AddArray(SCRIPTINSTANCE_SERVER);
        }
    }
}
static void __cdecl GScr_GetPersistentProfileVar_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2)
        Scr_Error("GetPersistentProfileVar() called with wrong params.\n", false);
    const int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    const int defaultValue = Scr_GetInt(1, SCRIPTINSTANCE_SERVER);
    int value = defaultValue;
    if (static_cast<unsigned int>(index) >= MISSION_GLOBAL_MAX_DVARS)
        Scr_Error("GetPersistentProfileVar() called with wrong param range. Index must be 0 to MISSION_GLOBAL_MAX_DVARS.\n", false);
    else if (CL_ControllerIndex_GetSignInState(Com_LocalClient_GetControllerIndex(0)) > 0)
        value = mission_global_dvar[index]->current.integer;
    Scr_AddInt(value, SCRIPTINSTANCE_SERVER);
}
static void __cdecl GScr_GetSpawnerTeamArray_SP()
{
    unsigned int teamFlags = 0;
    for (unsigned int i = 0; i < static_cast<unsigned int>(Scr_GetNumParam(SCRIPTINSTANCE_SERVER)); ++i)
        teamFlags |= GScr_GetAITeamFlag_SP(Scr_GetString(i, SCRIPTINSTANCE_SERVER), "getspawnerteamarray");
    if (!teamFlags)
        Scr_Error("no team was specified - use getspawnerarray instead", SCRIPTINSTANCE_SERVER);

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for (int i = 0; i < level.num_entities; ++i)
    {
        gentity_s *ent = &level.gentities[i];
        if (!ent->r.inuse || ent->s.eType != ET_ACTOR_SPAWNER)
            continue;
        if (!(teamFlags & (1u << (ent->spawner.team & 31))))
            continue;
        Scr_AddEntity(ent, SCRIPTINSTANCE_SERVER);
        Scr_AddArray(SCRIPTINSTANCE_SERVER);
    }
}
static double GScr_WeaponGraphAccuracy_SP(const WeaponVariantDef *weapon, int graphType, float distance)
{
    float fraction = distance * 0.00025f;
    if (fraction > 1.0f)
        fraction = 1.0f;
    const WeaponDef *def = weapon->weapDef;
    if (!def->accuracyGraphKnots[graphType] || !def->accuracyGraphKnotCount[graphType])
    {
        const char *name = BG_WeaponName(BG_GetWeaponIndex(weapon));
        if (graphType == 0)
            Com_Error(ERR_DROP, "\x15No 'AI vs AI' accuracy graph for weapon '%s'", name);
        else if (graphType == 1)
            Com_Error(ERR_DROP, "\x15No 'AI vs Player' accuracy graph for weapon '%s'", name);
    }
    return GraphGetValueFromFraction(def->accuracyGraphKnotCount[graphType],
        def->accuracyGraphKnots[graphType], fraction);
}

static double GScr_TargetWeaponAccuracy_SP(actor_s *actor, sentient_s *enemy,
    const WeaponVariantDef *weapon, int graphType)
{
    float origin[3], targetOrigin[3];
    Sentient_GetOrigin(actor->sentient, origin);
    Sentient_GetOrigin(enemy, targetOrigin);
    const float dx = origin[0] - targetOrigin[0];
    const float dy = origin[1] - targetOrigin[1];
    const float dz = origin[2] - targetOrigin[2];
    const volatile float dzSquared = dz * dz;
    const volatile float dySquared = dy * dy;
    const volatile float dxSquared = dx * dx;
    const volatile float zySquared = dzSquared + dySquared;
    float distance = sqrtf(zySquared + dxSquared);
    if (graphType == 1)
        distance = ai_accuracyDistScale->current.value * distance;
    return GScr_WeaponGraphAccuracy_SP(weapon, graphType, distance);
}

static void GScr_GetWeaponAccuracy_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2)
        Scr_Error(SCRIPTINSTANCE_SERVER, "Illegal call to getweaponaccuracy(self,self.weapon), AI entity and weapon name are mandatory fields.", false);
    gentity_s *self = Scr_GetEntity(0);
    if (!self->actor)
        Scr_Error(SCRIPTINSTANCE_SERVER, "getweaponaccuracy must be called on an AI only.", false);
    sentient_s *enemy = Actor_GetTargetSentient(self->actor);
    char *name = Scr_GetString(1, SCRIPTINSTANCE_SERVER);
    const unsigned int index = G_GetWeaponIndexForName(name);
    Scr_VerifyWeaponIndex(index, name);
    const WeaponVariantDef *weapon = BG_GetWeaponVariantDef(index);
    const int graphType = enemy->ent->client ? 1 : 0;
    Scr_AddFloat(static_cast<float>(GScr_TargetWeaponAccuracy_SP(self->actor, enemy, weapon, graphType)),
        SCRIPTINSTANCE_SERVER);
}
static void GScr_GetWeaponClipModel_SP()
{
    char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    unsigned int weapon = G_GetWeaponIndexForName(name);
    const char *modelName = "";
    if (weapon)
    {
        const WeaponDef *def = BG_GetWeaponDef(weapon);
        if (def->worldClipModel)
            modelName = XModelGetName(def->worldClipModel);
    }
    else if (*name && I_stricmp(name, "none"))
    {
        Com_PrintWarning(24, "unknown weapon '%s' in getWeaponClipModel\n", name);
    }
    Scr_AddString(modelName, SCRIPTINSTANCE_SERVER);
}

#ifndef KISAK_NX
static_assert(offsetof(WeaponDef, worldClipModel) == 0x310);
static_assert(offsetof(WeaponDef, weapType) == 0x1c);
#endif

static void GScr_IsNodeOccupied_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1)
    {
        Scr_Error(SCRIPTINSTANCE_SERVER, "illegal call to isnodeoccupied()", false);
        return;
    }
    pathnode_t *node = Scr_GetPathnode(0, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(level.time <= node->dynamic.iFreeTime, SCRIPTINSTANCE_SERVER);
}

static void GScr_GetNodeOwner_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1)
    {
        Scr_Error(SCRIPTINSTANCE_SERVER, "illegal call to getnodeowner()", false);
        return;
    }
    pathnode_t *node = Scr_GetPathnode(0, SCRIPTINSTANCE_SERVER);
    if (node->dynamic.pOwner.isDefined())
        Scr_AddEntity(node->dynamic.pOwner.sentient()->ent, SCRIPTINSTANCE_SERVER);
}
static void GScr_Checkpoint_issavesuccessful_SP(){Scr_AddInt(G_CheckpointSaveSuccessful_SP()?1:0,SCRIPTINSTANCE_SERVER);}
static void GScr_WeaponIsGasWeapon_SP()
{
    char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    unsigned int weapon = G_GetWeaponIndexForName(name);
    Scr_AddInt(weapon && BG_GetWeaponDef(weapon)->weapType == WEAPTYPE_GAS, SCRIPTINSTANCE_SERVER);
}

static void GScr_WeaponMaxGibDistance_SP()
{
    char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    unsigned int weapon = G_GetWeaponIndexForName(name);
    Scr_VerifyWeaponIndex(weapon, name);
    Scr_AddFloat(BG_GetWeaponDef(weapon)->maxGibDistance, SCRIPTINSTANCE_SERVER);
}
static void __cdecl GScr_BulletSpread_SP()
{
    float origin[3], target[3], end[3];
    weaponParms wp;
    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1, target, SCRIPTINSTANCE_SERVER);
    const float spread = Scr_GetFloat(2, SCRIPTINSTANCE_SERVER);
    Weapon_SetWeaponParamsWeapon(&wp, 0);
    Bullet_BuildSpreadParams_SP(origin, target, &wp);
    Bullet_Endpos(level.time, spread, end, nullptr, &wp, sv_bullet_range->current.value);
    Scr_AddVector(end, SCRIPTINSTANCE_SERVER);
}
static void __cdecl GScr_CanSpawnTurret_SP()
{
    Scr_AddInt(G_CanSpawnTurret(), SCRIPTINSTANCE_SERVER);
}
static double GScr_EntSearchNormalize_SP(float *v)
{
    float squaredLength = v[0] * v[0];
    squaredLength += v[1] * v[1];
    squaredLength += v[2] * v[2];
    const double length = sqrt(static_cast<double>(squaredLength));
    float divisor = static_cast<float>(length);
    if (length <= 0.0)
        divisor = 1.0f;
    const float scale = 1.0f / divisor;
    v[0] *= scale;
    v[1] *= scale;
    v[2] *= scale;
    return length;
}

static void __cdecl GScr_EntSearch_SP()
{
    const unsigned int numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (!numParam)
    {
        Scr_MakeArray(SCRIPTINSTANCE_SERVER);
        for (int i = 0; i < level.num_entities; ++i)
        {
            if (g_entities[i].r.inuse)
            {
                Scr_AddEntityNum(g_entities[i].s.number, 0, SCRIPTINSTANCE_SERVER, 0);
                Scr_AddArray(SCRIPTINSTANCE_SERVER);
            }
        }
        return;
    }

    const int contentmask = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (numParam == 1)
    {
        Scr_MakeArray(SCRIPTINSTANCE_SERVER);
        for (int i = 0; i < level.num_entities; ++i)
        {
            if (g_entities[i].r.inuse && (g_entities[i].r.contents & contentmask))
            {
                Scr_AddEntityNum(g_entities[i].s.number, 0, SCRIPTINSTANCE_SERVER, 0);
                Scr_AddArray(SCRIPTINSTANCE_SERVER);
            }
        }
        return;
    }

    if (numParam < 3)
    {
        Scr_ParamError(1, "Cannot specify an origin without a radius", SCRIPTINSTANCE_SERVER);
        return;
    }
    float origin[3];
    Scr_GetVector(1, origin, SCRIPTINSTANCE_SERVER);
    const double radiusValue = Scr_GetFloat(2, SCRIPTINSTANCE_SERVER);
    const float radius = static_cast<float>(radiusValue);
    if (radiusValue <= 0.0)
    {
        Scr_ParamError(2, "Range should be greater than 0", SCRIPTINSTANCE_SERVER);
        return;
    }

    float direction[3] = { 1.0f, 0.0f, 0.0f };
    float cosineLimit = -1.0f;
    if (numParam > 3)
    {
        if (numParam < 5)
        {
            Scr_ParamError(3, "Cannot specify an direction without a FOV", SCRIPTINSTANCE_SERVER);
            return;
        }
        Scr_GetVector(3, direction, SCRIPTINSTANCE_SERVER);
        const double fovValue = Scr_GetFloat(4, SCRIPTINSTANCE_SERVER);
        if (fovValue <= 0.0)
        {
            Scr_ParamError(4, "FOV should be greater than 0", SCRIPTINSTANCE_SERVER);
            return;
        }
        float fov = static_cast<float>(fovValue);
        if (fov > 360.0f)
            fov = 360.0f;
        const float halfFov = fov * 0.5f;
        if (halfFov < 179.9f)
            cosineLimit = static_cast<float>(cos(static_cast<double>(halfFov * 0.017453292f)));
    }
    if (GScr_EntSearchNormalize_SP(direction) == 0.0)
    {
        Scr_ParamError(3, "Direction length must be greater than 0", SCRIPTINSTANCE_SERVER);
        return;
    }

    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    float mins[3], maxs[3];
    for (int axis = 0; axis < 3; ++axis)
    {
        mins[axis] = origin[axis] - radius;
        maxs[axis] = origin[axis] + radius;
    }
    int entityList[1024];
    const int count = CM_AreaEntities(mins, maxs, entityList, 1024, contentmask);
    for (int i = 0; i < count; ++i)
    {
        gentity_s *ent = SV_GentityNum(entityList[i]);
        float delta[3];
        for (int axis = 0; axis < 3; ++axis)
            delta[axis] = ent->r.currentOrigin[axis] - origin[axis];
        const double distance = GScr_EntSearchNormalize_SP(delta);
        if (distance < radius)
        {
            float dot = delta[2] * direction[2] + delta[1] * direction[1];
            dot += delta[0] * direction[0];
            if (dot >= cosineLimit)
            {
                Scr_AddEntityNum(ent->s.number, 0, SCRIPTINSTANCE_SERVER, 0);
                Scr_AddArray(SCRIPTINSTANCE_SERVER);
            }
        }
    }
}
static void __cdecl GScr_GetDestructibleDefs_SP()
{
    Scr_MakeArray(SCRIPTINSTANCE_SERVER);
    for (int i = 0; i < level.num_entities; ++i)
    {
        gentity_s *ent = &g_entities[i];
        if (ent->r.inuse && ent->destructible)
        {
            Scr_AddString((char *)ent->destructible->ddef->name, SCRIPTINSTANCE_SERVER);
            Scr_AddArray(SCRIPTINSTANCE_SERVER);
        }
    }
}
SP_STUB_FUNCTION(GScr_SPStubFn_getsnapshotindexarray,     "getsnapshotindexarray", "0x007ff1b0")
static void GScr_IsCoopEPD_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
        Scr_Error(SCRIPTINSTANCE_SERVER, "USAGE: bool = IsCoopEPD()\n", false);
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}
static void __cdecl GScr_IsTurretActive_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1)
    {
        Scr_Error("illegal call to isturretactive()\n", false);
        return;
    }
    gentity_s *ent = Scr_GetEntity(0);
    if (!ent)
    {
        Scr_Error("NULL turret passed to isturretactive", false);
        return;
    }
    Scr_AddInt(ent->active, SCRIPTINSTANCE_SERVER);
}
static void GScr_ModelHasPhysPreset_SP()
{
    const XModel *model = SV_XModelGet(Scr_GetString(0, SCRIPTINSTANCE_SERVER));
    Scr_AddInt(model->physPreset != nullptr, SCRIPTINSTANCE_SERVER);
}
static void GScr_PlayerPositionValid_SP()
{
    float position[3];
    Scr_GetVector(0, position, SCRIPTINSTANCE_SERVER);
    col_context_t context;
    trace_t trace;
    G_TraceCapsule(&trace, position, playerMins, playerMaxs, position, 1023, 0x281C011, &context);
    Scr_AddInt(!(trace.fraction < 1.0f) && !trace.allsolid && !trace.startsolid, SCRIPTINSTANCE_SERVER);
}
SP_STUB_FUNCTION(GScr_SPStubFn_snapshotacknowledged,      "snapshotacknowledged", "0x007ff230")
static void GScr_WeaponDoGibbing_SP()
{
    char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    unsigned int weapon = G_GetWeaponIndexForName(name);
    Scr_VerifyWeaponIndex(weapon, name);
    Scr_AddInt(BG_GetWeaponDef(weapon)->doGibbing, SCRIPTINSTANCE_SERVER);
}

static int GScr_BadPlaceDuration_SP(double seconds)
{
    volatile float milliseconds = seconds * 1000.0f;
    const double rounded = std::nearbyint(static_cast<double>(milliseconds) + 0.4999999990686774);
    if (!(rounded >= -2147483648.0 && rounded < 2147483648.0))
        return INT_MIN;
    return static_cast<int>(rounded);
}

static int GScr_BadPlaceTeams_SP(unsigned int first, const char *functionName)
{
    int teams = 0;
    for (unsigned int i = first; i < Scr_GetNumParam(SCRIPTINSTANCE_SERVER); ++i)
    {
        const char *team = Scr_GetString(i, SCRIPTINSTANCE_SERVER);
        if (!I_stricmp(team, "axis")) teams |= 2;
        else if (!I_stricmp(team, "allies")) teams |= 4;
        else if (!I_stricmp(team, "neutral")) teams |= 8;
        else if (!I_stricmp(team, "all")) teams |= 14;
        else Scr_Error(va("unknown team '%s' in %s (should be axis, allies, or neutral)", team, functionName), 0);
    }
    return teams;
}

static void GScr_SPStubFn_badplace_cylinder()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 5)
    {
        Scr_Error("Incorrect badplace_cylinder() call.\n", 0);
        return;
    }
    const char *nameText = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const unsigned int name = *nameText ? Scr_GetConstString(0, SCRIPTINSTANCE_SERVER) : 0;
    const int duration = GScr_BadPlaceDuration_SP(Scr_GetFloat(1, SCRIPTINSTANCE_SERVER));
    badplace_arc_t arc;
    Scr_GetVector(2, arc.origin, SCRIPTINSTANCE_SERVER);
    arc.radius = Scr_GetFloat(3, SCRIPTINSTANCE_SERVER);
    arc.halfheight = Scr_GetFloat(4, SCRIPTINSTANCE_SERVER) * 0.5f;
    arc.angle0 = 0.0f;
    arc.angle1 = 360.0f;
    const int teams = GScr_BadPlaceTeams_SP(5, "badplace_cylinder");
    Path_MakeBadPlaceArc_SP(name, duration, teams ? teams : 14, &arc);
}
static void GScr_BadPlaceBrush_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 3)
    {
        Scr_Error("Incorrect badplace_cylinder() call.\n", 0);
        return;
    }
    const char *nameText = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const unsigned int name = *nameText ? Scr_GetConstString(0, SCRIPTINSTANCE_SERVER) : 0;
    const int duration = GScr_BadPlaceDuration_SP(Scr_GetFloat(1, SCRIPTINSTANCE_SERVER));
    gentity_s *volume = Scr_GetEntity(2);
    const int teams = GScr_BadPlaceTeams_SP(3, "badplace_brush");
    Path_MakeBadPlaceBrush_SP(name, duration, teams ? teams : 14, volume);
}

static int GScr_GetThreatBiasGroupIndex_SP(unsigned __int16 groupName)
{
    for ( int groupIndex = 0; groupIndex < g_threatBias.threatGroupCount; ++groupIndex )
    {
        if ( g_threatBias.groupName[groupIndex] == groupName )
            return groupIndex;
    }
    return -1;
}

static int GScr_RequireThreatBiasGroup_SP(unsigned int parameter)
{
    const unsigned __int16 groupName = Scr_GetConstString(parameter, SCRIPTINSTANCE_SERVER);
    const int groupIndex = GScr_GetThreatBiasGroupIndex_SP(groupName);
    if ( groupIndex < 0 )
    {
        Scr_Error(
            va("Invalid threat bias group '%s'.\n", Scr_GetString(parameter, SCRIPTINSTANCE_SERVER)),
            SCRIPTINSTANCE_SERVER);
    }
    return groupIndex;
}

static void __cdecl GScr_SetIgnoreMeGroup_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2)
        Scr_ParamError(0, "setignoremegroup [group for] [group ignoring]", SCRIPTINSTANCE_SERVER);

    const int groupFor = GScr_GetThreatBiasGroupIndex_SP(Scr_GetConstString(0, SCRIPTINSTANCE_SERVER));
    const int groupIgnoring = GScr_GetThreatBiasGroupIndex_SP(Scr_GetConstString(1, SCRIPTINSTANCE_SERVER));
    if (groupFor < 0)
    {
        Scr_Error(va("Invalid threat bias group '%s'.\n", Scr_GetString(0, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);
        return;
    }
    if (groupIgnoring < 0)
    {
        Scr_Error(va("Invalid threat bias group '%s'.\n", Scr_GetString(1, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);
        return;
    }
    g_threatBias.threatTable[groupFor][groupIgnoring] = (-2147483647 - 1);
}

static void __cdecl GScr_GetThreatBias_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2 )
        Scr_ParamError(0, "getthreatbias [group for] [group against]", SCRIPTINSTANCE_SERVER);

    const int groupFor = GScr_RequireThreatBiasGroup_SP(0);
    const int groupAgainst = GScr_RequireThreatBiasGroup_SP(1);
    Scr_AddInt(Actor_GetThreatBias(groupFor, groupAgainst), SCRIPTINSTANCE_SERVER);
}
struct SPChangeLevelState
{
    int changePending;
    int reloadDelayTime;
    int exitTime;
    char nextMap[64];
    unsigned int deathReloadTime;
    int missionFailed;
    bool playerWasAlive;
};

static SPChangeLevelState s_spChangeLevel;

static void GScr_ReloadPlayer_SP()
{
    if (g_reloading->current.integer != 0)
        return;

    Dvar_SetInt((dvar_s *)g_reloading, 1);
    const int delay = g_deathDelay->current.integer;
    s_spChangeLevel.deathReloadTime = Sys_Milliseconds() + static_cast<unsigned int>(delay) + 1000u;
    const dvar_t *quote = Dvar_FindVar("ui_deadquote");
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c 0 %i", 0x71, delay));
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c 0 0 0 0", 0x41));
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c %i %f %i %i", 0x41, delay, 32.0, 1, 2));
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c %i %f %f", 0x58, delay, 1.0, (double)0.1f));
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c 0 0", 0x34));
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c 0", 0x30));
    if (quote)
        SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c 1 %s ", 0x59, quote->current.string));
}

static void GScr_MissionFailed_SP()
{
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c hud_missionFailed 1", 0x76));
    if (G_Find(NULL, G_FIND_CLASSNAME, scr_const.player))
    {
        GScr_ReloadPlayer_SP();
        s_spChangeLevel.missionFailed = 1;
    }
}

static void GScr_ContinueAfterDeath_SP()
{
    if (!G_CheckpointTryLoad_SP())
        Cbuf_AddText(0, "fast_restart\n");
}

static int GScr_ChangeLevelMsec_SP(float seconds)
{
    const float milliseconds = seconds * 1000.0f;
    return (int)std::nearbyint((double)milliseconds + 9.31322574615479e-10);
}

static bool GScr_CanChangeLevel_SP()
{
    gentity_s *player = G_Find(NULL, G_FIND_CLASSNAME, scr_const.player);
    return player && player->health > 0 && g_reloading && g_reloading->current.integer == 0;
}

static void GScr_ChangeLevel_SP()
{
    if (!GScr_CanChangeLevel_SP())
    {
        return;
    }

    const int argc = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (argc != 1)
    {
        if (argc != 2)
        {
            s_spChangeLevel.exitTime = GScr_ChangeLevelMsec_SP(Scr_GetFloat(2, SCRIPTINSTANCE_SERVER));
            if (s_spChangeLevel.exitTime < 0)
                Scr_ParamError(1, "exitTime cannot be negative", SCRIPTINSTANCE_SERVER);
        }
        level.savepersist = Scr_GetInt(1, SCRIPTINSTANCE_SERVER);
    }

    const char *map = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if (argc < 3 && g_changelevel_time && g_changelevel_time->current.value >= 0.0f)
        s_spChangeLevel.exitTime = GScr_ChangeLevelMsec_SP(g_changelevel_time->current.value);

    s_spChangeLevel.changePending = 1;
    I_strncpyz(s_spChangeLevel.nextMap, map, sizeof(s_spChangeLevel.nextMap));
}

void __cdecl GScr_ResetChangeLevel_SP()
{
    memset(&s_spChangeLevel, 0, sizeof(s_spChangeLevel));
    if (g_reloading)
        Dvar_SetInt((dvar_s *)g_reloading, 0);
}

void __cdecl GScr_UpdateChangeLevel_SP()
{
    // The shipped solo death callback does not call missionfailed(). Observe
    // authoritative campaign health here so ordinary deaths reach recovery too.
    // Wait until this map has actually had a living player before arming it.
    if (!zombiemode->current.enabled && !Com_IsMenuLevel(Dvar_GetString("mapname")))
    {
        gentity_s *player = G_Find(NULL, G_FIND_CLASSNAME, scr_const.player);
        if (player && player->health > 0)
            s_spChangeLevel.playerWasAlive = true;
        else if (player && s_spChangeLevel.playerWasAlive && g_reloading->current.integer == 0)
            GScr_ReloadPlayer_SP();
    }
    if (s_spChangeLevel.changePending)
    {
        s_spChangeLevel.changePending = 0;
        if (GScr_CanChangeLevel_SP())
        {
            if (s_spChangeLevel.exitTime != 0)
            {
                SV_GameSendServerCommand(-1, SV_CMD_RELIABLE,
                    va("%c 1 %i %i", 0x55, 250, s_spChangeLevel.exitTime + 750));
            }
            if (!level.missionSuccess)
            {
                SV_GameSendServerCommand(-1, SV_CMD_RELIABLE,
                    va("%c 0 %i\n", 0x71, s_spChangeLevel.exitTime + 1000));
            }

            s_spChangeLevel.reloadDelayTime = level.time + s_spChangeLevel.exitTime + 1000;
            Dvar_SetInt((dvar_s *)g_reloading, 4);
        }
    }

    if (s_spChangeLevel.reloadDelayTime != 0 && s_spChangeLevel.reloadDelayTime < level.time)
    {
        s_spChangeLevel.reloadDelayTime = 0;
        if (level.missionSuccess)
        {
            level.missionSuccess = 0;
            if (level.missionSuccessCinematic[0])
                Cbuf_AddText(0, va("cinematic %s 0\n", level.missionSuccessCinematic));
            else
                SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c", 0x31));
            level.missionSuccessCinematic[0] = 0;
        }
        else if (s_spChangeLevel.missionFailed)
        {
            s_spChangeLevel.missionFailed = 0;
            GScr_ContinueAfterDeath_SP();
        }
        else if (!s_spChangeLevel.nextMap[0])
        {
            Cbuf_AddText(0, "disconnect\n");
        }
        else if (Dvar_GetBool("sv_cheats"))
        {
            Cbuf_AddText(0, va("spdevmap %s\n", s_spChangeLevel.nextMap));
        }
        else
        {
            Cbuf_AddText(0, va("spmap %s\n", s_spChangeLevel.nextMap));
        }
    }

    if (s_spChangeLevel.deathReloadTime != 0
        && std::bit_cast<int>(s_spChangeLevel.deathReloadTime - Sys_Milliseconds()) < 0)
    {
        s_spChangeLevel.deathReloadTime = 0;
        GScr_ContinueAfterDeath_SP();
    }
}
static void __cdecl GScr_SetThreatBias_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3 )
        Scr_ParamError(0, "setthreatbias [threatener] [threatened] [threat]", SCRIPTINSTANCE_SERVER);

    const int threatener = GScr_RequireThreatBiasGroup_SP(0);
    const int threatened = GScr_RequireThreatBiasGroup_SP(1);
    g_threatBias.threatTable[threatener][threatened] = Scr_GetInt(2, SCRIPTINSTANCE_SERVER);
}
static void GScr_Checkpoint_savegame_SP(){G_CheckpointRequest_SP(Scr_GetString(0,SCRIPTINSTANCE_SERVER),true);}
static void __cdecl GScr_MissionSuccess_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 2)
        Scr_Error("missionSuccess only takes two parameters: missionSuccess(nextMap, persistent)\n",
            SCRIPTINSTANCE_SERVER);

    GScr_ChangeLevel_SP();
    level.missionSuccess = 1;
}
static void __cdecl GScr_SetMissionDvar_SP()
{
    const char *dvarName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    static const char *const missionDvarNames[] = { "mis_01", "mis_difficulty", nullptr };
    for (const char *const *name = missionDvarNames; *name; ++name)
    {
        if (I_strcmp(dvarName, *name))
            continue;

        const dvar_s *dvar = Dvar_FindVar(dvarName);
        if (!dvar)
        {
            Scr_Error(va("Mission dvar has valid name, but was not found: %s\n", dvarName), 0);
            return;
        }

        switch (dvar->type)
        {
        case DVAR_TYPE_INT:
            Dvar_SetIntByName(dvarName, Scr_GetInt(1, SCRIPTINSTANCE_SERVER));
            break;
        case DVAR_TYPE_STRING:
            Dvar_SetStringByName(dvarName, Scr_GetString(1, SCRIPTINSTANCE_SERVER));
            break;
        default:
            Scr_Error(va("Mission dvar has valid name, but invalid type: %s is of type %i\n", dvarName, dvar->type), 0);
            return;
        }
        Com_Printf(16, " * SetMissionDvar(): \"%s\" set to \"%s\".\n", dvarName, Dvar_GetVariantString(dvarName));
        return;
    }
    Scr_Error(va("Invalid mission dvar name: %s\n", dvarName), 0);
}
static void __cdecl GScr_BulletTracer_SP()
{
    float start[3], end[3];
    Scr_GetVector(0, start, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1, end, SCRIPTINSTANCE_SERVER);
    gentity_s *ent = G_TempEntity(start, EV_BULLET_TRACER);
    ent->s.lerp.u.bulletTracer.end[0] = end[0];
    ent->s.lerp.u.bulletTracer.end[1] = end[1];
    ent->s.lerp.u.bulletTracer.end[2] = end[2];
    ent->s.eventParm = Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 3
        ? Scr_GetInt(2, SCRIPTINSTANCE_SERVER) != 0 : 0;
}
static void __cdecl GScr_SetPersistentProfileVar_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2)
        Scr_Error("SetPersistentProfileVar() called with wrong params.\n", false);
    const int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    const int value = Scr_GetInt(1, SCRIPTINSTANCE_SERVER);
    if (index < 0 || index >= MISSION_GLOBAL_MAX_DVARS)
        Scr_Error("SetPersistentProfileVar() called with wrong param range. Index must be 0 to MISSION_GLOBAL_MAX_DVARS.\n", false);
    if (value < 0 || value > 255)
        Scr_Error("SetPersistentProfileVar() called with wrong param range. Value must be 0 to 255.\n", false);
    Dvar_SetInt((dvar_s *)mission_global_dvar[index], value);
}
static void __cdecl GScr_UpdateGamerProfile_SP()
{
    // Scripts can execute on the server thread. Queue this offline config
    // write for the main-thread command buffer; filesystem writes assert if
    // performed here. Archived dvars also retain their regular frame flush.
    Cbuf_AddText(0, "writeconfig config.cfg\n");
}
static void GScr_Checkpoint_commitsave_SP(){const int id=Scr_GetInt(0,SCRIPTINSTANCE_SERVER);if(!G_CheckpointCommit_SP(id))Com_Printf(15,"CHECKPOINT_SAVE_ERROR commit id=%i\n",id);}
SP_STUB_FUNCTION(GScr_SPStubFn_setuinextlevel, "setuinextlevel", "0x00651a30")
SP_STUB_FUNCTION(GScr_SPStubFn_prefetchlevel, "prefetchlevel", "0x00651a30")
static void __cdecl GScr_CreateThreatBiasGroup_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_ParamError(0, "createthreatbiasgroup [name]", SCRIPTINSTANCE_SERVER);

    const unsigned __int16 groupName = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( g_threatBias.threatGroupCount >= 16 )
    {
        Com_PrintWarning(
            18,
            "Too many threat groups, can't create '%s'\n",
            SL_ConvertToString(groupName, SCRIPTINSTANCE_SERVER));
        return;
    }

    if ( GScr_GetThreatBiasGroupIndex_SP(groupName) >= 0 )
        return;

    Scr_SetString(
        &g_threatBias.groupName[g_threatBias.threatGroupCount],
        groupName,
        SCRIPTINSTANCE_SERVER);
    ++g_threatBias.threatGroupCount;
}
static void GScr_SetPhysicsGravityDir_SP()
{
    float gravity[3];
    Scr_GetVector(0, gravity, SCRIPTINSTANCE_SERVER);
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE,
        va("%c %g %g %g", '2', gravity[0], gravity[1], gravity[2]));
}
static void GScr_SetWaterBrush_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        return;

    gentity_s *ent = Scr_GetEntity(0);
    Dvar_SetInt(const_cast<dvar_s *>(waterbrush_entity), ent->s.number);
}
static void GScr_SetCollectible_SP()
{
    BG_SetCollectible(Scr_GetInt(0, SCRIPTINSTANCE_SERVER));
}
static void __cdecl GScr_ForceLevelEnd_SP()
{
    level.forceLevelEnd_SP = 1;
    Dvar_SetString(const_cast<dvar_s *>(nextmap), "");
}
SP_STUB_FUNCTION(GScr_SPStubFn_setdebugangles, "setdebugangles", "0x00803da0")
SP_STUB_FUNCTION(GScr_SPStubFn_setdebugorigin, "setdebugorigin", "0x00803d80")
SP_STUB_FUNCTION(GScr_SPStubFn_reportclientdisconnected, "reportclientdisconnected", "0x00804d70")
static void __cdecl GScr_ThreatBiasGroupExists_SP()
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_ParamError(0, "threatbiasgroupexists [name]", SCRIPTINSTANCE_SERVER);

    const unsigned __int16 groupName = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(GScr_GetThreatBiasGroupIndex_SP(groupName) >= 0, SCRIPTINSTANCE_SERVER);
}
static void __cdecl GScr_BadPlaceDelete_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1)
    {
        Scr_Error(SCRIPTINSTANCE_SERVER, "Incorrect number of parameters\n", false);
        return;
    }
    if (!*Scr_GetString(0, SCRIPTINSTANCE_SERVER))
    {
        Scr_Error(SCRIPTINSTANCE_SERVER, "badplace_delete called with name \"\"", false);
        return;
    }
    Path_FreeBadPlace(Path_FindBadPlace(Scr_GetConstString(0, SCRIPTINSTANCE_SERVER)));
    Actor_BadPlace_UpdateFleeingActors();
}
static void __cdecl GScr_ZeroGravityVolumeOn()
{
    CG_SetZeroGravityVolumeEnabled(Scr_GetString(0, SCRIPTINSTANCE_SERVER), true);
}

static void __cdecl GScr_ZeroGravityVolumeOff()
{
    CG_SetZeroGravityVolumeEnabled(Scr_GetString(0, SCRIPTINSTANCE_SERVER), false);
}

static void __cdecl GScr_SetEntGravityTrajectory_SP(scr_entref_t entref)
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1)
        Scr_Error(SCRIPTINSTANCE_SERVER, "Bad number of paramaters\n", false);
    gentity_s *ent = GetEntity(entref);
    if (ent->s.lerp.pos.trType == TR_GRAVITY || ent->s.lerp.pos.trType == TR_MOON_GRAVITY)
    {
        int enabled = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
        if (enabled == 0)
            ent->s.lerp.pos.trType = TR_GRAVITY;
        else if (enabled == 1)
            ent->s.lerp.pos.trType = TR_MOON_GRAVITY;
    }
}

static void __cdecl GScr_SetZombieShrink_SP(scr_entref_t entref)
{
    gentity_s *entity = GetEntity(entref);
    entity->s.un1.zombieShrink = (unsigned char)Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
}

static void GScr_SetWaterSheeting_SP(scr_entref_t entref)
{
    if (entref.classnum)
        Scr_ObjectError("not an entity", SCRIPTINSTANCE_SERVER);
    iassert(entref.entnum < MAX_GENTITIES);
    gentity_s *ent = &g_entities[entref.entnum];
    if (ent->s.eType != ET_PLAYER)
        Scr_Error("SetWaterSheeting called on an ent that's not a player.", SCRIPTINSTANCE_SERVER);
    int enabled = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    int duration = 0;
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2)
    {
        double milliseconds = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER) * 1000.0;
        duration = milliseconds >= -2147483648.0 && milliseconds < 2147483648.0
            ? static_cast<int>(milliseconds) : (std::numeric_limits<int>::min)();
    }
    SV_GameSendServerCommand(ent->s.clientNum, SV_CMD_RELIABLE, va("%c %i %i", '!', enabled, duration));
}

static void GScr_ActivateClientExploder_SP()
{
    int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c %i", '@', index));
}
static void GScr_DeactivateClientExploder_SP()
{
    int index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    index = std::bit_cast<int>(0u - static_cast<unsigned int>(index));
    SV_GameSendServerCommand(-1, SV_CMD_RELIABLE, va("%c %i", '@', index));
}
SP_STUB_FUNCTION(GScr_SPStubFn_splitviewallowed, "splitviewallowed", "0x00651a30")
static void GScr_BadPlaceArc_SP()
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 8)
    {
        Scr_Error("Incorrect usage for badplace_arc()\n", 0);
        return;
    }
    const char *nameText = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const unsigned int name = *nameText ? Scr_GetConstString(0, SCRIPTINSTANCE_SERVER) : 0;
    const int duration = GScr_BadPlaceDuration_SP(Scr_GetFloat(1, SCRIPTINSTANCE_SERVER));
    badplace_arc_t arc;
    Scr_GetVector(2, arc.origin, SCRIPTINSTANCE_SERVER);
    arc.radius = Scr_GetFloat(3, SCRIPTINSTANCE_SERVER);
    arc.halfheight = Scr_GetFloat(4, SCRIPTINSTANCE_SERVER) * 0.5f;
    float direction[3];
    Scr_GetVector(5, direction, SCRIPTINSTANCE_SERVER);
    const float yaw = vectoyaw(direction);
    const double leftAngle = static_cast<double>(yaw) - Scr_GetFloat(6, SCRIPTINSTANCE_SERVER);
    arc.angle0 = static_cast<float>(leftAngle);
    if (static_cast<double>(yaw) < leftAngle)
        Scr_Error("left angle < 0 in badplace_arc\n", 0);
    const double rightAngle = static_cast<double>(yaw) + Scr_GetFloat(7, SCRIPTINSTANCE_SERVER);
    arc.angle1 = static_cast<float>(rightAngle);
    if (rightAngle < static_cast<double>(yaw))
        Scr_Error("right angle < 0 in badplace_arc\n", 0);
    const float extent = arc.angle1 - arc.angle0;
    if (extent >= 360.0f)
    {
        arc.angle0 = 0.0f;
        arc.angle1 = 360.0f;
    }
    else
    {
        arc.angle0 = static_cast<float>(AngleNormalize360(arc.angle0));
        arc.angle1 = static_cast<float>(AngleNormalize360(arc.angle1));
    }
    const int teams = GScr_BadPlaceTeams_SP(8, "badplace_arc");
    Path_MakeBadPlaceArc_SP(name, duration, teams ? teams : 14, &arc);
}
static void GScr_RefreshHudAmmoCounter_SP()
{
    SV_GameSendServerCommand(-1, SV_CMD_CAN_IGNORE, va("%c \"%i\"", 74, 1));
}
static void __cdecl GScr_StartMultiplayerGame_SP()
{
    Cbuf_AddText(0, "startMultiplayer\n");
}

static void __cdecl GScr_WeaponDualWieldWeaponName_SP()
{
    const char *weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const unsigned int weaponIndex = G_GetWeaponIndexForName((char *)weaponName);
    Scr_VerifyWeaponIndex(weaponIndex, (char *)weaponName);

    const WeaponDef *weapDef = BG_GetWeaponDef(weaponIndex);
    if (weapDef->bDualWield && weapDef->dualWieldWeaponIndex)
        Scr_AddString((char *)BG_WeaponName(weapDef->dualWieldWeaponIndex), SCRIPTINSTANCE_SERVER);
    else
        Scr_AddConstString(scr_const.none, SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_DisableGrenadeSuicide_SP()
{
    G_SetGrenadeSuicideDisabled_SP(true);
}

static void __cdecl GScr_EnableGrenadeSuicide_SP()
{
    G_SetGrenadeSuicideDisabled_SP(false);
}

static void __cdecl GScr_SetAILimit_SP()
{
    const int limit = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( limit < 0 || limit > 32 )
        Scr_ParamError(0, "SetAILimit must take a value between 0 and 32 inclusive.", SCRIPTINSTANCE_SERVER);
    G_SetAILimit_SP(limit);
}

static void __cdecl GScr_GetAILimit_SP()
{
    Scr_AddInt(G_GetAILimit_SP(), SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_ResetAILimit_SP()
{
    G_SetAILimit_SP(0);
}
#endif // KISAK_SP

BuiltinFunctionDef functions[] =
{
  { "createprintchannel", GScr_CreatePrintChannel, 1 },
  { "setprintchannel", GScr_printChannelSet, 1 },
  { "print", print_0, 1 },
  { "println", println_0, 1 },
  { "iprintln", iprintln, 0 },
  { "iprintlnbold", iprintlnbold, 0 },
  { "print3d", GScr_print3d, 1 },
  { "box", GScr_box, 1 },
  { "line", GScr_line, 1 },
  { "linelist", GScr_linelist, 1 },
  { "debugstar", GScr_debugstar, 1 },
  { "circle", GScr_circle, 1 },
  { "sphere", GScr_sphere, 1 },
  { "getreflectionlocs", Scr_GetReflectionLocs, 1 },
  { "getreflectionorigin", Scr_GetReflectionOrigin, 1 },
  { "logstring", FUNCTION_NULLSUB, 0 },
  { "bbprint", FUNCTION_NULLSUB, 0 },
  { "getent", Scr_GetEnt, 0 },
  { "getentarray", Scr_GetEntArray, 0 },
  { "getnode", Scr_GetNode, 0 },
  { "getnodearray", Scr_GetNodeArray, 0 },
  { "getallnodes", Scr_GetAllNodes, 0 },
#ifdef KISAK_SP
  { "setenablenode", Scr_SetEnableNode, 0 },   // retail SP 0x00642a80
  { "linknodes", Scr_LinkNodes, 0 },           // retail SP 0x00563f90
  { "unlinknodes", Scr_UnlinkNodes, 0 },       // retail SP 0x00522eb0
#endif
  { "getatrloaded", GScr_GetAnimTreesLoaded, 0 },
  { "findanimbyname", GScr_FindAnimByName, 0 },
#ifndef KISAK_SP
  { "spawn", GScr_Spawn, 0 },
#endif
  { "spawncollision", GScr_SpawnCollision, 0 },
  { "spawntimedfx", GScr_SpawnTimedFX, 0 },
#ifndef KISAK_SP
  { "spawnvehicle", GScr_SpawnVehicle, 0 },
#endif
  { "spawnplane", GScr_SpawnPlane, 0 },
#ifndef KISAK_SP
  { "spawnturret", GScr_SpawnTurret, 0 },
#endif
  { "precacheturret", GScr_PrecacheTurret, 0 },
  { "spawnstruct", Scr_AddStruct, 0 },
  { "spawnhelicopter", GScr_SpawnHelicopter, 0 },
  { "assert", assertCmd_0, 1 },
  { "assertex", assertexCmd_0, 1 },
  { "assertmsg", assertmsgCmd_0, 1 },
  { "adddebugcommand", GScr_AddDebugCommand, 1 },
  { "isdefined", GScr_IsDefined, 0 },
  { "ismp", GScr_IsMP, 0 },
  { "isint", GScr_IsInt, 0 },
  { "isfloat", GScr_IsFloat, 0 },
  { "isvec", GScr_IsVec, 0 },
  { "isstring", GScr_IsString, 0 },
  { "isarray", GScr_IsArray, 0 },
  { "isalive", GScr_IsAlive, 0 },
  { "getdvar", GScr_GetDvar, 0 },
  { "getdvarint", GScr_GetDvarInt, 0 },
  { "getdvarfloat", GScr_GetDvarFloat, 0 },
  { "getdvarcolorred", GScr_GetDvarColorRed, 0 },
  { "getdvarcolorgreen", GScr_GetDvarColorGreen, 0 },
  { "getdvarcolorblue", GScr_GetDvarColorBlue, 0 },
  { "setdvar", GScr_SetDvar, 0 },
  { "gettime", GScr_GetTime, 0 },
  { "getentbynum", Scr_GetEntByNum, 1 },
  { "getweaponmodel", Scr_GetWeaponModel, 0 },
  { "getweaponstowedmodel", Scr_GetWeaponStowedModel, 0 },
  { "getanimlength", GScr_GetAnimLength, 0 },
  { "animhasnotetrack", GScr_AnimHasNotetrack, 0 },
  { "getnotetracktimes", GScr_GetNotetrackTimes, 0 },
  { "getbrushmodelcenter", GScr_GetBrushModelCenter, 0 },
  { "getattachmentindex", GScr_GetAttachmentIndex, 0 },
  // LWSS ADD FROM RETAIL BLOPS MP (LATEST FILES FROM STEAM REFERENCE NEWER FUNCTIONS THAT WERE ADDED LATER)
  { "getcorpsearray", GScr_GetCorpseArray, 0 },
#ifdef KISAK_SP
  { "clearallcorpses", G_RemoveAllActorCorpses, 0 },
#endif
  // LWSS END

  { "objective_add", Scr_Objective_Add, 0 },
  { "objective_delete", Scr_Objective_Delete, 0 },
#ifdef KISAK_SP
  { "objective_add_team", Scr_Objective_AddTeam_SP, 0 },
  { "objective_additionalcurrent", Scr_Objective_AdditionalCurrent_SP, 0 },
  { "objective_ring", Scr_Objective_Ring_SP, 0 },
  { "objective_set3d", Scr_Objective_Set3D_SP, 0 },
  { "objective_string", Scr_Objective_String_SP, 0 },
  { "objective_additionalposition", Scr_Objective_AdditionalPosition_SP, 0 },
#endif
  { "objective_state", Scr_Objective_State, 0 },
  { "objective_icon", Scr_Objective_Icon, 0 },
  { "objective_position", Scr_Objective_Position, 0 },
  { "objective_onentity", Scr_Objective_OnEntity, 0 },
  { "objective_current", Scr_Objective_Current, 0 },
  { "objective_setvisibletoplayer", Scr_Objective_SetVisibleToPlayer, 0 },
  { "objective_setinvisibletoplayer", Scr_Objective_SetInvisibleToPlayer, 0 },
  { "objective_setvisibletoall", Scr_Objective_SetVisibleToAll, 0 },
  { "objective_setinvisibletoall", Scr_Objective_SetInvisibleToAll, 0 },
  { "objective_setsize", Scr_Objective_SetSize, 0 },
  { "objective_setcolor", Scr_Objective_SetColor, 0 },
  { "missile_createattractorent", Scr_MissileCreateAttractorEnt, 0 },
  { "missile_createattractororigin", Scr_MissileCreateAttractorOrigin, 0 },
  { "missile_createrepulsorent", Scr_MissileCreateRepulsorEnt, 0 },
  { "missile_createrepulsororigin", Scr_MissileCreateRepulsorOrigin, 0 },
  { "missile_deleteattractor", Scr_MissileDeleteAttractor, 0 },
  { "bullettrace", Scr_BulletTrace, 0 },
#ifdef KISAK_SP
  { "groundtrace", Scr_GroundTrace_SP, 0 },                              // IMPLEMENTED from SP table entry 0x00B766D8, handler 0x00807C00
  { "createrope", Scr_CreateRope_SP, 0 },
  { "deleterope", Scr_DeleteRope_SP, 0 },
  { "ropesetparam", Scr_RopeSetParam_SP, 0 },
  { "ropeaddentityanchor", Scr_RopeAddEntityAnchor_SP, 0 },
  { "ropeaddworldanchor", Scr_RopeAddWorldAnchor_SP, 0 },
  { "roperemoveanchor", Scr_RopeRemoveAnchor_SP, 0 },
  { "ropemoveanchor", Scr_RopeMoveAnchor_SP, 0 },
  { "ropegetposition", Scr_RopeGetPosition_SP, 0 },
  { "getrope", Scr_GetRope_SP, 0 },
  { "zerogravityvolumeon", GScr_ZeroGravityVolumeOn, 0 },
  { "zerogravityvolumeoff", GScr_ZeroGravityVolumeOff, 0 },
  { "setphysicsgravitydir", GScr_SetPhysicsGravityDir_SP, 0 },
  { "setwaterbrush", GScr_SetWaterBrush_SP, 0 },
  { "ropesetflag", Scr_RopeSetFlag_SP, 0 },                              // IMPLEMENTED from SP table record 0x00B76B1C, handler 0x007FDD90
#endif
  { "bullettracepassed", Scr_BulletTracePassed, 0 },
  { "sighttracepassed", Scr_SightTracePassed, 0 },
  { "physicstrace", Scr_PhysicsTrace, 0 },
  { "playerphysicstrace", Scr_PlayerPhysicsTrace, 0 },
  // LWSS ADD FROM RETAIL BLOPS MP (LATEST FILES FROM STEAM REFERENCE NEWER FUNCTIONS THAT WERE ADDED LATER)
  { "playerbullettrace", Scr_PlayerBulletTrace, 0 },
  // LWSS END
  { "getmovedelta", GScr_GetMoveDelta, 0 },
  { "getangledelta", GScr_GetAngleDelta, 0 },
  { "getnorthyaw", GScr_GetNorthYaw, 0 },
#ifdef KISAK_SP
  { "setnorthyaw", GScr_SetNorthYaw_SP, 0 },
#endif
  { "randomint", Scr_RandomInt, 0 },
  { "randomfloat", Scr_RandomFloat, 0 },
  { "randomintrange", Scr_RandomIntRange, 0 },
  { "randomfloatrange", Scr_RandomFloatRange, 0 },
  { "log", GScr_log, 0 },
  { "sin", GScr_sin, 0 },
  { "cos", GScr_cos, 0 },
  { "tan", GScr_tan, 0 },
  { "asin", GScr_asin, 0 },
  { "acos", GScr_acos, 0 },
  { "atan", GScr_atan, 0 },
  { "int", GScr_CastInt, 0 },
  { "float", GScr_CastFloat, 0 },
  { "abs", GScr_abs, 0 },
  { "min", GScr_min, 0 },
  { "max", GScr_max, 0 },
  { "floor", GScr_floor, 0 },
  { "ceil", GScr_ceil, 0 },
  { "sqrt", GScr_sqrt, 0 },
  { "lerpfloat", GScr_LerpFloat, 0 },
  { "lerpvector", GScr_LerpVector, 0 },
  { "vectorfromlinetopoint", GScr_VectorFromLineToPoint, 0 },
  { "pointonsegmentnearesttopoint", GScr_PointOnSegmentNearestToPoint, 0 },
  { "distance", Scr_Distance, 0 },
  { "distance2d", Scr_Distance2D, 0 },
  { "distancesquared", Scr_DistanceSquared, 0 },
  { "length", Scr_Length, 0 },
  { "lengthsquared", Scr_LengthSquared, 0 },
  { "closer", Scr_Closer, 0 },
  { "vectordot", Scr_VectorDot, 0 },
  { "vectorcross", Scr_VectorCross, 0 },
  { "vectornormalize", Scr_VectorNormalize, 0 },
  { "vectortoangles", Scr_VectorToAngles, 0 },
  { "vectorlerp", Scr_VectorLerp, 0 },
  { "anglestoup", Scr_AnglesToUp, 0 },
  { "anglestoright", Scr_AnglesToRight, 0 },
  { "anglestoforward", Scr_AnglesToForward, 0 },
  { "combineangles", Scr_CombineAngles, 0 },
  { "angleclamp180", Scr_ClampAngle180, 0 },
  { "absangleclamp180", Scr_AbsAngleClamp180, 0 },
  { "rotatepoint", Scr_RotatePoint, 0 },
  { "issubstr", Scr_IsSubStr, 0 },
  { "getsubstr", Scr_GetSubStr, 0 },
  { "tolower", Scr_ToLower, 0 },
  { "strtok", Scr_StrTok, 0 },
  { "soundfade", Scr_SoundFade, 0 },
  { "playsoundatposition", Scr_PlaySoundAtPosition, 0 },
  { "getvehiclenode", GScr_GetVehicleNode, 0 },
  { "precachemodel", Scr_PrecacheModel, 0 },
  { "precacheshellshock", Scr_PrecacheShellShock, 0 },
  { "precacheitem", Scr_PrecacheItem, 0 },
  { "precacheshader", Scr_PrecacheShader, 0 },
  { "precachestring", Scr_PrecacheString, 0 },
  { "precacherumble", FUNCTION_NULLSUB, 0 },
  { "precachevehicle", GScr_PrecacheVehicle, 0 },
  { "precachemenu", GScr_PrecacheMenu, 0 },
  { "precachestatusicon", GScr_PrecacheStatusIcon, 0 },
  { "precacheheadicon", GScr_PrecacheHeadIcon, 0 },
  { "precachelocationselector", GScr_PrecacheLocationSelector, 0 },
  { "loadfx", Scr_LoadFX, 0 },
  { "playfx", Scr_PlayFX, 0 },
  { "playfxontag", Scr_PlayFXOnTag, 0 },
  { "getwaterheight", GScr_GetWaterHeight, 0 },
#ifndef KISAK_SP
  { "playloopedfx", Scr_PlayLoopedFX, 0 },
#endif
#ifndef KISAK_SP
  { "spawnfx", Scr_SpawnFX, 0 },
#endif
  { "triggerfx", Scr_TriggerFX, 0 },
  { "physicsexplosionsphere", Scr_PhysicsExplosionSphere, 0 },
  { "physicsexplosioncylinder", Scr_PhysicsExplosionCylinder, 0 },
  { "physicsjolt", Scr_PhysicsRadiusJolt, 0 },
  { "createstreamerhint", Scr_CreateStreamerHint, 0 },
  { "setexpfog", Scr_SetExponentialFog, 0 },
  { "setvolfog", Scr_SetVolumetricFog, 0 },
  { "setculldist", Scr_SetCullDist, 0 },
  { "grenadeexplosioneffect", Scr_GrenadeExplosionEffect, 0 },
  { "magicbullet", GScr_MagicBullet, 0 },
  { "radiusdamage", GScr_RadiusDamage, 0 },
  { "setplayerignoreradiusdamage", GScr_SetPlayerIgnoreRadiusDamage, 0 },
  { "glassradiusdamage", GScr_GlassRadiusDamage, 0 },
  { "getnumparts", GScr_GetNumParts, 0 },
  { "getpartname", GScr_GetPartName, 0 },
  { "earthquake", GScr_Earthquake, 0 },
  { "newhudelem", GScr_NewHudElem, 0 },
  { "newclienthudelem", GScr_NewClientHudElem, 0 },
  { "newteamhudelem", GScr_NewTeamHudElem, 0 },
  { "newscorehudelem", GScr_NewScoreHudElem, 0 },
  { "newdebughudelem", GScr_NewDebugHudElem, 0 },
  { "resettimeout", Scr_ResetTimeout, 0 },
  { "weaponfiretime", GScr_WeaponFireTime, 0 },
  { "weaponreloadtime", GScr_WeaponReloadTime, 0 },
  { "isweaponcliponly", GScr_IsWeaponClipOnly, 0 },
  { "isweapondetonationtimed", GScr_IsWeaponDetonationTimed, 0 },
  { "weaponclipsize", GScr_WeaponClipSize, 0 },
  { "weaponissemiauto", GScr_WeaponIsSemiAuto, 0 },
  { "weaponisboltaction", GScr_WeaponIsBoltAction, 0 },
  { "weapontype", GScr_WeaponType, 0 },
  { "weaponclass", GScr_WeaponClass, 0 },
  { "weaponmountable", GScr_WeaponIsMountable, 0 },
  { "weaponinventorytype", GScr_WeaponInventoryType, 0 },
  { "weaponstartammo", GScr_WeaponStartAmmo, 0 },
  { "weaponmaxammo", GScr_WeaponMaxAmmo, 0 },
  { "weaponaltweaponname", GScr_WeaponAltWeaponName, 0 },
#ifdef KISAK_SP
  { "weapondualwieldweaponname", GScr_WeaponDualWieldWeaponName_SP, 0 },            // IMPLEMENTED from SP 0x007FC800
#endif
  { "getwatcherweapons", GScr_GetWatcherWeapons, 0 },
  { "getretrievableweapons", GScr_GetRetrievableWeapons, 0 },
  { "getweaponmindamagerange", GScr_GetWeaponMinDamageRange, 0 },
  { "getweaponmaxdamagerange", GScr_GetWeaponMaxDamageRange, 0 },
  { "getweaponmindamage", GScr_GetWeaponMinDamage, 0 },
  { "getweaponmaxdamage", GScr_GetWeaponMaxDamage, 0 },
  { "getweaponfusetime", GScr_GetWeaponFuseTime, 0 },
  { "getweaponprojexplosionsound", GScr_GetWeaponProjExplosionSound, 0 },
  { "isweaponspecificuse", GScr_IsWeaponSpecificUse, 0 },
  { "isweaponscopeoverlay", GScr_IsWeaponScopeOverlay, 0 },
  { "isweaponequipment", GScr_IsWeaponEquipment, 0 },
  { "isweaponprimary", GScr_IsWeaponPrimary, 0 },
  { "isturretfiring", GScr_IsTurretFiring, 0 },
  { "getweaponfiresound", GScr_GetWeaponFireSound, 0 },
  { "getweaponfiresoundplayer", GScr_GetWeaponFireSoundPlayer, 0 },
  { "getweaponpickupsoundplayer", GScr_GetWeaponPickupSoundPlayer, 0 },
  { "getweaponpickupsound", GScr_GetWeaponPickupSound, 0 },
  { "getweaponindexfromname", GScr_GetWeaponIndexFromName, 0 },
  { "isplayer", GScr_IsPlayer, 0 },
  { "isplayernumber", GScr_IsPlayerNumber, 0 },
  { "setwinningplayer", GScr_SetWinningPlayer, 0 },
  { "setwinningteam", GScr_SetWinningTeam, 0 },
  { "announcement", GScr_Announcement, 0 },
  { "clientannouncement", GScr_ClientAnnouncement, 0 },
  { "getteamscore", GScr_GetTeamScore, 0 },
  { "setteamscore", GScr_SetTeamScore, 0 },
  { "setclientnamemode", GScr_SetClientNameMode, 0 },
  { "updateclientnames", GScr_UpdateClientNames, 0 },
  { "getteamplayersalive", GScr_GetTeamPlayersAlive, 0 },
  { "getdroppedweapons", GScr_GetDroppedWeapons, 0 },
  { "objective_team", GScr_Objective_Team, 0 },
  { "artilleryiconlocation", SetArtilleryIconLocation, 0 },
  { "logprint", GScr_LogPrint, 0 },
  { "worldentnumber", GScr_WorldEntNumber, 0 },
  { "obituary", GScr_Obituary, 0 },
  { "reviveobituary", GScr_ReviveObituary, 0 },
  { "adddemobookmark", GScr_AddDemoBookmark, 0 },
  { "positionwouldtelefrag", GScr_positionWouldTelefrag, 0 },
  { "boundswouldtelefrag", GScr_BoundsWouldTelefrag, 0 },
  { "recordusedspawnpoint", GScr_RecordUsedSpawnPoint, 0 },
  { "testspawnpoint", GScr_TestSpawnPoint, 0 },
  { "getstarttime", GScr_getStartTime, 0 },
  { "map_restart", GScr_MapRestart, 0 },
  { "exitlevel", GScr_ExitLevel, 0 },
  { "killserver", GScr_KillServer, 0 },
  { "addtestclient", GScr_AddTestClient, 0 },
  { "makedvarserverinfo", GScr_MakeDvarServerInfo, 0 },
  { "setbombtimer", GScr_SetBombTimer, 0 },
  { "setmatchflag", GScr_SetMatchFlag, 0 },
  { "setmatchtalkflag", GScr_SetMatchTalkFlag, 0 },
  { "setarchive", FUNCTION_NULLSUB, 0 },
  { "allclientsprint", GScr_AllClientsPrint, 0 },
  { "clientprint", GScr_ClientPrint, 0 },
  { "mapexists", GScr_MapExists, 0 },
  { "isvalidgametype", GScr_IsValidGameType, 0 },
  { "matchend", FUNCTION_NULLSUB, 0 },
  { "setplayerteamrank", FUNCTION_NULLSUB, 0 },
  { "sendranks", FUNCTION_NULLSUB, 0 },
  { "endparty", FUNCTION_NULLSUB, 0 },
  { "setteamspyplane", GScr_SetTeamSpyplane, 0 },
  { "getteamspyplane", GScr_GetTeamSpyplane, 0 },
  { "setteamsatellite", GScr_SetTeamSatellite, 0 },
  { "getteamsatellite", GScr_GetTeamSatellite, 0 },
  { "getassignedteam", GScr_GetAssignedTeam, 0 },
  { "uploadstats", GScr_UploadStats, 0 },
  { "getdefaultclassslot", GScr_GetDefaultClassSlot, 0 },
  { "getitemattachment", GScr_GetItemAttachment, 0 },
  { "getreffromitemindex", GScr_GetRefFromItemIndex, 0 },
  { "getitemgroupfromitemindex", GScr_GetItemGroupFromItemIndex, 0 },
  { "getbaseweaponitemindex", GScr_GetBaseWeaponItemIndex, 0 },
  { "getgametypeenumfromname", GScr_GetGameTypeEnumFromName, 0 },
  { "getwagergametypelist", GScr_GetWagerGametypeList, 0 },
  { "setscoreboardcolumns", GScr_SetScoreboardColumns, 0 },
  { "iscollectors", GScr_IsCollectors, 0 },
  { "recordplayerstats", GScr_SetPlayerStatsForMatchRecording, 0 },
  { "recordplayermatchend", GScr_SetPlayerFinalForMatchRecording, 0 },
  { "recordmatchbegin", GScr_SetBeginForMatchRecording, 0 },
  { "setvotestring", GScr_SetVoteString, 0 },
  { "setvotetime", GScr_SetVoteTime, 0 },
  { "setvoteyescount", GScr_SetVoteYesCount, 0 },
  { "setvotenocount", GScr_SetVoteNoCount, 0 },
  { "openfile", GScr_OpenFile, 1 },
  { "closefile", GScr_CloseFile, 1 },
  { "fprintln", GScr_FPrintln, 1 },
  { "fprintfields", GScr_FPrintFields, 1 },
  { "freadln", GScr_FReadLn, 1 },
  { "fgetarg", GScr_FGetArg, 1 },
  { "execdevgui", GScr_ExecDevgui, 1 },
  { "reportmtu", FUNCTION_NULLSUB, 0 },
  { "pcserverupdateplaylist", GScr_PCServerUpdatePlaylist, 0 },
  { "kick", GScr_KickPlayer, 0 },
  { "ban", GScr_BanPlayer, 0 },
  { "map", GScr_LoadMap, 0 },
  { "playrumbleonposition", FUNCTION_NULLSUB, 0 },
  { "playrumblelooponposition", FUNCTION_NULLSUB, 0 },
  { "stopallrumbles", FUNCTION_NULLSUB, 0 },
  { "soundexists", ScrCmd_SoundExists, 0 },
  { "issplitscreen", GScr_GetAssignedTeam, 0 },
  { "isglobalstatsserver", Scr_IsGlobalStatsServer, 0 },
  { "setminimap", GScr_SetMiniMap, 0 },
  { "setmapcenter", GScr_SetMapCenter, 0 },
  { "isdemorecording", GScr_IsDemoRecording, 0 },
  { "isdemoenabled", isDemoEnabled, 0 },
  { "setdemointermissionpoint", GScr_SetDemoIntermissionPoint, 0 },
  { "startdemorecording", GScr_StartDemoRecording, 0 },
  { "stopdemorecording", GScr_StopDemoRecording, 0 },
  { "setgameendtime", GScr_SetGameEndTime, 0 },
  { "settimescale", GScr_SetTimeScale, 0 },
  { "incrementescrow", GScr_IncrementEscrow, 0 },
  { "getarraykeys", GScr_GetArrayKeys, 0 },
  { "searchforonlinegames", FUNCTION_NULLSUB, 0 },
  { "quitlobby", FUNCTION_NULLSUB, 0 },
  { "quitparty", FUNCTION_NULLSUB, 0 },
  { "startparty", FUNCTION_NULLSUB, 0 },
  { "startprivatematch", FUNCTION_NULLSUB, 0 },
  { "visionsetnaked", Scr_VisionSetNaked, 0 },
  { "visionsetnight", Scr_VisionSetNight, 0 },
  { "tablelookup", Scr_TableLookup, 0 },
  { "tablelookupistring", Scr_TableLookupIString, 0 },
  { "tablelookuprownum", Scr_TableLookupRowNum, 0 },
  { "tablelookupcolumnforrow", Scr_TableLookupColumnForRow, 0 },
  { "endlobby", FUNCTION_NULLSUB, 0 },
  { "clientsysregister", GScr_ClientSysRegister, 0 },
  { "clientsyssetstate", GScr_ClientSysSetState, 0 },
  { "isai", GScr_IsAI, 0 },
  { "getaitriggerflags", GScr_GetAITriggerFlags, 0 },
  { "isvehicle", GScr_IsVehicle, 0 },
  { "getmaxvehicles", GScr_GetMaxVehicles, 0 },
#ifdef KISAK_SP
  { "getnumvehicles", GScr_GetNumVehicles, 0 },
#endif
  { "getvehicletreadfxarray", GScr_GetVehicleTreadFXArray, 0 },
  { "disabledestructiblepieces", GScr_DisableDestructiblePieces, 0 },
  { "enablealldestructiblepieces", GScr_EnableAllDestructiblePieces, 0 },
  { "createdynentandlaunch", GScr_CreateDynEntAndLaunch, 0 },
  { "getvehicletriggerflags", GScr_GetMaxVehicles, 0 },
  { "collisiontestpointsinsphere", GScr_CollisionTestPointsInSphere, 0 },
  { "collisiontestpointsincylinder", GScr_CollisionTestPointsInCylinder, 0 },
  { "collisiontestpointsinpill", GScr_CollisionTestPointsInPill, 0 },
  { "collisiontestpointsincone", GScr_CollisionTestPointsInCone, 0 },
  { "collisiontestpointsinbox", GScr_CollisionTestPointsInBox, 0 },
  {
    "qsortscoredspawnpointsascending",
    GScr_QSortScoredSpawnPointArrayAscending,
    0
  },
  { "matrix4x4transformpoints", GScr_Matrix4x4TransformPoints, 0 },
  { "setspawnpointrandomvariation", GScr_SetSpawnPointRandomVariation, 0 },
  { "clearspawnpoints", GScr_ClearSpawnPoints, 0 },
  { "addspawnpoints", GScr_AddSpawnPoints, 0 },
  { "getsortedspawnpoints", GScr_GetSortedSpawnPoints, 0 },
  { "clearspawnpointsbaseweight", GScr_ClearSpawnPointsBaseWeight, 0 },
  { "setspawnpointsbaseweight", GScr_SetSpawnPointsBaseWeight, 0 },
  { "getplayerspawnid", FUNCTION_NULLSUB, 0 },
  { "isspawnpointvisible", GScr_IsSpawnPointVisible, 0 },
  { "addsphereinfluencer", GScr_AddSphereInfluencer, 0 },
  { "addcylinderinfluencer", GScr_AddCylinderInfluencer, 0 },
  { "removeinfluencer", GScr_RemoveInfluencer, 0 },
  { "enableinfluencer", GScr_EnableInfluencer, 0 },
  { "setinfluencerteammask", GScr_SetInfluencerTeamMask, 0 },
  { "setdebugsideswitch", GScr_SetDebugSideSwitch, 1 },
  { "target_set", Scr_Target_Set, 0 },
  { "target_remove", Scr_Target_Remove, 0 },
  { "target_setshader", Scr_Target_SetShader, 0 },
  { "target_setoffscreenshader", Scr_Target_SetOffscreenShader, 0 },
  { "target_isinrect", Scr_Target_IsInRect, 0 },
  { "target_isincircle", Scr_Target_IsInCircle, 0 },
  { "target_startreticlelockon", Scr_Target_StartLockOn, 0 },
  { "target_clearreticlelockon", Scr_Target_ClearLockOn, 0 },
  { "target_getarray", Scr_Target_GetArray, 0 },
  { "target_istarget", Scr_Target_IsTarget, 0 },
  { "target_setattackmode", Scr_Target_SetAttackMode, 0 },
  { "target_setjavelinonly", Scr_Target_SetJavelinOnly, 0 },
  { "target_setturretaquire", Scr_Target_SetTurretAquire, 0 },
  { "getnumgvrules", GScr_GetNumGVRules, 0 },
  { "getgvrule", GScr_GetGVRule, 0 },
  { "getmaxactivecontracts", GScr_GetMaxActiveContracts, 0 },
  { "getcontractstattype", GScr_GetContractStatType, 0 },
  { "getcontractstatname", GScr_GetContractStatName, 0 },
  { "getcontractrewardxp", GScr_GetContractRewardXP, 0 },
  { "getcontractrewardcp", GScr_GetContractRewardCP, 0 },
  { "getcontractrequirements", GScr_GetContractRequirements, 0 },
  { "getcontractname", GScr_GetContractName, 0 },
  { "getcontractrequiredcount", GScr_GetContractRequiredCount, 0 },
  { "getcontractresetconditions", GScr_GetContractResetConditions, 0 },
  { "getfogsettings", GScr_GetFogSettings, 0 },
  { "getcustomclassloadoutitem", Gscr_GetCustomClassLoadoutItem, 0 },
  { "getcustomclassmodifier", Gscr_GetCustomClassLoadoutModifier, 0 },
  { "pixbeginevent", GScr_PixBeginEvent, 0 },
  { "pixendevent", GScr_PixEndEvent, 0 },
  { "pixmarker", GScr_PixMarker, 0 },
  { "changeadvertisedstatus", FUNCTION_NULLSUB, 0 },
  { "setqosgamedatapayload", FUNCTION_NULLSUB, 0 },
  { "resetqosgamedatapayload", FUNCTION_NULLSUB, 0 },
  { "incrementcounter", GScr_IncrementCounter, 0 },
  { "getcountertotal", GScr_GetCounterTotal, 0 },
  { "enableoccluder", GScr_EnableOccluder, 0 },
  { "ispregameenabled", GScr_IsPregameEnabled, 0 },
  { "ispregamegamestarted", GScr_IsPregameGameStarted, 0 },
  { "pregamestartgame", GScr_PregameStartGame, 0 },
  { "resetpregamedata", GScr_ResetPregameData, 0 },
  { "sethostmigrationstatus", FUNCTION_NULLSUB, 0 },
  // LWSS ADD FROM RETAIL BLOPS MP (LATEST FILES FROM STEAM REFERENCE NEWER FUNCTIONS THAT WERE ADDED LATER)
  { "starthostmigration", FUNCTION_NULLSUB, 0 }, // (Stubbing these out because who cares)
  { "reportfilm", FUNCTION_NULLSUB, 0 },
  // LWSS END
#ifdef KISAK_SP
  // TODO(SP-STUB): retail-SP builtin FUNCTIONS referenced by the SP script
  // corpus and absent from this table. Each handler below is a no-op stub that
  // warns once and evaluates to undefined -- NOT an implementation. See the
  // TODO(SP-STUB) header above this array for why an empty body is stack-safe
  // and for what a future pass still owes. Verified before adding: none of
  // these names was already present in functions[]. Trailing comment on each
  // row is the retail SP handler address for that name.
  { "getaiarray", GScr_GetAIArray_SP, 0 },                                         // IMPLEMENTED from SP 0x007f0970
  { "issentient", GScr_SPStubFn_issentient, 0 },                                    // TODO(SP-STUB) SP 0x007f00a0
  { "getplayers", GScr_GetPlayers_SP, 0 },                                          // real impl; SP 0x007f0a10
  { "setsunlight", GScr_SetSunLight_SP, 0 },
  { "resetsunlight", GScr_ResetSunLight_SP, 0 },
  { "getmapsundirection", GScr_GetMapSunDirection_SP, 0 },
  { "setcellinvisibleatpos", GScr_SetCellInvisibleAtPos_SP, 0 },
  { "setcellvisibleatpos", GScr_SetCellVisibleAtPos_SP, 0 },
  { "setsundirection", GScr_SetSunDirection_SP, 0 },
  { "lerpsundirection", GScr_LerpSunDirection_SP, 0 },
  { "resetsundirection", GScr_ResetSunDirection_SP, 0 },
  { "getcycleoriginoffset", GScr_GetCycleOriginOffset_SP, 0 },
  { "getstartorigin", GScr_GetStartOrigin_SP, 0 },                                  // IMPLEMENTED from SP handler 0x005c8770
  { "getaispeciesarray", GScr_GetAISpeciesArray_SP, 0 },                           // IMPLEMENTED from SP 0x007f0b20
  { "getspawnerarray", GScr_GetSpawnerArray_SP, 0 },                                 // IMPLEMENTED from SP 0x007f0be0
  { "getitemarray", GScr_GetItemArray_SP, 0 },
  { "getstartangles", GScr_GetStartAngles_SP, 0 },                                  // IMPLEMENTED from SP handler 0x004dcfc0
  { "getcinematictimeremaining", GScr_GetCinematicTimeRemaining_SP, 0 }, // SP 0x007FBF20
  { "codespawn", GScr_CodeSpawn_SP, 0 },                                            // IMPLEMENTED from SP handler 0x007f15c0
  { "getnotetracksindelta", GScr_GetNotetracksInDelta_SP, 0 },                    // SP 0x007f1180
  { "getdifficulty", GScr_GetDifficulty_SP, 0 },                                    // IMPLEMENTED from SP 0x007f07e0 (name table 0x00b75ecc + g_gameskill)
  { "isassetloaded", GScr_IsAssetLoaded_SP, 0 },                                    // IMPLEMENTED from SP 0x007faad0 -> 0x00694550 -> DB_FindXAssetEntry
  { "codeplayloopedfx", GScr_CodePlayLoopedFX_SP, 0 },
  { "gettimescale", GScr_GetTimeScale_SP, 0 },
  { "isgodmode", GScr_IsGodMode_SP, 0 },
  { "issaverecentlyloaded", GScr_Checkpoint_issaverecentlyloaded_SP, 0 },
  { "numremoteclients", GScr_SPStubFn_numremoteclients, 0 },                        // TODO(SP-STUB) SP 0x00642850
  { "visionsetlaststand", GScr_VisionSetLastStand_SP, 0 },
  { "anglelerp", GScr_AngleLerp_SP, 0 },
  { "absangleclamp360", GScr_AbsAngleClamp360_SP, 0 },
  { "codespawnfx", Scr_SpawnFX, 0 },
  { "distance2dsquared", GScr_SP_distance2dsquared, 0 },                           // SP 0x007f92c0
  { "findpath", GScr_FindPath_SP, 0 },
  { "hascollectible", GScr_HasCollectible_SP, 0 },
  { "oktospawn", GScr_OkToSpawn_SP, 0 },
  { "savegamenocommit", GScr_Checkpoint_savegamenocommit_SP, 0 },
  { "codespawnvehicle", GScr_CodeSpawnVehicle_SP, 0 },
  { "getanynodearray", Scr_GetAnyNodeArray_SP, 0 },
  { "getdynmodels", GScr_GetDynModels_SP, 0 },
  { "getmiscmodels", GScr_GetMiscModels_SP, 0 },
  { "getnumconnectedplayers", GScr_GetNumConnectedPlayers_SP, 0 },                  // IMPLEMENTED from SP 0x0068e8b0
  { "getnumexpectedplayers", GScr_GetNumExpectedPlayers_SP, 0 },                    // IMPLEMENTED from SP 0x005e6b20
  { "getpersistentprofilevar", GScr_GetPersistentProfileVar_SP, 0 },
  { "getspawnerteamarray", GScr_GetSpawnerTeamArray_SP, 0 },
  { "getweaponaccuracy", GScr_GetWeaponAccuracy_SP, 0 },
  { "getweaponclipmodel", GScr_GetWeaponClipModel_SP, 0 },
  { "isnodeoccupied", GScr_IsNodeOccupied_SP, 0 },
  { "getnodeowner", GScr_GetNodeOwner_SP, 0 },
  { "issavesuccessful", GScr_Checkpoint_issavesuccessful_SP, 0 },
  { "weaponfightdist", GScr_WeaponFightDist_SP, 0 },                                // IMPLEMENTED from SP 0x007fc870
  { "weaponisgasweapon", GScr_WeaponIsGasWeapon_SP, 0 },
  { "weaponmaxdist", GScr_WeaponMaxDist_SP, 0 },                                    // IMPLEMENTED from SP 0x007fc8c0
  { "weaponmaxgibdistance", GScr_WeaponMaxGibDistance_SP, 0 },
  { "bulletspread", GScr_BulletSpread_SP, 0 },
  { "canspawnturret", GScr_CanSpawnTurret_SP, 0 },
  { "codespawnturret", GScr_CodeSpawnTurret_SP, 0 },
  { "entsearch", GScr_EntSearch_SP, 0 },
  { "getallvehiclenodes", GScr_GetAllVehicleNodes, 0 },
  { "setvehiclenodeenabled", GScr_SetVehicleNodeEnabled, 0 },
  { "getdestructibledefs", GScr_GetDestructibleDefs_SP, 0 },
  { "getsnapshotindexarray", GScr_SPStubFn_getsnapshotindexarray, 0 },              // TODO(SP-STUB) SP 0x007ff1b0
  { "getvehiclenodearray", GScr_GetVehicleNodeArray, 0 },
  { "iscoopepd", GScr_IsCoopEPD_SP, 0 },
  { "isturretactive", GScr_IsTurretActive_SP, 0 },
  { "modelhasphyspreset", GScr_ModelHasPhysPreset_SP, 0 },
  { "playerpositionvalid", GScr_PlayerPositionValid_SP, 0 },
  { "snapshotacknowledged", GScr_SPStubFn_snapshotacknowledged, 0 },                // TODO(SP-STUB) SP 0x007ff230
  { "weapondogibbing", GScr_WeaponDoGibbing_SP, 0 },
  { "setsaveddvar", GScr_SetSavedDvar_SP, 0 },                            // IMPLEMENTED from SP functions 0x00B75EE0 idx 81, handler 0x007f06a0
  { "badplace_cylinder", GScr_SPStubFn_badplace_cylinder, 0 },            // TODO(SP-STUB) SP functions 0x00B75EE0 idx 345, 0x00800060
  { "badplace_brush", GScr_BadPlaceBrush_SP, 0 },
  { "makelocalizedstring", GScr_MakeLocalizedString_SP, 0 },
  { "setignoremegroup", GScr_SetIgnoreMeGroup_SP, 0 },
  { "getthreatbias", GScr_GetThreatBias_SP, 0 },                         // IMPLEMENTED from SP 0x00819320
  { "start3dcinematic", GScr_Start3DCinematic_SP, 0 },                    // IMPLEMENTED from SP functions 0x00B75EE0 idx 295, handler 0x007fbd50
  { "stop3dcinematic", GScr_Stop3DCinematic_SP, 0 },                      // IMPLEMENTED from SP functions 0x00B75EE0 idx 296, handler 0x007fbf40
  { "cleanupspawneddynents", GScr_CleanupSpawnedDynEnts_SP, 0 },          // Retail SP 0x00805340; real client cleanup via '>'.
  { "getdebugdvar", GScr_GetDebugDvar_SP, 1 },
  { "getdebugdvarint", GScr_GetDebugDvarInt_SP, 1 },
  { "getdebugdvarfloat", GScr_GetDebugDvarFloat_SP, 1 },
  { "record3dtext", GScr_RecordDebug_SP, 1 },
  { "recordenttext", GScr_RecordDebug_SP, 1 },
  { "recordent", GScr_RecordDebug_SP, 1 },
  { "recordline", GScr_RecordDebug_SP, 1 },
  { "missionfailed", GScr_MissionFailed_SP, 0 },
  { "changelevel", GScr_ChangeLevel_SP, 0 },                              // IMPLEMENTED from SP handler 0x007FBBB0 + scheduler/checker 0x007E30B0/0x0041DE40
  { "setthreatbias", GScr_SetThreatBias_SP, 0 },                         // IMPLEMENTED from SP 0x008193D0
  { "savegame", GScr_Checkpoint_savegame_SP, 0 },
  { "missionsuccess", GScr_MissionSuccess_SP, 0 },
  { "setmissiondvar", GScr_SetMissionDvar_SP, 0 },
  { "pause3dcinematic", GScr_Pause3DCinematic_SP, 0 },                    // IMPLEMENTED from SP functions 0x00B75EE0 idx 297, handler 0x007fbef0
  { "bullettracer", GScr_BulletTracer_SP, 0 },
  { "setpersistentprofilevar", GScr_SetPersistentProfileVar_SP, 0 },
  { "updategamerprofile", GScr_UpdateGamerProfile_SP, 0 },
  { "commitsave", GScr_Checkpoint_commitsave_SP, 0 },
  { "setuinextlevel", GScr_SPStubFn_setuinextlevel, 0 },                  // TODO(SP-STUB) SP functions 0x00B75EE0 idx 391, 0x00651a30
  { "setheliheightpatchenabled", GScr_SetHeliHeightPatchEnabled_SP, 0 },
  { "prefetchlevel", GScr_SPStubFn_prefetchlevel, 0 },                    // TODO(SP-STUB) SP functions 0x00B75EE0 idx 274, 0x00651a30
  { "createthreatbiasgroup", GScr_CreateThreatBiasGroup_SP, 0 },         // IMPLEMENTED from SP 0x00819290
  { "setcollectible", GScr_SetCollectible_SP, 0 },
  { "forcelevelend", GScr_ForceLevelEnd_SP, 0 },
  { "setdebugangles", GScr_SPStubFn_setdebugangles, 0 },                  // TODO(SP-STUB) SP functions 0x00B75EE0 idx 362, 0x00803da0, retail type 1 -> 0 here
  { "setdebugorigin", GScr_SPStubFn_setdebugorigin, 0 },                  // TODO(SP-STUB) SP functions 0x00B75EE0 idx 361, 0x00803d80, retail type 1 -> 0 here
  { "reportclientdisconnected", GScr_SPStubFn_reportclientdisconnected, 0 }, // TODO(SP-STUB) SP functions 0x00B75EE0 idx 388, 0x00804d70
  { "watersimenable", GScr_WaterSimEnable_SP, 0 },                        // IMPLEMENTED from SP functions 0x00B75EE0 idx 393, handler 0x007fa500
  { "threatbiasgroupexists", GScr_ThreatBiasGroupExists_SP, 0 },         // IMPLEMENTED from SP 0x008192D0
  { "badplace_delete", GScr_BadPlaceDelete_SP, 0 },
  { "activateclientexploder", GScr_ActivateClientExploder_SP, 0 },
  { "deactivateclientexploder", GScr_DeactivateClientExploder_SP, 0 },
  { "splitviewallowed", GScr_SPStubFn_splitviewallowed, 0 },              // TODO(SP-STUB) SP functions 0x00B75EE0 idx 401, 0x00651a30
  { "badplace_arc", GScr_BadPlaceArc_SP, 0 },
  { "refreshhudammocounter", GScr_RefreshHudAmmoCounter_SP, 0 },
  { "startmultiplayergame", GScr_StartMultiplayerGame_SP, 0 },           // IMPLEMENTED from retail SP handler 0x008052d0
  { "disablegrenadesuicide", GScr_DisableGrenadeSuicide_SP, 0 },        // IMPLEMENTED from SP functions table entry 0x00B77284, handler 0x00804D40
  { "enablegrenadesuicide", GScr_EnableGrenadeSuicide_SP, 0 },          // IMPLEMENTED from SP functions table entry 0x00B77290, handler 0x00804D50
  { "setailimit", GScr_SetAILimit_SP, 0 },                              // IMPLEMENTED from SP functions table entry 0x00B77254, handler 0x00804AD0
  { "getailimit", GScr_GetAILimit_SP, 0 },                              // IMPLEMENTED from SP functions table entry 0x00B77260, handler 0x00804B10
  { "resetailimit", GScr_ResetAILimit_SP, 0 },                          // IMPLEMENTED from SP functions table entry 0x00B77278, handler 0x00804D60
#endif // KISAK_SP
};



void (__cdecl *__cdecl Scr_GetFunction(const char **pName, int *type))()
{
    unsigned int i; // [esp+18h] [ebp-4h]

#ifdef KISAK_SP
    *type = 0;
#endif

    for (i = 0; i < ARRAY_COUNT(functions); ++i)
    {
        if (!strcmp(*pName, functions[i].actionString))
        {
            *pName = functions[i].actionString;
            *type = functions[i].type;
            return functions[i].actionFunc;
        }
    }
    return 0;
}

// LWSS ADD
void __cdecl GScr_GetClientFlag(scr_entref_t entref)
{
    const char *v1; // eax
    gentity_s *pSelf; // [esp+0h] [ebp-8h]
    int flag; // [esp+4h] [ebp-4h]

    pSelf = GetEntity(entref);
    flag = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);

    if ((unsigned int)flag < 0x10)
    {
        if (pSelf->client)
            Scr_AddInt((pSelf->client->ps.eFlags2 >> flag) & 1, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddInt((pSelf->s.lerp.eFlags2 >> flag) & 1, SCRIPTINSTANCE_SERVER);
    }
    else
    {
        Scr_ParamError(0, va("SetClientFlag: Index %i out of range (0 - %i)\n", flag, 15), SCRIPTINSTANCE_SERVER);
    }
}
// LWSS END

void __cdecl GScr_SetClientFlag(scr_entref_t entref)
{
    const char *v1; // eax
    gentity_s *pSelf; // [esp+0h] [ebp-8h]
    int flag; // [esp+4h] [ebp-4h]

    pSelf = GetEntity(entref);
    flag = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)flag < 0x10 )
    {
        if ( pSelf->client )
            pSelf->client->ps.eFlags2 |= 1 << flag;
        else
            pSelf->s.lerp.eFlags2 |= 1 << flag;
    }
    else
    {
        v1 = va("SetClientFlag: Index %i out of range (0 - %i)\n", flag, 15);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_ClearClientFlag(scr_entref_t entref)
{
    const char *v1; // eax
    gentity_s *pSelf; // [esp+0h] [ebp-8h]
    int flag; // [esp+4h] [ebp-4h]

    pSelf = GetEntity(entref);
    flag = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)flag < 0x10 )
    {
        if ( pSelf->client )
            pSelf->client->ps.eFlags2 &= ~(1 << flag);
        else
            pSelf->s.lerp.eFlags2 &= ~(1 << flag);
    }
    else
    {
        v1 = va("ClearClientFlag: Index %i out of range (0 - %i)\n", flag, 15);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

void __cdecl GScr_IsMissileInsideHeightLock(scr_entref_t entref)
{
    float meshMaxs[3]; // [esp+8h] [ebp-20h] BYREF
    float meshMins[3]; // [esp+14h] [ebp-14h] BYREF
    int i; // [esp+20h] [ebp-8h]
    gentity_s *pSelf; // [esp+24h] [ebp-4h]

    pSelf = GetEntity(entref);
    if (!pSelf
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 18507, 0, "%s", "pSelf"))
    {
        __debugbreak();
    }
    for (i = 0; i < num_heli_height_lock_patches; ++i)
    {
        CM_ModelBounds(heli_height_lock_patches[i].brushmodel, meshMins, meshMaxs);
        meshMins[0] = meshMins[0] + heli_height_lock_patches[i].origin[0];
        meshMins[1] = meshMins[1] + heli_height_lock_patches[i].origin[1];
        meshMins[2] = meshMins[2] + heli_height_lock_patches[i].origin[2];
        meshMaxs[0] = meshMaxs[0] + heli_height_lock_patches[i].origin[0];
        meshMaxs[1] = meshMaxs[1] + heli_height_lock_patches[i].origin[1];
        meshMaxs[2] = meshMaxs[2] + heli_height_lock_patches[i].origin[2];
        if (pSelf->r.currentOrigin[0] >= meshMins[0]
            && meshMaxs[0] >= pSelf->r.currentOrigin[0]
            && pSelf->r.currentOrigin[1] >= meshMins[1]
            && meshMaxs[1] >= pSelf->r.currentOrigin[1])
        {
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
            return;
        }
    }
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}

// LWSS ADD
void GScr_GetGroundEnt(scr_entref_t entref)
{
    gentity_s *pSelf;
    int groundEntityNum;

    pSelf = GetEntity(entref);

    if (pSelf->client)
        groundEntityNum = pSelf->client->ps.groundEntityNum;
    else
        groundEntityNum = pSelf->s.groundEntityNum;

    if (groundEntityNum < 1023)
        Scr_AddEntity(&level.gentities[groundEntityNum], SCRIPTINSTANCE_SERVER);
}

#ifdef KISAK_SP
void __cdecl GScr_IsOnGround(scr_entref_t entref)
{
    gentity_s *pSelf;
    int worldOnly;

    pSelf = GetEntity(entref);
    if ( !pSelf
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 18532, 0, "%s", "pSelf") )
    {
        __debugbreak();
    }
    worldOnly = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
        worldOnly = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( pSelf->client )
    {
        if ( (pSelf->client->ps.eFlags & 0x300) != 0
            || pSelf->client->ps.groundEntityNum == 1022
            || !worldOnly && pSelf->client->ps.groundEntityNum != 1023 )
        {
LABEL_18:
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
            return;
        }
    }
    else if ( pSelf->s.groundEntityNum == 1022 || !worldOnly && pSelf->s.groundEntityNum != 1023 )
    {
        goto LABEL_18;
    }
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}
#else
void __cdecl GScr_IsOnGround(scr_entref_t entref)
{
    gentity_s *pSelf; // [esp+0h] [ebp-8h]
    int worldOnly; // [esp+4h] [ebp-4h]

    pSelf = GetEntity(entref);
    if ( !pSelf
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp", 18532, 0, "%s", "pSelf") )
    {
        __debugbreak();
    }
    worldOnly = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
        worldOnly = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( pSelf->client )
    {
        if ( (pSelf->client->ps.eFlags & 0x300) != 0
            || (pSelf->client->ps.eFlags & 0x4000) != 0
            || pSelf->client->ps.groundEntityNum == 1022
            || !worldOnly && pSelf->client->ps.groundEntityNum != 1023 )
        {
LABEL_18:
            Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
            return;
        }
    }
    else if ( pSelf->s.groundEntityNum == 1022 || !worldOnly && pSelf->s.groundEntityNum != 1023 )
    {
        goto LABEL_18;
    }
    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
}
#endif

XAnimTree_s *__cdecl GScr_GetEntAnimTree(gentity_s *ent)
{
    const char *EntityTypeName; // eax
    const char *v2; // eax
    char *v4; // [esp-4h] [ebp-24h]
    double v5; // [esp+0h] [ebp-20h]
    double v6; // [esp+8h] [ebp-18h]
    double v7; // [esp+10h] [ebp-10h]
    XAnimTree_s *tree; // [esp+1Ch] [ebp-4h]

    tree = G_GetEntAnimTree(ent);
    if ( !tree )
    {
        v7 = ent->r.currentOrigin[2];
        v6 = ent->r.currentOrigin[1];
        v5 = ent->r.currentOrigin[0];
        v4 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        EntityTypeName = G_GetEntityTypeName(ent);
        v2 = va(
                     "entity of type '%s', classname '%s', origin (%f, %f, %f) does not have an animation tree",
                     EntityTypeName,
                     v4,
                     v5,
                     v6,
                     v7);
        Scr_Error(v2, 0);
    }
    return tree;
}

void __cdecl G_FlagAnimForUpdate(gentity_s *ent)
{
    if ( (ent->flags & FL_NO_AUTO_ANIM_UPDATE) == 0 )
        ent->flags |= 0x40000u;
}

void __cdecl GScr_SetAnim(scr_entref_t entref)
{
    PROF_SCOPED("SetAnim");
    GScr_SetAnimInternal(entref, 1);
}

#ifdef KISAK_SP

#endif

void __cdecl GScr_SetAnimInternal(scr_entref_t entref, char flags)
{
    unsigned int v2; // [esp+1Ch] [ebp-38h]
    unsigned int notifyType; // [esp+20h] [ebp-34h]
    float rate; // [esp+30h] [ebp-24h] BYREF
    XAnimTree_s *tree; // [esp+34h] [ebp-20h]
    DObj *obj; // [esp+38h] [ebp-1Ch]
    int cmdIndex; // [esp+3Ch] [ebp-18h]
    float goalWeight; // [esp+40h] [ebp-14h] BYREF
    float goalTime; // [esp+44h] [ebp-10h] BYREF
    int error; // [esp+48h] [ebp-Ch]
    scr_anim_s anim; // [esp+4Ch] [ebp-8h]
    gentity_s *ent; // [esp+50h] [ebp-4h]

    ent = GetEntity(entref);
    tree = GScr_GetEntAnimTree(ent);
    rate = 1.0f;
    goalTime = 0.2f;
    goalWeight = 1.0f;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        anim = Scr_GetAnim(0, tree, SCRIPTINSTANCE_SERVER);
        XAnimGetParamValue(tree, anim.index, "rate", &rate);
        XAnimGetParamValue(tree, anim.index, "goaltime", &goalTime);
        XAnimGetParamValue(tree, anim.index, "goalweight", &goalWeight);
    }
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 1:
            goto $LN7_51;
        case 2:
            goto $LN9_46;
        case 3:
            goto $LN11_35;
        case 4:
            goto $LN12_25;
        default:
            Scr_Error("too many parameters", 0);
$LN12_25:
            rate = GScr_GetOptionalFloat(3u, rate);
            if ( rate < 0.0 )
                Scr_ParamError(3u, "must set nonnegative rate", SCRIPTINSTANCE_SERVER);
$LN11_35:
            goalTime = GScr_GetOptionalFloat(2u, goalTime);
            if ( goalTime < 0.0 )
                Scr_ParamError(2u, "must set nonnegative goal time", SCRIPTINSTANCE_SERVER);
$LN9_46:
            goalWeight = GScr_GetOptionalFloat(1u, goalWeight);
            if ( goalWeight < 0.0 || goalWeight > 1.0 )
                Scr_ParamError(1u, "must set nonnegative weight", SCRIPTINSTANCE_SERVER);
$LN7_51:
            anim = Scr_GetAnim(0, tree, SCRIPTINSTANCE_SERVER);
            obj = Com_GetServerDObj(ent->s.number);
            if ( !obj )
                Scr_ObjectError("No model exists.", SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
            cmdIndex = G_StoreAnimCommand_SP(ent, tree, 3, anim.index, 0, goalWeight, goalTime, rate, flags);
#else
            cmdIndex = 0;
#endif
            if ( (flags & 1) != 0 )
            {
                if ( goalWeight <= 0.001 )
                    notifyType = 0;
                else
                    notifyType = 2;
                error = XAnimSetCompleteGoalWeight(
                                    obj,
                                    anim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    0,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            else
            {
                if ( goalWeight <= 0.001 )
                    v2 = 0;
                else
                    v2 = 2;
                error = XAnimSetGoalWeight(obj, anim.index, goalWeight, goalTime, rate, 0, v2, (flags & 2) != 0, cmdIndex);
            }
            if ( error )
                GScr_HandleAnimError(error);
            else
                G_FlagAnimForUpdate(ent);
            return;
    }
}

void __cdecl GScr_HandleAnimError(int error)
{
    if ( error == 1 )
        Scr_Error("root anim is not an ancestor of the anim", 0);
    if ( error != 2
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    18581,
                    0,
                    "%s",
                    "error == XANIM_ERROR_BAD_NOTIFY") )
    {
        __debugbreak();
    }
    Scr_Error("cannot flag anim since it has 0 effective goal weight", 0);
}

double __cdecl GScr_GetOptionalFloat(unsigned int iParamIndex, float fDefault)
{
    if ( Scr_GetType(iParamIndex, SCRIPTINSTANCE_SERVER) )
        return Scr_GetFloat(iParamIndex, SCRIPTINSTANCE_SERVER);
    else
        return fDefault;
}

#ifdef KISAK_SP
// ===========================================================================
// SP-only server anim script methods.
//
// Retail SP's builtin method table (0x00A54218) holds its anim script methods
// in the index range 103..135; the MP reconstruction only ever had "setanim"
// (SP idx 114, 0x00801640 -> GScr_SetAnimInternal, already above). Everything
// below is transcribed from the SP decompiles of the addresses named on each
// function, using GScr_SetAnimInternal as the established server-side shape.
// Not every slot in that index range was identified -- see the table comment
// next to the entries themselves for exactly which ones were.
//
// Retail's G_StoreAnimCommand (SP 0x004E7D80) is reconstructed as the exact
// 44-byte command layout plus an integrated server/client ring. The client
// applies it at the same DObj lifecycle points found in the binary. Command
// types are 1 = clear, 3 = anim, 4 = knob, 5 = knob-all, 6 = set-time.
//
// One retail-only diagnostic remains deliberately omitted:
//
//  1. TODO(SP): each method has a debug-print block emitting
//     "%s (tree=%s anim=%s root=%s weight=%f time=%f rate=%f level time=%d)\n"
//     via Com_Printf channel 0x13, entered only when the dvar at 0x01B4C758
//     has current.integer == ent->s.number, and then only when the dvar at
//     0x01BFCFB8 has current.enabled == 0 or the new goal weight differs from
//     XAnimGetWeight's by more than 0.001. Neither dvar was identified by
//     name, so the block is omitted rather than guessed at.
//
// The "%s" in that print is a per-variant literal ("SetAnimKnob",
// "SetAnimKnobLimited", "SetAnimKnobRestart", "SetAnimKnobLimitedRestart",
// "SetAnimKnobAll", "SetFlaggedAnim", "ClearAnim", ...), and those literals
// are the only attestation for the symbol names chosen here. GScr_GetAnimTime
// and GScr_SetAnimTime have no such literal and are named purely from their
// script command names. The GScr_ prefix throughout is by symmetry with the
// CScr_ client family in cgame/cg_scr_main.cpp, not an attested symbol.
//
// flags bit 0 selects the XAnimSetComplete* entry point over the plain one,
// flags bit 1 is bRestart. So 0 = Limited, 1 = plain, 2 = LimitedRestart,
// 3 = Restart -- read directly out of SP's own name selection in each
// *Internal, and matching the client family's use of the same values.
// ===========================================================================

// SP 0x00800660. G_StoreAnimCommand type 1.
void __cdecl GScr_ClearAnim(scr_entref_t entref)
{
    XAnimTree_s *tree; // [esp+8h]
    float blendTime; // [esp+Ch]
    scr_anim_s anim; // [esp+10h]
    gentity_s *ent; // [esp+14h]

    ent = GetEntity(entref);
    tree = GScr_GetEntAnimTree(ent);
    anim = Scr_GetAnim(0, tree, SCRIPTINSTANCE_SERVER);
    blendTime = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
    const int cmdIndex = G_StoreAnimCommand_SP(ent, tree, 1, anim.index, 0, 0.0f, blendTime, 1.0f, 1);
    XAnimClearTreeGoalWeights(tree, anim.index, blendTime, cmdIndex);
}

// SP 0x00800880. G_StoreAnimCommand type 4.
// Differs from GScr_SetAnimInternal only in the XAnim entry points and in
// having no upper bound on the weight (SetAnim rejects weight > 1, SetAnimKnob
// does not) -- both read straight off 0x00800880.
void __cdecl GScr_SetAnimKnobInternal(scr_entref_t entref, char flags)
{
    unsigned int notifyType; // [esp+20h]
    float rate; // [esp+30h]
    XAnimTree_s *tree; // [esp+34h]
    DObj *obj; // [esp+38h]
    int cmdIndex; // [esp+3Ch]
    float goalWeight; // [esp+40h]
    float goalTime; // [esp+44h]
    int error; // [esp+48h]
    scr_anim_s anim; // [esp+4Ch]
    gentity_s *ent; // [esp+50h]

    ent = GetEntity(entref);
    tree = GScr_GetEntAnimTree(ent);
    rate = 1.0f;
    goalTime = 0.2f;
    goalWeight = 1.0f;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        anim = Scr_GetAnim(0, tree, SCRIPTINSTANCE_SERVER);
        XAnimGetParamValue(tree, anim.index, "rate", &rate);
        XAnimGetParamValue(tree, anim.index, "goaltime", &goalTime);
        XAnimGetParamValue(tree, anim.index, "goalweight", &goalWeight);
    }
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 1:
            goto LN_ANIM;
        case 2:
            goto LN_WEIGHT;
        case 3:
            goto LN_TIME;
        case 4:
            goto LN_RATE;
        default:
            Scr_Error("too many parameters", 0);
LN_RATE:
            rate = GScr_GetOptionalFloat(3u, rate);
            if ( rate < 0.0 )
                Scr_ParamError(3u, "must set nonnegative rate", SCRIPTINSTANCE_SERVER);
LN_TIME:
            goalTime = GScr_GetOptionalFloat(2u, goalTime);
            if ( goalTime < 0.0 )
                Scr_ParamError(2u, "must set nonnegative goal time", SCRIPTINSTANCE_SERVER);
LN_WEIGHT:
            goalWeight = GScr_GetOptionalFloat(1u, goalWeight);
            if ( goalWeight < 0.0 )
                Scr_ParamError(1u, "must set nonnegative weight", SCRIPTINSTANCE_SERVER);
LN_ANIM:
            anim = Scr_GetAnim(0, tree, SCRIPTINSTANCE_SERVER);
            cmdIndex = G_StoreAnimCommand_SP(ent, tree, 4, anim.index, 0, goalWeight, goalTime, rate, flags);
            obj = Com_GetServerDObj(ent->s.number);
            if ( !obj )
                Scr_ObjectError("No model exists.", SCRIPTINSTANCE_SERVER);
            if ( !(goalWeight > 0.001f) )
                notifyType = 0;
            else
                notifyType = 2;
            if ( (flags & 1) != 0 )
            {
                error = XAnimSetCompleteGoalWeightKnob(
                                    obj,
                                    anim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    0,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            else
            {
                error = XAnimSetGoalWeightKnob(
                                    obj,
                                    anim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    0,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            if ( error )
                GScr_HandleAnimError(error);
            else
                G_FlagAnimForUpdate(ent);
            return;
    }
}

// SP 0x00800CC0
void __cdecl GScr_SetAnimKnob(scr_entref_t entref)
{
    GScr_SetAnimKnobInternal(entref, 1);
}

// SP 0x00800CE0
void __cdecl GScr_SetAnimKnobLimited(scr_entref_t entref)
{
    GScr_SetAnimKnobInternal(entref, 0);
}

// SP 0x00800D00
void __cdecl GScr_SetAnimKnobRestart(scr_entref_t entref)
{
    GScr_SetAnimKnobInternal(entref, 3);
}

void __cdecl GScr_SetAnimKnobLimitedRestart(scr_entref_t entref)
{
    GScr_SetAnimKnobInternal(entref, 2);
}

// SP 0x00800D40. G_StoreAnimCommand type 5.
// Parameters shift by one against the non-All form: 0 = anim, 1 = root anim,
// 2 = weight, 3 = goal time, 4 = rate; 2..5 parameters accepted.
void __cdecl GScr_SetAnimKnobAllInternal(scr_entref_t entref, char flags)
{
    unsigned int notifyType; // [esp+20h]
    float rate; // [esp+40h]
    XAnimTree_s *tree; // [esp+4Ch]
    DObj *obj; // [esp+54h]
    int cmdIndex; // [esp+58h]
    float goalWeight; // [esp+5Ch]
    float goalTime; // [esp+60h]
    int error; // [esp+64h]
    scr_anim_s anim; // [esp+6Ch]
    scr_anim_s rootAnim; // [esp+74h]
    gentity_s *ent; // [esp+78h]

    ent = GetEntity(entref);
    tree = GScr_GetEntAnimTree(ent);
    rate = 1.0f;
    goalTime = 0.2f;
    goalWeight = 1.0f;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        anim = Scr_GetAnim(0, tree, SCRIPTINSTANCE_SERVER);
        XAnimGetParamValue(tree, anim.index, "rate", &rate);
        XAnimGetParamValue(tree, anim.index, "goaltime", &goalTime);
        XAnimGetParamValue(tree, anim.index, "goalweight", &goalWeight);
    }
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 2:
            goto LN_ANIM;
        case 3:
            goto LN_WEIGHT;
        case 4:
            goto LN_TIME;
        case 5:
            goto LN_RATE;
        default:
            Scr_Error("incorrect number of parameters", 0);
LN_RATE:
            rate = GScr_GetOptionalFloat(4u, rate);
            if ( rate < 0.0 )
                Scr_ParamError(4u, "must set nonnegative rate", SCRIPTINSTANCE_SERVER);
LN_TIME:
            goalTime = GScr_GetOptionalFloat(3u, goalTime);
            if ( goalTime < 0.0 )
                Scr_ParamError(3u, "must set nonnegative goal time", SCRIPTINSTANCE_SERVER);
LN_WEIGHT:
            goalWeight = GScr_GetOptionalFloat(2u, goalWeight);
            if ( goalWeight < 0.0 )
                Scr_ParamError(2u, "must set nonnegative weight", SCRIPTINSTANCE_SERVER);
LN_ANIM:
            rootAnim = Scr_GetAnim(1u, tree, SCRIPTINSTANCE_SERVER);
            anim = Scr_GetAnim(0, tree, SCRIPTINSTANCE_SERVER);
            if ( rootAnim.tree != anim.tree )
                Scr_Error("root anim is not in the same anim tree", 0);
            if ( rootAnim.index == anim.index )
                Scr_Error("root anim is not an ancestor of the anim", 0);
            cmdIndex = G_StoreAnimCommand_SP(ent, tree,
                5,
                anim.index,
                rootAnim.index,
                goalWeight,
                goalTime,
                rate,
                flags);
            obj = Com_GetServerDObj(ent->s.number);
            if ( !obj )
                Scr_ObjectError("No model exists.", SCRIPTINSTANCE_SERVER);
            if ( goalWeight <= 0.001 )
                notifyType = 0;
            else
                notifyType = 2;
            if ( (flags & 1) != 0 )
            {
                error = XAnimSetCompleteGoalWeightKnobAll(
                                    obj,
                                    anim.index,
                                    rootAnim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    0,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            else
            {
                error = XAnimSetGoalWeightKnobAll(
                                    obj,
                                    anim.index,
                                    rootAnim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    0,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            if ( error )
                GScr_HandleAnimError(error);
            else
                G_FlagAnimForUpdate(ent);
            return;
    }
}

// SP 0x00801170
void __cdecl GScr_SetAnimKnobAll(scr_entref_t entref)
{
    GScr_SetAnimKnobAllInternal(entref, 1);
}

// SP 0x008011B0
void __cdecl GScr_SetAnimKnobAllRestart(scr_entref_t entref)
{
    GScr_SetAnimKnobAllInternal(entref, 3);
}

// SP 0x00801660
void __cdecl GScr_SetAnimLimited(scr_entref_t entref)
{
    PROF_SCOPED("SetAnimLimited");
    GScr_SetAnimInternal(entref, 0);
}

void __cdecl GScr_SetAnimRestart(scr_entref_t entref)
{
    PROF_SCOPED("SetAnimRestart");
    GScr_SetAnimInternal(entref, 3);
}

void __cdecl GScr_GetAnimTime(scr_entref_t entref)
{
    XAnimTree_s *tree;
    scr_anim_s anim;
    gentity_s *ent;

    ent = GetEntity(entref);
    tree = GScr_GetEntAnimTree(ent);
    anim = Scr_GetAnim(0, tree, SCRIPTINSTANCE_SERVER);
    if ( !XAnimHasTime(XAnimGetAnims(tree), anim.index) )
        Scr_ParamError(0, "blended nonsynchronized animation has no concept of time", SCRIPTINSTANCE_SERVER);
    Scr_AddFloat(XAnimGetTime(tree, anim.index), SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_SetFlaggedAnimKnobInternal(scr_entref_t entref, char flags)
{
    unsigned int notifyType;
    unsigned int notifyName;
    float rate;
    XAnimTree_s *tree;
    DObj *obj;
    int cmdIndex;
    float goalWeight;
    float goalTime;
    int error;
    scr_anim_s anim;
    gentity_s *ent;

    ent = GetEntity(entref);
    tree = GScr_GetEntAnimTree(ent);
    rate = 1.0f;
    goalTime = 0.2f;
    goalWeight = 1.0f;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
    {
        anim = Scr_GetAnim(1u, tree, SCRIPTINSTANCE_SERVER);
        XAnimGetParamValue(tree, anim.index, "rate", &rate);
        XAnimGetParamValue(tree, anim.index, "goaltime", &goalTime);
        XAnimGetParamValue(tree, anim.index, "goalweight", &goalWeight);
    }
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 2:
            goto LN_ANIM;
        case 3:
            goto LN_WEIGHT;
        case 4:
            goto LN_TIME;
        case 5:
            goto LN_RATE;
        default:
            Scr_Error("too many parameters", 0);
LN_RATE:
            rate = GScr_GetOptionalFloat(4u, rate);
            if ( rate < 0.0 )
                Scr_ParamError(4u, "must set nonnegative rate", SCRIPTINSTANCE_SERVER);
LN_TIME:
            goalTime = GScr_GetOptionalFloat(3u, goalTime);
            if ( goalTime < 0.0 )
                Scr_ParamError(3u, "must set nonnegative goal time", SCRIPTINSTANCE_SERVER);
LN_WEIGHT:
            goalWeight = GScr_GetOptionalFloat(2u, goalWeight);
            if ( goalWeight <= 0.0 )
                Scr_ParamError(2u, "must set positive weight", SCRIPTINSTANCE_SERVER);
LN_ANIM:
            anim = Scr_GetAnim(1u, tree, SCRIPTINSTANCE_SERVER);
            notifyName = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
            if ( !XAnimHasTime(XAnimGetAnims(tree), anim.index) )
                Scr_ParamError(1u, "blended nonsynchronized animation has no concept of time", SCRIPTINSTANCE_SERVER);
            obj = Com_GetServerDObj(ent->s.number);
            if ( !obj )
                Scr_ObjectError("No model exists.", SCRIPTINSTANCE_SERVER);
            cmdIndex = G_StoreAnimCommand_SP(ent, tree, 4, anim.index, 0, goalWeight, goalTime, rate, flags);
            if ( goalWeight <= 0.001 )
                notifyType = 0;
            else
                notifyType = 2;
            if ( (flags & 1) != 0 )
            {
                error = XAnimSetCompleteGoalWeightKnob(
                                    obj,
                                    anim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    notifyName,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            else
            {
                error = XAnimSetGoalWeightKnob(
                                    obj,
                                    anim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    notifyName,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            if ( error )
                GScr_HandleAnimError(error);
            else
                G_FlagAnimForUpdate(ent);
            return;
    }
}

void __cdecl GScr_SetFlaggedAnimKnob(scr_entref_t entref)
{
    GScr_SetFlaggedAnimKnobInternal(entref, 1);
}

void __cdecl GScr_SetFlaggedAnimKnobRestart(scr_entref_t entref)
{
    GScr_SetFlaggedAnimKnobInternal(entref, 3);
}

void __cdecl GScr_SetFlaggedAnimKnobLimitedRestart(scr_entref_t entref)
{
    GScr_SetFlaggedAnimKnobInternal(entref, 2);
}

void __cdecl GScr_SetFlaggedAnimKnobAllInternal(scr_entref_t entref, char flags, const char *pszUsageError)
{
    unsigned int notifyType;
    unsigned int notifyName;
    float rate;
    XAnimTree_s *tree;
    DObj *obj;
    int cmdIndex;
    float goalWeight;
    float goalTime;
    int error;
    scr_anim_s anim;
    scr_anim_s rootAnim;
    gentity_s *ent;

    ent = GetEntity(entref);
    tree = GScr_GetEntAnimTree(ent);
    rate = 1.0f;
    goalTime = 0.2f;
    goalWeight = 1.0f;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
    {
        anim = Scr_GetAnim(1u, tree, SCRIPTINSTANCE_SERVER);
        XAnimGetParamValue(tree, anim.index, "rate", &rate);
        XAnimGetParamValue(tree, anim.index, "goaltime", &goalTime);
        XAnimGetParamValue(tree, anim.index, "goalweight", &goalWeight);
    }
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 3:
            goto LN_ANIM;
        case 4:
            goto LN_WEIGHT;
        case 5:
            goto LN_TIME;
        case 6:
            goto LN_RATE;
        default:
            Scr_Error(pszUsageError, 0);
LN_RATE:
            rate = GScr_GetOptionalFloat(5u, rate);
            if ( rate < 0.0 )
                Scr_ParamError(5u, "must set nonnegative rate", SCRIPTINSTANCE_SERVER);
LN_TIME:
            goalTime = GScr_GetOptionalFloat(4u, goalTime);
            if ( goalTime < 0.0 )
                Scr_ParamError(4u, "must set nonnegative goal time", SCRIPTINSTANCE_SERVER);
LN_WEIGHT:
            goalWeight = GScr_GetOptionalFloat(3u, goalWeight);
            if ( goalWeight <= 0.0 )
                Scr_ParamError(3u, "must set positive weight", SCRIPTINSTANCE_SERVER);
LN_ANIM:
            rootAnim = Scr_GetAnim(2u, tree, SCRIPTINSTANCE_SERVER);
            anim = Scr_GetAnim(1u, tree, SCRIPTINSTANCE_SERVER);
            notifyName = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
            if ( !XAnimHasTime(XAnimGetAnims(tree), anim.index) )
                Scr_ParamError(1u, "blended nonsynchronized animation has no concept of time", SCRIPTINSTANCE_SERVER);
            if ( rootAnim.tree != anim.tree )
                Scr_Error("root anim is not in the same anim tree", 0);
            if ( rootAnim.index == anim.index )
                Scr_Error("root anim is not an ancestor of the anim", 0);
            cmdIndex = G_StoreAnimCommand_SP(ent, tree,
                5,
                anim.index,
                rootAnim.index,
                goalWeight,
                goalTime,
                rate,
                flags);
            obj = Com_GetServerDObj(ent->s.number);
            if ( !obj )
                Scr_ObjectError("No model exists.", SCRIPTINSTANCE_SERVER);
            if ( goalWeight <= 0.001 )
                notifyType = 0;
            else
                notifyType = 2;
            if ( (flags & 1) != 0 )
            {
                error = XAnimSetCompleteGoalWeightKnobAll(
                                    obj,
                                    anim.index,
                                    rootAnim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    notifyName,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            else
            {
                error = XAnimSetGoalWeightKnobAll(
                                    obj,
                                    anim.index,
                                    rootAnim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    notifyName,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            if ( error )
                GScr_HandleAnimError(error);
            else
                G_FlagAnimForUpdate(ent);
            return;
    }
}

void __cdecl GScr_SetFlaggedAnimKnobAll(scr_entref_t entref)
{
    GScr_SetFlaggedAnimKnobAllInternal(entref, 1, "illegal call to SetFlaggedAnimKnobAll()\n");
}

void __cdecl GScr_SetFlaggedAnimKnobAllRestart(scr_entref_t entref)
{
    GScr_SetFlaggedAnimKnobAllInternal(entref, 3, "illegal call to SetFlaggedAnimKnobAllRestart()\n");
}

void __cdecl GScr_SetFlaggedAnimInternal(scr_entref_t entref, char flags)
{
    unsigned int notifyType; // [esp+20h]
    unsigned int notifyName; // [esp+24h]
    float rate; // [esp+30h]
    XAnimTree_s *tree; // [esp+34h]
    DObj *obj; // [esp+38h]
    int cmdIndex; // [esp+3Ch]
    float goalWeight; // [esp+40h]
    float goalTime; // [esp+44h]
    int error; // [esp+48h]
    scr_anim_s anim; // [esp+4Ch]
    gentity_s *ent; // [esp+50h]

    ent = GetEntity(entref);
    tree = GScr_GetEntAnimTree(ent);
    rate = 1.0f;
    goalTime = 0.2f;
    goalWeight = 1.0f;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 )
    {
        anim = Scr_GetAnim(1u, tree, SCRIPTINSTANCE_SERVER);
        XAnimGetParamValue(tree, anim.index, "rate", &rate);
        XAnimGetParamValue(tree, anim.index, "goaltime", &goalTime);
        XAnimGetParamValue(tree, anim.index, "goalweight", &goalWeight);
    }
    switch ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        case 2:
            goto LN_ANIM;
        case 3:
            goto LN_WEIGHT;
        case 4:
            goto LN_TIME;
        case 5:
            goto LN_RATE;
        default:
            Scr_Error("incorrect number of parameters", 0);
LN_RATE:
            rate = GScr_GetOptionalFloat(4u, rate);
            if ( rate < 0.0 )
                Scr_ParamError(4u, "must set nonnegative rate", SCRIPTINSTANCE_SERVER);
LN_TIME:
            goalTime = GScr_GetOptionalFloat(3u, goalTime);
            if ( goalTime < 0.0 )
                Scr_ParamError(3u, "must set nonnegative goal time", SCRIPTINSTANCE_SERVER);
LN_WEIGHT:
            goalWeight = GScr_GetOptionalFloat(2u, goalWeight);
            if ( goalWeight <= 0.0 )
                Scr_ParamError(2u, "must set positive weight", SCRIPTINSTANCE_SERVER);
LN_ANIM:
            anim = Scr_GetAnim(1u, tree, SCRIPTINSTANCE_SERVER);
            notifyName = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
            if ( !XAnimHasTime(XAnimGetAnims(tree), anim.index) )
                Scr_ParamError(1u, "blended nonsynchronized animation has no concept of time", SCRIPTINSTANCE_SERVER);
            cmdIndex = G_StoreAnimCommand_SP(ent, tree, 3, anim.index, 0, goalWeight, goalTime, rate, flags);
            obj = Com_GetServerDObj(ent->s.number);
            if ( !obj )
                Scr_ObjectError("No model exists.", SCRIPTINSTANCE_SERVER);
            if ( goalWeight <= 0.001 )
                notifyType = 0;
            else
                notifyType = 2;
            if ( (flags & 1) != 0 )
            {
                error = XAnimSetCompleteGoalWeight(
                                    obj,
                                    anim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    notifyName,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            else
            {
                error = XAnimSetGoalWeight(
                                    obj,
                                    anim.index,
                                    goalWeight,
                                    goalTime,
                                    rate,
                                    notifyName,
                                    notifyType,
                                    (flags & 2) != 0,
                                    cmdIndex);
            }
            if ( error )
                GScr_HandleAnimError(error);
            else
                G_FlagAnimForUpdate(ent);
            return;
    }
}

void __cdecl GScr_SetFlaggedAnim(scr_entref_t entref)
{
    GScr_SetFlaggedAnimInternal(entref, 1);
}

void __cdecl GScr_SetFlaggedAnimLimited(scr_entref_t entref)
{
    GScr_SetFlaggedAnimInternal(entref, 0);
}

void __cdecl GScr_SetFlaggedAnimRestart(scr_entref_t entref)
{
    GScr_SetFlaggedAnimInternal(entref, 3);
}

static void G_SetAnimRateComplete_SP(gentity_s *ent, XAnimTree_s *tree, float rate, unsigned int infoIndex)
{
    XAnim_s *anims = XAnimGetAnims(tree);
    XAnimInfo *info = GetAnimInfo(infoIndex);
    if (info->next)
        G_SetAnimRateComplete_SP(ent, tree, rate, info->next);
    if (info->children)
        G_SetAnimRateComplete_SP(ent, tree, rate, info->children);
    if (XAnimIsPrimitive(anims, info->animIndex))
    {
        info->state.rate = rate;
        G_StoreAnimCommand_SP(ent, tree, 3, info->animIndex, 0, info->state.goalWeight, info->state.goalTime, rate, 1);
    }
}

static void GScr_SetAnimRateComplete_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    XAnimTree_s *tree = GScr_GetEntAnimTree(ent);
    if (!tree || !tree->children)
        return;
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1)
        Scr_Error("SetAnimRateComplete: Wrong param count, should be 1", SCRIPTINSTANCE_SERVER);
    const float rate = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if (rate < 0.0f)
        Scr_ParamError(0, "SetAnimRateComplete: must set nonnegative animRate", SCRIPTINSTANCE_SERVER);
    G_SetAnimRateComplete_SP(ent, tree, rate, tree->children);
    G_FlagAnimForUpdate(ent);
}

void __cdecl GScr_SetAnimTime(scr_entref_t entref)
{
    XAnim_s *anims;
    int NumParam;
    XAnimTree_s *tree;
    scr_anim_s anim;
    gentity_s *ent;
    float time;

    ent = GetEntity(entref);
    time = 0.0f;
    tree = GScr_GetEntAnimTree(ent);
    NumParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( NumParam != 1 )
    {
        if ( NumParam != 2 )
            Scr_Error("too many parameters", 0);
        time = Scr_GetFloat(1u, SCRIPTINSTANCE_SERVER);
        if ( time < 0.0 )
        {
            Scr_ParamError(1u, "must be > 0", SCRIPTINSTANCE_SERVER);
            time = 0.0f;
        }
        else if ( time > 1.0 )
        {
            Scr_ParamError(1u, "must be < 1", SCRIPTINSTANCE_SERVER);
            time = 1.0f;
        }
    }
    anim = Scr_GetAnim(0, tree, SCRIPTINSTANCE_SERVER);
    anims = XAnimGetAnims(tree);
    if ( !XAnimHasTime(anims, anim.index) )
        Scr_ParamError(0, "not a timed animation", SCRIPTINSTANCE_SERVER);
    if ( time == 1.0 && XAnimIsLooped(anims, anim.index) )
        Scr_ParamError(1u, "cannot set time 1 on looping animation", SCRIPTINSTANCE_SERVER);
    const int cmdIndex = G_StoreAnimCommand_SP(ent, tree, 6, anim.index, 0, 1.0f, time, 1.0f, 1);
    XAnimSetTime(tree, anim.index, time, static_cast<unsigned __int16>(cmdIndex));
    G_FlagAnimForUpdate(ent);
}
#endif

#ifdef KISAK_SP
void __cdecl G_SetAnimTree(gentity_s *ent, scr_animtree_t *animtree)
{
    GScr_StopScriptedRecord_SP(ent);
    XAnimTree_s *oldTree = ent->pAnimTree;
    if (animtree)
    {
        if (oldTree && XAnimGetAnims(oldTree) == animtree->anims)
            return;
        ent->pAnimTree = Com_XAnimCreateSmallTree(animtree->anims);
    }
    else
    {
        if (!oldTree) return;
        ent->pAnimTree = nullptr;
    }
    G_DObjUpdate(ent);
    if (oldTree) Com_XAnimFreeSmallTree(oldTree);
    G_ReleaseAnimCommandsForEnt_SP(ent);
    XAnimTree_s *tree = G_GetEntAnimTree(ent);
    const unsigned int treeIndex = tree ? Scr_GetAnimsIndex(XAnimGetAnims(tree), SCRIPTINSTANCE_SERVER) : 0;
    iassert(treeIndex < MAX_XANIMTREE_NUM);
    ent->s.animTreeIndex = static_cast<unsigned char>(treeIndex);
}
#else
void __cdecl G_SetAnimTree(gentity_s *ent, scr_animtree_t *animtree)
{
    XAnimTree_s *oldAnimTree;

    if ( G_GetEntAnimTree(ent) != ent->pAnimTree
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    18729,
                    0,
                    "%s",
                    "G_GetEntAnimTree( ent ) == ent->pAnimTree") )
    {
        __debugbreak();
    }
    oldAnimTree = ent->pAnimTree;
    if ( !animtree )
    {
        if ( !oldAnimTree )
#ifdef KISAK_SP
        {
            ent->s.animTreeIndex = 0;
            if ( ent->actor )
                g_scr_data.actorXAnimTrees[G_GetActorIndex(ent->actor)] = 0;
            return;
        }
#else
            return;
#endif
        ent->pAnimTree = 0;
        goto LABEL_10;
    }
    if ( !oldAnimTree || XAnimGetAnims(oldAnimTree) != animtree->anims )
    {
        ent->pAnimTree = Com_XAnimCreateSmallTree(animtree->anims);
LABEL_10:
        G_DObjUpdate(ent);
        if ( oldAnimTree )
            Com_XAnimFreeSmallTree(oldAnimTree);
    }
#ifdef KISAK_SP
    if ( ent->actor )
        g_scr_data.actorXAnimTrees[G_GetActorIndex(ent->actor)] = ent->pAnimTree;

    XAnimTree_s *publishedTree = G_GetEntAnimTree(ent);
    const unsigned int treeIndex = publishedTree
                                 ? Scr_GetAnimsIndex(XAnimGetAnims(publishedTree), SCRIPTINSTANCE_SERVER)
                                 : 0;
    if ( treeIndex > 0x7F
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    18729,
                    0,
                    "%s\n\t(treeIndex) = %i",
                    "treeIndex <= MAX_XANIMTREE_NUM - 1",
                    treeIndex) )
    {
        __debugbreak();
    }
    ent->s.animTreeIndex = static_cast<unsigned char>(treeIndex);
    Com_Printf(
        15,
        "SP animtree publish: ent %d index %u anims %s\n",
        ent->s.number,
        treeIndex,
        publishedTree && XAnimGetAnims(publishedTree)->debugName
            ? XAnimGetAnims(publishedTree)->debugName
            : "<none>");
#endif
}
#endif

void __cdecl GScr_UseAnimTree(scr_entref_t entref)
{
    char *v1;
    const char *v2;
    scr_animtree_t animtree;
    gentity_s *ent;

    ent = GetEntity(entref);
    animtree.anims = Scr_GetAnimTree(0, 1u, SCRIPTINSTANCE_SERVER).anims;
    if ( G_GetEntAnimTree(ent) != ent->pAnimTree )
    {
        v1 = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        v2 = va("cannot change the animtree of classname '%s'", v1);
        Scr_Error(v2, 0);
    }
    G_SetAnimTree(ent, &animtree);
}

#ifdef KISAK_SP
static void __cdecl GScr_StopUseAnimTree_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (G_GetEntAnimTree(ent) != ent->pAnimTree)
        Scr_Error(va("cannot change the animtree of classname '%s'",
            SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), false);
    G_SetAnimTree(ent, nullptr);
}

#endif
void (__cdecl *__cdecl Scr_GetMethod(const char **pName, int *type))(scr_entref_t)
{
    void (__cdecl *method)(scr_entref_t);

    *type = 0;
    method = Player_GetMethod(pName);
    if ( method )
        return method;
    method = ScriptEnt_GetMethod(pName);
    if (method)
        return method;
    method = ScriptVehicle_GetMethod(pName);
    if (method)
        return method;
    method = HudElem_GetMethod(pName);
    if (method)
        return method;
    method = Helicopter_GetMethod(pName);
    if (method)
        return method;
    method = Actor_GetMethod(pName);
    if (method)
        return method;

    method = BuiltIn_GetMethod(pName, type);

    return method;
}

static void METHOD_NULLSUB(scr_entref_t ref)
{

}

#ifdef KISAK_SP
static void __cdecl GScr_SPMethod_playweapondeatheffects(scr_entref_t entref)
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 1 )
        Scr_Error("PlayWeaponDeathEffects <weaponName>.\n", false);

    char *weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const unsigned int weapon = G_GetWeaponIndexForName(weaponName);
    if ( !weapon )
    {
        Scr_Error(
            va("PlayWeaponDeathEffects called with unknown weapon name %s\n", weaponName),
            false);
    }

    gentity_s *source = GetEntity(entref);
    gentity_s *tempEnt = G_TempEntity(vec3_origin, static_cast<entity_event_t>(0xC1));
    tempEnt->s.groundEntityNum = source->s.number;
    tempEnt->s.weapon = static_cast<unsigned __int16>(weapon);

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 2 )
        tempEnt->s.eventParm = static_cast<unsigned __int16>(Scr_GetInt(1, SCRIPTINSTANCE_SERVER));
}

static void __cdecl GScr_SPMethod_setdeathcontents(scr_entref_t entref)
{
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
        Scr_Error("setdeathcontents takes one parameter\n", false);

    gentity_s *ent = GetEntity(entref);
    if ( !ent->actor )
        Scr_Error("setdeathcontents must be called on an AI only.", false);

    ent->actor->deathContents = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_BypassSledgehammer_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (ent->client)
    {
        Scr_Error("BypassSledgehammer called on a player.", false);
        return;
    }
    ent->s.lerp.eFlags2 |= 0x20000u;
}

static void __cdecl GScr_EnableSledgehammer_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (ent->client)
    {
        Scr_Error("EnableSledgehammer called on a player.", false);
        return;
    }
    ent->s.lerp.eFlags2 &= ~0x20000u;
}

static void __cdecl GScr_PlayerAnimScriptEvent_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    ent->client->ps.pm_flags |= 0x100000u;
    ent->client->ps.scriptedAnim = BG_StringHashValue(Scr_GetString(0, SCRIPTINSTANCE_SERVER));
}

static void __cdecl GScr_SPMethod_haseyes(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
    {
        Scr_Error("HasEyes() called with wrong params.\n", false);
        return;
    }

    if ( Scr_GetInt(0, SCRIPTINSTANCE_SERVER) )
        ent->s.lerp.eFlags |= 0x20000u;
    else
        ent->s.lerp.eFlags &= ~0x20000u;
}

static void __cdecl GScr_SPMethod_setphysparams(scr_entref_t entref)
{
    if ( !zombiemode->current.enabled )
    {
        Scr_Error("Invalid call to SetPhysParams()\n", false);
        return;
    }

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3 )
        Scr_Error("setphysparams takes three parameters\n", false);

    gentity_s *ent = GetEntity(entref);
    if ( !ent->actor )
        Scr_Error("setphysparams must be called on an AI only.", false);

    const float radius = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    const float minZ = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    const float maxZ = Scr_GetFloat(2, SCRIPTINSTANCE_SERVER);

    ent->r.mins[0] = -radius;
    ent->r.mins[1] = -radius;
    ent->r.mins[2] = minZ;
    ent->r.maxs[0] = radius;
    ent->r.maxs[1] = radius;
    ent->r.maxs[2] = maxZ;
    Vec3Copy(ent->r.mins, ent->actor->Physics.vMins);
    Vec3Copy(ent->r.maxs, ent->actor->Physics.vMaxs);
}

static void __cdecl GScr_SPMethod_settransported(scr_entref_t entref)
{
    int clientNum = -1;
    const float transportedTime = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    if ( transportedTime < 0.0f )
        Scr_ParamError(1u, "Time must be positive", SCRIPTINSTANCE_SERVER);

    gentity_s *ent = GetEntity(entref);
    if ( ent && ent->r.inuse && ent->client )
        clientNum = ent->s.number;
    else
        Scr_Error("settransported() called on an invalid client entity.\n", false);

    SV_GameSendServerCommand(
        clientNum,
        SV_CMD_RELIABLE,
        va("%c %i", 0x5D, static_cast<int>(transportedTime * 1000.0f)));
}

static void __cdecl GScr_SetClientFlagAsVal_SP(scr_entref_t entref)
{
    const char *v1;
    gentity_s *pSelf;
    int val;

    pSelf = GetEntity(entref);
    val = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( (unsigned int)val < 0x10000 )
    {
        if ( pSelf->client )
            pSelf->client->ps.eFlags2 = pSelf->client->ps.eFlags2 & 0xFFFF0000 | val;
        else
            pSelf->s.lerp.eFlags2 = pSelf->s.lerp.eFlags2 & 0xFFFF0000 | val;
    }
    else
    {
        v1 = va("SetClientFlagAsVal: Index %i out of range (0 - %i)\n", val, 15);
        Scr_ParamError(0, v1, SCRIPTINSTANCE_SERVER);
    }
}

static void __cdecl GScr_GetPlayerConnectionState_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->client)
        Scr_Error(SCRIPTINSTANCE_SERVER,
            "GetPlayerConnectionState: entity must be a player entity", false);
    switch (svs.clients[ent->s.clientNum].header.connectionState_SP)
    {
    case CA_DISCONNECTED: Scr_AddConstString(scr_const.ca_disconnected, SCRIPTINSTANCE_SERVER); break;
    case CA_CINEMATIC: Scr_AddConstString(scr_const.ca_cinematic, SCRIPTINSTANCE_SERVER); break;
    case CA_UICINEMATIC: Scr_AddConstString(scr_const.ca_uicinematic, SCRIPTINSTANCE_SERVER); break;
    case CA_LOGO: Scr_AddConstString(scr_const.ca_logo, SCRIPTINSTANCE_SERVER); break;
    case CA_CONNECTING: Scr_AddConstString(scr_const.ca_connecting, SCRIPTINSTANCE_SERVER); break;
    case CA_CHALLENGING: Scr_AddConstString(scr_const.ca_challenging, SCRIPTINSTANCE_SERVER); break;
    case CA_CONNECTED: Scr_AddConstString(scr_const.ca_connected, SCRIPTINSTANCE_SERVER); break;
    case CA_SENDINGSTATS: Scr_AddConstString(scr_const.ca_sendingstats, SCRIPTINSTANCE_SERVER); break;
    case CA_LOADING: Scr_AddConstString(scr_const.ca_loading, SCRIPTINSTANCE_SERVER); break;
    case CA_PRIMED: Scr_AddConstString(scr_const.ca_primed, SCRIPTINSTANCE_SERVER); break;
    case CA_ACTIVE: Scr_AddConstString(scr_const.ca_active, SCRIPTINSTANCE_SERVER); break;
    case 11: Scr_AddConstString(scr_const.ca_map_restart, SCRIPTINSTANCE_SERVER); break;
    default: Scr_AddString((char *)"unknown", SCRIPTINSTANCE_SERVER); break;
    }
}

static void __cdecl GScr_HasUsedWeapon_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->client)
        Scr_Error("GetPlayerConnectionState: entity must be a player entity", false);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2)
        Scr_Error("Requires 2 params.  Params are weaponname and bool for exclusive check.\n", false);
    if (Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) != VAR_ARRAY)
        Scr_ParamError(0, va("Parameter (%s) must be an array",
            Scr_GetTypeName(0, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);

    const unsigned int arrayId = Scr_GetObject(0, SCRIPTINSTANCE_SERVER);
    const int exclusive = Scr_GetInt(1, SCRIPTINSTANCE_SERVER);
    char weaponNames[33][128] = {};
    int member = 0;
    for (unsigned int id = FindFirstSibling(SCRIPTINSTANCE_SERVER, arrayId);
         id;
         id = FindNextSibling(SCRIPTINSTANCE_SERVER, id), ++member)
    {
        if (GetValueType(SCRIPTINSTANCE_SERVER, id) != VAR_STRING)
            Scr_Error(va("Array passed to hasusedweapon contained member [%i] - valid types are string.", member), false);
        strncpy(weaponNames[member], SL_ConvertToString(
            GetVariableValueAddress(SCRIPTINSTANCE_SERVER, id)->u.stringValue,
            SCRIPTINSTANCE_SERVER), 127);
        if (member > 31)
            break;
    }

    Scr_AddInt(static_cast<int>(G_HasUsedWeapons_SP(ent->client->ps.clientNum,
        weaponNames, exclusive)), SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_SPMethod_gib(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    unsigned int eventParm = 0;

    const unsigned int direction = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    if ( direction == scr_const.freeze )
        eventParm = 0x100;
    else if ( direction == scr_const.up )
        eventParm = 0x200;

    if ( Scr_GetPointerType(1u, SCRIPTINSTANCE_SERVER) != VAR_ARRAY )
    {
        Scr_ParamError(
            1u,
            va("Parameter (%s) must be an array", Scr_GetTypeName(1u, SCRIPTINSTANCE_SERVER)),
            SCRIPTINSTANCE_SERVER);
    }

    const unsigned int arrayId = Scr_GetObject(1u, SCRIPTINSTANCE_SERVER);
    int arrayIndex = 0;
    for ( unsigned int id = FindFirstSibling(SCRIPTINSTANCE_SERVER, arrayId);
          id;
          id = FindNextSibling(SCRIPTINSTANCE_SERVER, id), ++arrayIndex )
    {
        if ( GetValueType(SCRIPTINSTANCE_SERVER, id) != VAR_INTEGER )
        {
            Scr_Error(
                va("Array passed to gib contained member [%i] - valid types are int.", arrayIndex),
                false);
        }

        const int tag = GetVariableValueAddress(SCRIPTINSTANCE_SERVER, id)->u.intValue;
        if ( (unsigned int)tag > 7u )
        {
            Scr_Error("Gib tag array passed to 'Gib' contains value out of range 0 -> 7", false);
            return;
        }
        eventParm |= 1u << tag;
    }

    G_AddEvent(ent, EV_GIB, eventParm);
}

static void __cdecl GScr_SPMethod_resetmissiledetonationtime(scr_entref_t entref)
{
    gentity_s *missile = GetEntity(entref);
    if ( missile->s.eType != ET_MISSILE || !missile->s.weapon )
        return;

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) )
    {
        missile->nextthink = level.time
            + (int)(Scr_GetFloat(0, SCRIPTINSTANCE_SERVER) * 1000.0f);
        return;
    }

    const WeaponDef *weapDef = BG_GetWeaponDef(missile->s.weapon);
    if ( !weapDef || !weapDef->timedDetonation )
        return;

    const bool hasPlayerOwner = missile->r.ownerNum.isDefined() && missile->r.ownerNum.ent()->client;
    missile->nextthink = level.time + (hasPlayerOwner ? weapDef->fuseTime : weapDef->aiFuseTime);
}

static void __cdecl GScr_SPMethod_getcentroid(scr_entref_t entref)
{
    float centroid[3];
    G_EntityCentroid(GetEntity(entref), centroid);
    Scr_AddVector(centroid, SCRIPTINSTANCE_SERVER);
}

static bool GScr_SPFillWeaponParms(gentity_s *ent, weaponParms *wp)
{
    Weapon_SetWeaponParamsWeapon(wp, ent->s.weapon);

    if ( ent->client )
    {
        G_CalcMuzzlePoints(ent, wp, 1);
    }
    else if ( ent->actor )
    {
        Actor_FillWeaponParms(ent->actor, wp);
    }
    else
    {
        Scr_ObjectError(
            "SP weapon parameters are not implemented for this entity type",
            SCRIPTINSTANCE_SERVER);
        return false;
    }

    return true;
}

static void __cdecl GScr_SPMethod_getweaponforwarddir(scr_entref_t entref)
{
    weaponParms wp;
    if ( !GScr_SPFillWeaponParms(GetEntity(entref), &wp) )
        return;
    Scr_AddVector(wp.forward, SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_SPMethod_getweaponmuzzlepoint(scr_entref_t entref)
{
    weaponParms wp;
    if ( !GScr_SPFillWeaponParms(GetEntity(entref), &wp) )
        return;
    Scr_AddVector(wp.muzzleTrace, SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_ItemWeaponSetOptions_SP(scr_entref_t entref)
{
    gentity_s *item = GetEntity(entref);
    if (item->s.eType != ET_ITEM)
        Scr_Error(SCRIPTINSTANCE_SERVER, "Entity is not an item.", false);
    if (bg_itemlist[item->s.un3.item].giType != IT_WEAPON)
        Scr_Error(SCRIPTINSTANCE_SERVER, "Item entity is not a weapon.", false);
    item->s.renderOptions.i = Scr_GetInt(0, SCRIPTINSTANCE_SERVER) & 0x3F;
}
static void GScr_TransformScriptedRoot_SP(
    const float (*baseAxis)[3], const float *translation, const float *rotation,
    float *origin, float *angles)
{
    float yawAxis[3][3], axis[3][3];
    MatrixTransformVector43(translation, baseAxis, origin);
    YawToAxis(static_cast<float>(RotationToYaw(rotation)), yawAxis);
    MatrixMultiply(yawAxis, baseAxis, axis);
    AxisToAngles(axis, angles);
}

static void GScr_CalcScriptedRoot_SP(
    DObj *obj, unsigned int animIndex, const ScriptedAnimRecord_SP *record,
    float *origin, float *angles)
{
    float rotation[2], translation[3];
    XAnimCalcAbsDelta(obj, animIndex, rotation, translation);
    GScr_TransformScriptedRoot_SP(record->baseAxis, translation, rotation, origin, angles);
}

static void GScr_PublishScriptedTransform_SP(
    gentity_s *ent, const float *origin, const float *angles)
{
    if (ent->tagInfo)
    {
        float parentAxis[4][3], tagAxis[4][3], rootAxis[4][3], axis[4][3];
        G_CalcTagParentAxis(ent, parentAxis);
        MatrixMultiply43(ent->tagInfo->parentInvAxis, parentAxis, tagAxis);
        AnglesToAxis(angles, rootAxis);
        Vec3Copy(origin, rootAxis[3]);
        MatrixMultiply43(rootAxis, tagAxis, axis);
        Vec3Copy(axis[3], ent->r.currentOrigin);
        AxisToAngles(axis, ent->r.currentAngles);
        G_CalcTagAxis(ent, 0);
    }
    else
    {
        Vec3Copy(origin, ent->r.currentOrigin);
        Vec3Copy(angles, ent->r.currentAngles);
    }
}

static bool GScr_TraceScriptedPlant_SP(
    const gentity_s *ent, const float *start, const float *end, float *hit)
{
    const float mins[3] = {-15.0f, -15.0f, 0.0f};
    const float maxs[3] = {15.0f, 15.0f, 72.0f};
    col_context_t context;
    trace_t trace;
    G_TraceCapsule(&trace, start, mins, maxs, end, ent->s.number, 0x820011, &context);
    if (trace.startsolid || !(trace.fraction < 1.0f))
        return false;
    for (int n = 0; n < 3; ++n)
        hit[n] = start[n] + (end[n] - start[n]) * trace.fraction;
    return true;
}

static void GScr_InitScriptedDeathPlant_SP(
    gentity_s *ent, const float *origin, const float *angles,
    const XAnim_s *anims, unsigned int animIndex)
{
    ScriptedAnimRecord_SP *record = ent->scriptedAnim_SP;
    float start[3], end[3], hit[3];
    Vec3Copy(origin, start);
    Vec3Copy(origin, end);
    start[2] += 36.0f;
    end[2] -= 18.0f;
    if (GScr_TraceScriptedPlant_SP(ent, start, end, hit))
        Vec3Copy(hit, record->baseAxis[3]);
    else
        Vec3Copy(origin, record->baseAxis[3]);
    record->deathPlant[0] = 0.0f;
    const float baseAngles[3] = {0.0f, angles[1], angles[2]};
    AnglesToAxis(baseAngles, record->baseAxis);
    float rotation[2], translation[3], rootOrigin[3], rootAngles[3];
    XAnimGetAbsDelta(anims, animIndex, rotation, translation, 1.0f);
    GScr_TransformScriptedRoot_SP(record->baseAxis, translation, rotation, rootOrigin, rootAngles);
    const float extent = std::sqrt((translation[1]*translation[1]
        + translation[2]*translation[2]) + translation[0]*translation[0]) + 128.0f;
    Vec3Copy(rootOrigin, start);
    Vec3Copy(rootOrigin, end);
    start[2] += extent;
    end[2] -= extent;
    if (GScr_TraceScriptedPlant_SP(ent, start, end, hit))
        Actor_GetBodyPlantAngles(ent->s.number, 0x820011, hit, rootAngles[1],
            &record->deathPlant[1], &record->deathPlant[2], nullptr);
    else
        record->deathPlant[1] = record->deathPlant[2] = 0.0f;
    record->deathPlant[3] = XAnimGetLength(anims, animIndex) < 1.0f ? -1.0f : 0.0f;
}

static void GScr_UpdateScriptedDeathPlant_SP(
    gentity_s *ent, const XAnimTree_s *tree, float *origin, float *angles)
{
    ScriptedAnimRecord_SP *record = ent->scriptedAnim_SP;
    float start[3], end[3], hit[3];
    Vec3Copy(origin, start);
    Vec3Copy(origin, end);
    start[2] += record->deathPlant[0] + 18.0f;
    end[2] += record->deathPlant[0] - 18.0f;
    if (GScr_TraceScriptedPlant_SP(ent, start, end, hit))
    {
        record->deathPlant[0] = hit[2] - origin[2];
        Vec3Copy(hit, origin);
    }
    float progress;
    if (record->deathPlant[3] < 0.0f)
        progress = static_cast<float>(XAnimGetTime(tree, record->animIndex));
    else
    {
        progress = record->deathPlant[3] + 0.05f;
        if (progress > 1.0f) progress = 1.0f;
        record->deathPlant[3] = progress;
    }
    angles[0] += record->deathPlant[1] * progress;
    angles[2] += record->deathPlant[2] * progress;
}

static void GScr_PrintScriptedGoal_SP(
    const char *label, XAnimTree_s *tree, unsigned int animIndex,
    float weight, float goalTime, float rate)
{
    const XAnim_s *anims = XAnimGetAnims(tree);
    Com_Printf(19,
        "%s (tree=%s anim=%s root=%s weight=%f time=%f rate=%f level time=%d)\n",
        label, anims->debugName, XAnimGetAnimDebugName(anims, animIndex), "none",
        weight, goalTime, rate, level.time);
}

static void GScr_PrepareScriptedAnim_SP(
    gentity_s *ent, DObj *obj, XAnimTree_s *tree, unsigned int rootIndex)
{
    actor_s *actor = ent->actor;
    gentity_s *commandEnt = ent;
    if (actor && actor->lookAtInfo.fLookAtTurnAngle != 0.0f && actor->lookAtInfo.bLookAtSetup)
    {
        commandEnt = actor->ent;
        const float straightWeight = static_cast<float>(
            XAnimGetWeight(tree, actor->lookAtInfo.animLookAtStraight));
        unsigned int turnAnim = actor->lookAtInfo.animLookAtLeft;
        float turnWeight = static_cast<float>(XAnimGetWeight(tree, turnAnim));
        if (!(turnWeight > 0.0f))
        {
            turnAnim = actor->lookAtInfo.animLookAtRight;
            turnWeight = static_cast<float>(XAnimGetWeight(tree, turnAnim));
        }
        if (turnWeight > 0.0f)
        {
            const int command = G_StoreAnimCommand_SP(commandEnt, tree, 4, turnAnim, 0, turnWeight, 0.2f, 1.0f, 0);
            XAnimSetGoalWeightKnob(obj, turnAnim, turnWeight, 0.2f, 1.0f, 0, 0, 0, command);
        }
        const int command = G_StoreAnimCommand_SP(commandEnt, tree, 3, actor->lookAtInfo.animLookAtStraight, 0,
            straightWeight, 0.2f, 1.0f, 0);
        XAnimSetCompleteGoalWeight(obj, actor->lookAtInfo.animLookAtStraight,
            straightWeight, 0.2f, 1.0f, 0, 0, 0, command);
        if (!rootIndex)
            return;
    }
    const int command = G_StoreAnimCommand_SP(commandEnt, tree, 2, rootIndex, 0, 0.0f, 0.2f, 1.0f, 1);
    XAnimClearTreeGoalWeightsStrict(tree, rootIndex, 0.2f, command);
}

bool __cdecl GScr_GetScriptedAnimState_SP(
    const gentity_s *ent, unsigned short *animIndex, int *startTime)
{
    const ScriptedAnimRecord_SP *record = ent->scriptedAnim_SP;
    if (!record)
        return false;
    *animIndex = record->animIndex;
    *startTime = record->startTime;
    return true;
}

// Keep this trace restricted to contextual takedowns and the opening crate
// worker. It records interruptions without changing mission-script behavior.
void GScr_LogScriptedAnimTransition_SP(gentity_s *ent, const char *event)
{
    const ScriptedAnimRecord_SP *record = ent->scriptedAnim_SP;
    if (!record || !record->animIndex)
        return;
    XAnimTree_s *tree = G_GetEntAnimTree(ent);
    if (!tree)
        return;
    XAnim_s *anims = XAnimGetAnims(tree);
    const char *name = XAnimGetAnimDebugName(anims, record->animIndex);
    if (!strstr(name, "contextual_melee") && !strstr(name, "container_worker"))
        return;
    const actor_s *actor = ent->actor;
    const char *target = ent->targetname
        ? SL_ConvertToString(ent->targetname, SCRIPTINSTANCE_SERVER) : "";
    Com_Printf(15,
        "SP scripted transition: event=%s time=%d ent=%d target='%s' anim='%s' elapsed=%d animTime=%g weight=%g health=%d alive=%d state=%d pendingState=%d\n",
        event, level.time, ent->s.number, target, name,
        level.time - record->startTime, XAnimGetTime(tree, record->animIndex),
        XAnimGetWeight(tree, record->animIndex), ent->health,
        actor ? actor->Physics.bIsAlive : -1,
        actor ? actor->eState[actor->stateLevel] : -1,
        actor ? actor->eSimulatedState[actor->simulatedStateLevel] : -1);
    if (!strcmp(event, "stop-request") && gScrVmPub[SCRIPTINSTANCE_SERVER].function_count > 0)
    {
        Scr_PrintPrevCodePos(SCRIPTINSTANCE_SERVER, 15,
            const_cast<char *>(gScrVmPub[SCRIPTINSTANCE_SERVER].function_frame->fs.pos), 0);
    }
}

void __cdecl GScr_ClearScriptedAnim_SP(gentity_s *ent)
{
    ScriptedAnimRecord_SP *record = ent->scriptedAnim_SP;
    if (record)
    {
        MT_Free(reinterpret_cast<unsigned char *>(record), 100, SCRIPTINSTANCE_SERVER);
        ent->scriptedAnim_SP = nullptr;
    }
}

void __cdecl GScr_ApplyScriptedRecord_SP(
    gentity_s *ent, const float *origin, const float *angles,
    unsigned int animIndex, unsigned int rootIndex, unsigned int notifyName,
    unsigned char mode, float rate, bool dontInterpolate, float goalTime)
{
    if (!(ent->flags & FL_SUPPORTS_ANIMSCRIPTED))
        Scr_ObjectError(va("entity (classname: '%s') does not currently support animscripted",
            SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);
    XAnimTree_s *tree = GScr_GetEntAnimTree(ent);
    ScriptedAnimRecord_SP *record = ent->scriptedAnim_SP;
    if (!record)
    {
        record = reinterpret_cast<ScriptedAnimRecord_SP *>(
            MT_Alloc(100, 18, SCRIPTINSTANCE_SERVER));
        ent->scriptedAnim_SP = record;
    }
    record->updatedOnce = 0;
    record->animIndex = static_cast<unsigned short>(animIndex);
    record->rootIndex = static_cast<unsigned short>(rootIndex);
    record->mode = mode;
    record->startTime = level.time;
    if (ent->s.eType == ET_SCRIPTMOVER)
    {
        ent->s.lerp.u.scriptMover.animScriptedAnimTime = record->startTime;
        ent->s.lerp.u.scriptMover.animScriptedAnim = record->animIndex;
    }
    XAnim_s *anims = XAnimGetAnims(tree);
    if (mode == 1)
        GScr_InitScriptedDeathPlant_SP(ent, origin, angles, anims, animIndex);
    else
    {
        record->baseAxis[3][0] = origin[0];
        record->baseAxis[3][1] = origin[1];
        record->baseAxis[3][2] = origin[2];
        AnglesToAxis(angles,record->baseAxis);
    }
    DObj *obj = Com_GetServerDObj(ent->s.number);
    GScr_PrepareScriptedAnim_SP(ent,obj,tree,rootIndex);
    if (g_dumpAnimsCommands->current.integer == ent->s.number)
        GScr_PrintScriptedGoal_SP("animscripted(internal)", tree, animIndex, 1.0f, goalTime, rate);
    const int flags = 1 | ((!dontInterpolate && !XAnimIsLooped(anims,animIndex)) ? 2 : 0);
    const int command = G_StoreAnimCommand_SP(
        ent,tree,3,animIndex,0,1.0f,goalTime,rate,flags);
    const bool restart = !dontInterpolate && !XAnimIsLooped(anims,animIndex);
    XAnimSetCompleteGoalWeight(obj,animIndex,1.0f,goalTime,rate,notifyName,2,restart,command);
    G_FlagAnimForUpdate(ent);
    float rotation[2], translation[3], calculatedOrigin[3], calculatedAngles[3];
    float yawAxis[3][3], calculatedAxis[3][3];
    XAnimCalcAbsDelta(obj,animIndex,rotation,translation);
    MatrixTransformVector43(translation,record->baseAxis,calculatedOrigin);
    YawToAxis(static_cast<float>(RotationToYaw(rotation)),yawAxis);
    MatrixMultiply(yawAxis,record->baseAxis,calculatedAxis);
    AxisToAngles(calculatedAxis,calculatedAngles);
    for (int n=0;n<3;++n)
        record->originOffset[n] = ent->r.currentOrigin[n] - calculatedOrigin[n];
    AnglesSubtract(ent->r.currentAngles,calculatedAngles,record->angleOffset);
    GScr_LogScriptedAnimTransition_SP(ent, "start");
}

void __cdecl GScr_UpdateScriptedRecord_SP(gentity_s *ent)
{
    ScriptedAnimRecord_SP *record = ent->scriptedAnim_SP;
    if (!record)
        return;
    XAnimTree_s *tree = G_GetEntAnimTree(ent);
    if (!tree || !record->animIndex)
    {
        GScr_ClearScriptedAnim_SP(ent);
        return;
    }
    DObj *obj = Com_GetServerDObj(ent->s.number);
    float origin[3], angles[3];
    GScr_CalcScriptedRoot_SP(obj,record->animIndex,record,origin,angles);
    if (record->mode == 1)
        GScr_UpdateScriptedDeathPlant_SP(ent,tree,origin,angles);
    Vec3AddPartialOffset(origin,record->originOffset,0.25f,origin,record->originOffset);
    const float step = ent->angleLerpRate * 0.05f;
    AnglesAddPartialOffset(angles,record->angleOffset,step);
    GScr_PublishScriptedTransform_SP(ent,origin,angles);
    if (!record->updatedOnce)
    {
        record->updatedOnce = 1;
        return;
    }
    if (!XAnimHasFinished(tree,record->animIndex))
        return;
    GScr_LogScriptedAnimTransition_SP(ent, "complete");
    const int command = G_StoreAnimCommand_SP(
        ent,tree,3,record->animIndex,0,1.0f,0.2f,1.0f,1);
    XAnimSetCompleteGoalWeight(obj,record->animIndex,1.0f,0.2f,1.0f,0,0,false,command);
    record->animIndex = 0;
    GScr_ClearScriptedAnim_SP(ent);
    if (ent->s.eType == ET_SCRIPTMOVER || ent->s.eType == ET_VEHICLE || ent->s.eType == ET_VEHICLE_CORPSE)
    {
        ent->s.lerp.u.scriptMover.animScriptedAnimTime = 0;
        ent->s.lerp.u.scriptMover.animScriptedAnim = 0;
        G_SetOrigin(ent,ent->r.currentOrigin);
        G_SetAngle(ent,ent->r.currentAngles);
    }
}

void __cdecl GScr_StopScriptedRecord_SP(gentity_s *ent)
{
    GScr_LogScriptedAnimTransition_SP(ent, "stop-request");
    actor_s *actor = ent->actor;
    if (actor && actor->eSimulatedState[actor->simulatedStateLevel] == AIS_SCRIPTEDANIM)
        Actor_PopState(actor);
    if (ent->s.eType == ET_SCRIPTMOVER)
    {
        ent->s.lerp.u.scriptMover.animScriptedAnimTime = 0;
        ent->s.lerp.u.scriptMover.animScriptedAnim = 0;
    }
    ScriptedAnimRecord_SP *record = ent->scriptedAnim_SP;
    if (!record)
        return;
    if (record->animIndex)
    {
        float blendTime = 0.2f;
        if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) && Scr_GetType(0,SCRIPTINSTANCE_SERVER))
            blendTime = Scr_GetFloat(0,SCRIPTINSTANCE_SERVER);
        XAnimTree_s *tree = GScr_GetEntAnimTree(ent);
        if (g_dumpAnimsCommands->current.integer == ent->s.number)
            GScr_PrintScriptedGoal_SP("stopanimscripted", tree, record->animIndex,
                1.0f, blendTime, 1.0f);
        const int command = G_StoreAnimCommand_SP(
            ent,tree,3,record->animIndex,0,0.0f,blendTime,1.0f,0);
        DObj *obj = Com_GetServerDObj(ent->s.number);
        if (obj)
            XAnimSetCompleteGoalWeight(obj,record->animIndex,0.0f,blendTime,1.0f,0,0,false,command);
    }
    GScr_ClearScriptedAnim_SP(ent);
}


struct ScriptedAnimParams_SP
{
    unsigned int notifyName;
    float origin[3];
    float angles[3];
    scr_anim_s anim;
    unsigned int mode;
    scr_anim_s root;
    float rate;
    float goalTime;
};

static ScriptedAnimParams_SP GScr_ReadScriptedAnimParams_SP(XAnimTree_s *tree)
{
    ScriptedAnimParams_SP params = {};
    params.rate = 1.0f;
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 4)
    {
        params.mode = Scr_GetConstString(4, SCRIPTINSTANCE_SERVER);
        if (params.mode != scr_const.normal && params.mode != scr_const.deathplant)
            Scr_Error(SCRIPTINSTANCE_SERVER,
                va("Illegal mode %s for animScripted. Valid modes are normal and deathplant",
                    SL_ConvertToString(params.mode, SCRIPTINSTANCE_SERVER)), false);
        if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 5
            && Scr_GetType(5, SCRIPTINSTANCE_SERVER))
            params.root = Scr_GetAnim(5, tree, SCRIPTINSTANCE_SERVER);
        if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 6
            && Scr_GetType(6, SCRIPTINSTANCE_SERVER))
            params.rate = Scr_GetFloat(6, SCRIPTINSTANCE_SERVER);
        if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 7
            && Scr_GetType(7, SCRIPTINSTANCE_SERVER))
            params.goalTime = Scr_GetFloat(7, SCRIPTINSTANCE_SERVER);
    }
    params.anim = Scr_GetAnim(3, tree, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2, params.angles, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1, params.origin, SCRIPTINSTANCE_SERVER);
    params.notifyName = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    return params;
}

void __cdecl GScr_StartScriptedAnim_SP(
    scr_entref_t entref, bool startActorThread, bool dontInterpolate)
{
    gentity_s *ent = GetEntity(entref);
    DObj *obj = Com_GetServerDObj(ent->s.number);
    if (!obj)
        Scr_ObjectError("No model exists.", SCRIPTINSTANCE_SERVER);
    XAnimTree_s *tree = GScr_GetEntAnimTree(ent);
    const ScriptedAnimParams_SP params = GScr_ReadScriptedAnimParams_SP(tree);
    actor_s *actor = ent->actor;
    if (actor && !actor->Physics.bIsAlive)
    {
        const char *targetname = ent->targetname
            ? SL_ConvertToString(ent->targetname, SCRIPTINSTANCE_SERVER) : "<undefined>";
        const char *classname = SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER);
        const char *team = Sentient_NameForTeam(ent->sentient->eTeam);
        Scr_Error(SCRIPTINSTANCE_SERVER,
            va("tried to play a scripted animation on a dead AI; entity %i team %s origin %g %g %g targetname %s classname %s\n",
                ent->s.number, team, ent->r.currentOrigin[0], ent->r.currentOrigin[1],
                ent->r.currentOrigin[2], targetname, classname), false);
    }
    if (actor && startActorThread)
    {
        Actor_PushState(actor, AIS_SCRIPTEDANIM);
        Actor_KillAnimScript(actor);
        ScriptedAnimRecord_SP *record = ent->scriptedAnim_SP;
        if (record && record->animIndex)
        {
            if (g_dumpAnimsCommands->current.integer == ent->s.number)
                GScr_PrintScriptedGoal_SP("animscripted", tree, record->animIndex,
                    1.0f, params.goalTime, params.rate);
            const int command = G_StoreAnimCommand_SP(ent, tree, 3, record->animIndex,
                0, 1.0f, params.goalTime, params.rate, 1);
            XAnimSetCompleteGoalWeight(obj, record->animIndex, 1.0f,
                params.goalTime, params.rate, 0, 0, false, command);
            G_FlagAnimForUpdate(ent);
        }
        Scr_AddFloat(params.goalTime, SCRIPTINSTANCE_SERVER);
        Scr_AddFloat(params.rate, SCRIPTINSTANCE_SERVER);
        if (params.root.tree)
            Scr_AddAnim(params.root, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddUndefined(SCRIPTINSTANCE_SERVER);
        if (params.mode)
            Scr_AddConstString(params.mode, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddUndefined(SCRIPTINSTANCE_SERVER);
        Scr_AddAnim(params.anim, SCRIPTINSTANCE_SERVER);
        Scr_AddVector(const_cast<float *>(params.angles), SCRIPTINSTANCE_SERVER);
        Scr_AddVector(const_cast<float *>(params.origin), SCRIPTINSTANCE_SERVER);
        Scr_AddConstString(params.notifyName, SCRIPTINSTANCE_SERVER);
        const unsigned short thread = Scr_ExecEntThread(ent, g_scr_data.scripted, 8);
        Scr_FreeThread(thread, SCRIPTINSTANCE_SERVER);
    }
    else
        GScr_ApplyScriptedRecord_SP(ent, params.origin, params.angles,
            params.anim.index, params.root.index, params.notifyName,
            static_cast<unsigned char>(params.mode == scr_const.deathplant),
            params.rate, dontInterpolate, params.goalTime);
    if ((!actor || startActorThread) && ent->tagInfo)
    {
        float parentAxis[4][3];
        G_CalcTagParentAxis(ent, parentAxis);
        MatrixInverseOrthogonal43(parentAxis, ent->tagInfo->parentInvAxis);
    }
}

void __cdecl GScr_AnimScripted_SP(scr_entref_t entref)
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 8)
        Scr_Error(SCRIPTINSTANCE_SERVER, "too many parameters", false);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 8
        || Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 4)
        Scr_Error(SCRIPTINSTANCE_SERVER,
            "Incorrect number of parameters for animscripted command", false);
    GScr_StartScriptedAnim_SP(entref, true, false);
}

void __cdecl GScr_AnimScriptedSkipRestart_SP(scr_entref_t entref)
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 8)
        Scr_Error(SCRIPTINSTANCE_SERVER, "too many parameters", false);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 8
        || Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 4)
        Scr_Error(SCRIPTINSTANCE_SERVER,
            "Incorrect number of parameters for animscripted command", false);
    GScr_StartScriptedAnim_SP(entref, true, true);
}

bool __cdecl GScr_IsScriptedAnimActive_SP(const gentity_s *ent)
{
    return ent->scriptedAnim_SP != nullptr;
}

void __cdecl GScr_UpdateScriptedAnim_SP(gentity_s *ent)
{
    GScr_UpdateScriptedRecord_SP(ent);
}

bool __cdecl GScr_RunScriptedMover_SP(gentity_s *ent)
{
    if (!ent->scriptedAnim_SP)
        return false;
    const int oldType = ent->s.lerp.pos.trType;
    GScr_UpdateScriptedRecord_SP(ent);
    G_SetOrigin(ent, ent->r.currentOrigin);
    G_SetAngle(ent, ent->r.currentAngles);
    SV_LinkEntity(ent);
    if (!ent->scriptedAnim_SP)
    {
        if (oldType >= TR_RAGDOLL && oldType <= TR_RAGDOLL_INTERPOLATE)
        {
            ent->s.lerp.pos.trType = TR_RAGDOLL;
            ent->s.lerp.apos.trType = TR_RAGDOLL;
        }
        return false;
    }
    ent->s.lerp.pos.trType = TR_INTERPOLATE;
    ent->s.lerp.apos.trType = TR_INTERPOLATE;
    G_RunThink(ent);
    const ScriptedAnimRecord_SP *record = ent->scriptedAnim_SP;
    Vec3Copy(record->baseAxis[3], ent->s.lerp.pos.trDelta);
    AxisToAngles(record->baseAxis, ent->s.lerp.apos.trDelta);
    if (oldType >= TR_RAGDOLL && oldType <= TR_RAGDOLL_INTERPOLATE)
    {
        ent->s.lerp.pos.trType = TR_RAGDOLL_INTERPOLATE;
        ent->s.lerp.apos.trType = TR_RAGDOLL_INTERPOLATE;
    }
    return true;
}

void __cdecl GScr_StopAnimScripted_SP(scr_entref_t entref)
{
    GScr_StopScriptedRecord_SP(GetEntity(entref));
}

static void __cdecl GScr_PlayerLinkToAbsolute_SP(scr_entref_t entref)
{
    unsigned int tagName;
    gentity_s *parent;
    gentity_s *ent;

    ent = GetEntity(entref);
    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) != 1 || Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) != 19 )
        Scr_ParamError(0, "Not an entity", SCRIPTINSTANCE_SERVER);
    if ( !ent->client )
        Scr_ObjectError("Not a player entity", SCRIPTINSTANCE_SERVER);

    parent = Scr_GetEntity(0);
    tagName = 0;
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) > 1 && Scr_GetType(1u, SCRIPTINSTANCE_SERVER) )
    {
        tagName = Scr_GetConstLowercaseString(1u, SCRIPTINSTANCE_SERVER);
        if ( tagName == scr_const._ )
            tagName = 0;
    }

    ent->client->linkAnglesFrac = 1.0f;
    ent->client->linkAnglesLocked = 1;
    ent->client->ps.pm_flags |= 0x4000000u;
    Vec3Clear(ent->client->ps.linkAngles);
    ent->client->ps.linkFlags |= 1u;

    if ( !G_EntLinkTo(ent, parent, tagName) )
        Scr_Error("Failed to link entity", 0);
}

static void __cdecl GScr_SPMethod_PlayerLinkTo(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) != VAR_POINTER
        || Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) != VAR_ENTITY )
        Scr_ParamError(0, "not an entity", SCRIPTINSTANCE_SERVER);
    if ( !ent->client )
        Scr_ObjectError("not a player entity", SCRIPTINSTANCE_SERVER);

    gentity_s *parent = Scr_GetEntity(0);
    int numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    unsigned __int16 tagName = 0;
    if ( numParam > 1 && Scr_GetType(1, SCRIPTINSTANCE_SERVER) != VAR_UNDEFINED )
    {
        tagName = Scr_GetConstLowercaseString(1, SCRIPTINSTANCE_SERVER);
        if ( tagName == scr_const._ )
            tagName = 0;
    }

    float fraction = numParam > 2 ? Scr_GetFloat(2, SCRIPTINSTANCE_SERVER) : 0.0f;
    float arcs[4];
    for ( unsigned int i = 0; i < 4; ++i )
    {
        float arc = numParam > i + 3 ? Scr_GetFloat(i + 3, SCRIPTINSTANCE_SERVER) : 180.0f;
        float upper = arc - 180.0f >= 0.0f ? 180.0f : arc;
        arcs[i] = -arc >= 0.0f ? 0.0f : upper;
    }

    gclient_s *client = ent->client;
    G_UpdateViewAngleClamp(client, parent->r.currentAngles);
    if ( numParam > 7 && Scr_GetInt(7, SCRIPTINSTANCE_SERVER) )
        client->ps.linkFlags |= 2u;
    else
        client->ps.linkFlags &= ~2u;

    client->linkAnglesFrac = fraction;
    client->linkAnglesLocked = 0;
    client->ps.linkFlags &= ~1u;
    client->ps.pm_flags &= ~0x4000000u;
    client->prevLinkAnglesSet = 0;
    client->linkAnglesMinClamp[1] = -arcs[0];
    client->linkAnglesMaxClamp[1] = arcs[1];
    client->linkAnglesMinClamp[0] = -arcs[2];
    client->linkAnglesMaxClamp[0] = arcs[3];
    if ( !G_EntLinkTo(ent, parent, tagName) )
    {
        Scr_Error("failed to link entity", SCRIPTINSTANCE_SERVER);
        return;
    }

    if ( client->ps.linkFlags & 2 )
    {
        float parentAxis[4][3];
        G_CalcTagParentAxis(ent, parentAxis);
        AxisToAngles(parentAxis, client->ps.linkAngles);
    }
    else
    {
        Vec3Clear(client->ps.linkAngles);
    }
}
static void __cdecl GScr_LookAtEntity_SP(scr_entref_t entref)
{
    if (entref.classnum)
    {
        Scr_ObjectError("not an entity", SCRIPTINSTANCE_SERVER);
        return;
    }
    iassert(entref.entnum < MAX_GENTITIES);
    gentity_s *ent = &g_entities[entref.entnum];
    if (!ent->actor)
    {
        Scr_Error(SCRIPTINSTANCE_SERVER, va("lookatentity must be called on an AI, not on a '%s'",
            SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), false);
        return;
    }
    if (!Scr_GetNumParam(SCRIPTINSTANCE_SERVER))
    {
        ent->actor->state.lookAtEntNum = ENTITYNUM_NONE;
        return;
    }
    if (Scr_GetType(0, SCRIPTINSTANCE_SERVER) == VAR_POINTER
        && Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) == VAR_ENTITY)
        ent->actor->state.lookAtEntNum = Scr_GetEntity(0)->s.number;
}
static int GScr_BlurDurationMilliseconds_SP(float seconds)
{
    volatile float milliseconds = seconds * 1000.0f;
    if (!(milliseconds >= -2147483648.0f && milliseconds < 2147483648.0f))
        return (-2147483647 - 1);
    return (int)milliseconds;
}

static void GScr_SetBlur_SP(scr_entref_t entref)
{
    const float blur = (float)Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    const double seconds = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    const float roundedSeconds = (float)seconds;
    if (seconds < 0.0)
        Scr_ParamError(1, "Time must be positive", SCRIPTINSTANCE_SERVER);
    const int duration = GScr_BlurDurationMilliseconds_SP(roundedSeconds);
    if (blur < 0.0f)
        Scr_ParamError(0, "Blur value must be greater than 0", SCRIPTINSTANCE_SERVER);

    int clientNum = -1;
    if (entref.classnum)
    {
        Scr_ObjectError("not an entity", SCRIPTINSTANCE_SERVER);
        Scr_Error(SCRIPTINSTANCE_SERVER, "setblur() called on an invalid client entity.\n", false);
    }
    else
    {
        gentity_s *ent = &g_entities[entref.entnum];
        if (!ent || !ent->r.inuse || !ent->client)
            Scr_Error(SCRIPTINSTANCE_SERVER, "setblur() called on an invalid client entity.\n", false);
        else
            clientNum = ent->s.number;
    }
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE,
                            va("%c %i %f %i %i", 'A', duration, (double)blur, 0, 1));
}
static void GScr_GetVisionSetNaked_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->r.inuse || !ent->client)
        Scr_Error("getvisionsetnaked() called on an invalid client entity.\n", SCRIPTINSTANCE_SERVER);
    char config[1024];
    char name[1024];
    SV_GetConfigstring(CS_VISIONSET_NAKED, config, sizeof(config));
    I_strncpyz(name, Info_ValueForKey(config, va("%i", ent->s.number)), sizeof(name));
    for (int i = 0; name[i]; ++i)
    {
        const char c = name[i];
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')
            && !(c >= '0' && c <= '9') && c != '_' && c != '-')
        {
            name[i] = 0;
            break;
        }
    }
    Scr_AddString(name, SCRIPTINSTANCE_SERVER);
}
static void __cdecl GScr_BloodImpact_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 1)
    {
        Scr_Error("BloodImpact() called without an argument\n", SCRIPTINSTANCE_SERVER);
        return;
    }

    const unsigned int mode = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    unsigned __int16 eventParm;
    if (mode == scr_const.none)
        eventParm = 0;
    else if (mode == scr_const.normal)
        eventParm = 1;
    else if (mode == scr_const.hero)
        eventParm = 2;
    else
    {
        Scr_Error("BloodImpact() only takes \"normal\", \"hero\" or \"none\" as parameters.\n", SCRIPTINSTANCE_SERVER);
        return;
    }

    gentity_s *event = G_TempEntity(vec3_origin, EV_BLOOD_IMPACTS);
    event->s.lerp.pos.trTime = ent->s.number;
    event->s.eventParm = eventParm;
    Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
}
static int s_zombieNameDevIndex_SP = -1;
static const char *const s_zombieNameDevList_SP[40] = {
    "PLivingstone",
    "Astronaut Greer",
    "RElghazi",
    "ATabor",
    "GSpinrad",
    "ALivingston",
    "BPearson",
    "Cosmonaut Alex",
    "PonyGirl66",
    "JSimanello",
    "CCowell",
    "RHiga",
    "CAyers",
    "DLaufer",
    "Lamia",
    "Admiral Athena",
    "Nouriani",
    "Pink Unicorn",
    "JZielinski",
    "DSielke",
    "ducki02",
    "DKing",
    "SuzieQ64",
    "MAnthony",
    "CPierro",
    "ObusWolf",
    "Rezbox",
    "M Maestas",
    "HCheng",
    "WIp",
    "ABhura",
    "OGonzalez",
    "Spationaute Laura",
    "ALawton",
    "DDumlao",
    "DAnthony",
    "Zoe",
    "JDelgado",
    "BGlines",
    "Gordon"
};


static void GScr_SetHUDWarningType_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->actor)
        Scr_Error(SCRIPTINSTANCE_SERVER,
            va("sethudwarningtype must be called on an AI, not on a '%s'",
                SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), false);

    const char *type = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if (!I_stricmp(type, "banzai"))
        ent->actor->state.hudwarningType = 1;
    else if (!I_stricmp(type, "banzai_grenadesuicide"))
        ent->actor->state.hudwarningType = 2;
    else if (!I_stricmp(type, "zombie_friend"))
        ent->actor->state.hudwarningType = 3;
    else
        ent->actor->state.hudwarningType = 0;
}

static void GScr_SetZombieName_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->actor)
        Scr_Error(SCRIPTINSTANCE_SERVER,
            va("SetZombieName must be called on an AI, not on a '%s'",
                SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), false);

    const char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    unsigned char allowDevList;
    memcpy(&allowDevList, &r_zombieNameAllowDevList->current.enabled, sizeof(allowDevList));
    if (allowDevList == 1)
    {
        if (s_zombieNameDevIndex_SP == -1)
            s_zombieNameDevIndex_SP = G_irand(0, 40);
        else
        {
            unsigned int next = static_cast<unsigned int>(s_zombieNameDevIndex_SP) + 1u;
            memcpy(&s_zombieNameDevIndex_SP, &next, sizeof(next));
            if (next >= 40u)
                s_zombieNameDevIndex_SP = 0;
        }
        name = s_zombieNameDevList_SP[s_zombieNameDevIndex_SP];
    }
    strcpy(ent->actor->state.name, name);
}

static void GScr_SetPlayerCollision_SP(scr_entref_t entref)
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1)
        Scr_Error("Bad number of paramaters\n", SCRIPTINSTANCE_SERVER);

    gentity_s *ent = GetEntity(entref);
    const int enabled = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (enabled)
        ent->s.lerp.eFlags2 &= ~0x01000000;
    else
        ent->s.lerp.eFlags2 |= 0x01000000;

    if (ent->client)
    {
        if (enabled)
            ent->client->ps.eFlags2 &= ~0x01000000;
        else
            ent->client->ps.eFlags2 |= 0x01000000;
    }
}

static void GScr_SetHeliHeightPatchEnabled_SP()
{
    const unsigned __int16 targetname = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    const int enabled = Scr_GetInt(1, SCRIPTINSTANCE_SERVER);
    for (int i = 0; i < num_heli_height_lock_patches; ++i)
    {
        if (heli_height_lock_patches[i].targetname == targetname)
            heli_height_lock_patches[i].enabled = enabled;
    }
}
static void __cdecl GScr_SPMethod_stopsounds(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    ent->r.svFlags &= ~1u;
    G_AddEvent(ent, EV_STOPSOUNDS, 0);
}
static void GScr_UseWeaponHideTags_SP(scr_entref_t entref)
{
    if (Scr_GetNumParam(SCRIPTINSTANCE_SERVER) < 1)
        Scr_Error("useweaponhidetags <weaponName>.\n", SCRIPTINSTANCE_SERVER);
    char *weaponName = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const unsigned int weaponIndex = G_GetWeaponIndexForName(weaponName);
    if (!weaponIndex)
        Scr_Error(va("useweaponhidetags called with unknown weapon name %s\n", weaponName), SCRIPTINSTANCE_SERVER);
    const WeaponVariantDef *variant = BG_GetWeaponVariantDef(weaponIndex);
    gentity_s *ent = GetEntity(entref);
    memset(ent->s.partBits, 0, sizeof(ent->s.partBits));
    DObj *obj = Com_GetServerDObj(ent->s.number);
    for (int i = 0; i < 32 && variant->hideTags[i]; ++i)
    {
        unsigned char boneIndex = 0xFE;
        if (DObjGetBoneIndex(obj, variant->hideTags[i], &boneIndex, -1))
            ent->s.partBits[boneIndex >> 5] |= 0x80000000u >> (boneIndex & 31);
    }
    DObjSetHidePartBits(obj, ent->s.partBits);
}
static void __cdecl GScr_SPMethod_AddAIEventListener(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    const unsigned __int16 eventString = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);
    Actor_EventListener_Add(ent->s.number, eventString);
}
static void __cdecl GScr_SPMethod_DontInterpolate(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if ( ent->client )
        ent->client->ps.eFlags ^= 2;
    else
        ent->s.lerp.eFlags ^= 2;
}
static int GScr_GetGrenadeFuseTime(unsigned int index)
{
    double time = Scr_GetFloat(index, SCRIPTINSTANCE_SERVER) * 1000.0;
    return time >= -2147483648.0 && time < 2147483648.0 ? (int)time : INT_MIN;
}

static void __cdecl GScr_SPMethod_MagicGrenade(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if ( !ent->actor )
        Scr_Error("MagicGrenade only supports actors.\n", SCRIPTINSTANCE_SERVER);
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 2
        && Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 3 )
        Scr_Error("<actor> MagicGrenade <origin> <target position> [time to blow (seconds)].\n", SCRIPTINSTANCE_SERVER);

    float origin[3], target[3], velocity[3];
    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1, target, SCRIPTINSTANCE_SERVER);
    if ( !Actor_Grenade_GetMagicVelocity_SP(ent->actor, origin, target, velocity) )
    {
        Com_PrintWarning(24, "MagicGrenade: None of the methods worked (probably distance or blocked)...need a good failsafe or remove this print?\n");
        return;
    }
    const int time = Scr_GetNumParam(SCRIPTINSTANCE_SERVER) == 3 ? GScr_GetGrenadeFuseTime(2) : 5000;
    const int weaponIndex = ent->actor->iGrenadeWeaponIndex;
    if ( !weaponIndex )
        Scr_Error(va("Actor [%s] doesn't have a grenade weapon set.",
            SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);

    gentity_s *grenade = G_FireGrenade(ent, origin, velocity, weaponIndex, 0, 1, time);
    grenade->r.ownerNum.setEnt(NULL);
    grenade->parent.setEnt(NULL);
    Scr_AddEntity(grenade, SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_SPMethod_MagicGrenadeManual(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if ( !ent->actor )
        Scr_Error("MagicGrenadeManual only supports actors.\n", SCRIPTINSTANCE_SERVER);

    int numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( numParam != 2 && numParam != 3 )
        Scr_Error("<actor> MagicGrenadeManual <origin> <velocity> [time To Blow (seconds)].\n", SCRIPTINSTANCE_SERVER);

    float origin[3];
    float velocity[3];
    Scr_GetVector(0, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(1, velocity, SCRIPTINSTANCE_SERVER);
    int time = numParam == 3 ? GScr_GetGrenadeFuseTime(2) : 5000;
    int weaponIndex = ent->actor->iGrenadeWeaponIndex;
    if ( !weaponIndex )
        Scr_Error(va("Actor [%s] doesn't have a grenade weapon set.",
            SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);

    gentity_s *grenade = G_FireGrenade(ent, origin, velocity, weaponIndex, 0, 1, time);
    Scr_AddEntity(grenade, SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_SPMethod_MagicGrenadeType(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    int weaponIndex = G_GetWeaponIndexForName(Scr_GetString(0, SCRIPTINSTANCE_SERVER));
    if ( BG_GetWeaponDef(weaponIndex)->weapType != WEAPTYPE_GRENADE )
        Scr_Error("MagicGrenadeManual invalid weapon type specified.\n", SCRIPTINSTANCE_SERVER);

    int numParam = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if ( numParam != 3 && numParam != 4 )
        Scr_Error("<ent> MagicGrenadeManual <weaponname> <origin> <velocity> [time To Blow (seconds)].\n", SCRIPTINSTANCE_SERVER);

    float origin[3];
    float velocity[3];
    Scr_GetVector(1, origin, SCRIPTINSTANCE_SERVER);
    Scr_GetVector(2, velocity, SCRIPTINSTANCE_SERVER);
    int time = numParam == 4 ? GScr_GetGrenadeFuseTime(3) : 5000;
    gentity_s *grenade = G_FireGrenade(ent, origin, velocity, weaponIndex, 0, 1, time);
    Scr_AddEntity(grenade, SCRIPTINSTANCE_SERVER);
}
static void GScr_MakeFakeAI_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (ent->s.eType != ET_SCRIPTMOVER || !(ent->flags & FL_SUPPORTS_ANIMSCRIPTED))
        Scr_Error(SCRIPTINSTANCE_SERVER, "makeFakeAI must be applied to a script_model", false);

    ent->r.svFlags = static_cast<unsigned __int16>((ent->r.svFlags & ~4u) | 2u);
    ent->r.mins[0] = -15.0f;
    ent->r.mins[1] = -15.0f;
    ent->r.mins[2] = 0.0f;
    ent->r.maxs[0] = 15.0f;
    ent->r.maxs[1] = 15.0f;
    ent->r.maxs[2] = 72.0f;
    ent->r.contents |= 0x8000;
    ent->s.surfType = 7;
    SV_LinkEntity(ent);
}
static void GScr_VisionSetNightPlayer_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    char config[1024];
    SV_GetConfigstring(CS_VISIONSET_NIGHT, config, sizeof(config));
    if (!ent || !ent->r.inuse || !ent->client)
        Scr_Error(SCRIPTINSTANCE_SERVER, "visionsetnight() called on an invalid client entity.\n", false);
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (count != 1 && count != 2)
    {
        Scr_Error(SCRIPTINSTANCE_SERVER, "USAGE: player VisionSetNight( <visionset name>, <transition time> )\n", false);
        return;
    }
    const int duration = count == 2 ? GScr_VisionTransitionMsec_SP() : 1000;
    const char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const char *value = va("\"%s\" %i", name, duration);
    char *key = va("%i", ent->s.number);
    Info_SetValueForKey(config, key, value);
    SV_SetConfigstring(CS_VISIONSET_NIGHT, config);
}

static unsigned char visionSetLastStandChangeCounter_SP;

static void GScr_VisionSetLastStandPlayer_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    char config[1024];
    SV_GetConfigstring(CS_SP_VISIONSET_LASTSTAND, config, sizeof(config));
    if (!ent || !ent->r.inuse || !ent->client)
        Scr_Error("visionsetlaststand() called on an invalid client entity.\n", SCRIPTINSTANCE_SERVER);
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (count != 1 && count != 2)
    {
        Scr_Error("USAGE: player visionsetlaststand( <visionset name>, <transition time> )\n", SCRIPTINSTANCE_SERVER);
        return;
    }
    const int duration = count == 2 ? GScr_VisionTransitionMsec_SP() : 1000;
    const char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const char *value = va("\"%s\" %i %c", name, duration, visionSetLastStandChangeCounter_SP);
    char *key = va("%i", ent->s.number);
    Info_SetValueForKey(config, key, value);
    visionSetLastStandChangeCounter_SP = (unsigned char)((visionSetLastStandChangeCounter_SP + 1) | 1);
    SV_SetConfigstring(CS_SP_VISIONSET_LASTSTAND, config);
}
static void GScr_IsWeaponOverheating_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->client)
        Scr_ObjectError(va("entity %i is not a player", entref.entnum), SCRIPTINSTANCE_SERVER);

    const int returnHeat = Scr_GetNumParam(SCRIPTINSTANCE_SERVER)
        ? Scr_GetInt(0, SCRIPTINSTANCE_SERVER) : 0;
    const playerState_s *ps = &ent->client->ps;
    if (ps->eFlags & 0x4000)
    {
        gentity_s *vehicleEnt = &g_entities[ps->viewlocked_entNum];
        const scr_vehicle_s *vehicle = vehicleEnt->scr_vehicle;
        const vehicle_info_t *info = BG_GetVehicleInfo(vehicle->infoIdx);
        const VehicleTurret *turret;
        const unsigned int gunnerIndex = static_cast<unsigned int>(ps->vehiclePos) - 1u;
        if (gunnerIndex <= 3u)
        {
            turret = &vehicle->gunnerTurrets[gunnerIndex];
        }
        else
        {
            if (ps->vehiclePos)
            {
                Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
                return;
            }
            if (static_cast<unsigned char>(vehicleEnt->s.weapon) || info->driverControlledGunPos < 0)
                turret = &vehicle->turret;
            else
                turret = &vehicle->gunnerTurrets[info->driverControlledGunPos];
        }
        if (returnHeat)
            Scr_AddFloat(turret->heatVal, SCRIPTINSTANCE_SERVER);
        else
            Scr_AddInt(turret->overheating, SCRIPTINSTANCE_SERVER);
        return;
    }
    if (ps->eFlags & 0x300)
    {
        const TurretInfo *turret = g_entities[ps->viewlocked_entNum].pTurretInfo;
        Scr_AddInt(turret ? turret->overheating : 0, SCRIPTINSTANCE_SERVER);
        return;
    }
    Scr_AddInt(BG_PlayerWeaponOverheating(ps, static_cast<unsigned char>(ps->weapon)) ? 1 : 0,
        SCRIPTINSTANCE_SERVER);
}

static void GScr_IsPlayerReloading_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->client)
    {
        Scr_Error("isplayerreloading should only be called for players.", SCRIPTINSTANCE_SERVER);
        return;
    }
    const int state = ent->client->ps.weaponstate;
    Scr_AddInt(state >= WEAPON_RELOADING && state <= WEAPON_RELOAD_QUICK_EMPTY,
        SCRIPTINSTANCE_SERVER);
}

static void GScr_SPMeth_visionsetnaked(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    char config[1024];
    SV_GetConfigstring(CS_VISIONSET_NAKED, config, sizeof(config));
    if (!ent->r.inuse || !ent->client)
        Scr_Error("visionsetnaked() called on an invalid client entity.\n", SCRIPTINSTANCE_SERVER);
    const int count = Scr_GetNumParam(SCRIPTINSTANCE_SERVER);
    if (count != 1 && count != 2)
        Scr_Error("USAGE: player VisionSetNaked( <visionset name>, <transition time> )\n", SCRIPTINSTANCE_SERVER);
    const int duration = count == 2 ? GScr_VisionTransitionMsec_SP() : 1000;
    const char *name = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    const char *value = va("\"%s\" %i", name, duration);
    char *key = va("%i", ent->s.number);
    Info_SetValueForKey(config, key, value);
    SV_SetConfigstring(CS_VISIONSET_NAKED, config);
}
// ---------------------------------------------------------------------------
// self CodeSpawnerSpawn( [flag], [targetname] ) / self CodeSpawnerForceSpawn(...)
// -- SP only. REAL BODIES (these rows used to be no-op TODO(SP-STUB)s).
//
// EVIDENCE: retail SP handlers 0x007f3250 (spawn) and 0x007f33c0 (forcespawn),
// decompiled 2026-08-22; the two are byte-for-byte identical except the
// forceSpawn argument passed to SpawnActor (0x00526e50). Retail flow: entref
// decode (>>16 != 0 -> Scr_ObjectError "not an entity" -- exactly what this
// tree's GetEntity() does), eType check against the actor-spawner entity type
// (retail SP raw 0x11; this tree's enum has ET_ACTOR_SPAWNER = 0x12 because
// the MP eType enum inserts an extra entry -- the SYMBOL is what SP_actor_spawner
// (actor_spawner.cpp:269) actually stores, so the symbol is correct here),
// once-per-frame gate on the dword retail keeps at gentity+0x218 (== this
// tree's spawner.timestamp union slot: item_ent_t item[0].clipAmmoCount,
// which SP_actor_spawner:270 initializes to -1), optional params
// [0]=int flag, [1]=targetname const-string, then
// SpawnActor(spawner, targetname, CHECK_SPAWN/FORCE_SPAWN, flag == 0).
// NOTE param 0 selects get-enemy-info (flag==0 -> copy), NOT forcing --
// forcing is the separate builtin. SpawnActor is already fully ported
// (actor_spawner.cpp:89, verified statement-for-statement against retail
// 0x00526e50). On success: gate := level.time, Scr_AddEntity(spawn); on NULL
// or gated: nothing pushed -> undefined, which maps/_utility.gsc's DoSpawn
// IsDefined() check expects.
//
// DOCUMENTED DIVERGENCES from retail:
//  - retail writes spawner's gentity+0x21c dword (this tree: item[0].index)
//    into the spawned ent's svEntity-side record at +0xF8 (0x027f9808 +
//    n*0x168). OPEN ITEM: that record dword is networked by
//    MSG_WriteEntityDelta but its semantics are unestablished; no counterpart
//    field is identified in this tree's svEntity_s. Omitted until mapped.
static void __cdecl GScr_CodeSpawnerSpawn_Common(scr_entref_t entref, enumForceSpawn forceSpawn, const char *builtinName)
{
    char *v1; // eax
    const char *v2; // eax
    const char *nameStr; // [esp+0h] [ebp-10h]
    unsigned int targetname; // [esp+4h] [ebp-Ch]
    int flag; // [esp+8h] [ebp-8h]
    gentity_s *spawner; // [esp+Ch] [ebp-4h]
    gentity_s *spawn;

    spawner = GetEntity(entref);
    if ( spawner->s.eType != ET_ACTOR_SPAWNER )
    {
        if ( spawner->targetname )
            nameStr = SL_ConvertToString(spawner->targetname, SCRIPTINSTANCE_SERVER);
        else
            nameStr = "<unnamed>";
        v1 = SL_ConvertToString(spawner->classname, SCRIPTINSTANCE_SERVER);
        v2 = va(
            "%s can only be called on actor spawners\n"
            "attempted to call %s on entity with name '%s' of type '%s' at (%.0f %.0f %.0f)\n",
            builtinName,
            builtinName,
            nameStr,
            v1,
            spawner->r.currentOrigin[0],
            spawner->r.currentOrigin[1],
            spawner->r.currentOrigin[2]);
        Scr_Error(v2, 0);
        return;
    }
    if ( spawner->spawner.timestamp >= level.time )
        return;                             // once-per-frame gate -> undefined
    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 1 )
        flag = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    else
        flag = 0;
    if ( (unsigned int)Scr_GetNumParam(SCRIPTINSTANCE_SERVER) >= 2 )
        targetname = (unsigned __int16)Scr_GetConstString(1u, SCRIPTINSTANCE_SERVER);
    else
        targetname = 0;
    G_RecordSpawnAttempt_SP(spawner->classname);
    spawn = SpawnActor(spawner, targetname, forceSpawn, flag == 0);
    if ( spawn )
    {
        spawner->spawner.timestamp = level.time;
        Scr_AddEntity(spawn, SCRIPTINSTANCE_SERVER);
    }
}

static void __cdecl GScr_CodeSpawnerSpawn_SP(scr_entref_t entref)
{
    GScr_CodeSpawnerSpawn_Common(entref, CHECK_SPAWN, "CodeSpawnerSpawn");
}

static void __cdecl GScr_CodeSpawnerForceSpawn_SP(scr_entref_t entref)
{
    // Retail 0x007f33c0: identical to 0x007f3250 but SpawnActor forceSpawn=1
    // (skips the telefrag + player-visibility refusals).
    GScr_CodeSpawnerSpawn_Common(entref, FORCE_SPAWN, "CodeSpawnerForceSpawn");
}
static void __cdecl GScr_SPMethod_GetAiVelocity(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if ( !ent->sentient )
        Scr_Error(va("entity type '%s' is not a sentient",
            SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);

    float velocity[3];
    Sentient_GetVelocity(ent->sentient, velocity);
    Scr_AddVector(velocity, SCRIPTINSTANCE_SERVER);
}
static void GScr_StartFadingBlur_SP(scr_entref_t entref)
{
    float blur = (float)Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    double seconds = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    float roundedSeconds = (float)seconds;
    if (seconds < 0.0)
        Scr_ParamError(1, "Time must be positive", SCRIPTINSTANCE_SERVER);
    int duration = GScr_BlurDurationMilliseconds_SP(roundedSeconds);
    if (blur < 0.0f)
        Scr_ParamError(0, "Blur value must be greater than 0", SCRIPTINSTANCE_SERVER);
    int clientNum = -1;
    if (entref.classnum)
    {
        Scr_ObjectError("not an entity", SCRIPTINSTANCE_SERVER);
        Scr_Error(SCRIPTINSTANCE_SERVER, "setblur() called on an invalid client entity.\n", false);
    }
    else
    {
        gentity_s *ent = &g_entities[entref.entnum];
        if (!ent || !ent->r.inuse || !ent->client)
            Scr_Error(SCRIPTINSTANCE_SERVER, "setblur() called on an invalid client entity.\n", false);
        else
            clientNum = ent->s.number;
    }
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE, va("%c %i %f", '|', duration, (double)blur));
}
SP_STUB_METHOD(GScr_SPStubMeth_getdestructiblename,   "getdestructiblename", "0x007f1db0")
    // TODO(SP-STUB) 2 GSC ref(s), e.g. maps/_createdynents:1873 -- model_name = level.selected_object getdestructiblename();
static void GScr_SetExploderId_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (ent->s.eType != ET_SCRIPTMOVER)
    {
        Scr_Error("SetExploderId called on an ent that's not a script mover.", SCRIPTINSTANCE_SERVER);
        return;
    }
    ent->s.lerp.u.scriptMover.exploderIndex = std::bit_cast<__int16>(
        static_cast<unsigned __int16>(Scr_GetInt(0, SCRIPTINSTANCE_SERVER)));
}
// Retail SP setlookattext (0x0048CD50) is implemented in g_sp_crosshair.cpp.
static void __cdecl GScr_SetMaxHealth_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    const int health = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    ent->health = health;
    ent->maxHealth = health;
    if (ent->client)
        ent->client->sess.maxHealth = health;
}
static void __cdecl GScr_SetVolFog_SP(scr_entref_t entref)
{
    if (entref.classnum)
        Scr_ObjectError("not an entity", SCRIPTINSTANCE_SERVER);
    gentity_s *ent = &g_entities[entref.entnum];
    if (!ent->r.inuse || !ent->client)
        Scr_Error("setvolumetricfog() called on an invalid client entity.\n", SCRIPTINSTANCE_SERVER);
    Scr_ReadVolumetricFog_SP(ent->s.number, false);
}
SP_STUB_METHOD(GScr_SPStubMeth_getdebugeye,           "getdebugeye", "0x007f4310")
    // TODO(SP-STUB) 1 GSC ref(s), e.g. animscripts/utility:1184 -- Print3d ((self GetDebugEye()) + (0,0,8), stringtodraw, (1, 1, 1), 1, 0.2);
// ---------------------------------------------------------------------------
// <entity> getlinkedent() -- SP only. REAL BODY (this row used to be an
// undefined-returning TODO(SP-STUB)).
//
// EVIDENCE: retail SP handler 0x007f2640 is, in full:
//     ent = GetEntity(entref);
//     if (ent+0x298) Scr_AddEntity(*(void **)(ent+0x298));
// i.e. a null-guarded push of the FIRST member of a struct hanging off the
// entity. The only such pointer on gentity_s in link-land is tagInfo, whose
// first member is `gentity_s *parent` (bg_public.h:233-234) -- and this tree's
// own code does exactly this dereference chain, e.g. g_items.cpp:352-356
// `ent->tagInfo && ent->tagInfo->parent`, turret.cpp:104, g_mover.cpp:166.
// "the ent I am linked to" IS tagInfo->parent, so the identification is both
// structural and semantic.
//
// RETURN SHAPE: when the entity is not linked, retail pushes NOTHING, so the
// call evaluates to undefined. That is deliberate and is what the corpus
// expects -- maps/_utility.gsc:13572 assigns the result and IsDefined-guards it.
// This body reproduces that exactly rather than substituting a placeholder.
static void __cdecl GScr_GetLinkedEnt_SP(scr_entref_t entref)
{
    gentity_s *ent; // [esp+0h] [ebp-4h]

    ent = GetEntity(entref);
    if ( ent->tagInfo )
        Scr_AddEntity(ent->tagInfo->parent, SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_ScaleBuoyancy_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    const float scale = static_cast<float>(Scr_GetFloat(0, SCRIPTINSTANCE_SERVER));
    gentity_s *temp = G_TempEntity(vec3_origin, EV_SCALE_BUOYANCY);
    temp->s.otherEntityNum = ent->s.number;
    const float value = scale * 1000.0f;
    const int encoded = value >= -2147483648.0f && value < 2147483648.0f
        ? static_cast<int>(value) : INT_MIN;
    temp->s.eventParm = static_cast<unsigned __int16>(encoded);
}

static void __cdecl GScr_FloatLonger_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    gentity_s *temp = G_TempEntity(vec3_origin, EV_FLOAT_LONGER);
    temp->s.otherEntityNum = ent->s.number;
}

static void __cdecl GScr_SetLocalWindSource_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    const int enabled = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    G_AddEvent(ent, EV_SETLOCALWIND, enabled);
}

static void __cdecl GScr_DisableClientLinkTo_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    ent->r.svFlags |= 0x200u;
}

static void __cdecl GScr_EnableClientLinkTo_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    ent->r.svFlags &= ~0x200u;
}

static void __cdecl GScr_HasPath_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if (!ent->actor)
    {
        Scr_Error(va("haspath must be called on an AI, not on a '%s'",
            SL_ConvertToString(ent->classname, SCRIPTINSTANCE_SERVER)), false);
        return;
    }
    Scr_AddInt(Actor_HasPath(ent->actor), SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_SPStubMeth_iswaitingonsound(scr_entref_t entref)
{
    // Retail SP 0x007F4AA0 is exactly (ent->soundNotifyString != 0). The
    // reconstructed MP-shaped gentity stores that field in an SP sidecar.
    Scr_AddInt(G_SPIsWaitingOnSound(GetEntity(entref)), SCRIPTINSTANCE_SERVER);
}
static void GScr_SetDoubleVision_SP(scr_entref_t entref)
{
    const float amount = (float)Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
    const double seconds = Scr_GetFloat(1, SCRIPTINSTANCE_SERVER);
    const float roundedSeconds = (float)seconds;
    if (seconds < 0.0)
        Scr_ParamError(1, "Time must be positive", SCRIPTINSTANCE_SERVER);
    const int duration = GScr_BlurDurationMilliseconds_SP(roundedSeconds);
    if (amount < 0.0f)
        Scr_ParamError(0, "Double vision value must be greater than 0", SCRIPTINSTANCE_SERVER);

    int clientNum = -1;
    if (entref.classnum)
    {
        Scr_ObjectError("not an entity", SCRIPTINSTANCE_SERVER);
        Scr_Error(SCRIPTINSTANCE_SERVER, "setdoublevision() called on an invalid client entity.\n", false);
    }
    else
    {
        gentity_s *ent = &g_entities[entref.entnum];
        if (!ent || !ent->r.inuse || !ent->client)
            Scr_Error(SCRIPTINSTANCE_SERVER, "setdoublevision() called on an invalid client entity.\n", false);
        else
            clientNum = ent->s.number;
    }
    SV_GameSendServerCommand(clientNum, SV_CMD_RELIABLE,
                            va("%c %i %f", '4', duration, (double)amount));
}
SP_STUB_METHOD(GScr_SPStubMeth_setshadowhint,         "setshadowhint", "0x007f4c40")
    // TODO(SP-STUB) 1 GSC ref(s), e.g. maps/_vehicle:4286 -- self.mgturret[ 0 ] setshadowhint( "never" );
static void GScr_SetVehicleAttachments_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    const int enabled = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (enabled)
        ent->s.lerp.eFlags2 |= 0x02000000u;
    else
        ent->s.lerp.eFlags2 &= ~0x02000000u;
    SV_SendServerCommand(nullptr, SV_CMD_RELIABLE, "%c %d %d", '=', ent->s.number, enabled);
}
static void GScr_TransmitTargetName_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    ent->r.svFlags = static_cast<unsigned __int16>((ent->r.svFlags & ~1u) | 0x100u);
}
// ===========================================================================
// TODO(SP-STUB) -- retail-SP threat-bias methods. PLACEMENT IS A FALLBACK,
// READ THIS BEFORE MOVING THEM.
//
// These three come from a retail-SP method table at 0x00A55E40 (5 entries:
// getenemysqdist, getclosestenemysqdist, setthreatbiasgroup,
// getthreatbiasgroup, isnotarget), reached through the 2nd link of SP's
// Scr_GetMethod chain (SP dispatcher FUN_00546810). That table HAS NO
// COUNTERPART ANYWHERE IN THIS TREE -- its name overlap with all seven repo
// method tables is exactly ZERO, so there is no "right" table to put them in.
// (The repo's 5th chain link, Helicopter_GetMethod / s_methods[], is a
// different table entirely: retail SP has no helicopter method table at all
// and folds those names into its vehicle table instead.)
//
// They are registered HERE, in methods_3[], purely because BuiltIn_GetMethod
// is the LAST link of Scr_GetMethod's chain, so a name parked here still
// resolves for every entity type -- exactly as SP's own dedicated table does.
// This is a deliberate fallback placement, not an attribution claim: if a
// real threat-bias subsystem is ever reconstructed, these belong in its own
// table with its own dispatcher inserted into Scr_GetMethod.
//
// Placeholders, NOT implementations -- see the TODO(SP-STUB) header above
// BuiltinFunctionDef functions[]. type == 0 matches retail SP on all three.
// ===========================================================================

static sentient_s *GScr_GetSentient_SP(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if ( !ent->sentient )
    {
        Scr_ObjectError("not a sentient", SCRIPTINSTANCE_SERVER);
        return NULL;
    }
    return ent->sentient;
}

static void __cdecl GScr_SetThreatBiasGroup_SP(scr_entref_t entref)
{
    sentient_s *sentient = GScr_GetSentient_SP(entref);
    if ( !sentient )
        return;

    if ( Scr_GetNumParam(SCRIPTINSTANCE_SERVER) != 1 )
    {
        sentient->iThreatBiasGroupIndex = 0;
        return;
    }

    sentient->iThreatBiasGroupIndex = GScr_RequireThreatBiasGroup_SP(0);
}

static void __cdecl GScr_GetThreatBiasGroup_SP(scr_entref_t entref)
{
    sentient_s *sentient = GScr_GetSentient_SP(entref);
    if ( !sentient )
        return;

    if ( sentient->iThreatBiasGroupIndex > 0 )
    {
        Scr_AddString(
            SL_ConvertToString(
                g_threatBias.groupName[sentient->iThreatBiasGroupIndex],
                SCRIPTINSTANCE_SERVER),
            SCRIPTINSTANCE_SERVER);
        return;
    }

    Scr_AddString("", SCRIPTINSTANCE_SERVER);
}

static void __cdecl GScr_GetClosestEnemySqDist_SP(scr_entref_t entref)
{
    sentient_s *self = GScr_GetSentient_SP(entref);
    if ( !self )
        return;

    actor_s *actor = self->ent->actor;
    if ( !actor )
    {
        Scr_ObjectError("not an actor", SCRIPTINSTANCE_SERVER);
        return;
    }

    const team_t enemyTeam = Sentient_EnemyTeam(self->eTeam);
    if ( enemyTeam == TEAM_FREE )
        return;

    float selfOrigin[3];
    Sentient_GetOrigin(self, selfOrigin);
    float closestSqDist = 100000000.0f;

    const int enemyTeamFlags = 1 << enemyTeam;
    for ( sentient_s *enemy = Sentient_FirstSentient(enemyTeamFlags);
          enemy;
          enemy = Sentient_NextSentient(enemy, enemyTeamFlags) )
    {
        const int sentientIndex = (int)(enemy - level.sentients);
        if ( actor->sentientInfo[sentientIndex].lastKnownPosTime <= 0
            || (enemy->ent->flags & FL_NOTARGET) != 0
            || Actor_CheckIgnore(self, enemy) )
        {
            continue;
        }

        float enemyOrigin[3];
        Sentient_GetOrigin(enemy, enemyOrigin);
        const float dx = selfOrigin[0] - enemyOrigin[0];
        const float dy = selfOrigin[1] - enemyOrigin[1];
        const float dz = selfOrigin[2] - enemyOrigin[2];
        const float sqDist = dx * dx + dy * dy + dz * dz;
        if ( sqDist < closestSqDist )
            closestSqDist = sqDist;
    }

    Scr_AddFloat(closestSqDist, SCRIPTINSTANCE_SERVER);
}

// Retail SP's dedicated threat-method table pairs the literal "isnotarget"
// at 0x009C9FDC with handler 0x0067B6A0. The handler requires a sentient and
// returns bit 2 of its owning entity's flags. Cmd_Notarget_f and
// gentityFlags_t independently attest that bit as FL_NOTARGET.
static void __cdecl GScr_SPMethod_isnotarget(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    if ( !ent->sentient )
    {
        Scr_ObjectError("not a sentient", SCRIPTINSTANCE_SERVER);
        return;
    }

    Scr_AddInt((ent->sentient->ent->flags & FL_NOTARGET) != 0, SCRIPTINSTANCE_SERVER);
}

// Retail SP table entry 0x00A54B9C pairs "setteamforentity" with handler
// 0x008042D0. Unlike Sentient_SetTeam, this method writes the entity's compact
// team byte directly. The body matches the attested GScr_SetTeamForTrigger
// mapping without that method's trigger-class restriction.
static void __cdecl GScr_SPMethod_setteamforentity(scr_entref_t entref)
{
    gentity_s *ent = GetEntity(entref);
    const unsigned __int16 team = Scr_GetConstString(0, SCRIPTINSTANCE_SERVER);

    if ( team == scr_const.allies )
        ent->team = TEAM_ALLIES;
    else if ( team == scr_const.axis )
        ent->team = TEAM_AXIS;
    else if ( team == scr_const.none )
        ent->team = TEAM_FREE;
    else
        Scr_Error(
            va(
                "setteamforentity: invalid team used must be %s, %s or %s",
                SL_ConvertToString(scr_const.allies, SCRIPTINSTANCE_SERVER),
                SL_ConvertToString(scr_const.axis, SCRIPTINSTANCE_SERVER),
                SL_ConvertToString(scr_const.none, SCRIPTINSTANCE_SERVER)),
            SCRIPTINSTANCE_SERVER);
}

// Retail SP methods_3 entry 23 at 0x00A54350 pairs the literal
// "playersetgroundreferenceent" with handler 0x007F27C0. The handler stores
// ENTITYNUM_NONE for an undefined argument, otherwise requires an entity
// pointer and stores its entity number in retail gclient_s +0x1D10. That
// offset is inside this reconstruction's much larger MP-derived playerState_s,
// so extending gclient_s is not layout-correct. Preserve the method state in
// SP-only side storage until the full SP player-state layout/transport is
// reconstructed; never alias an unrelated MP field.
static int g_groundReferenceEntNum_SP[32];
static bool g_groundReferenceEntNumInitialized_SP;

void __cdecl GScr_ResetGroundReferenceState_SP()
{
    for ( int clientNum = 0; clientNum < 32; ++clientNum )
        g_groundReferenceEntNum_SP[clientNum] = ENTITYNUM_NONE;
    g_groundReferenceEntNumInitialized_SP = true;
}

static void GScr_EnsureGroundReferenceState_SP()
{
    if ( g_groundReferenceEntNumInitialized_SP )
        return;

    GScr_ResetGroundReferenceState_SP();
}

static void __cdecl GScr_SPMethod_playersetgroundreferenceent(scr_entref_t entref)
{
    gentity_s *player = GetEntity(entref);
    if ( !player->client )
    {
        Scr_ObjectError("not a player entity", SCRIPTINSTANCE_SERVER);
        return;
    }

    GScr_EnsureGroundReferenceState_SP();
    const int clientNum = player->s.number;
    if ( static_cast<unsigned int>(clientNum) >= 32u )
    {
        Scr_ObjectError("player entity index is outside the client range", SCRIPTINSTANCE_SERVER);
        return;
    }

    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) == VAR_UNDEFINED )
    {
        g_groundReferenceEntNum_SP[clientNum] = ENTITYNUM_NONE;
        return;
    }

    if ( Scr_GetType(0, SCRIPTINSTANCE_SERVER) != VAR_POINTER
        || Scr_GetPointerType(0, SCRIPTINSTANCE_SERVER) != VAR_ENTITY )
    {
        Scr_ParamError(0, "not an entity", SCRIPTINSTANCE_SERVER);
        return;
    }

    g_groundReferenceEntNum_SP[clientNum] = Scr_GetEntity(0)->s.number;
}

// Added 2026-08-28. Every SP aitype/<name>.gsc calls this from spawner(), and
// GScr_LoadScriptsAndAnimsForEntities now compiles those files, so an unregistered name is a
// compile error that kills the boot. This is an ENTITY method, not an actor one -- retail keeps
// it outside the 0x00A52058 actor table, at table slot 0x00a54a64.
//
// Retail 0x00803af0 read in full, recorded so a later pass need not re-derive it:
//     ent = <entity from entref>;                            // "not an entity"
//     if (ent-><short +0xbe> != 0x11)
//         Scr_Error("setspawnerteam can only be applied to AI spawners");
//     s = Scr_GetString(0);
//     "axis" -> ent-><dword +0x214> = 1;  "allies" -> 2;  "neutral" -> 3;
//     else Scr_ParamError(0, va("unknown team '%s', should be axis, allies, or neutral", s));
// The two retail offsets are now mapped semantically: s.eType is independently
static void __cdecl GScr_SPStubMeth_setspawnerteam(scr_entref_t entref)
{
    gentity_s *spawner = GetEntity(entref);
    if ( spawner->s.eType != ET_ACTOR_SPAWNER )
    {
        Scr_Error("setspawnerteam can only be applied to AI spawners", SCRIPTINSTANCE_SERVER);
        return;
    }

    const char *team = Scr_GetString(0, SCRIPTINSTANCE_SERVER);
    if ( !I_stricmp(team, "axis") )
        spawner->spawner.team = TEAM_AXIS;
    else if ( !I_stricmp(team, "allies") )
        spawner->spawner.team = TEAM_ALLIES;
    else if ( !I_stricmp(team, "neutral") )
        spawner->spawner.team = TEAM_SPECTATOR;
    else
        Scr_ParamError(
            0,
            va("unknown team '%s', should be axis, allies, or neutral", team),
            SCRIPTINSTANCE_SERVER);
}
static void __cdecl GScr_ToggleIK_SP(scr_entref_t entref)
{
    gentity_s *ent = 0;
    if ( entref.classnum )
        Scr_ObjectError("not an entity", SCRIPTINSTANCE_SERVER);
    else
        ent = &g_entities[entref.entnum];

    if ( Scr_GetInt(0, SCRIPTINSTANCE_SERVER) )
        ent->flags &= ~0x10000000;
    else
        ent->flags |= 0x10000000;
    Scr_AddInt(1, SCRIPTINSTANCE_SERVER);
}

#endif // KISAK_SP

BuiltinMethodDef methods_3[] =
{
  { "attach", ScrCmd_attach, 0 },
  { "detach", ScrCmd_detach, 0 },
  { "detachall", ScrCmd_detachAll, 0 },
  { "getattachsize", ScrCmd_GetAttachSize, 0 },
  { "getattachmodelname", ScrCmd_GetAttachModelName, 0 },
  { "getattachtagname", ScrCmd_GetAttachTagName, 0 },
  { "getattachignorecollision", ScrCmd_GetAttachIgnoreCollision, 0 },
  { "getammocount", GScr_GetAmmoCount, 0 },
  { "getclanid", ScrCmd_GetClanId, 0 },
  { "getclanname", ScrCmd_GetClanName, 0 },
  { "hidepart", ScrCmd_hidepart, 0 },
  { "showpart", ScrCmd_showpart, 0 },
  { "showallparts", ScrCmd_showallparts, 0 },
  { "setvisibletoplayer", ScrCmd_SetVisibleToPlayer, 0 },
  { "setinvisibletoplayer", ScrCmd_SetInvisibleToPlayer, 0 },
  { "setvisibletoall", ScrCmd_SetVisibleToAll, 0 },
  { "setinvisibletoall", ScrCmd_SetInvisibleToAll, 0 },
  { "setvisibletoteam", ScrCmd_SetVisibleToTeam, 0 },
  { "setforcenocull", ScrCmd_SetForceNoCull, 0 },
  // LWSS ADD FROM LATEST BLOPS RETAIL MP
  { "overridelightingorigin", ScrCmd_OverrideLightingOrigin, 0 },
  // LWSS END
  { "islinkedto", ScrCmd_IsLinkedTo, 0 },
  { "linkto", ScrCmd_LinkTo, 0 },
  { "playerlinktodelta", ScrCmd_PlayerLinkToDelta, 0 },
  { "unlink", ScrCmd_Unlink, 0 },
  { "enablelinkto", ScrCmd_EnableLinkTo, 0 },
  { "getorigin", ScrCmd_GetOrigin, 0 },
  { "getangles", ScrCmd_GetAngles, 0 },
  { "getmins", ScrCmd_GetMins, 0 },
  { "getmaxs", ScrCmd_GetMaxs, 0 },
  { "getabsmins", ScrCmd_GetAbsMins, 0 },
  { "getabsmaxs", ScrCmd_GetAbsMaxs, 0 },
  { "getpointinbounds", ScrCmd_GetPointInBounds, 0 },
  { "geteye", ScrCmd_GetEye, 0 },
  { "geteyeapprox", ScrCmd_GetEyeApprox, 0 },
  { "useby", ScrCmd_UseBy, 0 },
  { "setstablemissile", Scr_SetStableMissile, 0 },
  { "istouching", ScrCmd_IsTouching, 0 },
  { "istouchingswept", ScrCmd_IsTouchingSwept, 0 },
  // LWSS ADD FROM LATEST BLOPS RETAIL MP
  { "istouchingvolume", ScrCmd_IsTouchingVolume, 0 },
  // LWSS END
  { "playsound", ScrCmd_PlaySound, 0 },
  { "playsoundontag", ScrCmd_PlaySoundOnTag, 0 },
  { "playsoundasmaster", ScrCmd_PlaySound, 0 },
  { "playsoundtoteam", ScrCmd_PlaySoundToTeam, 0 },
  { "playbattlechattertoteam", ScrCmd_PlayBattleChatterToTeam, 0 },
  { "playsoundtoplayer", ScrCmd_PlaySoundToPlayer, 0 },
#ifdef KISAK_SP
  // Present in retail SP's method table (0x00A54218) but not in any of this
  // project's seven method tables. Index = slot index in that SP table.
  { "stopsound", ScrCmd_StopSound, 0 },      // SP methods_3 idx 56, handler 0x00807B30
  // SP idx 167 points at 0x00651A30, which is a single 0xC3 (RET) byte followed
  // by 0xCC padding -- the COMDAT-folded empty stub this table reuses for every
  // source-empty method. Verified by reading the bytes at that address directly,
  // not inherited from the (unrelated) symbol Ghidra has parked on it.
  { "setsoundblend", METHOD_NULLSUB, 0 },    // SP methods_3 idx 167, handler 0x00651A30
  { "forcetexturestoload", METHOD_NULLSUB, 0 },
  // The player-method table at 0x00A54E30 and its mirrored descriptor at
  // 0x00B84B98 both pair this exact name with 0x00651A30. That handler is a
  // single RET, so retail deliberately ignores the picture in this build.
  { "givegamerpicture", METHOD_NULLSUB, 0 }, // SP player method, handler 0x00651A30
  { "playweapondeatheffects", GScr_SPMethod_playweapondeatheffects, 0 }, // SP method record 0x00A54D04, handler 0x00808210
  { "setdeathcontents", GScr_SPMethod_setdeathcontents, 0 }, // SP method record 0x00A545C0, handler 0x007F54E0
  { "haseyes", GScr_SPMethod_haseyes, 0 }, // SP method record 0x00A54E78, handler 0x008062E0
  { "bypasssledgehammer", GScr_BypassSledgehammer_SP, 0 },
  { "enablesledgehammer", GScr_EnableSledgehammer_SP, 0 },
  { "setphysparams", GScr_SPMethod_setphysparams, 0 }, // SP methods_3 record 0x00A54F68, handler 0x00806BF0
  { "settransported", GScr_SPMethod_settransported, 0 }, // SP methods_3 record 0x00A54CE0, handler 0x007FA6A0
#endif
  { "playloopsound", ScrCmd_PlayLoopSound, 0 },
  { "stoploopsound", ScrCmd_StopLoopSound, 0 },
  { "playrumbleonentity", METHOD_NULLSUB, 0 },
  { "playrumblelooponentity", METHOD_NULLSUB, 0 },
  { "stoprumble", METHOD_NULLSUB, 0 },
  { "delete", ScrCmd_Delete, 0 },
  { "setmodel", ScrCmd_SetModel, 0 },
  { "setenemymodel", ScrCmd_SetEnemyModel, 0 },
  { "dodamage", ScrCmd_DoDamage, 0 },
  { "getnormalhealth", ScrCmd_GetNormalHealth, 0 },
  { "setnormalhealth", ScrCmd_SetNormalHealth, 0 },
  { "show", ScrCmd_Show, 0 },
  { "hide", ScrCmd_Hide, 0 },
  { "ghost", ScrCmd_Ghost, 0 },
  { "laseron", METHOD_NULLSUB, 0 },
  { "laseroff", METHOD_NULLSUB, 0 },
  { "showtoplayer", ScrCmd_ShowToPlayer, 0 },
  { "setcontents", ScrCmd_SetContents, 0 },
  { "startfiring", GScr_StartFiring, 0 },
  { "stopfiring", GScr_StopFiring, 0 },
  { "shootturret", GScr_ShootTurret, 0 },
  { "stopshootturret", GScr_StopShootTurret, 0 },
#ifdef KISAK_SP
  { "stopusingturret", GScr_StopUsingTurret_SP, 0 },
  { "turretfiredisable", GScr_TurretFireDisable_SP, 0 },
  { "turretfireenable", GScr_TurretFireEnable_SP, 0 },
#endif
  { "setmode", GScr_SetMode, 0 },
  { "getturretowner", GScr_GetTurretOwner, 0 },
  { "settargetentity", GScr_SetTargetEntity, 0 },
  { "setplayerspread", GScr_SetPlayerSpread, 0 },
  { "setaispread", GScr_SetAiSpread, 0 },
  { "setconvergencetime", GScr_SetConvergenceTime, 0 },
  { "setsuppressiontime", GScr_SetSuppressionTime, 0 },
  { "cleartargetentity", GScr_ClearTargetEntity, 0 },
  { "setturretteam", GScr_SetTurretTeam, 0 },
  { "maketurretusable", GScr_MakeTurretUsable, 0 },
  { "maketurretunusable", GScr_MakeTurretUnusable, 0 },
  { "setturretaccuracy", GScr_SetTurretAccuracy, 0 },
  { "setturretignoregoals", GScr_SetTurretIgnoreGoals, 0 },
  { "getturrettarget", GScr_GetTurretTarget, 0 },
  { "disconnectpaths", GScr_DisconnectPaths, 0 },
  { "connectpaths", GScr_ConnectPaths, 0 },
  { "getstance", ScrCmd_GetStance, 0 },
  // LWSS ADD FROM LATEST BLOPS RETAIL MP
  { "setstance", ScrCmd_SetStance, 0 },
  // LWSS END
  { "setcursorhint", GScr_SetCursorHint, 0 },
  { "setrevivehintstring", GScr_SetReviveHintString, 0 },
  { "sethintstring", GScr_SetHintString, 0 },
  { "sethintstringforperk", GScr_SetHintStringForPerk, 0 },
  { "sethintlowpriority", GScr_SetHintLowPriority, 0 },
  { "usetriggerrequirelookat", GScr_UseTriggerRequireLookAt, 0 },
  { "shellshock", GScr_ShellShock, 0 },
  { "gettagorigin", GScr_GetTagOrigin, 0 },
  { "gettagangles", GScr_GetTagAngles, 0 },
  { "getentnum", GScr_GetEntnum, 1 },
  { "stopshellshock", GScr_StopShellShock, 0 },
  { "setdepthoffield", GScr_SetDepthOfField, 0 },
#ifdef KISAK_SP
  { "getdepthoffield_nearstart", GScr_GetDepthOfField_NearStart, 0 },
  { "getdepthoffield_nearend", GScr_GetDepthOfField_NearEnd, 0 },
  { "getdepthoffield_farstart", GScr_GetDepthOfField_FarStart, 0 },
  { "getdepthoffield_farend", GScr_GetDepthOfField_FarEnd, 0 },
  { "getdepthoffield_nearblur", GScr_GetDepthOfField_NearBlur, 0 },
  { "getdepthoffield_farblur", GScr_GetDepthOfField_FarBlur, 0 },
#endif
  { "setburn", GScr_SetBurn, 0 },
  { "setelectrified", GScr_SetElectrified, 0 },
  { "spawnnapalmgroundflame", GScr_SpawnNapalmGroundFlame, 0 },
  { "needsrevive", GScr_NeedsRevive, 0 },
  { "isinsecondchance", GScr_IsInSecondChance, 0 },
  { "depthinwater", GScr_DepthInWater, 0 },
  { "shootup", GScr_ShootUp, 0 },
  { "depthofplayerinwater", GScr_DepthOfPlayerInWater, 0 },
  { "starttanning", GScr_StartTanning, 0 },
  { "stopburning", GScr_StopBurning, 0 },
  // LWSS ADD FROM LATEST BLOPS RETAIL MP
  { "setwaterdrops", GScr_SetWaterDrops, 0 },
  // LWSS END
  { "restoredefaultdroppitch", GScr_RestoreDefaultDropPitch, 0 },
  { "clearcenterpopups", GScr_clearCenterPopups, 0 },
  { "clearpopups", GScr_clearPopups, 0 },
  { "displaymedal", GScr_DisplayMedal, 0 },
  { "displaygamemodemessage", GScr_DisplayGameModeMessage, 0 },
  { "displayteammessage", GScr_DisplayTeamMessage, 0 },
  { "displaycontract", GScr_DisplayContract, 0 },
  { "displaychallengecomplete", GScr_DisplayChallengeComplete, 0 },
  { "clearendgame", GScr_ClearEndGameComplete, 0 },
  { "displayendgame", GScr_DisplayEndGame, 0 },
  { "displayendgamemilestone", GScr_DisplayEndGameMilestoneComplete, 0 },
  { "displaykillstreak", GScr_DisplayKillstreak, 0 },
  { "displayrankup", GScr_DisplayRankUp, 0 },
  { "displaywagerpopup", GScr_DisplayWagerPopup, 0 },
  { "displayhudanim", GScr_DisplayHudAnim, 0 },
  { "isfiringturret", GScr_IsFiringTurret, 0 },
  { "isturretlockedon", GScr_IsTurretLockedOn, 0 },
  { "setviewmodeldepthoffield", GScr_SetViewModelDepthOfField, 0 },
  { "viewkick", GScr_ViewKick, 0 },
  { "localtoworldcoords", GScr_LocalToWorldCoords, 0 },
  { "setrightarc", GScr_SetRightArc, 0 },
  { "setleftarc", GScr_SetLeftArc, 0 },
  { "settoparc", GScr_SetTopArc, 0 },
  { "setbottomarc", GScr_SetBottomArc, 0 },
  { "radiusdamage", GScr_EntityRadiusDamage, 0 },
  { "detonate", GScr_Detonate, 0 },
  { "damageconetrace", GScr_DamageConeTrace, 0 },
  { "sightconetrace", GScr_SightConeTrace, 0 },
  { "heliturretsighttrace", GScr_HeliTurretSightTrace, 0 },
  { "heliturretdogtrace", GScr_HeliTurretDogTrace, 0 },
  { "playersighttrace", GScr_PlayerSightTrace, 0 },
  { "visionsetlerpratio", GScr_VisionSetLerpRatio, 0 },
  { "docowardswayanims", GScr_DoCowardsWayAnims, 0 },
  { "startpoisoning", GScr_StartPoisoning, 0 },
  { "stoppoisoning", GScr_StopPoisoning, 0 },
  { "startbinocs", GScr_StartBinocs, 0 },
  { "stopbinocs", GScr_StopBinocs, 0 },
  { "isflared", GScr_IsFlared, 0 },
  { "ispoisoned", GScr_IsPoisoned, 0 },
  { "sightconetrace", GScr_SightConeTrace, 0 },
  { "setcameraspikeactive", GScr_SetCameraSpikeActive, 0 },
  { "ismissileinsideheightlock", GScr_IsMissileInsideHeightLock, 0 },
  { "isonground", GScr_IsOnGround, 0 },
  // LWSS ADD FROM LATEST BLOPS RETAIL MP
  { "getgroundent", GScr_GetGroundEnt, 0 },
  // LWSS END
  { "setanim", GScr_SetAnim, 0 },          // SP methods_3 idx 114, handler 0x00801640
#ifdef KISAK_SP
  // Retail SP's anim block, methods_3 indices 103..135, listed in SP table
  // order. This is a subset of that range, not the whole of it -- the index
  // gaps below are SP entries deliberately left out of this pass, not entries
  // simply out of scope here: idx 104 "clearanimlimited" (0x00800770),
  // idx 110 "setanimknoballlimited" (0x00801190),
  // idx 112 "setanimknoballlimitedrestart" (0x008011D0),
  // idx 117 "setanimlimitedrestart" (0x008016A0),
  // idx 130 "setflaggedanimlimitedrestart" (0x00802700).
  // The remaining skipped slots in the range (113, 119, 120, 122, 131..134)
  // were not identified and no claim is made about them.
  { "clearanim", GScr_ClearAnim, 0 },                                             // SP idx 103, 0x00800660
  { "setanimknob", GScr_SetAnimKnob, 0 },                                         // SP idx 105, 0x00800CC0
  { "setanimknoblimited", GScr_SetAnimKnobLimited, 0 },                           // SP idx 106, 0x00800CE0
  { "setanimknobrestart", GScr_SetAnimKnobRestart, 0 },                           // SP idx 107, 0x00800D00
  { "setanimknoblimitedrestart", GScr_SetAnimKnobLimitedRestart, 0 },
  { "setanimknoball", GScr_SetAnimKnobAll, 0 },                                   // SP idx 109, 0x00801170
  { "setanimknoballrestart", GScr_SetAnimKnobAllRestart, 0 },                     // SP idx 111, 0x008011B0
  { "setanimlimited", GScr_SetAnimLimited, 0 },                                   // SP idx 115, 0x00801660
  { "setanimrestart", GScr_SetAnimRestart, 0 },                                   // SP idx 116, 0x00801680
  { "getanimtime", GScr_GetAnimTime, 0 },                                         // SP idx 118, 0x008016C0
  { "setflaggedanimknob", GScr_SetFlaggedAnimKnob, 0 },                           // SP idx 121, 0x00801CF0
  { "setflaggedanimknobrestart", GScr_SetFlaggedAnimKnobRestart, 0 },             // SP idx 123, 0x00801D30
  { "setflaggedanimknoblimitedrestart", GScr_SetFlaggedAnimKnobLimitedRestart, 0 },// SP idx 124, 0x00801D50
  { "setflaggedanimknoball", GScr_SetFlaggedAnimKnobAll, 0 },                     // SP idx 125, 0x008021C0
  { "setflaggedanimknoballrestart", GScr_SetFlaggedAnimKnobAllRestart, 0 },       // SP idx 126, 0x008021F0
  { "setflaggedanim", GScr_SetFlaggedAnim, 0 },                                   // SP idx 127, 0x008026A0
  { "setflaggedanimlimited", GScr_SetFlaggedAnimLimited, 0 },                     // SP idx 128, 0x008026C0
  { "setflaggedanimrestart", GScr_SetFlaggedAnimRestart, 0 },                     // SP idx 129, 0x008026E0
  { "setanimtime", GScr_SetAnimTime, 0 },                                         // SP idx 135, 0x008027D0
  { "setanimratecomplete", GScr_SetAnimRateComplete_SP, 0 },
#endif
  { "useanimtree", GScr_UseAnimTree, 0 },
#ifdef KISAK_SP
  { "stopuseanimtree", GScr_StopUseAnimTree_SP, 0 },
#endif
  { "ismartyrdomgrenade", GScr_IsMartyrdomGrenade, 0 },
  { "getentitynumber", GScr_GetEntityNumber, 0 },
  { "enablegrenadetouchdamage", GScr_EnableGrenadeTouchDamage, 0 },
  { "disablegrenadetouchdamage", GScr_DisableGrenadeTouchDamage, 0 },
  { "enableaimassist", GScr_EnableAimAssist, 0 },
  { "disableaimassist", GScr_DisableAimAssist, 0 },
  { "placespawnpoint", GScr_PlaceSpawnPoint, 0 },
  { "setspawnclientflag", METHOD_NULLSUB, 0 },
  { "directionalhitindicator", GScr_DirectionalHitIndicator, 0 },
  { "sendfaceevent", ScrCmd_SendFaceEvent, 0 },
  { "setteamfortrigger", GScr_SetTeamForTrigger, 0 },
  { "setperkfortrigger", GScr_SetPerkForTrigger, 0 },
  { "setignoreentfortrigger", GScr_SetIgnoreEntForTrigger, 0 },
  { "clientclaimtrigger", GScr_ClientClaimTrigger, 0 },
  { "clientreleasetrigger", GScr_ClientReleaseTrigger, 0 },
  { "releaseclaimedtrigger", GScr_ReleaseClaimedTrigger, 0 },
  { "isitemlocked", GScr_IsItemLocked, 0 },
  { "isitempurchased", GScr_IsItemPurchased, 0 },
  { "getdstat", GScr_GetDStat, 0 },
  { "setdstat", GScr_SetDStat, 0 },
  { "sendleaderboards", GScr_SendLeaderboards, 0 },
  { "setnemesisxuid", GScr_SetNemesisXuid, 0 },
  { "getloadoutitemfromprofile", GScr_GetLoadoutItemFromProfile, 0 },
  { "setmovespeedscale", ScrCmd_SetMoveSpeedScale, 0 },
  { "getmovespeedscale", ScrCmd_GetMoveSpeedScale, 0 },
  { "logstring", METHOD_NULLSUB, 0 },
  { "missile_settarget", GScr_MissileSetTarget, 0 },
  { "isonladder", GScr_IsOnLadder, 0 },
  { "ismantling", GScr_IsMantling, 0 },
  { "startdoorbreach", GScr_StartDoorBreach, 0 },
  { "stopdoorbreach", GScr_StopDoorBreach, 0 },
  { "startragdoll", GScr_StartRagdoll, 0 },
  { "isragdoll", GScr_IsRagdoll, 0 },
  { "launchragdoll", GScr_RagdollLaunch, 0 },
  { "launchvehicle", GScr_VehicleLaunch, 0 },
  { "giveachievement", GScr_GiveAchievement, 0 },
  { "setvehicleteam", GScr_SetTeam, 0 },
  { "setteam", GScr_SetTeam, 0 },
  { "getteam", GScr_GetTeam, 0 },
  { "setowner", GScr_SetOwner, 0 },
  { "setturretowner", GScr_SetTurretOwner, 0 },
  { "setturrettype", GScr_SetTurretType, 0 },
  { "getcorpseanim", GScr_GetCorpseAnim, 0 },
  { "itemweaponsetammo", ScrCmd_ItemWeaponSetAmmo, 0 },
  { "getlightcolor", GScr_GetLightColor, 0 },
  { "setlightcolor", GScr_SetLightColor, 0 },
  { "getlightintensity", GScr_GetLightIntensity, 0 },
  { "setlightintensity", GScr_SetLightIntensity, 0 },
  { "getlightradius", GScr_GetLightRadius, 0 },
  { "setlightradius", GScr_SetLightRadius, 0 },
  { "getlightfovinner", GScr_GetLightFovInner, 0 },
  { "getlightfovouter", GScr_GetLightFovOuter, 0 },
  { "setlightfovrange", GScr_SetLightFovRange, 0 },
  { "getlightexponent", GScr_GetLightExponent, 0 },
  { "setlightexponent", GScr_SetLightExponent, 0 },
  { "setturretcarried", GScr_SetTurretCarried, 0 },
  { "reportuser", METHOD_NULLSUB, 0 },
  { "getvelocity", ScrCmd_GetVelocity, 0 },
  { "spawnactor", ScrCmd_SpawnActor, 0 },
  { "getshootatpos", ScrCmd_GetShootAtPosition, 0 },
  { "setdefaultdroppitch", GScr_SetDefaultDropPitch, 0 },
  { "setscanningpitch", GScr_SetScanningPitch, 0 },
  { "launchbomb", GScr_LaunchBomb, 0 },
  { "launch", GScr_Launch, 0 },
  { "makegrenadedud", GScr_MakeGrenadeDud, 0 },
  { "setclientflag", GScr_SetClientFlag, 0 },
  // LWSS ADD FROM LATEST BLOPS RETAIL MP
  { "getclientflag", GScr_GetClientFlag, 0 },
  // LWSS END
  { "clearclientflag", GScr_ClearClientFlag, 0 },
  { "fakefire", GScr_FakeFire, 0 },
  { "makeusable", ScrCmd_MakeUsable, 0 },
  { "makeunusable", ScrCmd_MakeUnusable, 0 },
  { "predictgrenade", GScr_PredictGrenade, 0 },
  { "getindexforactivecontract", GScr_GetIndexForActiveContract, 0 },
  { "getactivecontractprogress", GScr_GetActiveContractProgress, 0 },
  {
    "incrementactivecontractprogress",
    GScr_IncrementActiveContractProgress,
    0
  },
  { "incrementactivecontracttime", GScr_IncrementActiveContractTime, 0 },
  { "isactivecontractcomplete", GScr_IsActiveContractComplete, 0 },
  { "hasactivecontractexpired", GScr_HasActiveContractExpired, 0 },
  { "getactivecontracttimepassed", GScr_GetActiveContractTimePassed, 0 },
  { "resetactivecontractprogress", GScr_ResetActiveContractProgress, 0 },
  { "getpregameclass", GScr_GetPregameClass, 0 },
  { "getpregameteam", GScr_GetPregameTeam, 0 },
  { "setpregameclass", GScr_SetPregameClass, 0 },
  { "setpregameteam", GScr_SetPregameTeam, 0 },
  { "isdemoclient", GScr_isDemoClient, 0 },
  { "istestclient", GScr_isTestClient, 0 }
#ifdef KISAK_SP
  ,
  // TODO(SP-STUB): retail-SP builtin METHODS referenced by the SP script
  // corpus and absent from every table in the Scr_GetMethod chain. Each
  // handler below is a no-op stub that warns once and evaluates to undefined
  // -- NOT an implementation. See the TODO(SP-STUB) header above
  // BuiltinFunctionDef functions[]. Verified before adding: none of these
  // names was already present in methods_3[], nor in any of the six tables
  // Scr_GetMethod consults first (Player_ / ScriptEnt_ / ScriptVehicle_ /
  // HudElem_ / Helicopter_ / Actor_GetMethod). Trailing comment on each row
  // is the retail SP handler address for that name.
  { "setclientflagasval", GScr_SetClientFlagAsVal_SP, 0 },                          // IMPLEMENTED from SP 0x008064e0 (twin of GScr_SetClientFlag 0x00806400)
  { "hasusedweapon", GScr_HasUsedWeapon_SP, 0 },
  { "getplayerconnectionstate", GScr_GetPlayerConnectionState_SP, 0 },
  { "gib", GScr_SPMethod_gib, 0 },                                                  // IMPLEMENTED from SP methods_3 entry 289, handler 0x00806eb0
  { "resetmissiledetonationtime", GScr_SPMethod_resetmissiledetonationtime, 0 },    // IMPLEMENTED from SP methods_3 entry 256, handler 0x007fca60
  { "getcentroid", GScr_SPMethod_getcentroid, 0 },                                  // IMPLEMENTED from SP methods_3 entry 41, handler 0x007f35d0
  { "getweaponforwarddir", GScr_SPMethod_getweaponforwarddir, 0 },                  // IMPLEMENTED for the retail player path from SP methods_3 entry 149, handler 0x00510f50
  { "getweaponmuzzlepoint", GScr_SPMethod_getweaponmuzzlepoint, 0 },                // IMPLEMENTED for the retail player path from SP methods_3 entry 148, handler 0x005ba610
  { "isnotarget", GScr_SPMethod_isnotarget, 0 },                                    // IMPLEMENTED from SP name-pointer table entry 0x00a55e70, handler 0x0067b6a0
  { "setteamforentity", GScr_SPMethod_setteamforentity, 0 },                        // IMPLEMENTED from SP name-pointer table entry 0x00a54b9c, handler 0x008042d0
  { "disableweaponfire", PlayerCmd_DisableWeaponFire, 0 },
  { "enableweaponfire", PlayerCmd_EnableWeaponFire, 0 },
  { "disableweaponreload", PlayerCmd_DisableWeaponReload, 0 },
  { "enableweaponreload", PlayerCmd_EnableWeaponReload, 0 },
  { "issprinting", PlayerCmd_IsSprinting, 0 },
  { "setlowready", PlayerCmd_SetLowReady, 0 },
  { "setplayergravity", PlayerCmd_SetPlayerGravity, 0 },
  { "clearplayergravity", PlayerCmd_ClearPlayerGravity, 0 },
  { "isweaponoverheating", GScr_IsWeaponOverheating_SP, 0 },
  { "isplayerreloading", GScr_IsPlayerReloading_SP, 0 },
  { "worldtolocalcoords", GScr_WorldToLocalCoords_SP, 0 },
  { "setentgravitytrajectory", GScr_SetEntGravityTrajectory_SP, 0 },
  { "setloweredweapon", PlayerCmd_SetLoweredWeapon, 0 },
  { "setzombieshrink", GScr_SetZombieShrink_SP, 0 },
  { "setwatersheeting", GScr_SetWaterSheeting_SP, 0 },
  { "playersetgroundreferenceent", GScr_SPMethod_playersetgroundreferenceent, 0 },  // IMPLEMENTED from SP methods_3 entry 23, handler 0x007f27c0
  { "itemweaponsetoptions", GScr_ItemWeaponSetOptions_SP, 0 },
  { "animscripted", GScr_AnimScripted_SP, 0 },                                      // IMPLEMENTED (script_model path) from SP 0x00808690
  { "animscriptedskiprestart", GScr_AnimScriptedSkipRestart_SP, 0 },
  { "stopanimscripted", GScr_StopAnimScripted_SP, 0 },                              // IMPLEMENTED from SP 0x004ce100
  { "playerlinktoabsolute", GScr_PlayerLinkToAbsolute_SP, 0 },                      // IMPLEMENTED from SP 0x007f2f80
  { "playerlinkto", GScr_SPMethod_PlayerLinkTo, 0 },
  { "playeranimscriptevent", GScr_PlayerAnimScriptEvent_SP, 0 },
  { "lookatentity", GScr_LookAtEntity_SP, 0 },
  { "setspawnerteam", GScr_SPStubMeth_setspawnerteam, 0 },                          // IMPLEMENTED from SP 0x00803af0 (entity method, retail slot 0x00a54a64)
  { "setblur", GScr_SetBlur_SP, 0 },
  { "getvisionsetnaked", GScr_GetVisionSetNaked_SP, 0 },
  { "bloodimpact", GScr_BloodImpact_SP, 0 },
  { "setplayercollision", GScr_SetPlayerCollision_SP, 0 },
  { "sethudwarningtype", GScr_SetHUDWarningType_SP, 0 },
  { "setzombiename", GScr_SetZombieName_SP, 0 },
  { "stopsounds", GScr_SPMethod_stopsounds, 0 },                                    // IMPLEMENTED from SP methods_3 entry 0x00A544C4, handler 0x007f4a50
  { "useweaponhidetags", GScr_UseWeaponHideTags_SP, 0 },
  { "addaieventlistener", GScr_SPMethod_AddAIEventListener, 0 },                   // IMPLEMENTED from SP 0x008053c0 -> 0x00504260
  { "dontinterpolate", GScr_SPMethod_DontInterpolate, 0 },
  { "magicgrenade", GScr_SPMethod_MagicGrenade, 0 },
  { "magicgrenademanual", GScr_SPMethod_MagicGrenadeManual, 0 },
  { "magicgrenadetype", GScr_SPMethod_MagicGrenadeType, 0 },
  { "makefakeai", GScr_MakeFakeAI_SP, 0 },
  { "visionsetlaststand", GScr_VisionSetLastStandPlayer_SP, 0 },
  { "visionsetnaked", GScr_SPMeth_visionsetnaked, 0 },
  { "visionsetnight", GScr_VisionSetNightPlayer_SP, 0 },
  { "codespawnerforcespawn", GScr_CodeSpawnerForceSpawn_SP, 0 },                    // IMPLEMENTED from SP handler 0x007f33c0
  { "codespawnerspawn", GScr_CodeSpawnerSpawn_SP, 0 },                              // IMPLEMENTED from SP handler 0x007f3250
  { "getaivelocity", GScr_SPMethod_GetAiVelocity, 0 },
  { "startfadingblur", GScr_StartFadingBlur_SP, 0 },
  { "getdestructiblename", GScr_SPStubMeth_getdestructiblename, 0 },                // TODO(SP-STUB) SP 0x007f1db0
  { "setexploderid", GScr_SetExploderId_SP, 0 },
  { "setlookattext", G_SPSetLookAtText, 0 },                                       // Retail SP 0x0048CD50
  { "setmaxhealth", GScr_SetMaxHealth_SP, 0 },
  { "setvolfog", GScr_SetVolFog_SP, 0 },
  { "getdebugeye", GScr_SPStubMeth_getdebugeye, 0 },                                // TODO(SP-STUB) SP 0x007f4310
  { "floatlonger", GScr_FloatLonger_SP, 0 },
  { "scalebuoyancy", GScr_ScaleBuoyancy_SP, 0 },
  { "setlocalwindsource", GScr_SetLocalWindSource_SP, 0 },
  { "disableclientlinkto", GScr_DisableClientLinkTo_SP, 0 },
  { "enableclientlinkto", GScr_EnableClientLinkTo_SP, 0 },
  { "getlinkedent", GScr_GetLinkedEnt_SP, 0 },                                      // IMPLEMENTED from SP 0x007f2640 (returns ent->tagInfo->parent)
  { "haspath", GScr_HasPath_SP, 0 },
  { "iswaitingonsound", GScr_SPStubMeth_iswaitingonsound, 0 },                      // TODO(SP-STUB) SP 0x007f4aa0
  { "setdoublevision", GScr_SetDoubleVision_SP, 0 },
  { "toggleik", GScr_ToggleIK_SP, 0 },
  { "setshadowhint", GScr_SPStubMeth_setshadowhint, 0 },                            // TODO(SP-STUB) SP 0x007f4c40
  { "setvehicleattachments", GScr_SetVehicleAttachments_SP, 0 },
  { "transmittargetname", GScr_TransmitTargetName_SP, 0 },
  { "setthreatbiasgroup", GScr_SetThreatBiasGroup_SP, 0 },                // IMPLEMENTED from SP 0x008195A0
  { "getthreatbiasgroup", GScr_GetThreatBiasGroup_SP, 0 },                // IMPLEMENTED from SP 0x00819630
  { "getclosestenemysqdist", GScr_GetClosestEnemySqDist_SP, 0 },          // IMPLEMENTED from SP 0x00511750
#endif // KISAK_SP
};



void (__cdecl *__cdecl BuiltIn_GetMethod(const char **pName, int *type))(scr_entref_t)
{
    unsigned int i; // [esp+18h] [ebp-4h]

    for ( i = 0; i < ARRAY_COUNT(methods_3); ++i )
    {
        if ( !strcmp(*pName, methods_3[i].actionString) )
        {
            *pName = methods_3[i].actionString;
            *type = methods_3[i].type;
            return methods_3[i].actionFunc;
        }
    }
    return 0;
}

void __cdecl Scr_SetOrigin(gentity_s *ent, int)
{
    float org[3]; // [esp+0h] [ebp-Ch] BYREF
    int savedregs; // [esp+Ch] [ebp+0h] BYREF

    Scr_GetVector(0, org, SCRIPTINSTANCE_SERVER);
    G_SetOrigin(ent, org);
    if ( ent->r.linked )
        SV_LinkEntity(ent);
}

void __cdecl Scr_SetAngles(gentity_s *ent, int)
{
    float angles[3]; // [esp+0h] [ebp-Ch] BYREF

    Scr_GetVector(0, angles, SCRIPTINSTANCE_SERVER);
    G_SetAngle(ent, angles);
}

void __cdecl Scr_SetExposureIndex(gentity_s *ent, int)
{
    ent->item[1].index = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_SetExposureLerpToLighter(gentity_s *ent, int)
{
    ent->trigger.exposureLerpToLighter = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_SetExposureLerpToDarker(gentity_s *ent, int)
{
    ent->trigger.exposureLerpToDarker = Scr_GetFloat(0, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_SetHealth(gentity_s *ent, int)
{
#ifdef KISAK_SP
    const int health = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if (health <= 0)
    {
        const char *name = ent->targetname
            ? SL_ConvertToString(ent->targetname, SCRIPTINSTANCE_SERVER) : "<not set>";
        Scr_Error(va("self.health must be greater than 0 (tried to set %i on ent %i, name %s)",
            health, ent->s.number, name), 0);
    }
    if (!ent->client && ent->health <= 0 && ent->maxHealth != 0)
    {
        const char *name = ent->targetname
            ? SL_ConvertToString(ent->targetname, SCRIPTINSTANCE_SERVER) : "<not set>";
        Com_DPrintf(24, "^2Cannot set health on dead entities (health %i, max %i, ent %i, name %s)\n",
            ent->health, ent->maxHealth, ent->s.number, name);
        return;
    }
    ent->maxHealth = health;
    ent->health = health;
#else
    int health; // [esp+0h] [ebp-4h]

    health = Scr_GetInt(0, SCRIPTINSTANCE_SERVER);
    if ( ent->client )
    {
        ent->health = health;
        ent->client->ps.stats[0] = health;
    }
    else
    {
        ent->maxHealth = health;
        ent->health = health;
    }
#endif
}

void __cdecl GScr_AddVector(float *vVec)
{
    if ( vVec )
        Scr_AddVector(vVec, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddUndefined(SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_AddEntity(gentity_s *pEnt)
{
    if ( pEnt )
        Scr_AddEntity(pEnt, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddUndefined(SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_ParseGameTypeList()
{
    if ( useFastFile->current.enabled )
        ((void (__cdecl *)(void (*)()))Scr_ParseGameTypeList_FastFile)(Scr_ParseGameTypeList_FastFile);
    else
        ((void (__cdecl *)(int (*)()))Scr_ParseGameTypeList_LoadObj)(Scr_ParseGameTypeList_LoadObj);
}

int Scr_ParseGameTypeList_LoadObj()
{
    const char *v0; // eax
    const char *v1; // eax
    int result; // eax
    char *qpath; // [esp+10h] [ebp-1430h]
    char *src; // [esp+14h] [ebp-142Ch]
    unsigned __int8 buffer[1024]; // [esp+18h] [ebp-1428h] BYREF
    char *data_p; // [esp+418h] [ebp-1028h] BYREF
    char *s0; // [esp+41Ch] [ebp-1024h]
    char listbuf[4096]; // [esp+420h] [ebp-1020h] BYREF
    int f; // [esp+1424h] [ebp-1Ch] BYREF
    unsigned int v10; // [esp+1428h] [ebp-18h]
    int v11; // [esp+142Ch] [ebp-14h]
    int len; // [esp+1430h] [ebp-10h]
    int i; // [esp+1434h] [ebp-Ch]
    char *dest; // [esp+1438h] [ebp-8h]
    int FileList; // [esp+143Ch] [ebp-4h]

    memset(g_scr_data.gametype.list, 0, sizeof(g_scr_data.gametype.list));
    v11 = 0;
    FileList = FS_GetFileList("maps/mp/gametypes", (char*)"gsc", FS_LIST_PURE_ONLY, listbuf, 4096);
    src = listbuf;
    for ( i = 0; i < FileList; ++i )
    {
        v10 = strlen(src);
        if ( *src == 95 )
        {
            src += v10 + 1;
        }
        else
        {
            if ( !I_stricmp(&src[v10 - 4], ".gsc") )
                src[v10 - 4] = 0;
            if ( v11 == 32 )
            {
                Com_Printf(24, "Too many game type scripts found! Only loading the first %i\n", 31);
                break;
            }
            dest = g_scr_data.gametype.list[v11].pszScript;
            I_strncpyz(dest, src, 64);
            //strlwr(dest);
            _strlwr(dest);
            qpath = va("maps/mp/gametypes/%s.txt", src);
            len = FS_FOpenFileByMode(qpath, &f, FS_READ);
            if ( len > 0 && len < 1024 )
            {
                FS_Read(buffer, len, f);
                data_p = (char *)buffer;
                s0 = (char *)Com_Parse((const char **)&data_p);
                I_strncpyz(dest + 64, s0, 64);
                s0 = (char *)Com_Parse((const char **)&data_p);
                *((unsigned int *)dest + 32) = s0 && !I_stricmp(s0, "team");
            }
            else
            {
                if ( len > 0 )
                {
                    v1 = va("maps/mp/gametypes/%s.txt", src);
                    Com_PrintWarning(24, "WARNING: GameType description file %s is too big to load.\n", v1);
                }
                else
                {
                    v0 = va("maps/mp/gametypes/%s.txt", src);
                    Com_PrintWarning(24, "WARNING: Could not load GameType description file %s for gametype %s\n", v0, src);
                }
                I_strncpyz(dest + 64, dest, 64);
                *((unsigned int *)dest + 32) = 0;
            }
            ++v11;
            if ( len > 0 )
                FS_FCloseFile(f);
            src += v10 + 1;
        }
    }
    result = v11;
    g_scr_data.gametype.iNumGameTypes = v11;
    return result;
}

void Scr_ParseGameTypeList_FastFile()
{
    const char *v0; // eax
    const char *v1; // eax
    int v2; // [esp+0h] [ebp-44h]
    char *fullname; // [esp+1Ch] [ebp-28h]
    RawFile *rawfile; // [esp+20h] [ebp-24h]
    parseInfo_t *pszFileName; // [esp+24h] [ebp-20h]
    const char *pBuffParse; // [esp+28h] [ebp-1Ch] BYREF
    const char *pToken; // [esp+2Ch] [ebp-18h]
    int iNumGameTypes; // [esp+30h] [ebp-14h]
    int iFileLength; // [esp+34h] [ebp-10h]
    RawFile *gametypesFile; // [esp+38h] [ebp-Ch]
    const char *gametypesBuf; // [esp+3Ch] [ebp-8h] BYREF
    gameTypeScript_t *pGameType; // [esp+40h] [ebp-4h]

    memset(g_scr_data.gametype.list, 0, sizeof(g_scr_data.gametype.list));
    iNumGameTypes = 0;
    gametypesFile = DB_FindXAssetHeader(ASSET_TYPE_RAWFILE, (char*)GSCR_GAMETYPE_DIR "_gametypes.txt", 1, -1).rawfile;
    if ( gametypesFile )
    {
        gametypesBuf = gametypesFile->buffer;
        while ( 1 )
        {
            pszFileName = Com_Parse(&gametypesBuf);
            if ( !gametypesBuf )
                break;
            if ( iNumGameTypes == 32 )
            {
                Com_Printf(24, "Too many game type scripts found! Only loading the first %i\n", 31);
                break;
            }
            pGameType = &g_scr_data.gametype.list[iNumGameTypes];
            I_strncpyz(pGameType->pszScript, pszFileName->token, 64);
            I_strlwr(pGameType->pszScript);
            fullname = va(GSCR_GAMETYPE_DIR "%s.txt", pszFileName->token);
            rawfile = DB_FindXAssetHeader(ASSET_TYPE_RAWFILE, fullname, 1, -1).rawfile;
            if ( rawfile )
                v2 = strlen(rawfile->buffer);
            else
                v2 = 0;
            iFileLength = v2;
            if ( v2 > 0 && iFileLength < 1024 )
            {
                if ( !rawfile
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                                19406,
                                0,
                                "%s",
                                "rawfile") )
                {
                    __debugbreak();
                }
                pBuffParse = rawfile->buffer;
                pToken = (const char *)Com_Parse(&pBuffParse);
                I_strncpyz(pGameType->pszName, pToken, 64);
                pToken = (const char *)Com_Parse(&pBuffParse);
                pGameType->bTeamBased = pToken && !I_stricmp(pToken, "team");
            }
            else
            {
                if ( iFileLength > 0 )
                {
                    v1 = va(GSCR_GAMETYPE_DIR "%s.txt", pszFileName->token);
                    Com_PrintWarning(24, "WARNING: GameType description file %s is too big to load.\n", v1);
                }
                else
                {
                    v0 = va(GSCR_GAMETYPE_DIR "%s.txt", pszFileName->token);
                    Com_PrintWarning(
                        24,
                        "WARNING: Could not load GameType description file %s for gametype %s\n",
                        v0,
                        pszFileName->token);
                }
                I_strncpyz(pGameType->pszName, pGameType->pszScript, 64);
                pGameType->bTeamBased = 0;
            }
            ++iNumGameTypes;
        }
    }
    g_scr_data.gametype.iNumGameTypes = iNumGameTypes;
}

char *__cdecl Scr_GetGameTypeNameForScript(const char *pszGameTypeScript)
{
    int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < g_scr_data.gametype.iNumGameTypes; ++i )
    {
        if ( !I_stricmp(g_scr_data.gametype.list[i].pszScript, pszGameTypeScript) )
            return g_scr_data.gametype.list[i].pszName;
    }
    return 0;
}

bool __cdecl Scr_IsValidGameType(const char *pszGameType)
{
    return Scr_GetGameTypeNameForScript(pszGameType) != 0;
}

void __cdecl Scr_LoadGameType()
{
    unsigned __int16 t; // [esp+0h] [ebp-4h]

#ifdef KISAK_SP
    // Consumer half of the bEnforceExists=0 change in GScr_LoadGameTypeScript. SP has no usable
    // campaign gametype script (maps/gametypes/cmp.gsc is an empty 17-byte asset - see the
    // evidence there), so the handle is normally 0 and the assert below would fire on every
    // frontend/campaign spawn. If a zone ever does supply a real gametype script the handle is
    // non-zero and this behaves exactly as MP. Audit finding B3 (frontend-map-load audit).
    if ( !g_scr_data.gametype.main )
        return;
#endif
    if ( !g_scr_data.gametype.main
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game_mp\\g_scr_main_mp.cpp",
                    19467,
                    0,
                    "%s",
                    "g_scr_data.gametype.main") )
    {
        __debugbreak();
    }
    t = Scr_ExecThread(SCRIPTINSTANCE_SERVER, g_scr_data.gametype.main, 0);
    Scr_FreeThread(t, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_StartupGameType()
{
    unsigned __int16 callback; // [esp+0h] [ebp-4h]

    callback = Scr_ExecThread(SCRIPTINSTANCE_SERVER, g_scr_data.gametype.startupgametype, 0);
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_PlayerConnect(gentity_s *self)
{
    unsigned __int16 callback; // [esp+0h] [ebp-4h]

    callback = Scr_ExecEntThread(self, g_scr_data.gametype.playerconnect, 0);
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_PlayerDisconnect(gentity_s *self)
{
    unsigned __int16 callback; // [esp+0h] [ebp-4h]

    callback = Scr_ExecEntThread(self, g_scr_data.gametype.playerdisconnect, 0);
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_PlayerDamage(
                gentity_s *self,
                gentity_s *inflictor,
                gentity_s *attacker,
                int damage,
                int dflags,
                unsigned int meansOfDeath,
                unsigned int iWeapon,
                float *vPoint,
                float *vDir,
                hitLocation_t hitLoc,
#ifdef KISAK_SP
                int modelIndex,
#endif
                int timeOffset)
{
    unsigned __int16 HitLocationString; // ax
    char *v12; // eax
    unsigned __int16 callback; // [esp+0h] [ebp-4h]

    Scr_AddInt(timeOffset, SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
    // Retail Scr_PlayerDamage (0x00437030) pushes modelIndex immediately
    // after timeOffset, yielding script arguments 9 and 10 respectively.
    Scr_AddInt(modelIndex, SCRIPTINSTANCE_SERVER);
#endif
    HitLocationString = G_GetHitLocationString(hitLoc);
    Scr_AddConstString(HitLocationString, SCRIPTINSTANCE_SERVER);
    GScr_AddVector(vDir);
    GScr_AddVector(vPoint);
    v12 = (char *)BG_WeaponName(iWeapon);
    Scr_AddString(v12, SCRIPTINSTANCE_SERVER);
    if ( meansOfDeath <= 0x14 )
        Scr_AddConstString(*modNames[meansOfDeath], SCRIPTINSTANCE_SERVER);
    else
        Scr_AddString("badMOD", SCRIPTINSTANCE_SERVER);
    Scr_AddInt(dflags, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(damage, SCRIPTINSTANCE_SERVER);
    GScr_AddEntity(attacker);
    GScr_AddEntity(inflictor);
#ifdef KISAK_SP
    callback = Scr_ExecEntThread(self, g_scr_data.gametype.playerdamage, 0xBu);
#else
    callback = Scr_ExecEntThread(self, g_scr_data.gametype.playerdamage, 0xAu);
#endif
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_PlayerKilled(
                gentity_s *self,
                gentity_s *inflictor,
                gentity_s *attacker,
                int damage,
                unsigned int meansOfDeath,
                unsigned int iWeapon,
                float *vDir,
                hitLocation_t hitLoc,
                int psTimeOffset,
                int deathAnimDuration)
{
    unsigned __int16 HitLocationString; // ax
    char *v11; // eax
    unsigned __int16 callback; // [esp+0h] [ebp-4h]

    Scr_AddInt(deathAnimDuration, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(psTimeOffset, SCRIPTINSTANCE_SERVER);
    HitLocationString = G_GetHitLocationString(hitLoc);
    Scr_AddConstString(HitLocationString, SCRIPTINSTANCE_SERVER);
    GScr_AddVector(vDir);
    v11 = (char *)BG_WeaponName(iWeapon);
    Scr_AddString(v11, SCRIPTINSTANCE_SERVER);
    if ( meansOfDeath <= 0x14 )
        Scr_AddConstString(*modNames[meansOfDeath], SCRIPTINSTANCE_SERVER);
    else
        Scr_AddString("badMOD", SCRIPTINSTANCE_SERVER);
    Scr_AddInt(damage, SCRIPTINSTANCE_SERVER);
    GScr_AddEntity(attacker);
    GScr_AddEntity(inflictor);
    callback = Scr_ExecEntThread(self, g_scr_data.gametype.playerkilled, 9u);
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_ActorDamage(
                gentity_s *self,
                gentity_s *inflictor,
                gentity_s *attacker,
                int damage,
                int dflags,
                unsigned int meansOfDeath,
                unsigned int iWeapon,
                float *vPoint,
                float *vDir,
                hitLocation_t hitLoc,
#ifdef KISAK_SP
                int modelIndex,
#endif
                int timeOffset)
{
    unsigned __int16 HitLocationString; // ax
    char *v12; // eax
    unsigned __int16 callback; // [esp+0h] [ebp-4h]

    Scr_AddInt(timeOffset, SCRIPTINSTANCE_SERVER);
#ifdef KISAK_SP
    Scr_AddInt(modelIndex, SCRIPTINSTANCE_SERVER);
#endif
    HitLocationString = G_GetHitLocationString(hitLoc);
    Scr_AddConstString(HitLocationString, SCRIPTINSTANCE_SERVER);
    GScr_AddVector(vDir);
    GScr_AddVector(vPoint);
    v12 = (char *)BG_WeaponName(iWeapon);
    Scr_AddString(v12, SCRIPTINSTANCE_SERVER);
    if ( meansOfDeath <= 0x14 )
        Scr_AddConstString(*modNames[meansOfDeath], SCRIPTINSTANCE_SERVER);
    else
        Scr_AddString("badMOD", SCRIPTINSTANCE_SERVER);
    Scr_AddInt(dflags, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(damage, SCRIPTINSTANCE_SERVER);
    GScr_AddEntity(attacker);
    GScr_AddEntity(inflictor);
#ifdef KISAK_SP
    callback = Scr_ExecEntThread(self, g_scr_data.gametype.actordamage, 0xBu);
#else
    callback = Scr_ExecEntThread(self, g_scr_data.gametype.actordamage, 0xAu);
#endif
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_ActorKilled(
                gentity_s *self,
                gentity_s *inflictor,
                gentity_s *attacker,
                int damage,
                unsigned int meansOfDeath,
                unsigned int iWeapon,
                float *vDir,
                hitLocation_t hitLoc,
                int psTimeOffset)
{
    unsigned __int16 HitLocationString; // ax
    char *v10; // eax
    unsigned __int16 callback; // [esp+0h] [ebp-4h]

    Scr_AddInt(psTimeOffset, SCRIPTINSTANCE_SERVER);
    HitLocationString = G_GetHitLocationString(hitLoc);
    Scr_AddConstString(HitLocationString, SCRIPTINSTANCE_SERVER);
    GScr_AddVector(vDir);
    v10 = (char *)BG_WeaponName(iWeapon);
    Scr_AddString(v10, SCRIPTINSTANCE_SERVER);
    if ( meansOfDeath <= 0x14 )
        Scr_AddConstString(*modNames[meansOfDeath], SCRIPTINSTANCE_SERVER);
    else
        Scr_AddString("badMOD", SCRIPTINSTANCE_SERVER);
    Scr_AddInt(damage, SCRIPTINSTANCE_SERVER);
    GScr_AddEntity(attacker);
    GScr_AddEntity(inflictor);
    callback = Scr_ExecEntThread(self, g_scr_data.gametype.actorkilled, 8u);
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_VehicleRadiusDamage(
                gentity_s *self,
                gentity_s *inflictor,
                gentity_s *attacker,
                int damage,
                float fInnerDamage,
                float fOuterDamage,
                int dflags,
                unsigned int meansOfDeath,
                unsigned int iWeapon,
                float *vPoint,
                float fRadius,
                float coneAngleCos,
                float *coneDirection,
                int timeOffset)
{
    char *value; // eax
    unsigned __int16 callback; // [esp+8h] [ebp-4h]

#ifdef KISAK_SP
    // Consumer half of the CodeCallback_VehicleRadiusDamage guard above -- SP's callbacksetup
    // script does not define that label, so g_scr_data.gametype.vehicleradiusdamage is never
    // populated under KISAK_SP.
    //
    // The return is placed HERE, before the first Scr_Add*, deliberately. Returning at the
    // Scr_ExecEntThread call below instead would still push all 13 arguments onto the script VM
    // stack and then abandon them, corrupting VM state for every later script call -- a subtler
    // and much harder-to-diagnose failure than the one being fixed. Guarding the producer without
    // its consumer (or vice versa) is the specific mistake this project has already made once.
    return;
#endif
    if ( iWeapon == -1 )
        iWeapon = 0;
    Scr_AddInt(timeOffset, SCRIPTINSTANCE_SERVER);
    GScr_AddVector(coneDirection);
    Scr_AddFloat(coneAngleCos, SCRIPTINSTANCE_SERVER);
    Scr_AddFloat(fRadius, SCRIPTINSTANCE_SERVER);
    GScr_AddVector(vPoint);
    value = (char *)BG_WeaponName(iWeapon);
    Scr_AddString(value, SCRIPTINSTANCE_SERVER);
    if ( meansOfDeath <= 0x14 )
        Scr_AddConstString(*modNames[meansOfDeath], SCRIPTINSTANCE_SERVER);
    else
        Scr_AddString("badMOD", SCRIPTINSTANCE_SERVER);
    Scr_AddInt(dflags, SCRIPTINSTANCE_SERVER);
    Scr_AddFloat(fOuterDamage, SCRIPTINSTANCE_SERVER);
    Scr_AddFloat(fInnerDamage, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(damage, SCRIPTINSTANCE_SERVER);
    GScr_AddEntity(attacker);
    GScr_AddEntity(inflictor);
    callback = Scr_ExecEntThread(self, g_scr_data.gametype.vehicleradiusdamage, 0xDu);
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_VehicleDamage(
                gentity_s *self,
                gentity_s *inflictor,
                gentity_s *attacker,
                int damage,
                int dflags,
                unsigned int meansOfDeath,
                unsigned int iWeapon,
                float *vPoint,
                float *vDir,
                hitLocation_t hitLoc,
                int timeOffset,
                unsigned int damageFromUnderneath,
                unsigned int modelIndex,
                unsigned int partName)
{
    unsigned __int16 HitLocationString; // ax
    char *v15; // eax
    unsigned __int16 callback; // [esp+0h] [ebp-4h]

    Scr_AddInt(partName, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(modelIndex, SCRIPTINSTANCE_SERVER);
    Scr_AddBool(damageFromUnderneath, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(timeOffset, SCRIPTINSTANCE_SERVER);
    HitLocationString = G_GetHitLocationString(hitLoc);
    Scr_AddConstString(HitLocationString, SCRIPTINSTANCE_SERVER);
    GScr_AddVector(vDir);
    GScr_AddVector(vPoint);
    v15 = (char *)BG_WeaponName(iWeapon);
    Scr_AddString(v15, SCRIPTINSTANCE_SERVER);
    if ( meansOfDeath <= 0x14 )
        Scr_AddConstString(*modNames[meansOfDeath], SCRIPTINSTANCE_SERVER);
    else
        Scr_AddString("badMOD", SCRIPTINSTANCE_SERVER);
    Scr_AddInt(dflags, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(damage, SCRIPTINSTANCE_SERVER);
    GScr_AddEntity(attacker);
    GScr_AddEntity(inflictor);
    callback = Scr_ExecEntThread(self, g_scr_data.gametype.vehicledamage, 0xDu);
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_PlayerLastStand(
                gentity_s *self,
                gentity_s *inflictor,
                gentity_s *attacker,
                int damage,
                unsigned int meansOfDeath,
                unsigned int iWeapon,
                float *vDir,
                hitLocation_t hitLoc,
                int psTimeOffset)
{
    unsigned __int16 HitLocationString; // ax
    char *v10; // eax
    unsigned __int16 callback; // [esp+0h] [ebp-4h]

    Scr_AddInt(0, SCRIPTINSTANCE_SERVER);
    Scr_AddInt(psTimeOffset, SCRIPTINSTANCE_SERVER);
    HitLocationString = G_GetHitLocationString(hitLoc);
    Scr_AddConstString(HitLocationString, SCRIPTINSTANCE_SERVER);
    GScr_AddVector(vDir);
    v10 = (char *)BG_WeaponName(iWeapon);
    Scr_AddString(v10, SCRIPTINSTANCE_SERVER);
    if ( meansOfDeath <= 0x14 )
        Scr_AddConstString(*modNames[meansOfDeath], SCRIPTINSTANCE_SERVER);
    else
        Scr_AddString("badMOD", SCRIPTINSTANCE_SERVER);
    Scr_AddInt(damage, SCRIPTINSTANCE_SERVER);
    GScr_AddEntity(attacker);
    GScr_AddEntity(inflictor);
    callback = Scr_ExecEntThread(self, g_scr_data.gametype.playerlaststand, 9u);
    Scr_FreeThread(callback, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_VoteCalled(gentity_s *self, char *command, char *param1, char *param2)
{
    Scr_AddString(param2, SCRIPTINSTANCE_SERVER);
    Scr_AddString(param1, SCRIPTINSTANCE_SERVER);
    Scr_AddString(command, SCRIPTINSTANCE_SERVER);
    Scr_Notify(self, scr_const.call_vote, 3u);
}

void __cdecl Scr_PlayerVote(gentity_s *self, char *option)
{
    Scr_AddString(option, SCRIPTINSTANCE_SERVER);
    Scr_Notify(self, scr_const.vote, 1u);
}

void __cdecl GScr_Shutdown()
{
    if ( level.cachedTagMat.name )
        Scr_SetString(&level.cachedTagMat.name, 0, SCRIPTINSTANCE_SERVER);
}

void __cdecl GScr_Gdt_Update(char *asset, char *keyValue)
{
    unsigned __int16 t; // [esp+0h] [ebp-4h]

    Scr_AddString(keyValue, SCRIPTINSTANCE_SERVER);
    Scr_AddString(asset, SCRIPTINSTANCE_SERVER);
    Scr_AddString("gdt_update", SCRIPTINSTANCE_SERVER);
    t = Scr_ExecThread(SCRIPTINSTANCE_SERVER, g_scr_data.levelnotify, 3u);
    Scr_FreeThread(t, SCRIPTINSTANCE_SERVER);
}

void __cdecl Scr_GlassSmash(float *pos, float *dir)
{
    unsigned __int16 t; // [esp+0h] [ebp-4h]

    if (g_scr_data.glassSmash)
    {
        Scr_AddVector(dir, SCRIPTINSTANCE_SERVER);
        Scr_AddVector(pos, SCRIPTINSTANCE_SERVER);
        t = Scr_ExecThread(SCRIPTINSTANCE_SERVER, g_scr_data.glassSmash, 2u);
        Scr_FreeThread(t, SCRIPTINSTANCE_SERVER);
    }
}
