#pragma once

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace LocalPlayerName
{
// Match the server's name limits, and keep the value safe for userinfo and
// Com_ParseCommandLine (which splits at '+' even inside quoted arguments).
inline bool Read(const char *path, char (&name)[32], unsigned int entropy)
{
    name[0] = 0;
    if (FILE *file = fopen(path, "rb"))
    {
        char line[128] = "";
        if (fgets(line, sizeof(line), file))
        {
            size_t length = 0;
            for (const char *p = line; *p && *p != '\r' && *p != '\n' && length + 1 < sizeof(name); ++p)
            {
                const unsigned char c = static_cast<unsigned char>(*p);
                if (c < 0x20 || c == 0x7f || c == '"' || c == '\\' || c == ';' || c == '+')
                    continue;
                name[length++] = static_cast<char>(c);
            }
            name[length] = 0;
        }
        fclose(file);
    }
    char *start = name;
    while (*start == ' ')
        ++start;
    size_t length = strlen(start);
    while (length && start[length - 1] == ' ')
        --length;
    memmove(name, start, length);
    name[length] = 0;
    char lower[32];
    for (size_t i = 0; i <= length; ++i)
        lower[i] = static_cast<char>(tolower(static_cast<unsigned char>(name[i])));
    if (length >= 4 && !strstr(lower, "unknown soldier"))
        return false;

    snprintf(name, sizeof(name), "player%04u", entropy % 10000);
    if (FILE *file = fopen(path, "wb"))
    {
        fprintf(file, "%s\n", name);
        fclose(file);
    }
    return true;
}

// Insert before startup commands, retaining the initial allowdupe/nodump etc.
// prefix and allowing an explicit user +set name to override the file.
inline bool ApplyDefault(char *cmdline, size_t size, const char *name)
{
    char command[64];
    const int added = snprintf(command, sizeof(command), " +set name \"%s\" ", name);
    const size_t length = strlen(cmdline);
    if (added < 0 || static_cast<size_t>(added) >= sizeof(command)
        || length >= size || static_cast<size_t>(added) >= size - length)
        return false;
    const size_t prefix = strcspn(cmdline, "+\n");
    memmove(cmdline + prefix + added, cmdline + prefix, length - prefix + 1);
    memcpy(cmdline + prefix, command, added);
    return true;
}
}
