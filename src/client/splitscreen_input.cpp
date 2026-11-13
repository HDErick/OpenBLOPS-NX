#include "splitscreen.h"
#include <aim_assist/aim_assist.h>
#include <client/client.h>
#include <client/cl_keys.h>
#include <ui/keycodes.h>
#include <cgame/cg_weapons.h>
#include <bgame/bg_weapons.h>
#include <bgame/bg_misc.h>
#include <win32/win_gamepad.h>
#include <win32/win_input.h>
#include <ui/ui_main.h>
#include <cmath>
#include <cstdio>
#include <cstring>

// XInput ports are physical devices; the guest shares the host profile and aim
// settings. Native assist owns a private target list without a second CG_Init.
struct SplitGuestInputState
{
    bool initialized = false, active = false, connected = false;
    int port = -1, client = -1, mode = -1, teleport = 0, spawnCount = -1;
    float serverYaw = 0.0f, deltaYaw = 0.0f;
    float rawAngles[3]{}, viewAngles[3]{};
    unsigned int previous = 0, suppressed = 0;
    int stance = 0, stancePressTime = 0, oldStance = 0;
    bool stanceHeld = false, adsToggle = false;
    unsigned int weapon = 0, authoritativeWeapon = 0;
    int weaponRequestTime = 0;
    bool serverHolstered = false;
    int pendingGrenade = 0, grenadeRequestTime = 0, grenadeCommandTime = 0;
    bool grenadeRelease = false;
    unsigned short grenadeWeapon = 0;
    int forward = 0, side = 0;
    int commandTime = 0, acknowledgedTime = 0, weaponState = -1;
    unsigned int buttons[2]{};
};
static SplitGuestInputState s_guestInput;
struct SplitGuestWeaponRequest { bool valid = false; unsigned int weapon = 0; int time = 0; };
static SplitGuestWeaponRequest s_guestWeaponRequest;

void CL_SplitGuestSelectWeapon(unsigned int weapon)
{
    if (weapon >= BG_GetNumWeapons())
        return;
    s_guestWeaponRequest.valid = true;
    s_guestWeaponRequest.weapon = weapon;
    s_guestWeaponRequest.time = -1; // timestamp on the next input frame
}

void CL_SplitGuestRegisterInputDvars()
{
    _Dvar_RegisterInt("splitguest_controller", 1, 0, GPAD_PORT_COUNT - 1, 0,
        "Physical XInput slot for player two (0=first pad, 1=second pad)");
    _Dvar_RegisterFloat("splitguest_yaw_speed", 180.0f, 1.0f, 720.0f, 0,
        "Guest horizontal look degrees/second at profile sensitivity one");
    _Dvar_RegisterFloat("splitguest_pitch_speed", 120.0f, 1.0f, 720.0f, 0,
        "Guest vertical look degrees/second at profile sensitivity one");
}

void CL_SplitGuestResetInput()
{
    s_guestInput = SplitGuestInputState{};
    s_guestWeaponRequest = SplitGuestWeaponRequest{};
    AimAssist_ResetSplitGuest();
}

bool CL_SplitGuestOwnsController(int port)
{
    return CL_IsSplitGuest(1) && port == Dvar_GetInt("splitguest_controller");
}

static float SplitGuestNormalizeAngle(float angle)
{
    angle = std::fmod(angle + 180.0f, 360.0f);
    if (angle < 0.0f)
        angle += 360.0f;
    return angle - 180.0f;
}

// Poll the guest's device through the host's current profile. Never execute
// a binding in P1's command buffer or replace the saved bindings on menu entry.
static unsigned int SplitGuestBindingActions(const char *binding)
{
    unsigned int actions = 0;
    while (binding && *binding)
    {
        while (*binding == ' ' || *binding == '	' || *binding == ';') ++binding;
        const char *end = binding;
        while (*end && *end != ';' && *end != ' ' && *end != '	') ++end;
        auto is = [&](const char *command) {
            return std::strlen(command) == static_cast<size_t>(end - binding)
                && !I_strnicmp(binding, command, static_cast<int>(end - binding));
        };
        if (is("+gostand")) actions |= 1u << 0;
        if (is("+stance")) actions |= 1u << 1;
        if (is("+usereload") || is("+activate") || is("+reload")) actions |= 1u << 2;
        if (is("weapnext")) actions |= 1u << 3;
        if (is("+breath_sprint") || is("+sprint") || is("+holdbreath")) actions |= 1u << 4;
        if (is("+melee")) actions |= 1u << 5;
        if (is("+smoke")) actions |= 1u << 6;
        if (is("+frag")) actions |= 1u << 7;
        if (is("+speed_throw") || is("+speed")) actions |= 1u << 8;
        if (is("+attack")) actions |= 1u << 9;
        if (is("weapprev")) actions |= 1u << 10;
        if (is("+toggleads_throw") || is("toggleads")) actions |= 1u << 11;
        if (is("+goprone") || is("goprone")) actions |= 1u << 12;
        if (is("+gocrouch") || is("gocrouch")) actions |= 1u << 13;
        while (*end && *end != ';') ++end;
        binding = *end ? end + 1 : end;
    }
    return actions;
}

static void SplitGuestUpdateAxes(int port, bool connected)
{
    // Native CL_GamepadAxisValue applies the regular thumbstick layout and
    // linear/squared response curve to a private set of physical axis values.
    gaGlobs[1] = gaGlobs[0];
    const GamePadStick sticks[] = {GPAD_RX, GPAD_RY, GPAD_LX, GPAD_LY};
    for (int axis = 0; axis < 6; ++axis)
    {
        const double value = !connected ? 0.0 : axis < 4 ? GPad_GetStick(port, sticks[axis])
            : GPad_GetButton(port, axis == 4 ? GPAD_R_TRIG : GPAD_L_TRIG);
        gaGlobs[1].axesValues[axis] = static_cast<int>(value * 65535.0);
    }
}

static unsigned int SplitGuestHeldActions(int port)
{
    const GamePadButton buttons[] = {GPAD_A, GPAD_B, GPAD_X, GPAD_Y, GPAD_L3, GPAD_R3,
        GPAD_L_SHLDR, GPAD_R_SHLDR, GPAD_L_TRIG, GPAD_R_TRIG, GPAD_UP, GPAD_DOWN, GPAD_LEFT, GPAD_RIGHT};
    const int keys[] = {K_BUTTON_A, K_BUTTON_B, K_BUTTON_X, K_BUTTON_Y, K_BUTTON_LSTICK,
        K_BUTTON_RSTICK, K_BUTTON_LSHLDR, K_BUTTON_RSHLDR, K_BUTTON_LTRIG, K_BUTTON_RTRIG,
        K_DPAD_UP, K_DPAD_DOWN, K_DPAD_LEFT, K_DPAD_RIGHT};
    unsigned int held = 0;
    for (unsigned int i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
        if (GPad_GetButton(port, buttons[i]) > 0.15)
            for (int binding = 0; binding < 2; ++binding)
                held |= SplitGuestBindingActions(Key_GetBinding(0, keys[i], binding));
    if (CL_GamepadAxisValue(1, GPAD_VIRTAXIS_ATTACK) > 0.15) held |= 1u << 9;
    return held;
}

void CL_SplitGuestBuildInput(usercmd_s *command, const playerState_s *player,
    int realtime, int frameMsec, bool enabled)
{
    SplitGuestInputState &input = s_guestInput;
    const int port = Dvar_GetInt("splitguest_controller");
    const bool reset = !input.initialized || input.client != player->clientNum
        || input.mode != player->pm_type || input.teleport != (player->eFlags & 2)
        || input.spawnCount != player->stats[4];
    if (reset)
    {
        const bool keepCommandBasis = input.initialized; // same connection, even when following another client
        float previousRaw[3]; std::memcpy(previousRaw, input.rawAngles, sizeof(previousRaw));
        const SplitGuestWeaponRequest serverRequest = s_guestWeaponRequest;
        CL_SplitGuestResetInput();
        s_guestWeaponRequest = serverRequest; // selections may arrive with the spawn snapshot
        input.initialized = true;
        // SetClientViewAngle computes a new delta relative to the server's
        // existing raw usercmd. Preserve that same command basis through spawn,
        // teleport and last stand; snapshot viewangles may be prediction-backed
        // or lag behind the newer spawn delta.
        for (int axis = 0; axis < 3; ++axis)
            input.rawAngles[axis] = keepCommandBasis ? previousRaw[axis]
                : player->viewangles[axis] - player->delta_angles[axis];
        input.weapon = input.authoritativeWeapon = player->weapon;
        input.stance = (player->pm_flags & 1) ? 2 : (player->pm_flags & 2) ? 1 : 0;
    }
    if (input.port != port)
    {
        input.previous = input.suppressed = 0;
        input.active = false;
        input.pendingGrenade = 0;
    }
    input.port = port;
    input.client = player->clientNum;
    input.mode = player->pm_type;
    input.spawnCount = player->stats[4];
    input.serverYaw = player->viewangles[1];
    input.deltaYaw = player->delta_angles[1];
    input.teleport = player->eFlags & 2;
    input.connected = static_cast<unsigned int>(port) < GPAD_PORT_COUNT && GPad_IsActive(port);
    SplitGuestUpdateAxes(port, input.connected);
    unsigned int held = input.connected ? SplitGuestHeldActions(port) : 0;
    const bool following = (player->otherFlags & 2) != 0;
#ifdef KISAK_SP
    const bool respawnInput = false;
#else
    // MP scripts wait for Use to respawn, including when the dead connection
    // has a spectator/follow snapshot. Keep the camera still in these states.
    const bool respawnInput = player->pm_type == 4 || player->pm_type == 9 || player->pm_type == 10;
#endif
    const bool aimReset = reset || !input.active;
    input.active = enabled && !CL_SplitGuestMenuBlocksInput() && input.connected && Dvar_GetBool("gpad_enabled")
        && IN_IsForegroundWindow() && !Key_IsCatcherActive(0, -1)
        && !UI_IsFullscreen(0) && !Dvar_GetInt("cl_paused")
        && (following || respawnInput || ((player->pm_type <= 3 || player->pm_type == 6 || player->pm_type == 7)
            && !(player->pm_flags & 0x800) && player->weaponstate != 35));
    command->button_bits.array[0] = command->button_bits.array[1] = 0;
    command->forwardmove = command->rightmove = command->upmove = 0;
    if (!input.active)
    {
        // A trigger held while a menu is open cannot fire on closing that menu.
        input.suppressed = held;
        input.previous = 0;
        input.stancePressTime = 0;
        input.stanceHeld = false;
        input.weapon = player->weapon;
        input.pendingGrenade = 0;
    }
    input.suppressed &= held;
    held &= ~input.suppressed;
    const unsigned int pressed = held & ~input.previous;
    if (input.active && (pressed & (1u << 11))) input.adsToggle = !input.adsToggle;
    if (input.adsToggle) held |= 1u << 8;
    if (input.active && following)
    {
        // These native spectator buttons cycle the target or leave follow.
        // Movement/look must not steer the followed player's displayed camera.
        if (held & (1u << 9)) command->button_bits.setBit(0);
        if (held & (1u << 8)) command->button_bits.setBit(11);
        if (held & (1u << 5)) command->button_bits.setBit(2);
#ifndef KISAK_SP
        if (held & (1u << 2)) command->button_bits.setBit(5); // X: request respawn
#endif
    }
#ifndef KISAK_SP
    else if (input.active && respawnInput)
    {
        if (held & (1u << 2)) command->button_bits.setBit(5);
    }
#endif
    else if (input.active)
    {
        float seconds = (frameMsec > 0 ? frameMsec : 0) * 0.001f;
        if (seconds > 0.05f)
            seconds = 0.05f;
        float sensitivity = Dvar_GetFloat("input_viewSensitivity");
        if (sensitivity < 0.001f)
            sensitivity = 1.0f;
        const float yaw = static_cast<float>(CL_GamepadAxisValue(1, GPAD_VIRTAXIS_YAW));
        const float pitch = static_cast<float>(CL_GamepadAxisValue(1, GPAD_VIRTAXIS_PITCH));
        command->forwardmove = static_cast<char>(127.0 * CL_GamepadAxisValue(1, GPAD_VIRTAXIS_FORWARD));
        command->rightmove = static_cast<char>(127.0 * CL_GamepadAxisValue(1, GPAD_VIRTAXIS_SIDE));
        if (held & (1u << 9)) command->button_bits.setBit(0);  // RT: fire
        if (held & (1u << 8)) command->button_bits.setBit(11); // LT: aim
        if (held & (1u << 2)) command->button_bits.setBit(5);  // X: use/reload/revive
        if (held & (1u << 5)) command->button_bits.setBit(2);  // R3: melee
        if (held & (1u << 4))
        {
            command->button_bits.setBit(1);                  // L3: sprint/breath
            command->button_bits.setBit(13);
        }
        if (pressed & (1u << 12)) { input.stance = 2; input.stanceHeld = false; }
        if (pressed & (1u << 13)) { input.stance = 1; input.stanceHeld = false; }
        if (pressed & (1u << 1))
        {
            input.oldStance = input.stance;
            input.stance = input.stance == 0 ? 1 : 0;
            input.stancePressTime = realtime;
            input.stanceHeld = true;
        }
        if (!(held & (1u << 1)))
            input.stanceHeld = false;
        if (input.stanceHeld && realtime - input.stancePressTime >= 400)
        {
            input.stance = input.oldStance == 2 ? 0 : 2;
            input.stanceHeld = false; // one long-press action, not a command every frame
        }
        if (held & 1u)
        {
            input.stanceHeld = false;
            if (input.stance || (player->pm_flags & 3))
                input.stance = 0;
            else
                command->button_bits.setBit(10);             // A: stand/jump
        }
        if (pressed & (1u << 4))
        {
            input.stance = 0;
            input.stanceHeld = false;
        }
        if (input.stance == 1) command->button_bits.setBit(9);
        if (input.stance == 2) command->button_bits.setBit(8);
        // A requested weapon must survive several commands until the server
        // acknowledges it; every snapshot still contains the old weapon meanwhile.
        // Offhand animations temporarily holster the primary (ps.weapon = 0).
        // Keep the selected primary in commands so it comes back after the throw.
        const bool offhandActive = (player->weaponstate >= 20 && player->weaponstate <= 25)
            || (player->weapFlags & (2u | 0x20000u));
        if (player->weapon
            && (input.authoritativeWeapon != player->weapon || input.weapon == player->weapon
                || realtime - input.weaponRequestTime > 1500))
            input.weapon = player->weapon;
        input.authoritativeWeapon = player->weapon;
        if (s_guestWeaponRequest.valid)
        {
            if (s_guestWeaponRequest.time < 0)
                s_guestWeaponRequest.time = realtime;
            if (realtime - s_guestWeaponRequest.time > 3000)
                s_guestWeaponRequest.valid = false;
            else if (!s_guestWeaponRequest.weapon || BG_PlayerHasWeapon(player, s_guestWeaponRequest.weapon))
            {
                input.weapon = s_guestWeaponRequest.weapon;
                input.serverHolstered = !input.weapon;
                input.weaponRequestTime = realtime;
                if (player->weapon == input.weapon)
                    s_guestWeaponRequest.valid = false;
            }
        }
        // Purchases/revives can remove the old gun before the replacement is
        // raised. Do not keep commanding an inventory entry which no longer exists.
        if (!offhandActive && !input.serverHolstered && (player->pm_type == 0 || player->pm_type == 1)
            && (!input.weapon || !BG_PlayerHasWeapon(player, input.weapon)))
        {
            input.weapon = NextWeapInCycle(1, player, WEAPINVENTORY_PRIMARY, 0, true, false, false);
            input.weaponRequestTime = realtime;
        }
        if (pressed & ((1u << 3) | (1u << 10)))
        {
            const unsigned int next = NextWeapInCycle(1, player, WEAPINVENTORY_PRIMARY,
                input.weapon, !(pressed & (1u << 10)), false, false);
            if (next)
            {
                s_guestWeaponRequest.valid = false;
                input.serverHolstered = false;
                input.weapon = next;
                input.weaponRequestTime = realtime;
            }
        }
        command->weapon = static_cast<unsigned short>(input.weapon);
        // Retain a tap until an authoritative command acknowledgement, not a
        // wall-clock retry window. A release edge can otherwise be swallowed by
        // packet batching or duplicate command times, so retries wait for the
        // server to acknowledge the release before issuing another press.
        if (offhandActive || realtime - input.grenadeRequestTime > 1500)
            input.pendingGrenade = 0;
        if (input.pendingGrenade && player->commandTime >= input.grenadeCommandTime)
        {
            input.grenadeRelease = !input.grenadeRelease;
            input.grenadeCommandTime = command->serverTime > player->commandTime
                ? command->serverTime : player->commandTime + 1;
        }
        for (int grenade = 0; grenade < 2; ++grenade)
        {
            const unsigned int mask = 1u << (grenade ? 6 : 7);
            if ((pressed & mask) && !offhandActive)
            {
                const int offhand = BG_GetFirstAvailableOffhand(player, grenade
                    ? OFFHAND_SLOT_TACTICAL_GRENADE : OFFHAND_SLOT_LETHAL_GRENADE);
                if (offhand)
                {
                    input.pendingGrenade = grenade + 1;
                    input.grenadeWeapon = static_cast<unsigned short>(offhand);
                    input.grenadeRequestTime = realtime;
                    input.grenadeCommandTime = command->serverTime > player->commandTime
                        ? command->serverTime : player->commandTime + 1;
                    input.grenadeRelease = false;
                }
            }
            const bool pending = input.pendingGrenade == grenade + 1;
            if (pending ? !input.grenadeRelease : (held & mask) != 0)
            {
                command->button_bits.setBit(grenade ? 15 : 14);
                if (pending)
                    command->offHandIndex = input.grenadeWeapon;
            }
        }
        if (!AimAssist_UpdateSplitGuestInput(player, command, seconds,
            pitch * (Dvar_GetBool("input_invertPitch") ? 1.0f : -1.0f), -yaw,
            input.rawAngles, aimReset))
        {
            const float aimScale = (held & (1u << 8)) ? 0.5f : 1.0f;
            input.rawAngles[1] -= yaw * seconds * sensitivity * aimScale
                * Dvar_GetFloat("splitguest_yaw_speed");
            input.rawAngles[0] += pitch * seconds * sensitivity * aimScale
                * Dvar_GetFloat("splitguest_pitch_speed") * (Dvar_GetBool("input_invertPitch") ? 1.0f : -1.0f);
        }
        float up = Dvar_GetFloat("player_view_pitch_up");
        float down = Dvar_GetFloat("player_view_pitch_down");
        if (up <= 0.0f) up = 85.0f;
        if (down <= 0.0f) down = 85.0f;
        float viewPitch = SplitGuestNormalizeAngle(input.rawAngles[0] + player->delta_angles[0]);
        if (viewPitch < -up) viewPitch = -up;
        if (viewPitch > down) viewPitch = down;
        input.rawAngles[0] = viewPitch - player->delta_angles[0];
    }
    input.previous = input.active ? held : 0;
    for (int axis = 0; axis < 3; ++axis)
    {
        input.rawAngles[axis] = SplitGuestNormalizeAngle(input.rawAngles[axis]);
        input.viewAngles[axis] = following ? player->viewangles[axis]
            : SplitGuestNormalizeAngle(input.rawAngles[axis] + player->delta_angles[axis]);
        command->angles[axis] = static_cast<unsigned short>(static_cast<int>(input.rawAngles[axis] * (65536.0f / 360.0f)));
    }
#ifdef KISAK_SP
    // SP uses these absolute angles for bullet direction; leaving the default
    // zeroes would fire along world +X regardless of the guest's camera.
    command->gunPitch = static_cast<short>(static_cast<unsigned short>(static_cast<int>(input.viewAngles[0] * (65536.0f / 360.0f))));
    command->gunYaw = static_cast<short>(static_cast<unsigned short>(static_cast<int>(input.viewAngles[1] * (65536.0f / 360.0f))));
#endif
    input.forward = command->forwardmove;
    input.side = command->rightmove;
    const bool offhandTransition = ((input.buttons[0] ^ command->button_bits.array[0])
        & ((0x80000000u >> 14) | (0x80000000u >> 15) | (0x80000000u >> 8) | (0x80000000u >> 9))) != 0
        || (input.weaponState != player->weaponstate && ((player->weaponstate >= 20 && player->weaponstate <= 25)
            || (input.weaponState >= 20 && input.weaponState <= 25)));
    input.commandTime = command->serverTime;
    input.acknowledgedTime = player->commandTime;
    input.weaponState = player->weaponstate;
    std::memcpy(input.buttons, command->button_bits.array, sizeof(input.buttons));
    if (reset || offhandTransition)
        CL_SplitGuestLog(reset ? "Guest spawn/view basis reset" : "Guest offhand/stance command transition");
}

bool CL_SplitGuestGetViewAngles(float *angles)
{
    if (!s_guestInput.initialized)
        return false;
    std::memcpy(angles, s_guestInput.viewAngles, sizeof(s_guestInput.viewAngles));
    return true;
}

const char *CL_SplitGuestInputStatus()
{
    static char status[512];
    std::snprintf(status, sizeof(status), "guest_pad=%d connected=%d input_active=%d move=%d,%d buttons=%08x:%08x look=%.1f,%.1f cmd_time=%d acknowledged=%d offhand_pending=%d release=%d phase_time=%d stance=%d spawn=%d raw_yaw=%.1f delta_yaw=%.1f server_yaw=%.1f selected=%u requested=%d/%u",
        s_guestInput.port, s_guestInput.connected, s_guestInput.active, s_guestInput.forward,
        s_guestInput.side, s_guestInput.buttons[0], s_guestInput.buttons[1],
        s_guestInput.viewAngles[0], s_guestInput.viewAngles[1], s_guestInput.commandTime,
        s_guestInput.acknowledgedTime, s_guestInput.pendingGrenade, s_guestInput.grenadeRelease,
        s_guestInput.grenadeCommandTime, s_guestInput.stance, s_guestInput.spawnCount,
        s_guestInput.rawAngles[1], s_guestInput.deltaYaw, s_guestInput.serverYaw, s_guestInput.weapon,
        s_guestWeaponRequest.valid, s_guestWeaponRequest.weapon);
    return status;
}
