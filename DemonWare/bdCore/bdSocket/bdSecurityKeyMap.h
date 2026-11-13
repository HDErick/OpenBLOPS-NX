#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdAddr.h>
#include <DemonWare/bdCore/bdContainers/bdHashMap.h>

struct bdSecurityKeyMapListener;

struct bdSecurityKeyMap
{
    bdSecurityKeyMapListener *m_listener;
    bdHashMap<bdSecurityID, bdSecurityKey, bdHashingClass> m_map;

    bool get(const bdSecurityID &id, bdSecurityKey *key) const;

    bdSecurityKeyMap() : m_map(4, 0.75f)
    {
        m_listener = nullptr;
    }

    void setListener(bdSecurityKeyMapListener *listener)
    {
        m_listener = listener;
    }

    void removeListener()
    {
        m_listener = nullptr;
    }
};
static_assert(sizeof(void *) != 4 || sizeof(bdSecurityKeyMap) == 0x1c);
#endif
