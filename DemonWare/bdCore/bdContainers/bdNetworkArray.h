#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdContainers/bdArray.h>
#include <DemonWare/bdCore/bdContainers/bdString.h>
#include <new>

struct bdNetworkArrayCopyOrder {};

template<typename T>
struct bdNetworkArray
{
    T *m_data;
    unsigned int m_capacity;
    unsigned int m_size;

    bdNetworkArray() : m_data(NULL), m_capacity(0), m_size(0) {}
    ~bdNetworkArray() { clear(); }
    bdNetworkArray(const bdNetworkArray &other)
        : m_data(uninitializedCopy(other)), m_capacity(other.m_capacity), m_size(other.m_size) {}

    bdNetworkArray(const bdNetworkArray &other, bdNetworkArrayCopyOrder)
    {
        m_capacity = other.m_capacity;
        m_size = other.m_size;
        m_data = uninitializedCopy(other);
    }

    void clear()
    {
        for (unsigned int i = 0; i < m_size; ++i)
            m_data[i].~T();
        bdMemory::deallocate(m_data);
        m_data = NULL;
        m_capacity = 0;
        m_size = 0;
    }

    void increaseCapacity(unsigned int increase)
    {
        const unsigned int capacity = m_capacity + (increase <= m_capacity ? m_capacity : increase);
        T *data = NULL;
        if (capacity)
        {
            data = static_cast<T *>(bdMemory::allocate(sizeof(T) * capacity));
            for (unsigned int i = 0; i < m_size; ++i)
                if (data + i)
                    new (data + i) T(m_data[i]);
        }
        for (unsigned int i = 0; i < m_size; ++i)
            m_data[i].~T();
        bdMemory::deallocate(m_data);
        m_data = data;
        m_capacity = capacity;
    }

    void decreaseCapacity(unsigned int decrease)
    {
        if (m_capacity <= 4u * m_size)
            return;
        const unsigned int available = m_capacity - m_size;
        const unsigned int reduce = decrease <= available ? decrease : available;
        m_capacity -= reduce <= (m_capacity >> 1) ? (m_capacity >> 1) : reduce;
        T *data = NULL;
        if (m_capacity)
        {
            data = static_cast<T *>(bdMemory::allocate(sizeof(T) * m_capacity));
            for (unsigned int i = 0; i < m_size; ++i)
                if (data + i)
                    new (data + i) T(m_data[i]);
        }
        for (unsigned int i = 0; i < m_size; ++i)
            m_data[i].~T();
        bdMemory::deallocate(m_data);
        m_data = data;
    }

    static T *uninitializedCopy(const bdNetworkArray &other)
    {
        T *data = NULL;
        if (other.m_capacity)
        {
            data = static_cast<T *>(bdMemory::allocate(sizeof(T) * other.m_capacity));
            for (unsigned int i = 0; i < other.m_size; ++i)
                if (data + i)
                    new (data + i) T(other.m_data[i]);
        }
        return data;
    }

    bdNetworkArray &operator=(const bdNetworkArray &other)
    {
        if (this == &other)
            return *this;
        if (m_capacity < other.m_size)
        {
            clear();
            m_data = uninitializedCopy(other);
            m_size = other.m_size;
            m_capacity = other.m_capacity;
        }
        else if (m_size < other.m_size)
        {
            unsigned int i = 0;
            for (; i < m_size; ++i)
                m_data[i] = other.m_data[i];
            for (; i < other.m_size; ++i)
                if (m_data + i)
                    new (m_data + i) T(other.m_data[i]);
            m_size = other.m_size;
        }
        else
        {
            for (unsigned int i = 0; i < other.m_size; ++i)
                m_data[i] = other.m_data[i];
            for (unsigned int i = other.m_size; i < m_size; ++i)
                m_data[i].~T();
            m_size = other.m_size;
            decreaseCapacity(0);
        }
        return *this;
    }

    void pushBack(const T *value)
    {
        if (m_size == m_capacity)
            increaseCapacity(1);
        T *place = m_data + m_size;
        if (place)
            new (place) T(*value);
        ++m_size;
    }
};

template<> struct bdArray<bdInetAddr> : bdNetworkArray<bdInetAddr> {};
template<> struct bdArray<bdString> : bdNetworkArray<bdString> {};
static_assert(sizeof(void *) != 4 || sizeof(bdArray<bdInetAddr>) == 12);
static_assert(sizeof(void *) != 4 || sizeof(bdArray<bdString>) == 12);
#endif
