#if defined(KISAK_SP) && !defined(KISAK_DW)
#pragma once
#include <DemonWare/bdCore/bdSingleton.h>

class bdSingletonRegistryImpl
{
public:
    using Destructor = void (__cdecl *)();
    bdSingletonRegistryImpl();
    virtual ~bdSingletonRegistryImpl();
    static void operator delete(void *memory);
    bool addDestructor(Destructor destructor);
private:
    Destructor *m_destructors;
    unsigned int m_capacity;
    unsigned int m_size;
    bool m_destroying;
};

template <> bdSingletonRegistryImpl *bdSingleton<bdSingletonRegistryImpl>::getInstance();
template <> void bdSingleton<bdSingletonRegistryImpl>::destroyInstance();
static_assert(sizeof(void *) != 4 || sizeof(bdSingletonRegistryImpl) == 0x14);
#endif
