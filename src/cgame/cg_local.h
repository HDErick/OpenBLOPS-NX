#pragma once
#include <xanim/dobj.h>
#include <xanim/xanim.h>

#include <cgame/cg_weapons.h>

struct ViewModelHand // sizeof=0x8
{                                       // XREF: ViewModelInfo/r
    int iPrevAnim;
    int iHandAnimIndex;
};

struct ViewModelInfo // sizeof=0x34
{
    DObj *viewModelDObj;
    int hasAnimTree;
    struct XAnimTree_s *tree;
    XAnim_s *anims;
    unsigned int partBits[5];
    ViewModelHand hand[2];
};

extern ViewModelInfo *cg_viewModelArray;
extern weaponInfo_s *cg_weaponsArray[MAX_LOCAL_CLIENTS];

inline ViewModelInfo *__cdecl CG_GetLocalClientViewModelInfo(int localClientNum)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);

    return cg_viewModelArray ? &cg_viewModelArray[localClientNum] : nullptr;
}


inline weaponInfo_s *__cdecl CG_GetLocalClientWeaponInfo(int localClientNum, int weaponIndex)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);

    return &cg_weaponsArray[localClientNum][weaponIndex];
}
