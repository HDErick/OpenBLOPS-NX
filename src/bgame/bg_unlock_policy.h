#pragma once
// Direct multiplayer has no progression service. Availability is a shared game
// policy, independent of UI online/private flags, controller, rank or purchases.
constexpr bool BG_UnlockablesHasFullAccess()
{
#if defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH)
    return true;
#else
    return false;
#endif
}
