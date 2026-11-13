#include "com_stringtable_obj.h"
#include <server_mp/sv_bot_mp.h>
#include "com_stringtable.h"
#include "com_loadutils.h"
#include <qcommon/common.h>
#include <string.h>
#include "q_parse.h"
#include <ui/ui_utils.h>
#include <stringed/stringed_ingame.h>
#include <cstdlib>
#ifdef KISAK_SP
#include <cstddef>
#endif
#include "com_memory.h"

StringTable *s_sortTable;

int __cdecl StringTable_HashString(const char *string)
{
    return Com_HashString(string, 0);
}

void __cdecl StringTable_ParseFileIntoTable(const char *filename, StringTable *table)
{
    const char *v2; // eax
    int v3; // eax
    const char *v4; // eax
    int v5; // eax
    char *v6; // eax
    int columns; // [esp+10h] [ebp-28h]
    char *buffer; // [esp+14h] [ebp-24h]
    int rowCount; // [esp+18h] [ebp-20h]
    const char *parse; // [esp+1Ch] [ebp-1Ch] BYREF
    int idx; // [esp+20h] [ebp-18h]
    const char *stringRef; // [esp+24h] [ebp-14h]
    int filenameLen; // [esp+28h] [ebp-10h]
    int valueCount; // [esp+2Ch] [ebp-Ch]
    const char *token; // [esp+30h] [ebp-8h]
    int nextValue; // [esp+34h] [ebp-4h]

    if ( *filename )
    {
        table->columnCount = 0;
        table->rowCount = 0;
        buffer = Com_LoadRawTextFile(filename);
        if ( !buffer )
            Com_PrintWarning(16, "missing stringTable %s\n", filename);
        nextValue = 0;
        filenameLen = strlen(filename) + 1;
        table->name = (const char *)StringTable_Alloc(filenameLen);
        I_strncpyz((char *)table->name, filename, filenameLen);
        parse = buffer;
        Com_BeginParseSession(filename);
        Com_SetCSV(1);
        rowCount = 0;
        while ( 1 )
        {
            token = (const char *)Com_Parse(&parse);
            if ( !parse )
                break;
            if ( *token )
            {
                if ( *token != 35 )
                {
                    columns = Com_GetArgCountOnLine(&parse) + 1;
                    if ( columns > table->columnCount )
                        table->columnCount = columns;
                    ++rowCount;
                }
                Com_SkipRestOfLine(&parse);
            }
        }
        valueCount = table->columnCount * rowCount;
        if ( !valueCount )
            valueCount = 1;
        Com_EndParseSession();
        if ( table->values
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\universal\\com_stringtable_obj.cpp",
                        117,
                        0,
                        "%s",
                        "!table->values") )
        {
            __debugbreak();
        }
        table->values = (StringTableCell *)StringTable_Alloc(8 * valueCount);
        table->cellIndex = (__int16 *)StringTable_Alloc(2 * valueCount);
        for ( idx = 0; idx < valueCount; ++idx )
            table->cellIndex[idx] = idx;
        parse = buffer;
        Com_BeginParseSession(filename);
        Com_SetCSV(1);
        while ( 1 )
        {
            token = (const char *)Com_Parse(&parse);
            if ( !parse )
                break;
            if ( *token )
            {
                if ( *token != 35 )
                {
                    if ( nextValue >= (unsigned int)valueCount
                        && !Assert_MyHandler(
                                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\com_stringtable_obj.cpp",
                                    138,
                                    0,
                                    "nextValue doesn't index valueCount\n\t%i not in [0, %i)",
                                    nextValue,
                                    valueCount) )
                    {
                        __debugbreak();
                    }
                    v2 = String_Alloc(token);
                    table->values[nextValue].string = v2;
                    v3 = StringTable_HashString(token);
                    table->values[nextValue++].hash = v3;
                    if ( table->columnCount != 1
                        && nextValue % table->columnCount != 1
                        && !Assert_MyHandler(
                                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\com_stringtable_obj.cpp",
                                    143,
                                    0,
                                    "%s",
                                    "table->columnCount == 1 || nextValue % table->columnCount == 1") )
                    {
                        __debugbreak();
                    }
                    while ( nextValue % table->columnCount )
                    {
                        token = (const char *)Com_ParseOnLine(&parse);
                        if ( *token == 92 )
                            ++token;
                        if ( *token == 64 )
                        {
                            stringRef = token + 1;
                            token = (const char *)SE_GetString_LoadObj(token + 1);
                            if ( !token )
                                Com_Error(ERR_DROP, "Couldn't find localization entry for '%s' in file %s\n", stringRef, filename);
                        }
                        if ( nextValue >= (unsigned int)valueCount
                            && !Assert_MyHandler(
                                        "C:\\projects_pc\\cod\\codsrc\\src\\universal\\com_stringtable_obj.cpp",
                                        158,
                                        0,
                                        "nextValue doesn't index valueCount\n\t%i not in [0, %i)",
                                        nextValue,
                                        valueCount) )
                        {
                            __debugbreak();
                        }
                        v4 = String_Alloc(token);
                        table->values[nextValue].string = v4;
                        v5 = StringTable_HashString(token);
                        table->values[nextValue++].hash = v5;
                    }
                    ++table->rowCount;
                }
                Com_SkipRestOfLine(&parse);
            }
        }
        if ( table->rowCount != rowCount
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\universal\\com_stringtable_obj.cpp",
                        169,
                        0,
                        "%s",
                        "table->rowCount == rowCount") )
        {
            __debugbreak();
        }
        if ( table->rowCount * table->columnCount != valueCount
            && valueCount != 1
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\universal\\com_stringtable_obj.cpp",
                        170,
                        0,
                        "%s",
                        "table->columnCount * table->rowCount == valueCount || valueCount == 1") )
        {
            __debugbreak();
        }
        if ( table->rowCount * table->columnCount > 0x4000 )
        {
            v6 = va(
                         "CSV file %s has too many elements: %i (max is %i)",
                         filename,
                         table->rowCount * table->columnCount,
                         0x4000);
            Com_PrintWarning(10, v6);
        }
        if ( s_sortTable
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\universal\\com_stringtable_obj.cpp",
                        175,
                        0,
                        "%s",
                        "s_sortTable == NULL") )
        {
            __debugbreak();
        }
        s_sortTable = table;
        qsort(table->cellIndex, valueCount, 2u, (int (__cdecl *)(const void *, const void *))CellCompare);
        s_sortTable = 0;
        Com_EndParseSession();
        if ( buffer )
            Com_UnloadRawTextFile(buffer);
    }
}

unsigned __int8 *__cdecl StringTable_Alloc(unsigned int size)
{
    return Hunk_Alloc(size, "StringTable", 11);
}

#ifdef KISAK_SP
static void StringTable_SwapCells_SP(unsigned char *a, unsigned char *b, unsigned int width)
{
    if (a != b)
    {
        while (width--)
        {
            const unsigned char value = *a;
            *a++ = *b;
            *b++ = value;
        }
    }
}

static void StringTable_SmallSort_SP(unsigned char *base, std::ptrdiff_t low,
    std::ptrdiff_t high, unsigned int width, int (__cdecl *compare)(const void *, const void *))
{
    while (high > low)
    {
        std::ptrdiff_t maximum = low;
        for (std::ptrdiff_t current = low + 1; current <= high; ++current)
        {
            if (compare(base + current * width, base + maximum * width) > 0)
                maximum = current;
        }
        StringTable_SwapCells_SP(base + maximum * width, base + high * width, width);
        --high;
    }
}

static void StringTable_SortCells_SP(void *array, unsigned int count, unsigned int width,
    int (__cdecl *compare)(const void *, const void *))
{
    if ((!array && count) || !width || !compare)
    {
        qsort(array, count, width, compare);
        return;
    }
    if (count < 2)
        return;

    unsigned char *base = static_cast<unsigned char *>(array);
    std::ptrdiff_t low = 0;
    std::ptrdiff_t high = static_cast<std::ptrdiff_t>(count) - 1;
    std::ptrdiff_t lowPending[30];
    std::ptrdiff_t highPending[30];
    int pending = 0;
    const auto element = [base, width](std::ptrdiff_t index)
    {
        return base + index * static_cast<std::ptrdiff_t>(width);
    };

    for (;;)
    {
        const std::ptrdiff_t size = high - low + 1;
        if (size <= 8)
        {
            StringTable_SmallSort_SP(base, low, high, width, compare);
        }
        else
        {
            std::ptrdiff_t middle = low + size / 2;
            if (compare(element(low), element(middle)) > 0)
                StringTable_SwapCells_SP(element(low), element(middle), width);
            if (compare(element(low), element(high)) > 0)
                StringTable_SwapCells_SP(element(low), element(high), width);
            if (compare(element(middle), element(high)) > 0)
                StringTable_SwapCells_SP(element(middle), element(high), width);

            std::ptrdiff_t left = low;
            std::ptrdiff_t right = high;
            for (;;)
            {
                if (middle > left)
                {
                    do { ++left; }
                    while (left < middle && compare(element(left), element(middle)) <= 0);
                }
                if (middle <= left)
                {
                    do { ++left; }
                    while (left <= high && compare(element(left), element(middle)) <= 0);
                }
                do { --right; }
                while (right > middle && compare(element(right), element(middle)) > 0);
                if (left > right)
                    break;
                StringTable_SwapCells_SP(element(left), element(right), width);
                if (middle == right)
                    middle = left;
            }

            ++right;
            if (middle < right)
            {
                do { --right; }
                while (right > middle && compare(element(right), element(middle)) == 0);
            }
            if (middle >= right)
            {
                do { --right; }
                while (right > low && compare(element(right), element(middle)) == 0);
            }

            if (right - low >= high - left)
            {
                if (low < right)
                {
                    lowPending[pending] = low;
                    highPending[pending++] = right;
                }
                if (left < high)
                {
                    low = left;
                    continue;
                }
            }
            else
            {
                if (left < high)
                {
                    lowPending[pending] = left;
                    highPending[pending++] = high;
                }
                if (low < right)
                {
                    high = right;
                    continue;
                }
            }
        }
        if (!pending)
            return;
        --pending;
        low = lowPending[pending];
        high = highPending[pending];
    }
}

void __cdecl StringTable_SortTable(StringTable *table)
{
    const unsigned int count = static_cast<unsigned int>(table->rowCount)
        * static_cast<unsigned int>(table->columnCount);
    s_sortTable = table;
    StringTable_SortCells_SP(table->cellIndex, count, sizeof(*table->cellIndex),
        (int (__cdecl *)(const void *, const void *))CellCompare);
    s_sortTable = nullptr;
}
#endif

int __cdecl CellCompare(__int16 *a, __int16 *b)
{
    __int16 cellIndexB; // [esp+8h] [ebp-10h]
    __int16 cellIndexA; // [esp+Ch] [ebp-Ch]

    cellIndexA = *a;
    cellIndexB = *b;
    if ( !s_sortTable
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\com_stringtable_obj.cpp",
                    43,
                    0,
                    "%s",
                    "s_sortTable != NULL") )
    {
        __debugbreak();
    }
    if ( cellIndexA >= s_sortTable->columnCount * s_sortTable->rowCount
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\com_stringtable_obj.cpp",
                    44,
                    0,
                    "%s",
                    "cellIndexA < ( s_sortTable->rowCount * s_sortTable->columnCount )") )
    {
        __debugbreak();
    }
    if ( cellIndexB >= s_sortTable->columnCount * s_sortTable->rowCount
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\com_stringtable_obj.cpp",
                    45,
                    0,
                    "%s",
                    "cellIndexB < ( s_sortTable->rowCount * s_sortTable->columnCount )") )
    {
        __debugbreak();
    }
    if ( s_sortTable->values[cellIndexA].hash == s_sortTable->values[cellIndexB].hash )
        return cellIndexA % s_sortTable->columnCount - cellIndexB % s_sortTable->columnCount;
    else
#ifdef KISAK_SP
        return s_sortTable->values[cellIndexA].hash > s_sortTable->values[cellIndexB].hash ? 1 : -1;
#else
        return s_sortTable->values[cellIndexA].hash - s_sortTable->values[cellIndexB].hash;
#endif
}

void __cdecl StringTable_GetAsset_LoadObj(const char *filename, StringTable **tablePtr)
{
    StringTable *table; // [esp+0h] [ebp-4h]

    table = (StringTable *)Hunk_FindDataForFile(12, filename);
    if ( !table )
    {
        table = (StringTable *)StringTable_Alloc(0x14u);
        table->name = 0;
        table->columnCount = 0;
        table->rowCount = 0;
        table->values = 0;
        table->cellIndex = 0;
        StringTable_ParseFileIntoTable(filename, table);
        Hunk_SetDataForFile(12, filename, table, (void *(__cdecl *)(int))StringTable_Alloc);
    }
    *tablePtr = table;
}

