#pragma once
#ifdef KISAK_SP

#include <cstddef>
#include <game/teams.h>

struct actorState_s
{
    int actorNum;
    int entityNum;
    team_t team;
    int modelindex;
    int attachModelIndex[6];
    int attachTagIndex[6];
    int attachIgnoreCollision;
    char name[32];
    int attachedEntNum;
    int attachedEntTime;
    int attachedTagIndex;
    int animScriptedAnim;
    int animScriptedAnimTime;
    int hudwarningType;
    int lookAtEntNum;
};

static_assert(sizeof(actorState_s) == 0x80);
static_assert(offsetof(actorState_s, name) == 0x44);
static_assert(offsetof(actorState_s, attachedEntTime) == 0x68);
static_assert(offsetof(actorState_s, animScriptedAnim) == 0x70);
static_assert(offsetof(actorState_s, hudwarningType) == 0x78);

static_assert(offsetof(actorState_s, actorNum) == 0);
static_assert(offsetof(actorState_s, entityNum) == 4);
static_assert(offsetof(actorState_s, animScriptedAnimTime) == 0x74);
static_assert(offsetof(actorState_s, lookAtEntNum) == 0x7c);

#endif
