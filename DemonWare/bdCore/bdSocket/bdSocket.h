#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdAddr.h>
#include <cstddef>

struct bdSocket
{
    bdSocket();
    virtual ~bdSocket();
    virtual bool create(bool blocking, bool broadcast, bool option);
    virtual int bind(const bdAddr &address);
    virtual int bind(unsigned short port);
    virtual int sendTo(const bdAddr &address, const void *data, unsigned int size);
    virtual int receiveFrom(bdAddr &address, void *data, unsigned int size);
    virtual bool close();
    static void *operator new(std::size_t size);
    static void operator delete(void *memory);
    static void *operator new(std::size_t, void *memory) { return memory; }
    static void operator delete(void *, void *) {}
    int m_handle;
};
static_assert(sizeof(void *) != 4 || sizeof(bdSocket) == 8);
#endif
