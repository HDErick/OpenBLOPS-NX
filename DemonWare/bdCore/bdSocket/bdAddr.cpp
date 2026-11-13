#include "bdAddr.h"
#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DW/dwNet.h>
#endif

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdNetStartParams.h>
#include <DemonWare/bdCore/bdSocket/bdSocket.h>
#include <DemonWare/bdCore/bdSocket/bdSecurityKeyMap.h>
#include <DemonWare/bdPlatform/bdPlatformLog/bdPlatformLog.h>
#include <cstdint>
#include <cstring>
#include <new>

bdString::Storage bdString::s_empty = { 1, 0, 0, { 0 } };

bdString::Storage *bdString::storage() const
{
    return reinterpret_cast<Storage *>(m_string - offsetof(Storage, text));
}

bdString::bdString(const char *value)
{
    const unsigned int length = static_cast<unsigned int>(std::strlen(value));
    if (length)
    {
        const unsigned int capacity = ((length + 1u) >> 6) + (((length + 1u) & 63u) != 0);
        Storage *owned = static_cast<Storage *>(bdMemory::allocate((capacity << 6) + 12u));
        new (owned) Storage;
        owned->references = 1;
        owned->length = length;
        owned->capacity = capacity << 6;
        m_string = owned->text;
        std::memcpy(m_string, value, length + 1u);
    }
    else
    {
        s_empty.text[0] = 0;
        m_string = s_empty.text;
        ++s_empty.references;
    }
}

bdString::bdString(const bdString &other) : m_string(other.m_string)
{
    ++storage()->references;
}

void bdString::release()
{
    Storage *owned = storage();
    if (!--owned->references)
        bdMemory::deallocate(owned);
}

bdString::~bdString() { release(); }

bdString &bdString::operator=(const bdString &other)
{
    if (m_string != other.m_string)
    {
        release();
        m_string = other.m_string;
        ++storage()->references;
    }
    return *this;
}

bdNetStartParams::bdNetStartParams()
    : m_onlineGame(false), m_gamePort(3074), m_socket(NULL), m_natTravHosts(), m_localAddresses(),
      m_hostNameLookupConfig(), m_UPnPConfig(), m_useAnyIP(true)
{
    { const bdString host("stun.us.demonware.net"); m_natTravHosts.pushBack(&host); }
    { const bdString host("stun.eu.demonware.net"); m_natTravHosts.pushBack(&host); }
    { const bdString host("stun.jp.demonware.net"); m_natTravHosts.pushBack(&host); }
    { const bdString host("stun.au.demonware.net"); m_natTravHosts.pushBack(&host); }
    m_natTravPort = 3074;
}

bdNetStartParams &bdNetStartParams::operator=(const bdNetStartParams &other)
{
    m_onlineGame = other.m_onlineGame;
    m_gamePort = other.m_gamePort;
    m_socket = other.m_socket;
    m_natTravHosts = other.m_natTravHosts;
    m_natTravPort = other.m_natTravPort;
    m_localAddresses = other.m_localAddresses;
    m_hostNameLookupConfig = other.m_hostNameLookupConfig;
    m_UPnPConfig = other.m_UPnPConfig;
    m_useAnyIP = other.m_useAnyIP;
    return *this;
}

void *bdCommonAddr::operator new(std::size_t size) { return bdMemory::allocate(static_cast<unsigned int>(size)); }
void bdCommonAddr::operator delete(void *memory) { bdMemory::deallocate(memory); }

bdReference<bdCommonAddr>::bdReference(bdCommonAddr *pointer) : m_ptr(pointer)
{
    if (m_ptr)
        InterlockedIncrement(reinterpret_cast<volatile LONG *>(&m_ptr->m_refCount));
}

bdReference<bdCommonAddr>::bdReference(const bdReference &other) : bdReference(other.m_ptr) {}

bdReference<bdCommonAddr>::~bdReference()
{
    if (m_ptr && !InterlockedDecrement(reinterpret_cast<volatile LONG *>(&m_ptr->m_refCount)))
    {
        delete m_ptr;
        m_ptr = NULL;
    }
}

const bdReference<bdCommonAddr> *bdReference<bdCommonAddr>::operator=(const bdReference &other)
{
    if (this != &other)
    {
        if (m_ptr && !InterlockedDecrement(reinterpret_cast<volatile LONG *>(&m_ptr->m_refCount)))
            delete m_ptr;
        m_ptr = other.m_ptr;
        if (m_ptr)
            InterlockedIncrement(reinterpret_cast<volatile LONG *>(&m_ptr->m_refCount));
    }
    return this;
}

bdHashTiger192::bdHashTiger192()
{
    if (register_hash(&tiger_desc) == -1)
        bdLogMessage(BD_LOG_ERROR, "err/", "hashtiger192", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdCore\\bdCrypto\\bdHashTiger192.cpp",
            "bdHashTiger192::bdHashTiger192", 20, "Unable to register hash.");
}

void bdHashTiger192::operator delete(void *memory) { bdMemory::deallocate(memory); }

bool bdHashTiger192::hash(const void *data, unsigned int length, unsigned char *out, unsigned int *outLength)
{
    if (*outLength > HASH_SIZE)
        bdLogMessage(BD_LOG_WARNING, "warn/", "hashtiger192", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdCore\\bdCrypto\\bdHashTiger192.cpp",
            "bdHashTiger192::hash", 33, "Tiger hash only requires result buffer of 24 bytes. Remaineder of buffer will not be used");
    unsigned long size = *outLength;
    const int error = hash_memory(find_hash("tiger"), static_cast<const unsigned char *>(data), length, out, &size);
    if (error == CRYPT_BUFFER_OVERFLOW)
        bdLogMessage(BD_LOG_ERROR, "err/", "hashtiger192", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdCore\\bdCrypto\\bdHashTiger192.cpp",
            "bdHashTiger192::hash", 41, "Unable to create tiger hash of less than 24 bytes in length!.");
    else if (error != CRYPT_OK)
        bdLogMessage(BD_LOG_ERROR, "err/", "hashtiger192", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdCore\\bdCrypto\\bdHashTiger192.cpp",
            "bdHashTiger192::hash", 46, "Unable to create hash.");
    else
    {
        *outLength = size;
        return true;
    }
    return false;
}

namespace
{
std::uint64_t s_bytesSent;
std::uint64_t s_bytesReceived;
std::uint64_t s_packetsSent;
std::uint64_t s_packetsReceived;
}

bdSocket::bdSocket() : m_handle(-1) {}
bdSocket::~bdSocket() { close(); }
void *bdSocket::operator new(std::size_t size) { return bdMemory::allocate(static_cast<unsigned int>(size)); }
void bdSocket::operator delete(void *memory) { bdMemory::deallocate(memory); }

bool bdSocket::create(bool blocking, bool broadcast, bool option)
{
    if (m_handle != -1)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "socket", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdCore\\bdSocket\\bdSocket.cpp",
            "bdSocket::create", 55, "bdSocket::create(), already created.");
        return false;
    }
    int handle = static_cast<int>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (broadcast)
    {
        const int enabled = 1;
        if (::setsockopt(handle, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char *>(&enabled), 4))
            handle = -1;
        else
        {
            unsigned long nonblocking = !blocking;
            if (::ioctlsocket(handle, FIONBIO, &nonblocking))
                handle = -1;
        }
    }
    else
    {
        unsigned long nonblocking = !blocking;
        if (::ioctlsocket(handle, FIONBIO, &nonblocking))
            handle = -1;
    }
    if (handle < 0)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "socket", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdCore\\bdSocket\\bdSocket.cpp",
            "bdSocket::create", 45, "Failed to create socket. Err: %i", handle);
        return false;
    }
    m_handle = handle;
    return true;
}

int bdSocket::bind(const bdAddr &address)
{
    if (m_handle == -1)
        return -10;
    sockaddr_in local = {};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = address.m_address.getInAddr().inUn.m_iaddr;
    local.sin_port = htons(address.getPort());
    if (::bind(m_handle, reinterpret_cast<const sockaddr *>(&local), sizeof(local)) == -1)
    {
        const int error = WSAGetLastError();
        ::closesocket(m_handle);
        return error == WSAEACCES || error == WSAEADDRINUSE || error == WSAEADDRNOTAVAIL ? -4 : -1;
    }
    return 1;
}

int bdSocket::bind(unsigned short port)
{
    const bdInetAddr any = bdInetAddr::Any();
    const bdAddr address(&any, port);
    return bind(address);
}

int bdSocket::sendTo(const bdAddr &address, const void *data, unsigned int size)
{
    if (m_handle == -1)
        return -10;
    sockaddr_in remote = {};
    remote.sin_family = AF_INET;
    remote.sin_addr.s_addr = address.m_address.getInAddr().inUn.m_iaddr;
    remote.sin_port = htons(address.getPort());
    const int result = ::sendto(m_handle, static_cast<const char *>(data), size, 0,
        reinterpret_cast<const sockaddr *>(&remote), sizeof(remote));
    if (result < 0)
    {
        const int error = WSAGetLastError();
        switch (error)
        {
        case WSAEINTR: return -8;
        case WSAEWOULDBLOCK: return -2;
        case WSAEINVAL: case WSAEDESTADDRREQ: case WSAEAFNOSUPPORT: case WSAEADDRNOTAVAIL: return -7;
        case WSAEMSGSIZE: return -6;
        case WSAENETDOWN: case WSAENETUNREACH: case WSAECONNABORTED: case WSAECONNRESET:
        case WSAETIMEDOUT: case WSAEHOSTUNREACH:
            bdLogMessage(BD_LOG_INFO, "info/", "platform/socket", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdPlatform\\bdPlatformSocket\\bdPlatformSocket-win32.cpp",
                "bdPlatformSocket::sendTo", 190, "send: Received connection reset : %i", error);
            return -5;
        default: return -1;
        }
    }
    const std::int32_t bytes = static_cast<std::int32_t>(static_cast<std::uint32_t>(result) + 28u);
    s_bytesSent += static_cast<std::int64_t>(bytes);
    ++s_packetsSent;
    return result;
}

int bdSocket::receiveFrom(bdAddr &address, void *data, unsigned int size)
{
    if (m_handle == -1)
        return -10;
    sockaddr_in remote = {};
    int addressSize = sizeof(remote);
    const int result = ::recvfrom(m_handle, static_cast<char *>(data), size, 0,
        reinterpret_cast<sockaddr *>(&remote), &addressSize);
    if (result < 0)
    {
        const int error = WSAGetLastError();
        switch (error)
        {
        case WSAEWOULDBLOCK: return -2;
        case WSAEMSGSIZE: return -6;
        case WSAEINTR: return -8;
        case WSAEINVAL: return -9;
        case WSAENETDOWN: case WSAENETUNREACH: case WSAECONNABORTED: case WSAECONNRESET:
        case WSAETIMEDOUT: case WSAEHOSTUNREACH:
        {
            bdLogMessage(BD_LOG_INFO, "info/", "platform/socket", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdPlatform\\bdPlatformSocket\\bdPlatformSocket-win32.cpp",
                "bdPlatformSocket::receiveFrom", 247, "recv: Received connection reset : %i", error);
            const bdInetAddr source(remote.sin_addr.s_addr);
            address.set(&source, ntohs(remote.sin_port));
            return -5;
        }
        default: return -1;
        }
    }
    const bdInetAddr source(remote.sin_addr.s_addr);
    address.set(&source, ntohs(remote.sin_port));
    const std::int32_t bytes = static_cast<std::int32_t>(static_cast<std::uint32_t>(result) + 28u);
    s_bytesReceived += static_cast<std::int64_t>(bytes);
    ++s_packetsReceived;
    return result;
}

bool bdSocket::close()
{
    bool result = true;
    if (m_handle != -1)
    {
        result = ::closesocket(m_handle) == 0;
        m_handle = -1;
    }
    return result;
}
#endif


int bdInAddr::toString(char *const dst, unsigned int size) const
{
#if defined(KISAK_SP) && !defined(KISAK_DW)
    in_addr address;
    address.s_addr = inUn.m_iaddr;
    const char *source = inet_ntoa(address);
    if (!source)
        return 0;
    if (size && dst)
    {
        const unsigned int sourceLength = static_cast<unsigned int>(std::strlen(source));
        const unsigned int length = sourceLength < size - 1 ? sourceLength : size - 1;
        std::memcpy(dst, source, length);
        dst[length] = 0;
    }
    return static_cast<int>(std::strlen(dst));
#else
    return 0;
#endif
#if 0
    if (!dst || size == 0)
        return 0;

    in_addr addr;
    addr.s_addr = inUn.m_iaddr;

    const char *src = inet_ntoa(addr);
    if (!src)
        return 0;

    bdStrlcpy(dst, (char *)src, size);

    // Decompiled code calls strlen and returns its result
    return static_cast<int>(std::strlen(dst));
#endif
}


unsigned int bdAddr::toString(char *const dst, unsigned int capacity)
{
#if defined(KISAK_SP) && !defined(KISAK_DW)
    const unsigned int written = m_address.toString(dst, capacity);
    const unsigned int remaining = written <= capacity ? capacity - written : 0;
    return written + bdSnprintf(dst + written, remaining, ":%u", m_port);
#else
    return 0;
#if 0
    //unsigned int written = bdInetAddr::toString(&m_address, dst, capacity);
    unsigned int written = m_address.toString(dst, capacity);

    unsigned int remaining = (written <= capacity) ? (capacity - written) : 0;

    return written +
        bdSnprintf(&dst[written], remaining, ":%u", m_port);
#endif
#endif
}

#if defined(KISAK_SP) && !defined(KISAK_DW)
int bdCommonAddrToString(const bdCommonAddr *addr, char *dst, unsigned int capacity)
{
    int written;
    if (addr->m_publicAddr.m_address.isValid())
    {
        char publicAddr[22];
        const_cast<bdAddr &>(addr->m_publicAddr).toString(publicAddr, sizeof(publicAddr));
        written = bdSnprintf(dst, capacity, "Public Addr: %s\n", publicAddr);
    }
    else if (addr->m_localAddrs.m_size)
    {
        char localAddrs[72];
        char *next = localAddrs;
        for (unsigned int i = 0; i < addr->m_localAddrs.m_size; ++i)
        {
            next += addr->m_localAddrs.m_data[i].toString(next,
                static_cast<unsigned int>(sizeof(localAddrs) - (next - localAddrs)));
            next += bdSnprintf(next,
                static_cast<unsigned int>(sizeof(localAddrs) - (next - localAddrs)), ", ");
        }
        next -= 2;
        *next = '\0';
        written = bdSnprintf(dst, capacity, "Local Addr: %s\n", localAddrs);
    }
    else
        written = bdSnprintf(dst, capacity, "Empty Common Addr\n");
    if (written > 0 && static_cast<unsigned int>(written) <= capacity)
        return written;
    if (capacity)
        dst[capacity - 1] = '\0';
    return static_cast<int>(capacity - 1);
}

int bdCommonAddrToString(bdReference<bdCommonAddr> addr, char *dst, unsigned int capacity)
{
    return addr.m_ptr ? bdCommonAddrToString(addr.m_ptr, dst, capacity) : 0;
}
#endif

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdAddressMap.h>
#include <DemonWare/bdCore/bdContainers/bdHashMap.h>

unsigned int bdHashingClass::hash(const bdSecurityID &key) const
{
    unsigned int hash = 0;
    for (unsigned int i = 0; i < sizeof(key.ab); ++i)
        hash = 31 * hash + key.ab[i];
    return hash;
}

static unsigned int BD_SecurityBytesToString(const unsigned char *bytes, unsigned int length,
    char *output, unsigned int capacity)
{
    unsigned int written = 0;
    for (unsigned int i = 0; i < length; ++i)
    {
        if (i && i % 4 == 0)
            written += bdSnprintf(output + written, capacity - written, " ");
        if (bdSnprintf(output + written, capacity - written, "%02hhx", bytes[i]) != 2)
            return 0;
        written += 2;
    }
    return written;
}

unsigned int bdSecurityIDToString(const bdSecurityID &id, char *output, unsigned int capacity)
{
    return BD_SecurityBytesToString(id.ab, sizeof(id.ab), output, capacity);
}

unsigned int bdSecurityKeyToString(const bdSecurityKey &key, char *output, unsigned int capacity)
{
    return BD_SecurityBytesToString(key.ab, sizeof(key.ab), output, capacity);
}

bool bdSecurityKeyMap::get(const bdSecurityID &id, bdSecurityKey *key) const
{
    const bool found = m_map.get(id, key);
    char description[36];
    bdSecurityIDToString(id, description, sizeof(description));
    const char *source = "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdSecurityKeyMap.cpp";
    bdLogMessage(BD_LOG_INFO, "info/", "bdCore/bdSocket/bdSecurityKeyMap", source,
        "bdSecurityKeyMap::get", 80, "Searching for bdSecurityID: %s", description);
    if (found)
    {
        bdSecurityKeyToString(*key, description, sizeof(description));
        bdLogMessage(BD_LOG_INFO, "info/", "bdCore/bdSocket/bdSecurityKeyMap", source,
            "bdSecurityKeyMap::get", 85, "Found bdSecurityKey: %s", description);
    }
    else
        bdLogMessage(BD_LOG_INFO, "info/", "bdCore/bdSocket/bdSecurityKeyMap", source,
            "bdSecurityKeyMap::get", 89, "bdSecurityID not present");
    return found;
}

bdEndpoint::bdEndpoint(bdReference<bdCommonAddr> addr, const bdSecurityID &id)
    : m_ca(addr), m_secID(&id) {}

bdReference<bdCommonAddr> bdEndpoint::getCommonAddr() const
{
    return bdReference<bdCommonAddr>(m_ca);
}

const bdSecurityID &bdEndpoint::getSecID() const { return m_secID; }

bool bdEndpoint::operator==(const bdEndpoint &other) const
{
    if (m_ca.m_ptr && other.m_ca.m_ptr)
        return m_ca.m_ptr->getHash() == other.m_ca.m_ptr->getHash() && m_secID == other.m_secID;
    return !m_ca.m_ptr && !other.m_ca.m_ptr;
}

unsigned int bdEndpointHashingClass::hash(const bdEndpoint &key) const
{
    unsigned int hash = 0;
    bool hasCommon;
    {
        bdReference<bdCommonAddr> addr = key.getCommonAddr();
        hasCommon = addr.m_ptr != nullptr;
    }
    if (hasCommon)
    {
        {
            bdReference<bdCommonAddr> addr = key.getCommonAddr();
            hash = addr.m_ptr->getHash();
        }
        const bdSecurityID &id = key.getSecID();
        unsigned int securityHash = 0;
        for (unsigned int i = 0; i < sizeof(id.ab); ++i)
            securityHash = 31 * securityHash + id.ab[i];
        hash += securityHash;
    }
    return hash;
}

bdAddrHandle::bdAddrHandle(bdReference<bdCommonAddr> addr, const bdSecurityID &id)
    : bdReferencable(0), m_endpoint(addr, id), m_status(BD_ADDR_NOT_RESOLVED), m_realAddr() {}

void *bdAddrHandle::operator new(std::size_t size) { return bdMemory::allocate(static_cast<unsigned int>(size)); }
void bdAddrHandle::operator delete(void *memory) { bdMemory::deallocate(memory); }

bool bdAddrHandle::operator==(const bdAddrHandle &other) const { return m_endpoint == other.m_endpoint; }
bdAddr bdAddrHandle::getRealAddr() const { return bdAddr(&m_realAddr); }
bdAddrHandle::bdAddrHandleStatus bdAddrHandle::getStatus() const { return m_status; }
void bdAddrHandle::setRealAddr(const bdAddr &addr) { m_realAddr = addr; m_status = BD_ADDR_RESOLVED; }
void bdAddrHandle::setStatus(const bdAddrHandleStatus &status) { m_status = status; }

bdReference<bdAddrHandle>::bdReference(bdAddrHandle *pointer) : m_ptr(pointer)
{
    if (m_ptr)
        InterlockedIncrement(reinterpret_cast<volatile LONG *>(&m_ptr->m_refCount));
}

bdReference<bdAddrHandle>::bdReference(const bdReference &other) : bdReference(other.m_ptr) {}

bdReference<bdAddrHandle>::~bdReference()
{
    if (m_ptr && !InterlockedDecrement(reinterpret_cast<volatile LONG *>(&m_ptr->m_refCount)))
    {
        delete m_ptr;
        m_ptr = NULL;
    }
}

const bdReference<bdAddrHandle> *bdReference<bdAddrHandle>::operator=(const bdReference &other)
{
    if (this != &other)
    {
        if (m_ptr && !InterlockedDecrement(reinterpret_cast<volatile LONG *>(&m_ptr->m_refCount)))
            delete m_ptr;
        m_ptr = other.m_ptr;
        if (m_ptr)
            InterlockedIncrement(reinterpret_cast<volatile LONG *>(&m_ptr->m_refCount));
    }
    return this;
}

bdReference<bdAddrHandle> &bdReference<bdAddrHandle>::operator=(bdAddrHandle *pointer)
{
    if (m_ptr && !InterlockedDecrement(reinterpret_cast<volatile LONG *>(&m_ptr->m_refCount)))
        delete m_ptr;
    m_ptr = pointer;
    if (m_ptr)
        InterlockedIncrement(reinterpret_cast<volatile LONG *>(&m_ptr->m_refCount));
    return *this;
}

bool bdAddressMap::getAddrHandle(bdReference<bdCommonAddr> addr, const bdSecurityID &id,
    bdReference<bdAddrHandle> &out)
{
    if (findAddrHandle(addr, id, out))
        return true;
    void *memory = bdMemory::allocate(sizeof(bdAddrHandle));
    bdAddrHandle *handle = memory ? ::new(memory) bdAddrHandle(addr, id) : NULL;
    out = handle;
    pushBack(&out);
    return true;
}

bool bdAddressMap::findAddrHandle(bdReference<bdCommonAddr> addr, const bdSecurityID &id,
    bdReference<bdAddrHandle> &out)
{
    if (!addr.m_ptr)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "bdSocket/addrmap", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdAddressMap.cpp",
            "bdAddressMap::findAddrHandle", 44, "Invalid common addr reference.");
        return false;
    }
    bool found = false;
    bdEndpoint endpoint(addr, id);
    const unsigned int size = m_size;
    unsigned int matches = 0;
    for (unsigned int i = 0; i < size; ++i)
    {
        bdReference<bdAddrHandle> handle(m_data[i]);
        if (handle.m_ptr && handle.m_ptr->m_endpoint == endpoint)
        {
            if (!matches)
            {
                found = true;
                out = handle;
            }
            ++matches;
        }
    }
    if (matches > 1)
        bdLogMessage(BD_LOG_WARNING, "warn/", "bdSocket/addrmap", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdAddressMap.cpp",
            "bdAddressMap::findAddrHandle", 75, "Found %u matching address handles, expected 0 or 1.", matches);
    return found;
}

bool bdAddressMap::addrToCommonAddr(const bdReference<bdAddrHandle> &handle,
    bdReference<bdCommonAddr> &addr, bdSecurityID &id)
{
    if (!handle.m_ptr)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "bdSocket/addrmap", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdAddressMap.cpp",
            "bdAddressMap::addrToCommonAddr", 89, "A invalid addr handle ref cannot be used.");
        return false;
    }
    addr = handle.m_ptr->m_endpoint.getCommonAddr();
    id = handle.m_ptr->m_endpoint.getSecID();
    if (!addr.m_ptr)
        bdLogMessage(BD_LOG_WARNING, "warn/", "bdSocket/addrmap", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdAddressMap.cpp",
            "bdAddressMap::addrToCommonAddr", 99, "Addr handle ref has invalid common addr.");
    return true;
}

void bdAddressMap::eraseAddrHandle(unsigned int index)
{
    if (index < m_size)
    {
        m_data[index] = m_data[m_size - 1];
        --m_size;
        m_data[m_size].~bdReference<bdAddrHandle>();
        decreaseCapacity(0);
    }
}

bool bdAddressMap::unregisterAddr(const bdReference<bdAddrHandle> &handle)
{
    if (!handle.m_ptr)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "bdSocket/addrmap", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdAddressMap.cpp",
            "bdAddressMap::unregisterAddr", 110, "A invalid addr handle ref cannot be unregistered.");
        return false;
    }
    bool found = false;
    bool match = false;
    unsigned int matches = 0;
    for (unsigned int i = 0; i < m_size; ++i)
    {
        bdReference<bdAddrHandle> current(m_data[i]);
        if (current.m_ptr)
            match = *current.m_ptr == *handle.m_ptr;
        if (match)
        {
            eraseAddrHandle(i);
            --i;
            ++matches;
            found = true;
        }
    }
    if (matches > 1)
        bdLogMessage(BD_LOG_WARNING, "warn/", "bdSocket/addrmap", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdAddressMap.cpp",
            "bdAddressMap::unregisterAddr", 136, "Found %u matching address handles, expected 0 or 1.", matches);
    else if (!matches)
        bdLogMessage(BD_LOG_WARNING, "warn/", "bdSocket/addrmap", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdAddressMap.cpp",
            "bdAddressMap::unregisterAddr", 141, "Failed to remove addr handle from address map.");
    handle.m_ptr->setStatus(bdAddrHandle::BD_ADDR_UNRESOLVED);
    return found;
}

bool bdAddressMap::unregisterRealAddr(bdAddr addr)
{
    bool found = false;
    for (unsigned int i = 0; i < m_size; ++i)
    {
        bdReference<bdAddrHandle> handle(m_data[i]);
        if (handle.m_ptr && handle.m_ptr->getRealAddr() == addr)
        {
            handle.m_ptr->setStatus(bdAddrHandle::BD_ADDR_UNRESOLVED);
            found = true;
        }
    }
    if (!found)
    {
        char text[22] = {};
        addr.toString(text, sizeof(text));
        bdLogMessage(BD_LOG_WARNING, "warn/", "bdSocket/addrmap", "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdAddressMap.cpp",
            "bdAddressMap::unregisterRealAddr", 169, "No address handles found matching the real address %s.", text);
    }
    return found;
}
#endif


void bdCommonAddr::calculateHash()
{
#if defined(KISAK_SP) && !defined(KISAK_DW)
    bdAddr effective;
    const bdAddr local = m_localAddrs.m_size ? bdAddr(&m_localAddrs.m_data[0]) : bdAddr();
    effective = m_publicAddr.m_address.isValid() ? m_publicAddr : local;
    unsigned char buffer[6];
    unsigned int offset = 0;
    const bool serialized = effective.serialize(buffer, sizeof(buffer), 0, &offset);
    unsigned char digest[24];
    unsigned int digestSize = sizeof(digest);
    bdHashTiger192 hash;
    if (serialized && hash.hash(buffer, sizeof(buffer), digest, &digestSize))
    {
        unsigned int value;
        if (bdBytePacker::removeBuffer(digest, sizeof(digest), 0, &offset, reinterpret_cast<unsigned char *>(&value), sizeof(value)))
            m_hash = value;
    }
#endif
#if 0
    bdAddr effective;

    if (m_localAddrs.m_size > 0)
        effective = m_localAddrs.m_data[0];
    else
        effective = bdAddr();

    //if (bdInetAddr::isValid(&m_publicAddr.m_address))
    if (m_publicAddr.m_address.isValid())
        effective = m_publicAddr;

    unsigned char buffer[6];
    unsigned int offset = 0;

    //if (!bdAddr::serialize(&effective, buffer, 6u, 0, &offset))
    if (!effective.serialize(buffer, 6u, 0, &offset))
    {
        m_hash = 0;
        return;
    }

    bdHashTiger192 hash;
    unsigned char digest[24];
    unsigned int digestSize = 24;

    if (!hash.hash(buffer, 6u, digest, &digestSize))
    {
        m_hash = 0;
        return;
    }

    unsigned char hashBytes[4];
    offset = 0;

    if (bdBytePacker::removeBuffer(digest, 24u, 0, &offset, hashBytes, 4u))
        m_hash = *reinterpret_cast<unsigned int *>(hashBytes);
    else
        m_hash = 0;
#endif
}