#include "custom_class_sync.h"
#if defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH) && !defined(KISAK_DEDICATED)
#include "custom_class_transport.h"
#include "live_stats.h"
#include "live_storage.h"
#include <client_mp/cl_main_mp.h>
#include <qcommon/common.h>
#include <qcommon/net_chan_mp.h>
#include <server_mp/sv_main_mp.h>
#include <universal/q_shared.h>
#include <qcommon/com_clients.h>
#include <client/cl_main.h>
#include <client/client.h>
#include <client_mp/cl_cgame_mp.h>
#include <cgame_mp/cg_main_mp.h>
#include <universal/local_client_limits.h>

namespace
{
customclasses::Sender sending[MAX_LOCAL_CLIENTS];
struct RemoteClasses
{
    bool required = false, ready = false;
    customclasses::Receiver receiver;
} receiving[32];
unsigned Slot(const client_t *client) { return unsigned(client - svs.clients); }
bool Packet(bool ack, msg_t *msg, customclasses::Header &h, const unsigned char *&data, size_t &size)
{
    return msg && msg->cursize >= 4 && customclasses::Decode(ack, msg->data + 4, msg->cursize - 4, h, data, size);
}
// This is exclusively the game's cacLoadouts subtree: classes, equipment,
// perks, cosmetics and killstreak selections. No stats header, progression,
// account identifiers or bytes outside that subtree are sent to the host.
bool ClassLayout(unsigned &start, unsigned &bits, unsigned &bytes)
{
    const auto *state = LiveStats_GetCacDDLState();
    if (!state || !state->ddl || !state->member || state->absoluteOffset < 0 || state->member->size <= 0)
        return false;
    start = unsigned(state->absoluteOffset) + 320;
    bits = unsigned(state->member->size);
    if (start >= STATS_BUFFER_SIZE * 8 || bits > STATS_BUFFER_SIZE * 8 - start
        || state->absoluteOffset > state->ddl->size || bits > unsigned(state->ddl->size - state->absoluteOffset)) return false;
    bytes = (bits + 7) / 8;
    return true;
}
}

void CL_CustomClassSyncBegin(int localClientNum)
{
    auto *connection = CL_GetLocalClientConnection(localClientNum);
    const int controller = Com_LocalClient_GetControllerIndex(localClientNum);
    const auto *normal = LiveStorage_GetStatsBuffer(controller, STATS_LOCATION_NORMAL, 1);
    const auto *global = LiveStorage_GetStatsBuffer(controller, STATS_LOCATION_GLOBAL, 1);
    unsigned start, bits, bytes;
    auto *schema = LiveStats_GetStatsDDL();
    if (!normal || !global || !schema || !ClassLayout(start, bits, bytes))
        Com_Error(ERR_DROP, "Cannot send custom classes: local loadouts are unavailable");
    auto snapshot = customclasses::Pack(normal->statsBuffer, start, bits);
    auto globalClasses = customclasses::Pack(global->statsBuffer, start, bits);
    snapshot.insert(snapshot.end(), globalClasses.begin(), globalClasses.end());
    sending[localClientNum].Begin(std::move(snapshot), connection->qport, connection->challenge, schema->version);
    CL_SetLocalClientConnectionState(localClientNum, CA_SENDINGSTATS);
    Com_Printf(14, "Synchronizing custom-class loadouts with the connected host\n");
}

bool CL_CustomClassSyncPump(int localClientNum)
{
    if (CL_GetLocalClientConnectionState(localClientNum) != CA_SENDINGSTATS
        || sending[localClientNum].data.empty()) return false;
    auto *connection = CL_GetLocalClientConnection(localClientNum);
    sending[localClientNum].Pump(cls.realtime, [&](const std::vector<unsigned char> &packet) {
        NET_OutOfBandData(Com_LocalClient_GetNetworkID(localClientNum), connection->serverAddress,
            packet.data(), int(packet.size()));
    });
    return true;
}

bool CL_CustomClassSyncAck(int localClientNum, netadr_t from, msg_t *msg)
{
    customclasses::Header h; const unsigned char *data; size_t bytes;
    if (!Packet(true, msg, h, data, bytes)) return false;
    auto *connection = CL_GetLocalClientConnection(localClientNum);
    auto &sender = sending[localClientNum];
    if (CL_GetLocalClientConnectionState(localClientNum) != CA_SENDINGSTATS
        || !NET_CompareAdr(from, connection->serverAddress) || !customclasses::Same(sender.header, h)) return true;
    if (h.index == customclasses::Rejected)
        Com_Error(ERR_DROP, "The host rejected the custom classes (schema or transfer mismatch)");
    if (sender.Acknowledge(h))
    {
        connection->lastPacketTime = cls.realtime;
        if (sender.Done())
        {
            CL_SetLocalClientConnectionState(localClientNum, CA_CONNECTED);
            connection->lastPacketSentTime = -9999;
            sender.data.clear();
            Com_Printf(14, "Custom-class loadouts synchronized\n");
        }
    }
    return true;
}

void SV_CustomClassSyncReset(unsigned slot)
{
    if (slot < 32) receiving[slot] = RemoteClasses{};
}

bool SV_CustomClassSyncConnect(client_t *client)
{
    const unsigned slot = Slot(client);
    SV_CustomClassSyncReset(slot);
    if (slot >= 32) return false;
    return receiving[slot].required = !client->bIsTestClient && !client->bIsDemoClient
        && client->header.netchan.remoteAddress.type != NA_LOOPBACK
        && !strcmp(Info_ValueForKey(client->userinfo, "openblops_classes"), "1");
}

bool SV_CustomClassSyncPending(const client_t *client)
{
    const unsigned slot = Slot(client);
    return slot < 32 && receiving[slot].required && !receiving[slot].ready;
}

bool SV_CustomClassSyncReady(const client_t *client)
{
    const unsigned slot = Slot(client);
    return slot < 32 && receiving[slot].required && receiving[slot].ready;
}

bool SV_CustomClassSyncPacket(netadr_t from, msg_t *msg)
{
    customclasses::Header h; const unsigned char *payload; size_t bytes;
    if (!Packet(false, msg, h, payload, bytes)) return false;
    if (!svs.clients || !com_maxclients) return true;
    for (unsigned slot = 0; slot < 32 && slot < unsigned(com_maxclients->current.integer); ++slot)
    {
        auto &client = svs.clients[slot]; auto &classes = receiving[slot];
        if (!classes.required || client.header.state != CS_CONNECTED
            || h.qport != unsigned(client.header.netchan.qport) || h.challenge != unsigned(client.challenge)
            || !NET_CompareAdr(from, client.header.netchan.remoteAddress)) continue;
        unsigned start, bits, classBytes;
        auto *schema = LiveStats_GetStatsDDL();
        if (!schema || h.schema != unsigned(schema->version) || !ClassLayout(start, bits, classBytes)
            || h.size != 2 * classBytes)
        {
            h.index = customclasses::Rejected;
            auto ack = customclasses::Encode(true, h);
            NET_OutOfBandData(NS_SERVER, from, ack.data(), int(ack.size()));
            return true;
        }
        auto &receiver = classes.receiver;
        if (!receiver.Receive(h, payload, bytes, 2 * classBytes)) return true;
        if (receiver.complete && !classes.ready)
        {
            // Server-owned DDL headers stay local. Only the fixed CAC bit range
            // is populated; remote players never inherit the host's loadout.
            memset(client.stats, 0, sizeof(client.stats));
            memset(client.globalStats, 0, sizeof(client.globalStats));
            if (!DDL_AssociateBuffer((char *)client.stats, STATS_BUFFER_SIZE, schema)
                || !DDL_AssociateBuffer((char *)client.globalStats, STATS_BUFFER_SIZE, schema)) receiver.failed = true;
            else
            {
                customclasses::Unpack(client.stats, start, bits, receiver.data.data());
                customclasses::Unpack(client.globalStats, start, bits, receiver.data.data() + classBytes);
                memcpy(client.globalStatsStable, client.globalStats, sizeof(client.globalStatsStable));
                client.statPacketsReceived = ALL_STATS_PACKETS_RECEIVED;
                client.statsValidated = true;
                classes.ready = true;
                Com_Printf(15, "Custom classes ready for remote client %u (%s)\n", slot, client.name);
            }
        }
        client.lastPacketTime = svs.time;
        auto ack = customclasses::Encode(true, receiver.Ack());
        NET_OutOfBandData(NS_SERVER, from, ack.data(), int(ack.size()));
        return true;
    }
    return true;
}
#endif
