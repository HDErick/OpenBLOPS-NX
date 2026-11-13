#include "bg_actor_prone.h"

#ifdef KISAK_SP
#include <bit>
#include <cstdint>

static int BG_ProneTimeAdd(int left, int right)
{
    return std::bit_cast<int>(static_cast<std::uint32_t>(left) + static_cast<std::uint32_t>(right));
}

static int BG_ProneTimeSubtract(int left, int right)
{
    return std::bit_cast<int>(static_cast<std::uint32_t>(left) - static_cast<std::uint32_t>(right));
}

int __cdecl BG_ActorIsProne(actor_prone_info_s *pInfo, int iCurrentTime)
{
    if ( !pInfo->prone || pInfo->bCorpseOrientation || pInfo->orientPitch )
        return 0;
    if ( !pInfo->iProneTrans )
        return 1;
    if ( pInfo->iProneTrans > 0 )
    {
        if ( BG_ProneTimeAdd(pInfo->iProneTrans, pInfo->iProneTime) < iCurrentTime )
            pInfo->iProneTrans = 0;
    }
    else if ( BG_ProneTimeSubtract(pInfo->iProneTime, pInfo->iProneTrans) < iCurrentTime )
    {
        pInfo->prone = false;
        return 0;
    }
    return 1;
}

double __cdecl BG_GetActorProneFraction(actor_prone_info_s *pInfo, int iCurrentTime)
{
    if ( !pInfo->prone )
        return 0.0;
    if ( !pInfo->iProneTrans )
        return 1.0;
    if ( pInfo->iProneTrans > 0 )
    {
        if ( BG_ProneTimeAdd(pInfo->iProneTrans, pInfo->iProneTime) < iCurrentTime )
        {
            pInfo->iProneTrans = 0;
            return 1.0;
        }
        volatile double fraction = static_cast<double>(BG_ProneTimeSubtract(iCurrentTime, pInfo->iProneTime))
            / static_cast<double>(pInfo->iProneTrans);
        return fraction;
    }
    if ( BG_ProneTimeSubtract(pInfo->iProneTime, pInfo->iProneTrans) < iCurrentTime )
    {
        pInfo->prone = false;
        return 0.0;
    }
    volatile double fraction = static_cast<double>(BG_ProneTimeSubtract(iCurrentTime, pInfo->iProneTime))
        / static_cast<double>(BG_ProneTimeSubtract(0, pInfo->iProneTrans));
    return 1.0 - fraction;
}
#else
int __cdecl BG_ActorIsProne(actor_prone_info_s *pInfo, int iCurrentTime)
{
    if ( !pInfo->prone || pInfo->bCorpseOrientation || pInfo->orientPitch )
        return 0;
    if ( !pInfo->iProneTrans )
        return 1;
    if ( pInfo->iProneTrans >= 0 )
    {
        if ( pInfo->iProneTrans + pInfo->iProneTime < iCurrentTime )
            pInfo->iProneTrans = 0;
    }
    else if ( pInfo->iProneTime - pInfo->iProneTrans < iCurrentTime )
    {
        pInfo->prone = 0;
        return 0;
    }
    return 1;
}

double __cdecl BG_GetActorProneFraction(actor_prone_info_s *pInfo, int iCurrentTime)
{
    if ( !pInfo->prone )
        return 0.0;
    if ( !pInfo->iProneTrans )
        return 1.0;
    if ( pInfo->iProneTrans >= 0 )
    {
        if ( pInfo->iProneTrans + pInfo->iProneTime >= iCurrentTime )
        {
            return (double)(iCurrentTime - pInfo->iProneTime) / (double)pInfo->iProneTrans;
        }
        else
        {
            pInfo->iProneTrans = 0;
            return 1.0;
        }
    }
    else if ( pInfo->iProneTime - pInfo->iProneTrans >= iCurrentTime )
    {
        return 1.0 - (double)(iCurrentTime - pInfo->iProneTime) / (double)-pInfo->iProneTrans;
    }
    else
    {
        pInfo->prone = 0;
        return 0.0;
    }
}

#endif
