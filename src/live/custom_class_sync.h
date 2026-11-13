#pragma once
struct client_t;
struct netadr_t;
struct msg_t;
#if defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH) && !defined(KISAK_DEDICATED)
void CL_CustomClassSyncBegin(int localClientNum);
bool CL_CustomClassSyncPump(int localClientNum);
bool CL_CustomClassSyncAck(int localClientNum, netadr_t from, msg_t *msg);
void SV_CustomClassSyncReset(unsigned slot);
bool SV_CustomClassSyncConnect(client_t *client);
bool SV_CustomClassSyncPending(const client_t *client);
bool SV_CustomClassSyncReady(const client_t *client);
bool SV_CustomClassSyncPacket(netadr_t from, msg_t *msg);
#endif
