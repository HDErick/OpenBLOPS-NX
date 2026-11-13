#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdMemory/bdMemory.h>
#include <DemonWare/bdPlatform/bdPlatformLog/bdPlatformLog.h>
#include <cstdint>
#include <cstring>
#include <new>

struct bdSecurityID;
struct bdSecurityKey;
struct bdEndpoint;
struct bdAddrHandle;
class bdDTLSAssociation;
class bdNATTravClientData;
template <typename T> struct bdReference;

struct bdHashingClass
{
    unsigned int hash(const bdSecurityID &key) const;

    unsigned int hash(unsigned int key) const
    {
        unsigned int value = 0;
        for (unsigned int i = 0; i < 4; ++i)
            value = 31 * value + ((key >> (8 * i)) & 255);
        return value;
    }
};
struct bdEndpointHashingClass
{
    unsigned int hash(const bdEndpoint &key) const;
};

template <typename Key, typename Value, typename Hashing> struct bdHashMapName;

template <> struct bdHashMapName<bdSecurityID, bdSecurityKey, bdHashingClass>
{
    static const char *createMap()
    {
        return "bdHashMap<class bdSecurityID,class bdSecurityKey,class bdHashingClass>::createMap";
    }
};

template <> struct bdHashMapName<bdEndpoint, bdDTLSAssociation *, bdEndpointHashingClass>
{
    static const char *createMap()
    {
        return "bdHashMap<class bdEndpoint,class bdDTLSAssociation *,class bdEndpointHashingClass>::createMap";
    }
};

template <> struct bdHashMapName<bdEndpoint, bdReference<bdAddrHandle>, bdEndpointHashingClass>
{
    static const char *createMap()
    {
        return "bdHashMap<class bdEndpoint,class bdReference<class bdAddrHandle>,class bdEndpointHashingClass>::createMap";
    }
};

template <> struct bdHashMapName<unsigned int, bdNATTravClientData, bdHashingClass>
{
    static const char *createMap()
    {
        return "bdHashMap<unsigned int,class bdNATTravClientData,class bdHashingClass>::createMap";
    }
};

template <typename Key, typename Value, typename Hashing>
struct bdHashMap
{
    struct Node
    {
        Value m_value;
        Key m_key;
        Node *m_next;
    };

    unsigned int m_size;
    unsigned int m_capacity;
    float m_loadFactor;
    unsigned int m_threshold;
    Node **m_map;
    Hashing m_hashingClass;

    bdHashMap(unsigned int capacity, float loadFactor)
    {
        if (loadFactor <= 0.0f || loadFactor > 1.0f)
        {
            bdLogMessage(BD_LOG_WARNING, "warn/", "hashmap", "..\\bdCore/bdContainers/bdHashMap.inl",
                bdHashMapName<Key, Value, Hashing>::createMap(), 479,
                "Illegal loadFactor. Using default value.");
            m_loadFactor = 0.75f;
        }
        m_size = 0;
        unsigned int bits = capacity;
        bits |= bits >> 1;
        bits |= bits >> 2;
        bits |= bits >> 4;
        bits |= bits >> 8;
        bits |= bits >> 16;
        m_capacity = bits & ~(bits >> 1);
        if (m_capacity != capacity)
            m_capacity <<= 1;
        m_loadFactor = loadFactor;
        const double threshold = static_cast<double>(m_capacity) * m_loadFactor;
        m_threshold = threshold >= -9223372036854775808.0 && threshold < 9223372036854775808.0
            ? static_cast<unsigned int>(static_cast<std::int64_t>(threshold)) : 0;
        m_map = static_cast<Node **>(bdMemory::allocate(m_capacity * sizeof(Node *)));
        memset(m_map, 0, m_capacity * sizeof(Node *));
    }

    ~bdHashMap()
    {
        clear();
        bdMemory::deallocate(m_map);
    }

    void clear()
    {
        for (unsigned int i = 0; i < m_capacity; ++i)
        {
            Node *node = m_map[i];
            while (node)
            {
                Node *old = node;
                node = node->m_next;
                old->~Node();
                bdMemory::deallocate(old);
            }
            m_map[i] = nullptr;
        }
        m_size = 0;
    }

    Node *getIterator() const
    {
        if (!m_size)
            return nullptr;
        unsigned int i = 0;
        while (i < m_capacity && !m_map[i])
            ++i;
        return m_map[i];
    }

    Node *getIterator(const Key &key) const
    {
        if (!m_size)
            return nullptr;
        const unsigned int index = (m_capacity - 1) & m_hashingClass.hash(key);
        for (Node *node = m_map[index]; node; node = node->m_next)
            if (key == node->m_key)
                return node;
        return nullptr;
    }

    bool get(const Key &key, Value *value) const
    {
        Node *node = getIterator(key);
        if (!node)
            return false;
        *value = node->m_value;
        return true;
    }

    bool put(const Key &key, const Value &value)
    {
        const unsigned int hash = m_hashingClass.hash(key);
        unsigned int index = (m_capacity - 1) & hash;
        for (Node *node = m_map[index]; node; node = node->m_next)
            if (node->m_key == key)
                return false;
        if (m_threshold < m_size + 1)
        {
            resize(m_capacity << 1);
            index = (m_capacity - 1) & hash;
        }
        ++m_size;
        void *memory = bdMemory::allocate(sizeof(Node));
        Node *node = memory ? new (memory) Node{value, key, m_map[index]} : nullptr;
        m_map[index] = node;
        return true;
    }

    void resize(unsigned int capacity)
    {
        unsigned int bits = capacity;
        bits |= bits >> 1;
        bits |= bits >> 2;
        bits |= bits >> 4;
        bits |= bits >> 8;
        bits |= bits >> 16;
        unsigned int next = bits & ~(bits >> 1);
        if (next != capacity)
            next <<= 1;
        if (next <= m_capacity)
            return;
        Node **oldMap = m_map;
        const unsigned int oldCapacity = m_capacity;
        m_capacity = next;
        const double threshold = static_cast<double>(m_capacity) * m_loadFactor;
        m_threshold = threshold >= -9223372036854775808.0 && threshold < 9223372036854775808.0
            ? static_cast<unsigned int>(static_cast<std::int64_t>(threshold)) : 0;
        m_map = static_cast<Node **>(bdMemory::allocate(m_capacity * sizeof(Node *)));
        m_size = 0;
        memset(m_map, 0, m_capacity * sizeof(Node *));
        for (unsigned int i = 0; i < oldCapacity; ++i)
        {
            Node *node = oldMap[i];
            while (node)
            {
                put(node->m_key, node->m_value);
                Node *old = node;
                node = node->m_next;
                old->~Node();
                bdMemory::deallocate(old);
            }
        }
        bdMemory::deallocate(oldMap);
    }

    void getNext(Node **iterator) const
    {
        Node *node = *iterator;
        if (node->m_next)
        {
            *iterator = node->m_next;
            return;
        }
        unsigned int i = ((m_capacity - 1) & m_hashingClass.hash(node->m_key)) + 1;
        while (i < m_capacity)
        {
            if (m_map[i])
            {
                *iterator = m_map[i];
                return;
            }
            ++i;
        }
        *iterator = nullptr;
    }

    bool remove(const Key &key)
    {
        const unsigned int index = (m_capacity - 1) & m_hashingClass.hash(key);
        Node *previous = nullptr;
        for (Node *node = m_map[index]; node; node = node->m_next)
        {
            if (key == node->m_key)
            {
                if (previous)
                    previous->m_next = node->m_next;
                else
                    m_map[index] = node->m_next;
                node->~Node();
                bdMemory::deallocate(node);
                --m_size;
                return true;
            }
            previous = node;
        }
        return false;
    }

    bool removeIterator(Node **iterator)
    {
        Node *next = *iterator;
        getNext(&next);
        Node *old = *iterator;
        *iterator = next;
        return remove(old->m_key);
    }

    static void releaseIterator(Node *) {}
};
#endif
