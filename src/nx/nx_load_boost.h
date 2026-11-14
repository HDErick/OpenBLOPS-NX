#pragma once

enum class NxLoadPhase : unsigned
{
    Boot = 1,
    Server = 2,
    Client = 4,
};

void NX_LoadBoostInit();
void NX_LoadBoostSet(NxLoadPhase phase, bool active);
void NX_LoadBoostReset(const char *reason);
void NX_LoadBoostUpdate();
void NX_LoadBoostEndBootWhenLoaded();
bool NX_LoadBoostBackgroundLoading();
void NX_LoadBoostExit();
bool NX_LoadBoostActive();
