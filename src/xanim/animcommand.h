#pragma once

#ifdef KISAK_SP
struct AnimCommand_SP
{
    int index;
    int type;
    int serverTime;
    int sequence;
    int entNum;
    int animIndex;
    int rootAnimIndex;
    float weight;
    float rate;
    float goalTime;
    int flags;
};
static_assert(sizeof(AnimCommand_SP) == 0x2C, "retail SP animation command layout changed");
#endif
