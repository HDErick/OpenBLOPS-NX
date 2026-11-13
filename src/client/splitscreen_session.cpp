#include "splitscreen.h"
#include <qcommon/common.h>
#include <qcommon/cmd.h>
#include <cstring>

// Selection is process-local state owned by the frontend. No dvar or console
// command can arm a guest. A map must be selected by the split-screen menu.
static bool s_splitScreenRequested;
static char s_splitScreenMap[64];
enum class SplitScreenPhase { Off, Queued, Loading, Active };
static SplitScreenPhase s_splitScreenPhase;

bool CL_SplitScreenSessionRequested()
{
    return s_splitScreenRequested;
}

void CL_SplitScreenResetSession()
{
    if (s_splitScreenRequested) CL_SplitGuestLog("Split-Screen session cleared");
    s_splitScreenRequested = false;
    s_splitScreenMap[0] = 0;
    s_splitScreenPhase = SplitScreenPhase::Off;
}

void CL_SplitScreenFrontendEntered()
{
    // Starting a map rebuilds the frontend through Com_AssetLoadUI as well.
    // That transient UI is not a user returning from the completed match.
    if (s_splitScreenPhase != SplitScreenPhase::Queued && s_splitScreenPhase != SplitScreenPhase::Loading)
        CL_SplitScreenResetSession();
}

void CL_SplitScreenSessionActivated()
{
    if (s_splitScreenRequested && s_splitScreenPhase != SplitScreenPhase::Active)
    {
        s_splitScreenPhase = SplitScreenPhase::Active;
        CL_SplitGuestLog("Split-Screen host primed; guest admission enabled");
    }
}

bool CL_SplitScreenCanStartMap(const char *map)
{
    if (!map || !*map || std::strlen(map) >= sizeof(s_splitScreenMap))
        return false;
    for (const char *p = map; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_'))
            return false;
#ifdef KISAK_SP
    if (!Com_IsZombieMap(map)) return false;
#else
    if (std::strncmp(map, "mp_", 3)) return false;
#endif
    return true;
}

bool CL_SplitScreenStartMap(const char *map)
{
    if (!CL_SplitScreenCanStartMap(map)) return false;
    std::strcpy(s_splitScreenMap, map);
    s_splitScreenRequested = true;
    s_splitScreenPhase = SplitScreenPhase::Queued;
    CL_SplitGuestLog("Split-Screen match queued from menu");
    // Offline System Link supplies real co-op/MP networking without Steam or
    // the online-only private-match flag. The native lobby still selects rules.
    Dvar_SetBoolByName("bo1_directory_publish", false);
    Dvar_SetBoolByName("onlinegame", false);
    Dvar_SetBoolByName("systemlink", true);
    Dvar_SetBoolByName("xblive_privatematch", false);
    Dvar_SetBoolByName("xblive_rankedmatch", false);
    Dvar_SetBoolByName("xblive_basictraining", false);
    Dvar_SetBoolByName("playlist_enabled", false);
    Dvar_SetIntByName("party_maxlocalplayers", 2);
#ifdef KISAK_SP
    Dvar_SetIntByName("sv_maxclients", 2);
    Dvar_SetBoolByName("zombiemode", true);
    Dvar_SetBoolByName("ui_zombiemode", true);
#else
    if (Dvar_GetInt("sv_maxclients") < 2) Dvar_SetIntByName("sv_maxclients", 2);
    Dvar_SetStringByName("g_gametype", const_cast<char *>(Dvar_GetString("ui_gametype")));
    // Killcam presentation still shares the loaded world in this prototype.
    Dvar_SetBoolByName("scr_game_allowkillcam", false);
    Dvar_SetBoolByName("scr_game_allowfinalkillcam", false);
#endif
    Dvar_SetBoolByName("gpad_enabled", true);
    Dvar_SetIntByName("splitguest_controller", 1);
    // Both pads use the regular controller profile and current bindings.
    Cbuf_AddText(0, va("remapcontrollertoprimaryandactivate\n"
        "setclientbeingusedandprimaryandactive\n"
        "map %s\n", s_splitScreenMap));
    return true;
}

void CL_SplitScreenMapLoading(const char *map)
{
    // Same-map restarts preserve the selected session; frontend, campaign,
    // normal map commands and other servers never inherit it.
    if (!map || std::strcmp(map, s_splitScreenMap))
        CL_SplitScreenResetSession();
    else if (s_splitScreenRequested)
    {
        s_splitScreenPhase = SplitScreenPhase::Loading;
        CL_SplitGuestLog("Split-Screen selected map loading");
    }
}
