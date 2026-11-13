#pragma once

#ifdef KISAK_SP
#include <server/server.h>

inline int CL_PackServerCommandAcknowledgement_SP(int connectionState, int sequence)
{
    const unsigned int packed = (static_cast<unsigned int>(connectionState) << 28)
        | static_cast<unsigned int>(sequence);
    return static_cast<int>(packed);
}

inline bool SV_DecodeClientAcknowledgement_SP(client_t *client, int packedWord)
{
    const int previous = client->header.connectionState_SP;
    const unsigned int packed = static_cast<unsigned int>(packedWord);
    client->header.connectionState_SP = static_cast<int>(packed >> 28);
    client->reliableAcknowledge = static_cast<int>(packed & 0x0FFFFFFFu);
    return previous != client->header.connectionState_SP;
}

inline bool SV_ClientReliableAcknowledgementInRange_SP(const client_t *client)
{
    const int distance = static_cast<int>(static_cast<unsigned int>(client->reliableSequence)
        - static_cast<unsigned int>(client->reliableAcknowledge));
    return distance < 128;
}

inline void SV_ResetClientConnectionStates_SP(client_t *clients, int clientCount)
{
    for (int i = 0; i < clientCount; ++i)
        clients[i].header.connectionState_SP = 0;
}
#endif
