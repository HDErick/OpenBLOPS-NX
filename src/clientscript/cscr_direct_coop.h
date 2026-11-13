#pragma once

#include <string>

// Adjust owned source as it is loaded. Keep these changes confined to the
// opt-in PC co-op build; never modify the user's KBZ files.
inline bool Scr_PatchDirectCoopSource(const char *filename, std::string &source)
{
    const bool callbacks = std::string(filename) == "maps/_callbackglobal.gsc";
    const bool zombies = std::string(filename) == "maps/_zombiemode.gsc";
    if (!callbacks && !zombies)
        return false;

    std::string adjusted = source;
    auto replaceOnce = [&adjusted](const char *anchor, const char *replacement) {
        const size_t at = adjusted.find(anchor);
        if (at == std::string::npos || adjusted.find(anchor, at + 1) != std::string::npos)
            return false;
        adjusted.replace(at, std::char_traits<char>::length(anchor), replacement);
        return true;
    };
    // Reject unknown/modded bodies atomically rather than applying half a fix.
    if (callbacks)
    {
        if (!replaceOnce("self thread[[level.spawnClient]]();", R"gsc(self.kisak_direct_late_join = IsDefined( level.kisak_direct_roster_ready ) &&
        !IsDefined( self.kisak_direct_initial_player );
    self.kisak_direct_waiting_round = self.kisak_direct_late_join;
    if ( self.kisak_direct_late_join )
        self thread maps\_zombiemode::kisak_direct_wait_for_round();
    else
        self thread[[level.spawnClient]]();)gsc")
            || !replaceOnce("if( IsDefined( level.custom_spawnPlayer ) )", R"gsc(if( IsDefined( level.custom_spawnPlayer ) &&
        !( IsDefined( self.kisak_direct_late_join ) && self.kisak_direct_late_join && !self.initialized ) ))gsc"))
            return false;
    }
    else
    {
        if (!replaceOnce(
                R"gsc(println( "sessions: mapname=", level.script, " gametype zom isserver 1 player_count=", get_players().size );)gsc",
                R"gsc(direct_players = get_players();
    for ( direct_i = 0; direct_i < direct_players.size; direct_i++ )
        direct_players[direct_i].kisak_direct_initial_player = true;
    level.kisak_direct_roster_ready = true;
    println( "sessions: mapname=", level.script, " gametype zom isserver 1 player_count=", get_players().size );)gsc")
            || !replaceOnce("self freezecontrols( true ); // first spawn only, intro_black_screen will pull them out of it", R"gsc(if ( !IsDefined( self.kisak_direct_late_join ) || !self.kisak_direct_late_join )
                    self freezecontrols( true ); // Initial players wait for the intro.)gsc")
            || !replaceOnce("self waittill( \"spawned_player\" );", R"gsc(self waittill( "spawned_player" );
        self kisak_direct_set_spawn_points();)gsc")
            || !replaceOnce("self maps\\_zombiemode_score::player_reduce_points( \"died\" );",
                "self kisak_direct_set_spawn_points();")
            || !replaceOnce("players[i].score < 1500", "players[i].score < 500")
            || !replaceOnce("players[i].score = 1500;", "players[i].score = 500;")
            || !replaceOnce("if( players[i].sessionstate == \"spectator\" )", R"gsc(if( players[i].sessionstate == "spectator" &&
                !( IsDefined( players[i].kisak_direct_waiting_round ) && players[i].kisak_direct_waiting_round ) ))gsc"))
            return false;
        // Compressed retail sources include a trailing NUL in their length.
        const size_t terminator = adjusted.find('\0');
        if (terminator != std::string::npos)
            adjusted.resize(terminator);
        adjusted += R"gsc(

kisak_direct_set_spawn_points()
{
    // Each real spawn starts at 500. Retain cumulative earned-score stats
    // after the first spawn; a respawn resets only the spendable balance.
    self.score = 500;
    if ( !IsDefined( self.initialized ) || !self.initialized )
        self.score_total = self.score;
    self.old_score = self.score;
    if ( IsDefined( self.initialized ) && self.initialized )
        self maps\_zombiemode_score::set_player_score_hud();
}

kisak_direct_wait_for_round()
{
    self endon( "disconnect" );
    level endon( "intermission" );
    players = get_players();
    target = undefined;
    for ( i = 0; i < players.size; i++ )
    {
        if ( players[i] != self && players[i].sessionstate == "playing" )
        {
            target = players[i];
            break;
        }
    }
    points = getstructarray( "initial_spawn_points", "targetname" );
    index = self GetEntityNumber();
    if ( points.size > 0 )
        self.spectator_respawn = points[index % points.size];
    else
    {
        self.spectator_respawn = spawnstruct();
        self.spectator_respawn.origin = self.origin;
        self.spectator_respawn.angles = self.angles;
        if ( IsDefined( target ) )
        {
            self.spectator_respawn.origin = target.origin;
            self.spectator_respawn.angles = target.angles;
        }
    }
    self setorigin( self.spectator_respawn.origin );
    self setplayerangles( self.spectator_respawn.angles );
    self.is_zombie = true;
    self thread [[level.spawnSpectator]]();
    self waittill( "spawned_spectator" );
    self cameraactivate( false );
    self freezecontrols( false );
    if ( IsDefined( target ) )
        self.spectatorclient = target GetEntityNumber();
    println( "Direct co-op: late join waiting for round transition" );
    level waittill( "end_of_round" );
    // Claim the player before the regular spectator loop runs. If it ran
    // first, kisak_direct_waiting_round excluded us from that iteration.
    self.sessionstate = "playing";
    self.kisak_direct_waiting_round = false;
    self setSpectatePermissions( false );
    origin = check_for_valid_spawn_near_team( self );
    if ( !IsDefined( origin ) )
        origin = self.spectator_respawn.origin;
    self.is_zombie = false;
    self setorigin( origin );
    self setplayerangles( self.spectator_respawn.angles );
    // First spawn needs the normal loadout, HUD and script setup. Later deaths
    // use the map's existing spectator_respawn implementation unchanged.
    println( "Direct co-op: initializing late join player at round transition" );
    self [[level.spawnPlayer]]();
}
)gsc";
    }
    source.swap(adjusted);
    return true;
}
