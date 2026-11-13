#pragma once

#include <Windows.h>

template <typename T>
struct bdReference//<bdByteBuffer> // sizeof=0x4
{                                                                             // XREF: bdRemoteTask/r
    T *m_ptr;

    const bdReference<T> *operator=(const bdReference<T> &other)
    {
        if (&other != this)
        {
            if (m_ptr)
            {
                T *oldPtr = m_ptr;

                if (InterlockedDecrement(&m_ptr->m_refCount) == 0)
                {
                    // Demonware-style explicit destructor call
                    oldPtr->~T();
                }
            }

            m_ptr = other.m_ptr;

            if (m_ptr)
            {
                InterlockedIncrement(&m_ptr->m_refCount);
            }
        }

        return this;
    }

    bdReference &operator=(T *ptr)
    {
        if (m_ptr != ptr)
        {
            if (m_ptr)
            {
                T *old = m_ptr;
                InterlockedDecrement(&old->m_refCount);
                if (!old->m_refCount)
                {
                    old->~T();
                }
            }

            m_ptr = ptr;

            if (m_ptr)
                InterlockedIncrement(&m_ptr->m_refCount);
        }
        return *this;
    }
};

struct bdReferencable // sizeof=0x8
{                                                                             // XREF: bdByteBuffer/r
    volatile unsigned int m_refCount;

#if defined(KISAK_SP) && !defined(KISAK_DW)
    bdReferencable() = default;
protected:
    explicit bdReferencable(unsigned int references) : m_refCount(references) {}
public:
#endif
#if defined(KISAK_SP) && !defined(KISAK_DW)
    virtual ~bdReferencable() {}
#else
    virtual ~bdReferencable() = default;
#endif
};
#if defined(KISAK_SP) && !defined(KISAK_DW)
struct bdByteBuffer;
template <> struct bdReference<bdByteBuffer>
{
    bdByteBuffer *m_ptr;
    bdReference() : m_ptr(NULL) {}
    explicit bdReference(bdByteBuffer *pointer);
    bdReference(const bdReference &other);
    ~bdReference();
    const bdReference *operator=(const bdReference &other);
};
static_assert(sizeof(void *) != 4 || sizeof(bdReference<bdByteBuffer>) == 4);
struct bdCommonAddr;
template <> struct bdReference<bdCommonAddr>
{
    bdCommonAddr *m_ptr;
    bdReference() : m_ptr(NULL) {}
    explicit bdReference(bdCommonAddr *pointer);
    bdReference(const bdReference &other);
    ~bdReference();
    const bdReference *operator=(const bdReference &other);
};
static_assert(sizeof(void *) != 4 || sizeof(bdReference<bdCommonAddr>) == 4);
struct bdAddrHandle;
template <> struct bdReference<bdAddrHandle>
{
    bdAddrHandle *m_ptr;
    bdReference() : m_ptr(NULL) {}
    explicit bdReference(bdAddrHandle *pointer);
    bdReference(const bdReference &other);
    ~bdReference();
    const bdReference *operator=(const bdReference &other);
    bdReference &operator=(bdAddrHandle *pointer);
};
static_assert(sizeof(void *) != 4 || sizeof(bdReference<bdAddrHandle>) == 4);
struct bdRemoteTask;
template <> struct bdReference<bdRemoteTask>
{
    bdRemoteTask *m_ptr;
    bdReference() : m_ptr(NULL) {}
    explicit bdReference(bdRemoteTask *pointer);
    bdReference(const bdReference &other);
    ~bdReference();
    const bdReference *operator=(const bdReference &other);
    bdReference &operator=(bdRemoteTask *pointer);
};
static_assert(sizeof(void *) != 4 || sizeof(bdReference<bdRemoteTask>) == 4);
#endif
