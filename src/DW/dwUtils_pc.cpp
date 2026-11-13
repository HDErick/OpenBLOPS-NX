#if defined(KISAK_SP) && !defined(KISAK_DW)
#include "dwUtils_pc.h"
#include <live/live_steam.h>
#include <universal/q_shared.h>
#include <cstring>

char __cdecl dwGetOnlineUserName(int controllerIndex, char *buf, unsigned int bufsize)
{
    const char *ClientPersonaName;

    memset(buf, 0, bufsize);
    ClientPersonaName = LiveSteam_GetClientPersonaName(1);
    I_strncpyz(buf, ClientPersonaName, bufsize);
    return 1;
}


#include <DemonWare/bdCore/bdMemory/bdMemory.h>
#include <DemonWare/bdCore/bdSingletonRegistry.h>
#include <DemonWare/bdCore/bdCrypto/bdECCKey.h>
#include <DemonWare/bdPlatform/bdPlatformLog/bdPlatformLog.h>
#include <cstdlib>
#include <new>
#include <wincrypt.h>

namespace
{
HCRYPTPROV s_cryptoProvider;

void BD_ReleasePlatformCrypto()
{
    CryptReleaseContext(s_cryptoProvider, 0);
}

void BD_InitPlatformCrypto()
{
    CryptAcquireContextA(&s_cryptoProvider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    std::atexit(BD_ReleasePlatformCrypto);
}

struct BD_PlatformCryptoStartup
{
    BD_PlatformCryptoStartup() { BD_InitPlatformCrypto(); }
};
BD_PlatformCryptoStartup s_platformCryptoStartup;
}

void __cdecl bdGetRandomUChar8(BYTE *buffer, DWORD length)
{
    CryptGenRandom(s_cryptoProvider, length, buffer);
}

void bdTrulyRandomImpl::getRandomUByte8(BYTE *buffer, DWORD length)
{
    bdGetRandomUChar8(buffer, length);
}

bdSingletonRegistryImpl::bdSingletonRegistryImpl()
    : m_destructors(nullptr), m_capacity(0), m_size(0), m_destroying(false)
{
}

bdSingletonRegistryImpl::~bdSingletonRegistryImpl()
{
    bdMemory::deallocate(m_destructors);
    m_destructors = nullptr;
    m_size = 0;
    m_capacity = 0;
}

void bdSingletonRegistryImpl::operator delete(void *memory)
{
    bdMemory::deallocate(memory);
}

bool bdSingletonRegistryImpl::addDestructor(Destructor destructor)
{
    if (m_destroying)
        return false;
    if (m_size == m_capacity)
    {
        const unsigned int growth = m_capacity > 1 ? m_capacity : 1;
        const unsigned int capacity = m_capacity + growth;
        Destructor *data = static_cast<Destructor *>(bdMemory::allocate(capacity * sizeof(Destructor)));
        if (m_size)
            memcpy(data, m_destructors, m_size * sizeof(Destructor));
        bdMemory::deallocate(m_destructors);
        m_capacity = capacity;
        m_destructors = data;
    }
    m_destructors[m_size++] = destructor;
    return true;
}

template <> bdSingletonRegistryImpl *bdSingleton<bdSingletonRegistryImpl>::m_instance = nullptr;
template <> bdTrulyRandomImpl *bdSingleton<bdTrulyRandomImpl>::m_instance = nullptr;

template <> bdSingletonRegistryImpl *bdSingleton<bdSingletonRegistryImpl>::getInstance()
{
    if (!m_instance)
    {
        void *memory = bdMemory::allocate(sizeof(bdSingletonRegistryImpl));
        m_instance = memory ? new (memory) bdSingletonRegistryImpl : nullptr;
        if (m_instance)
        {
            if (!getInstance()->addDestructor(destroyInstance))
            {
                destroyInstance();
                DebugBreak();
            }
        }
        else
            DebugBreak();
    }
    return m_instance;
}

template <> void bdSingleton<bdSingletonRegistryImpl>::destroyInstance()
{
    if (m_instance)
    {
        delete m_instance;
        m_instance = nullptr;
    }
}

template <> bdTrulyRandomImpl *bdSingleton<bdTrulyRandomImpl>::getInstance()
{
    if (!m_instance)
    {
        void *memory = bdMemory::allocate(sizeof(bdTrulyRandomImpl));
        m_instance = memory ? new (memory) bdTrulyRandomImpl : nullptr;
        if (m_instance)
        {
            if (!bdSingleton<bdSingletonRegistryImpl>::getInstance()->addDestructor(destroyInstance))
            {
                bdMemory::deallocate(m_instance);
                m_instance = nullptr;
                DebugBreak();
            }
        }
        else
            DebugBreak();
    }
    return m_instance;
}

template <> void bdSingleton<bdTrulyRandomImpl>::destroyInstance()
{
    if (m_instance)
    {
        bdMemory::deallocate(m_instance);
        m_instance = nullptr;
    }
}

bdECCKey::bdECCKey() : m_initialized(0)
{
    ltc_mp = ltm_desc;
}

bdECCKey::~bdECCKey()
{
    if (m_initialized == 1)
        ecc_free(&m_key);
    m_initialized = 0;
}

bool bdECCKey::init()
{
    const char *source = "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdCore\\bdCrypto\\bdECCKey.cpp";
    if (m_initialized)
    {
        bdLogMessage(BD_LOG_WARNING, "warn/", "ecckey", source, "bdECCKey::init", 82,
            "Cannot reinitialize key.");
        return false;
    }
    bool success = true;
    if (register_prng(&yarrow_desc) == -1)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "ecckey", source, "bdECCKey::init", 39,
            "Register PRNG failed.");
        success = false;
    }
    unsigned char entropy[128];
    bdSingleton<bdTrulyRandomImpl>::getInstance()->getRandomUByte8(entropy, sizeof(entropy));
    prng_state prng;
    int error = 0;
    if (success && (error = yarrow_start(&prng)) != CRYPT_OK)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "ecckey", source, "bdECCKey::init", 51,
            "Start error %s.", error_to_string(error));
        success = false;
    }
    if (success && (error = yarrow_add_entropy(entropy, sizeof(entropy), &prng)) != CRYPT_OK)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "ecckey", source, "bdECCKey::init", 58,
            "Add_entropy error %s.", error_to_string(error));
        success = false;
    }
    if (success && (error = yarrow_ready(&prng)) != CRYPT_OK)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "ecckey", source, "bdECCKey::init", 65,
            "Ready error %s.", error_to_string(error));
        success = false;
    }
    if (success && (error = ecc_make_key(&prng, find_prng("yarrow"), 28, &m_key)) != CRYPT_OK)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "ecckey", source, "bdECCKey::init", 72,
            "Unable to create private key %s.", error_to_string(error));
        success = false;
    }
    if (success)
        m_initialized = 1;
    return success;
}

bool bdECCKey::generateSharedSecret(const unsigned char *publicKey, unsigned int publicKeyLength,
    unsigned char *out, unsigned int outCapacity)
{
    const char *source = "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdCore\\bdCrypto\\bdECCKey.cpp";
    bdECCKey peer;
    const int importError = ecc_import(publicKey, publicKeyLength, &peer.m_key);
    if (importError != CRYPT_OK)
    {
        bdLogMessage(BD_LOG_ERROR, "err/", "ecckey", source, "bdECCKey::generateSharedSecret", 131,
            "Unable to import public key: %s.", error_to_string(importError));
        return false;
    }
    bool success = true;
    if (m_initialized == 1)
    {
        unsigned long length = outCapacity;
        const int error = ecc_shared_secret(&m_key, &peer.m_key, out, &length);
        if (error != CRYPT_OK)
        {
            bdLogMessage(BD_LOG_ERROR, "err/", "ecckey", source, "bdECCKey::generateSharedSecret", 145,
                "Unable to generate secret key. Error: %s", error_to_string(error));
            success = false;
        }
        else if (length < outCapacity)
            std::memset(out + length, 0, outCapacity - length);
    }
    else
    {
        bdLogMessage(BD_LOG_WARNING, "warn/", "ecckey", source, "bdECCKey::generateSharedSecret", 161,
            "Cannot generate secret with uninitialized key.");
    }
    ecc_free(&peer.m_key);
    return success;
}

#endif

#ifdef KISAK_DW

#include "dwUtils_pc.h"
#include <live/live_steam.h>
#include <universal/q_shared.h>
#include <qcommon/net_chan_mp.h>
#include "dwMessaging.h"
#include <qcommon/common.h>
#include <client_mp/cl_main_mp.h>
#include <cgame_mp/cg_main_mp.h>
#include <qcommon/com_clients.h>

unsigned __int64 g_bdUserID;

const dvar_t *cl_senddwchallenge;

// LWSS: I have modified this crap function a bit
void __cdecl bdGetRandomUChar8(BYTE *pbBuffer, DWORD dwLen)
{
    static HCRYPTPROV hProv = 0;

    if (!hProv)
    {
        CryptAcquireContext(
            &hProv,
            NULL,
            NULL,
            PROV_RSA_FULL,
            CRYPT_VERIFYCONTEXT | CRYPT_SILENT
        );
    }

    //CryptGenRandom(hProv, size, (BYTE *)out);

    CryptGenRandom(hProv, dwLen, pbBuffer);
}

char __cdecl dwGetOnlineUserID(int controllerIndex, unsigned __int64 *userID)
{
    if ( g_bdUserID )
        *userID = g_bdUserID;
    else
        *userID = 0;
    return 1;
}

char __cdecl dwGetOnlineUserName(int controllerIndex, char *buf, unsigned int bufsize)
{
    const char *ClientPersonaName; // eax

    memset(buf, 0, bufsize);
    ClientPersonaName = LiveSteam_GetClientPersonaName(1);
    I_strncpyz(buf, ClientPersonaName, bufsize);
    return 1;
}

void __cdecl dwXnaddrtonetadr(unsigned __int8 *xnaddr, netadr_t *privadr, netadr_t *pubadr)
{
    if ( !privadr
        && !pubadr
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\Dw\\dwUtils_pc.cpp", 116, 0, "%s", "privadr || pubadr") )
    {
        __debugbreak();
    }
    if ( privadr )
    {
        *(unsigned int *)privadr->ip = *(unsigned int *)xnaddr;
        privadr->port = *((_WORD *)xnaddr + 2);
        privadr->type = NA_IP;
    }
    if ( pubadr )
    {
        *(unsigned int *)pubadr->ip = *(unsigned int *)(xnaddr + 18);
        pubadr->port = *((_WORD *)xnaddr + 11);
        pubadr->type = NA_IP;
    }
}

unsigned int __cdecl SV_SendClientChallenge(unsigned int nonce, unsigned int challenge, unsigned __int64 userID)
{
    bdTrulyRandomImpl *Instance; // eax
    unsigned int randNum; // [esp+50h] [ebp-4Ch] BYREF
    unsigned int retval; // [esp+54h] [ebp-48h]
    msg_t challengemessage; // [esp+58h] [ebp-44h] BYREF
    unsigned __int8 buff[16]; // [esp+88h] [ebp-14h] BYREF

    retval = 0;
    MSG_Init(&challengemessage, buff, 13);
    Instance = bdSingleton<bdTrulyRandomImpl>::getInstance();
    randNum = Instance->getRandomUInt();//bdTrulyRandomImpl::getRandomUInt(Instance);
    Com_DPrintf(
        15,
        "Client bdID:\t%llu\nClient challenge:\t%u\nServer challenge:\t%u\nClient nonce:\t%u\n",
        userID,
        challenge,
        randNum,
        nonce);
    MSG_WriteByte(&challengemessage, 1u);
    MSG_WriteData(&challengemessage, (unsigned __int8 *)&nonce, 4u);
    MSG_WriteData(&challengemessage, (unsigned __int8 *)&challenge, 4u);
    MSG_WriteData(&challengemessage, (unsigned __int8 *)&randNum, 4u);
    if ( dwMessaging_SendDeferredInstantMessage(userID, buff, 0xDu) )
        return randNum;
    return retval;
}

void __cdecl CL_HandleSVDWChallenge(unsigned int nonce, unsigned int serverchallenge, unsigned int clientchallenge)
{
    netsrc_t NetworkID; // eax
    netadr_t serverAddress; // [esp-18h] [ebp-D0h]
    unsigned __int8 *data; // [esp-8h] [ebp-C0h]
    int cursize; // [esp-4h] [ebp-BCh]
    unsigned __int8 msgBuffer[128]; // [esp+0h] [ebp-B8h] BYREF
    msg_t msg; // [esp+84h] [ebp-34h] BYREF
    clientConnection_t *clc; // [esp+B4h] [ebp-4h]

    if ( !cl_senddwchallenge )
        cl_senddwchallenge = _Dvar_RegisterBool(
                                                     "cl_senddwchallenge",
                                                     1,
                                                     0,
                                                     "set to false to verify server kicks client when no challenge received");
    if ( cl_senddwchallenge )
    {
        if ( cl_senddwchallenge->current.enabled )
        {
            clc = CL_GetLocalClientConnection(0);
            if ( clc )
            {
                if ( clc->nonce )
                {
                    if ( *(&clc->nonce + 1) == nonce )
                    {
                        if ( clc->nonce == clientchallenge )
                        {
                            clc->clientChallenge = serverchallenge;
                            clc = CL_GetLocalClientConnection(0);
                            memset(msgBuffer, 0, sizeof(msgBuffer));
                            MSG_Init(&msg, msgBuffer, 128);
                            MSG_WriteString(&msg, "dwcr");
                            MSG_WriteShort(&msg, clc->qport);
                            Com_DPrintf(14, "DWCHALLENGERESPONSE: Sending serverchallenge %u back to server\n", serverchallenge);
                            MSG_WriteData(&msg, (unsigned __int8 *)&clc->clientChallenge, 4u);
                            cursize = msg.cursize;
                            data = msg.data;
                            serverAddress = clc->serverAddress;
                            NetworkID = Com_LocalClient_GetNetworkID(0);
                            if ( !NET_OutOfBandData(NetworkID, serverAddress, data, cursize) )
                                Com_DPrintf(14, "CHALLENGERESPONSE: Couldn't send challengeresponse to server\n");
                        }
                        else
                        {
                            Com_DPrintf(
                                14,
                                "CHALLENGERESPONSE: Just received answer %u from server that doesn't match %u, dropping!\n",
                                clientchallenge,
                                clc->nonce);
                            Com_Error(ERR_DROP, "PATCH_SERVER_AUTHFAIL");
                        }
                    }
                    else
                    {
                        Com_DPrintf(
                            14,
                            "CHALLENGERESPONSE: Just received nonce %u from server that doesn't match %u, probably a stale message. If "
                            "you see this a lot, ask Ewan\n",
                            nonce,
                            *(&clc->nonce + 1));
                    }
                }
                else
                {
                    Com_DPrintf(
                        14,
                        "Er, we got a serverchallenge message, but not record of having sent a clientchallenge. Ask Ewan.\n");
                }
            }
        }
    }
}

unsigned int __cdecl Com_GametypeToInt(const char *gametype)
{
    int v1; // ecx
    int i; // [esp+10h] [ebp-10h]
    int stringlen; // [esp+14h] [ebp-Ch]
    unsigned int retval; // [esp+1Ch] [ebp-4h]

    if ( !gametype && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\Dw\\dwUtils_pc.cpp", 278, 0, "%s", "gametype") )
        __debugbreak();
    v1 = strlen(gametype);
    stringlen = v1;
    retval = 0;
    if ( v1 > 6 )
    {
        Com_PrintWarning(0, "Can only pack %i chars, stringlen is %i\n", 6, v1);
        stringlen = 6;
    }
    for ( i = stringlen - 1; i >= 0; --i )
    {
        retval |= (__int16)(tolower(gametype[i]) - 96);
        if ( i )
            retval *= 32;
    }
    return retval;
}

void __cdecl Com_IntToGametype(char *gametype_out, unsigned int gametype_in)
{
    char c; // [esp+3h] [ebp-5h]
    __int16 charcount; // [esp+4h] [ebp-4h]

    if ( !gametype_out
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\Dw\\dwUtils_pc.cpp", 304, 0, "%s", "NULL != gametype_out") )
    {
        __debugbreak();
    }
    for ( charcount = 5; charcount >= 0; --charcount )
    {
        c = (gametype_in >> (5 * charcount)) & 0x1F;
        if ( c )
            c += 96;
        gametype_out[charcount] = c;
    }
}


#endif