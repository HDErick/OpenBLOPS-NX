#pragma once
#ifdef KISAK_SP
#include "sv_main_mp.h"

void SV_AddServerCommand_SP(client_t *client, svscmd_type type, const char *cmd);
int SV_CanReplaceServerCommand_SP(client_t *client, const char *cmd);
bool SV_IsFirstTokenEqual_SP(const char *str1, const char *str2);
void SV_CullIgnorableServerCommands_SP(client_t *client);
void MSG_WriteReliableCommand_SP(const char *command, char *buffer, int size);
void SV_DelayDropClient_SP(client_t *drop, const char *reason);
#endif
