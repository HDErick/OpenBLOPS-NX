#include "cscr_save.h"

#ifdef KISAK_SP
#include "cscr_variable.h"
#include "cscr_animtree.h"
#include "cscr_debugger.h"
#include "cscr_memorytree.h"
#include "cscr_stringlist.h"
#include <universal/memfile.h>
#include <qcommon/common.h>
#include <cstring>

static thread_local unsigned int s_saveIdHead;
static thread_local unsigned short s_saveIdHistory[16];

static unsigned char Scr_ReadSaveByte(MemoryFile *memFile)
{
    unsigned char value;
    MemFile_ReadData(memFile, sizeof(value), &value);
    return value;
}

static unsigned short Scr_ReadSaveShort(MemoryFile *memFile)
{
    unsigned short value;
    MemFile_ReadData(memFile, sizeof(value), (unsigned char *)&value);
    return value;
}

static int Scr_ReadSaveInt(MemoryFile *memFile)
{
    int value;
    MemFile_ReadData(memFile, sizeof(value), (unsigned char *)&value);
    return value;
}

static const char *Scr_ReadSaveCodePos(scriptInstance_t inst, MemoryFile *memFile)
{
    int offset = Scr_ReadSaveInt(memFile);
    const auto &pub = gScrVarPub[inst];
    if (offset < -1 || (offset >= 0 &&
        static_cast<unsigned int>(offset) >= static_cast<unsigned int>(pub.endScriptBuffer - pub.programBuffer)))
        Com_Error(ERR_DROP, "Checkpoint: saved script code position out of range");
    return offset == -1 ? nullptr : pub.programBuffer + offset;
}

static unsigned int Scr_ReadSaveString(scriptInstance_t inst, MemoryFile *memFile)
{
    char *string = MemFile_ReadCString(memFile);
    return (unsigned short)SL_GetStringOfSize(inst, string, 0, strlen(string) + 1, 15);
}

unsigned int Scr_ReadObjectReference(scriptInstance_t inst, MemoryFile *memFile, unsigned int header)
{
    unsigned int code = header >> 3;
    int saveId;
    if (code)
        saveId = s_saveIdHistory[(s_saveIdHead + ((code + 1) >> 1)) & 15] + (code & 1);
    else
        saveId = (short)Scr_ReadSaveShort(memFile);

    if (saveId < 0 || saveId > gScrVarPub[inst].savecount)
        Com_Error(ERR_DROP, "Checkpoint: saved script object reference out of range");
    unsigned int id = gScrVarPub[inst].saveIdMapRev[saveId];
    if (id)
        AddRefToObject(inst, id);
    s_saveIdHistory[s_saveIdHead] = (unsigned short)saveId;
    s_saveIdHead = (s_saveIdHead - 1) & 15;
    return id;
}

static void Scr_ReadSaveValue(scriptInstance_t inst, VariableValue *value, MemoryFile *memFile);

static VariableStackBuffer *Scr_ReadSaveStack(scriptInstance_t inst, MemoryFile *memFile)
{
    unsigned short size = Scr_ReadSaveShort(memFile);
    int bufLen = SCR_STACKBUF_HEADER + SCR_STACKBUF_ENTRY * size;
    if (bufLen > 0xFFFF)
        Com_Error(ERR_DROP, "Checkpoint: saved script stack is too large");
    VariableStackBuffer *stack = (VariableStackBuffer *)MT_Alloc(bufLen, 1, inst);
    ++gScrVarPub[inst].numScriptThreads;
    stack->size = size;
    stack->bufLen = bufLen;
    stack->pos = Scr_ReadSaveCodePos(inst, memFile);
    stack->localId = Scr_ReadObjectReference(inst, memFile, Scr_ReadSaveByte(memFile));
    stack->time = Scr_ReadSaveByte(memFile);
    char *entry = stack->buf;
    for (unsigned int i = 0; i < size; ++i)
    {
        VariableValue value = {};
        Scr_ReadSaveValue(inst, &value, memFile);
        *entry++ = (unsigned char)value.type;
        memcpy(entry, &value.u, sizeof(value.u));
        entry += sizeof(value.u);
    }
    return stack;
}

static void Scr_ReadSaveValue(scriptInstance_t inst, VariableValue *value, MemoryFile *memFile)
{
    unsigned int header = Scr_ReadSaveByte(memFile);
    if (header & 7)
    {
        value->type = VAR_POINTER;
        value->u.pointerValue = Scr_ReadObjectReference(inst, memFile, header);
        return;
    }

    value->type = header >> 3;
    switch (value->type)
    {
    case VAR_STRING:
    case VAR_ISTRING:
        value->u.stringValue = Scr_ReadSaveString(inst, memFile);
        break;
    case VAR_VECTOR:
    {
        float vector[3];
        for (unsigned int i = 0; i < 3; ++i)
            MemFile_ReadData(memFile, sizeof(float), (unsigned char *)&vector[i]);
        value->u.vectorValue = Scr_AllocVector(inst, vector);
        break;
    }
    case VAR_FLOAT:
    {
        float number;
        MemFile_ReadData(memFile, sizeof(number), (unsigned char *)&number);
        value->u.floatValue = number;
        break;
    }
    case VAR_INTEGER:
    case VAR_ANIMATION:
        value->u.intValue = Scr_ReadSaveInt(memFile);
        break;
    case VAR_CODEPOS:
    case VAR_FUNCTION:
        value->u.codePosValue = Scr_ReadSaveCodePos(inst, memFile);
        break;
    case VAR_STACK:
        value->u.stackValue = Scr_ReadSaveStack(inst, memFile);
        break;
    }
}

static unsigned int Scr_ReadSaveVariable(scriptInstance_t inst, VariableValue *value, bool array, MemoryFile *memFile)
{
    Scr_ReadSaveValue(inst, value, memFile);
    unsigned int header = Scr_ReadSaveByte(memFile);
    if (!array)
    {
        unsigned int name = header >> 2;
        unsigned int bytes = header & 3;
        for (unsigned int i = 0; i < bytes; ++i)
            name |= (unsigned int)Scr_ReadSaveByte(memFile) << (6 + i * 8);
        return name;
    }

    switch (header & 7)
    {
    case 0:
        return (header >> 3) + 0x800000;
    case 1:
        return ((header >> 3) << 8 | Scr_ReadSaveByte(memFile)) + 0x800000;
    case 2:
        return (((header >> 3) << 16) | (int)(short)Scr_ReadSaveShort(memFile)) + 0x800000;
    case 3:
        return (unsigned int)Scr_ReadSaveInt(memFile) + 0x800000;
    case 4:
        return Scr_ReadSaveString(inst, memFile);
    case 5:
        return Scr_ReadObjectReference(inst, memFile, header) + 0x10000;
    default:
        return 0;
    }
}

static void Scr_ReadSaveObject(scriptInstance_t inst, unsigned int id, MemoryFile *memFile)
{
    VariableValueInternal *object = &gScrVarGlob[inst].variableList[id + VARIABLELIST_PARENT_BEGIN];
    unsigned int header = Scr_ReadSaveByte(memFile);
    unsigned int type;
    switch (header & 7)
    {
    case 1:
        type = VAR_THREAD;
        object->u.o.u.self = Scr_ReadObjectReference(inst, memFile, header);
        break;
    case 2:
        type = VAR_NOTIFY_THREAD;
        object->u.o.u.self = Scr_ReadObjectReference(inst, memFile, header);
        if (Scr_ReadSaveByte(memFile))
            object->w.notifyName |= Scr_ReadSaveString(inst, memFile) << VAR_NAME_BITS;
        break;
    case 3:
        type = VAR_TIME_THREAD;
        object->u.o.u.self = Scr_ReadObjectReference(inst, memFile, header);
        object->w.waitTime |= (unsigned int)Scr_ReadSaveInt(memFile) << VAR_NAME_BITS;
        break;
    case 4:
        type = VAR_CHILD_THREAD;
        object->u.o.u.self = Scr_ReadObjectReference(inst, memFile, header);
        object->w.parentLocalId |= Scr_ReadObjectReference(inst, memFile, Scr_ReadSaveByte(memFile)) << VAR_NAME_BITS;
        break;
    case 5:
        type = VAR_DEAD_ENTITY;
        object->u.o.u.self = Scr_ReadObjectReference(inst, memFile, header);
        break;
    default:
        type = header >> 3;
        if (type == VAR_ENTITY)
        {
            object->u.o.u.entnum = Scr_ReadSaveShort(memFile);
            object->w.classnum |= (unsigned int)(int)(short)Scr_ReadSaveShort(memFile) << VAR_NAME_BITS;
        }
        else if (type == VAR_ARRAY)
        {
            object->u.o.u.size = 0;
        }
        break;
    }
    object->w.type = (object->w.type & ~VAR_MASK) | type;

    unsigned int count = Scr_ReadSaveShort(memFile);
    for (unsigned int i = 0; i < count; ++i)
    {
        VariableValue value = {};
        unsigned int name = Scr_ReadSaveVariable(inst, &value, type == VAR_ARRAY, memFile);
        unsigned int child = GetVariable(inst, id, name);
        if (type == VAR_ARRAY)
        {
            VariableValue key = Scr_GetArrayIndexValue(inst, name);
            RemoveRefToValue(inst, key.type, key.u);
        }
        SetNewVariableValue(inst, child, &value);
    }
}

void Scr_LoadVariables(scriptInstance_t inst, MemoryFile *memFile)
{
    // Native checkpoint allocation occurs outside an executing script frame.
    // The real VM allocator requires an attribution string for debug ownership.
    const char* previousUsage = gScrVarPub[inst].varUsagePos;
    gScrVarPub[inst].varUsagePos = "<checkpoint restore>";
    gScrVarPub[inst].time = Scr_ReadSaveInt(memFile);
    gScrVarPub[inst].savecount = Scr_ReadSaveShort(memFile);
    if (gScrVarPub[inst].savecount >= 0x7FFE)
        Com_Error(ERR_DROP, "Checkpoint: saved script object count out of range");
    memset(gScrVarPub[inst].saveIdMap, 0, 0xFFFC);
    memset(gScrVarPub[inst].saveIdMapRev, 0, 0xFFFC);
    memset(s_saveIdHistory, 0, sizeof(s_saveIdHistory));
    s_saveIdHead = 0;
    for (unsigned int i = 1; i <= gScrVarPub[inst].savecount; ++i)
    {
        unsigned int id = AllocObject(inst);
        gScrVarPub[inst].saveIdMapRev[i] = id;
        gScrVarPub[inst].saveIdMap[id] = i;
    }
    for (unsigned int i = 1; i <= gScrVarPub[inst].savecount; ++i)
        Scr_ReadSaveObject(inst, gScrVarPub[inst].saveIdMapRev[i], memFile);

    gScrVarPub[inst].gameId = AllocValue(inst);
    VariableValue game = {};
    Scr_ReadSaveValue(inst, &game, memFile);
    SetNewVariableValue(inst, gScrVarPub[inst].gameId, &game);
    gScrVarPub[inst].levelId = Scr_ReadObjectReference(inst, memFile, Scr_ReadSaveByte(memFile));
    gScrVarPub[inst].animId = Scr_ReadObjectReference(inst, memFile, Scr_ReadSaveByte(memFile));
    gScrVarPub[inst].timeArrayId = Scr_ReadObjectReference(inst, memFile, Scr_ReadSaveByte(memFile));
    gScrVarPub[inst].pauseArrayId = Scr_ReadObjectReference(inst, memFile, Scr_ReadSaveByte(memFile));
    gScrVarPub[inst].freeEntList = Scr_ReadObjectReference(inst, memFile, Scr_ReadSaveByte(memFile));
    for (unsigned int i = 0; i < CLASS_NUM_COUNT; ++i)
    {
        gScrClassMap[inst][i].id = Scr_ReadObjectReference(inst, memFile, Scr_ReadSaveByte(memFile));
        gScrClassMap[inst][i].entArrayId = Scr_ReadObjectReference(inst, memFile, Scr_ReadSaveByte(memFile));
    }
    gScrVarPub[inst].varUsagePos = previousUsage;
}

void Scr_PostLoadVariables(scriptInstance_t inst)
{
    for (unsigned int i = 1; i <= gScrVarPub[inst].savecount; ++i)
        RemoveRefToObject(inst, gScrVarPub[inst].saveIdMapRev[i]);
}
#endif
