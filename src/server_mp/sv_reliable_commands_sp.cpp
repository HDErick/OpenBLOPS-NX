#include "sv_reliable_commands_sp.h"
#ifdef KISAK_SP
#include <qcommon/common.h>
#include <universal/q_shared.h>
#include <cstring>

static int SV_ReliableAdd_SP(int left, int right)
{
    return static_cast<int>(static_cast<unsigned int>(left) + static_cast<unsigned int>(right));
}

static int SV_ReliableSub_SP(int left, int right)
{
    return static_cast<int>(static_cast<unsigned int>(left) - static_cast<unsigned int>(right));
}

bool SV_IsFirstTokenEqual_SP(const char *str1, const char *str2)
{
    while (*str1 && *str2 && *str1 != ' ' && *str2 != ' ')
    {
        if (*str1 != *str2)
            return false;
        ++str1;
        ++str2;
    }
    return (!*str1 || *str1 == ' ') && (!*str2 || *str2 == ' ');
}

int SV_CanReplaceServerCommand_SP(client_t *client, const char *cmd)
{
    for (int index = SV_ReliableAdd_SP(client->reliableSent, 1);
         index <= client->reliableSequence; index = SV_ReliableAdd_SP(index, 1))
    {
        const svscmd_info_t *row = &client->reliableCommandInfo[static_cast<unsigned int>(index) & 127];
        const signed char code = static_cast<signed char>(*cmd);
        if (!row->type || *cmd != row->cmd[0] || (code >= 'x' && code <= 'z'))
            continue;
        if (!std::strcmp(cmd + 1, row->cmd + 1))
            return index;
        switch (code)
        {
        case 'C':
        case 'a':
        case 'q':
        case 't':
            return index;
        case 'd':
        case 'v':
            if (SV_IsFirstTokenEqual_SP(cmd + 2, row->cmd + 2))
                return index;
            break;
        }
    }
    return -1;
}

void SV_CullIgnorableServerCommands_SP(client_t *client)
{
    int to = SV_ReliableAdd_SP(client->reliableSent, 1);
    for (int from = to; from <= client->reliableSequence; from = SV_ReliableAdd_SP(from, 1))
    {
        const unsigned int fromIndex = static_cast<unsigned int>(from) & 127;
        if (client->reliableCommandInfo[fromIndex].type)
        {
            const unsigned int toIndex = static_cast<unsigned int>(to) & 127;
            if (toIndex != fromIndex)
                client->reliableCommandInfo[toIndex] = client->reliableCommandInfo[fromIndex];
            to = SV_ReliableAdd_SP(to, 1);
        }
    }
    client->reliableSequence = SV_ReliableAdd_SP(to, -1);
}

void MSG_WriteReliableCommand_SP(const char *command, char *buffer, int size)
{
    const int length = static_cast<int>(std::strlen(command));
    if (length >= size)
        Com_PrintWarning(16, "WARNING: Reliable command is too long (%i/%i) and will be truncated: '%s'\n",
            length, size, command);
    if (!length)
        Com_PrintWarning(16, "WARNING: Empty reliable command\n");
    int index = 0;
    for (; index < size && command[index]; ++index)
    {
        buffer[index] = static_cast<char>(I_CleanChar(static_cast<unsigned char>(command[index])));
        if (buffer[index] == '%')
            buffer[index] = '.';
    }
    if (index < size)
        buffer[index] = '\0';
    else
        buffer[size - 1] = '\0';
}

void SV_DelayDropClient_SP(client_t *drop, const char *reason)
{
    if (drop->header.state != CS_ZOMBIE && !drop->dropReason)
        drop->dropReason = reason;
}

void SV_AddServerCommand_SP(client_t *client, svscmd_type type, const char *cmd)
{
    if (client->bIsTestClient)
        return;
    if (SV_ReliableSub_SP(client->reliableSequence, client->reliableAcknowledge) >= 64
        || client->header.state != CS_ACTIVE)
    {
        SV_CullIgnorableServerCommands_SP(client);
        if (!type)
            return;
    }
    int to = SV_CanReplaceServerCommand_SP(client, cmd);
    const unsigned char specialCode = static_cast<unsigned char>(static_cast<unsigned char>(*cmd) - 'x');
    if (to >= 0)
    {
        for (int from = SV_ReliableAdd_SP(to, 1); from <= client->reliableSequence;
             from = SV_ReliableAdd_SP(from, 1), to = SV_ReliableAdd_SP(to, 1))
            client->reliableCommandInfo[static_cast<unsigned int>(to) & 127] =
                client->reliableCommandInfo[static_cast<unsigned int>(from) & 127];
    }
    else
    {
        if (specialCode > 2)
        {
            for (int index = SV_ReliableAdd_SP(client->reliableAcknowledge, 1);
                 index <= client->reliableSequence; index = SV_ReliableAdd_SP(index, 1))
            {
                if (!std::strcmp(cmd, client->reliableCommandInfo[static_cast<unsigned int>(index) & 127].cmd))
                {
                    Com_DPrintf(0, "Dropping redundant reliable command: %s\n", cmd);
                    return;
                }
            }
        }
        client->reliableSequence = SV_ReliableAdd_SP(client->reliableSequence, 1);
    }
    if (SV_ReliableSub_SP(client->reliableSequence, client->reliableAcknowledge) == 129)
    {
        Com_Printf(15, "===== pending server commands =====\n");
        int index = SV_ReliableAdd_SP(client->reliableAcknowledge, 1);
        for (; index <= client->reliableSequence; index = SV_ReliableAdd_SP(index, 1))
        {
            const svscmd_info_t *row = &client->reliableCommandInfo[static_cast<unsigned int>(index) & 127];
            Com_Printf(15, "cmd %5d: %8d: %s\n", index, row->time, row->cmd);
        }
        Com_Printf(15, "cmd %5d: %8d: %s\n", index, svs.time, cmd);
        Com_Printf(15, "ERROR: ran out of reliable commands\n");
        NET_OutOfBandPrint(NS_SERVER, client->header.netchan.remoteAddress, "disconnect");
        SV_DelayDropClient_SP(client, "EXE_SERVERCOMMANDOVERFLOW");
        type = SV_CMD_RELIABLE;
        cmd = va("%c \"EXE_SERVERCOMMANDOVERFLOW\"", 119);
    }
    const unsigned int index = static_cast<unsigned int>(client->reliableSequence) & 127;
    svscmd_info_t *row = &client->reliableCommandInfo[index];
    MSG_WriteReliableCommand_SP(cmd, row->cmd, 512);
    row->time = svs.time;
    row->type = type;
    if (sv_debugReliableCmds->current.enabled)
        Com_Printf(15, "buffered str cmd# %5d for %s: svs.time %8d: %s\n",
            index, client->name, row->time, row->cmd);
}
#endif
