#pragma once
#include <string>

// Adjust source before compilation; do not alter the user's converted assets.
inline bool Scr_PatchCampaignSceneSource(const char *filename, std::string &source)
{
    if (std::string(filename) != "maps/kowloon_interrogate.gsc")
        return false;
    static const char marker[] = "kisak_interrogation_player";
    if (source.find(marker) != std::string::npos)
        return false;
    // The stock event starts the next combat phase at 35 + 18 seconds, but
    // int_kowloon_b01_intro_player_out lasts 1758 / 30 = 58.6 seconds.
    // Kowloon stores its camera model in player.player_hands and deletes it
    // after Unlink()/ShowViewModel(). Wait for that actual handoff.
    static const char midway[] = "level waittill(\"interrogation_midway_through\");";
    static const char delay[] = "wait( 18 );";
    static const char transition[] = "level thread maps\\kowloon_gas_leak::event2();";
    static const char release[] = "flag_set( \"event2\" );";
    const auto once = [&source](const char *text) {
        const auto at = source.find(text);
        return at != std::string::npos && source.find(text, at + 1) == std::string::npos
            ? at : std::string::npos;
    };
    const auto mid = once(midway), wait = once(delay), start = once(transition), flag = once(release);
    if (mid == std::string::npos || wait == std::string::npos || start == std::string::npos ||
        flag == std::string::npos || !(mid < wait && wait < start && start < flag))
        return false; // Leave unknown/modded script bodies unchanged.
    source.insert(start, R"gsc(// Wait for the camera to unlink before releasing combat AI.
    kisak_interrogation_player = get_players()[0];
    while ( IsDefined( kisak_interrogation_player ) &&
            IsDefined( kisak_interrogation_player.player_hands ) )
        wait( 0.05 );

    )gsc");
    return true;
}
