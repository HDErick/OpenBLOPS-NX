#include "cscr_save_write.h"

#ifdef KISAK_SP
#include "cscr_variable.h"
#include "cscr_animtree.h"
#include "cscr_readwrite.h"
#include "cscr_stringlist.h"
#include <universal/memfile.h>
#include <qcommon/common.h>
#include <cstring>

static constexpr unsigned int SCR_SAVE_PARENT_CAPACITY = 0x7FFE;
static constexpr unsigned int SCR_SAVE_CHILD_BEGIN = 0x8000;
static thread_local unsigned int s_writeSaveIdHead;
static thread_local unsigned short s_writeSaveIdHistory[16];

static void Scr_QueueSaveObject(scriptInstance_t inst, unsigned int id)
{
    scrVarPub_t &pub = gScrVarPub[inst];
    if (id >= SCR_SAVE_PARENT_CAPACITY)
        Com_Error(ERR_DROP, "Checkpoint: script object ID out of range");
    if (id && !pub.saveIdMap[id])
    {
        if (pub.savecount + 1 >= SCR_SAVE_PARENT_CAPACITY)
            Com_Error(ERR_DROP, "Checkpoint: script object table is full");
        pub.saveIdMap[id] = pub.savecount + 1;
        pub.saveIdMapRev[pub.savecount + 1] = (unsigned short)id;
        ++pub.savecount;
    }
}

static void Scr_PrepareSaveValue(scriptInstance_t inst, unsigned int type, VariableUnion value);

static void Scr_PrepareSaveStack(scriptInstance_t inst, const VariableStackBuffer *stack)
{
    Scr_QueueSaveObject(inst, stack->localId);
    const char *entry = stack->buf;
    for (unsigned int i = 0; i < stack->size; ++i)
    {
        unsigned int type = (unsigned char)*entry++;
        VariableUnion value;
        memcpy(&value, entry, sizeof(value));
        entry += sizeof(value);
        Scr_PrepareSaveValue(inst, type, value);
    }
}

static void Scr_PrepareSaveValue(scriptInstance_t inst, unsigned int type, VariableUnion value)
{
    if (type == VAR_POINTER)
        Scr_QueueSaveObject(inst, value.pointerValue);
    else if (type == VAR_STACK)
        Scr_PrepareSaveStack(inst, value.stackValue);
}

static void Scr_PrepareSaveObject(scriptInstance_t inst, unsigned int id)
{
    VariableValueInternal *list = gScrVarGlob[inst].variableList;
    const VariableValueInternal &object = list[id + VARIABLELIST_PARENT_BEGIN];
    unsigned int type = object.w.type & VAR_MASK;
    for (unsigned int index = FindLastSibling(inst, id); index;
         index = list[SCR_SAVE_CHILD_BEGIN + index].hash.u.prevSibling)
    {
        const VariableValueInternal &child = list[SCR_SAVE_CHILD_BEGIN +
            list[SCR_SAVE_CHILD_BEGIN + index].hash.id];
        if (type == VAR_ARRAY)
        {
            unsigned int name = child.w.name >> VAR_NAME_BITS;
            if (name >= 0x10000 && name < 0x10000 + SCR_SAVE_PARENT_CAPACITY)
                Scr_QueueSaveObject(inst, name - 0x10000);
        }
        Scr_PrepareSaveValue(inst, child.w.type & VAR_MASK, child.u.u);
    }
    if (type == VAR_CHILD_THREAD)
        Scr_QueueSaveObject(inst, (unsigned short)(object.w.parentLocalId >> VAR_NAME_BITS));
    if (type == VAR_THREAD || type == VAR_NOTIFY_THREAD || type == VAR_TIME_THREAD ||
        type == VAR_CHILD_THREAD || type == VAR_DEAD_ENTITY)
        Scr_QueueSaveObject(inst, object.u.o.u.self);
}

static void Scr_PrepareSaveRoot(scriptInstance_t inst, unsigned int id)
{
    unsigned int processed = gScrVarPub[inst].savecount;
    Scr_QueueSaveObject(inst, id);
    while (processed < gScrVarPub[inst].savecount)
        Scr_PrepareSaveObject(inst, gScrVarPub[inst].saveIdMapRev[++processed]);
}

unsigned short Scr_WriteSaveId(unsigned short id, scriptInstance_t inst)
{
    if (!id) return 0;
    Scr_PrepareSaveRoot(inst, id);
    return gScrVarPub[inst].saveIdMap[id];
}

unsigned short Scr_ReadSaveId(unsigned short saveId, scriptInstance_t inst)
{
    if (!saveId) return 0;
    unsigned short id = gScrVarPub[inst].saveIdMapRev[saveId];
    AddRefToObject(inst, id);
    return id;
}

static void Scr_PrepareSaveRootStack(scriptInstance_t inst, const VariableStackBuffer *stack)
{
    Scr_PrepareSaveRoot(inst, stack->localId);
    const char *entry = stack->buf;
    for (unsigned int i = 0; i < stack->size; ++i)
    {
        unsigned int type = (unsigned char)*entry++;
        VariableUnion value;
        memcpy(&value, entry, sizeof(value));
        entry += sizeof(value);
        if (type == VAR_POINTER)
            Scr_PrepareSaveRoot(inst, value.pointerValue);
        else if (type == VAR_STACK)
            Scr_PrepareSaveRootStack(inst, value.stackValue);
    }
}

void Scr_PrepareSave(scriptInstance_t inst)
{
    scrVarPub_t &pub = gScrVarPub[inst];
    memset(pub.saveIdMap, 0, SCR_SAVE_PARENT_CAPACITY * sizeof(*pub.saveIdMap));
    memset(pub.saveIdMapRev, 0, SCR_SAVE_PARENT_CAPACITY * sizeof(*pub.saveIdMapRev));
    pub.savecount = 0;
    Scr_PrepareSaveRoot(inst, pub.levelId);
    Scr_PrepareSaveRoot(inst, pub.animId);
    Scr_PrepareSaveRoot(inst, pub.timeArrayId);
    Scr_PrepareSaveRoot(inst, pub.pauseArrayId);
    Scr_PrepareSaveRoot(inst, pub.freeEntList);
    for (unsigned int i = 0; i < CLASS_NUM_COUNT; ++i)
    {
        Scr_PrepareSaveRoot(inst, gScrClassMap[inst][i].id);
        Scr_PrepareSaveRoot(inst, gScrClassMap[inst][i].entArrayId);
    }
    const VariableValueInternal &game = gScrVarGlob[inst].variableList[SCR_SAVE_CHILD_BEGIN + pub.gameId];
    unsigned int type = game.w.type & VAR_MASK;
    if (type == VAR_POINTER)
        Scr_PrepareSaveRoot(inst, game.u.u.pointerValue);
    else if (type == VAR_STACK)
        Scr_PrepareSaveRootStack(inst, game.u.u.stackValue);
}

static void Scr_WriteSaveByte(MemoryFile *memFile, unsigned char value)
{
    MemFile_WriteData(memFile, sizeof(value), &value);
}

static void Scr_WriteSaveShort(MemoryFile *memFile, unsigned short value)
{
    MemFile_WriteData(memFile, sizeof(value), (unsigned char *)&value);
}

static void Scr_WriteSaveInt(MemoryFile *memFile, int value)
{
    MemFile_WriteData(memFile, sizeof(value), (unsigned char *)&value);
}

static unsigned int Scr_FindWriteSaveIdCode(unsigned int saveId)
{
    unsigned int code = 1;
    for (unsigned int n = 1; n <= 16; ++n, code += 2)
    {
        unsigned int id = s_writeSaveIdHistory[(s_writeSaveIdHead + n) & 15];
        if (saveId == id + 1)
            return code;
        if (saveId == id)
            return code + 1;
    }
    return 0;
}

static void Scr_WriteObjectReference(scriptInstance_t inst, unsigned int id, unsigned int tag, MemoryFile *memFile)
{
    unsigned int saveId = gScrVarPub[inst].saveIdMap[id];
    unsigned int code = Scr_FindWriteSaveIdCode(saveId) << 3;
    if (code >= 256)
        code = 0;
    Scr_WriteSaveByte(memFile, (unsigned char)(code + tag));
    if (!code)
        Scr_WriteSaveShort(memFile, (unsigned short)saveId);
    s_writeSaveIdHistory[s_writeSaveIdHead] = (unsigned short)saveId;
    s_writeSaveIdHead = (s_writeSaveIdHead - 1) & 15;
}

static void Scr_WriteSaveString(scriptInstance_t inst, unsigned int string, MemoryFile *memFile)
{
    MemFile_WriteCString(memFile, const_cast<char *>(SL_ConvertToString((unsigned short)string, inst)));
}

static void Scr_WriteSaveCodePos(scriptInstance_t inst, const char *pos, MemoryFile *memFile)
{
    const auto &pub = gScrVarPub[inst];
    if (pos && (pos < pub.programBuffer || pos >= pub.endScriptBuffer))
        Com_Error(ERR_DROP, "Checkpoint: script code position out of range");
    const int offset = pos ? (int)(pos - pub.programBuffer) : -1;
    Scr_WriteSaveInt(memFile, offset);
}

static void Scr_WriteSaveValue(scriptInstance_t inst, unsigned int type, VariableUnion value, MemoryFile *memFile);

static void Scr_WriteSaveStack(scriptInstance_t inst, const VariableStackBuffer *stack, MemoryFile *memFile)
{
    Scr_WriteSaveShort(memFile, stack->size);
    Scr_WriteSaveCodePos(inst, stack->pos, memFile);
    Scr_WriteObjectReference(inst, stack->localId, 0, memFile);
    Scr_WriteSaveByte(memFile, stack->time);
    const char *entry = stack->buf;
    for (unsigned int i = 0; i < stack->size; ++i)
    {
        unsigned int type = (unsigned char)*entry++;
        VariableUnion value;
        memcpy(&value, entry, sizeof(value));
        entry += sizeof(value);
        Scr_WriteSaveValue(inst, type, value, memFile);
    }
}

static void Scr_WriteSaveValue(scriptInstance_t inst, unsigned int type, VariableUnion value, MemoryFile *memFile)
{
    if (type == VAR_POINTER)
    {
        Scr_WriteObjectReference(inst, value.pointerValue, 1, memFile);
        return;
    }
    Scr_WriteSaveByte(memFile, (unsigned char)(type << 3));
    switch (type)
    {
    case VAR_STRING:
    case VAR_ISTRING:
        Scr_WriteSaveString(inst, value.stringValue, memFile);
        break;
    case VAR_VECTOR:
        MemFile_WriteData(memFile, 3 * sizeof(float), (unsigned char *)value.vectorValue);
        break;
    case VAR_FLOAT:
        MemFile_WriteFloat(memFile, value.floatValue);
        break;
    case VAR_INTEGER:
    case VAR_ANIMATION:
        Scr_WriteSaveInt(memFile, value.intValue);
        break;
    case VAR_CODEPOS:
    case VAR_FUNCTION:
        Scr_WriteSaveCodePos(inst, value.codePosValue, memFile);
        break;
    case VAR_STACK:
        Scr_WriteSaveStack(inst, value.stackValue, memFile);
        break;
    }
}

static void Scr_WriteSaveVariable(scriptInstance_t inst, unsigned int type, VariableUnion value,
    unsigned int name, bool array, MemoryFile *memFile)
{
    Scr_WriteSaveValue(inst, type, value, memFile);
    if (!array)
    {
        unsigned int extra = name < 0x40 ? 0 : name < 0x4000 ? 1 : name < 0x400000 ? 2 : 3;
        Scr_WriteSaveByte(memFile, (unsigned char)((name << 2) | extra));
        for (unsigned int i = 0; i < extra; ++i)
            Scr_WriteSaveByte(memFile, (unsigned char)(name >> (6 + i * 8)));
        return;
    }
    if (name < 0x10000)
    {
        Scr_WriteSaveByte(memFile, 4);
        Scr_WriteSaveString(inst, name, memFile);
    }
    else if (name < 0x10000 + SCR_SAVE_PARENT_CAPACITY)
    {
        Scr_WriteObjectReference(inst, name - 0x10000, 5, memFile);
    }
    else
    {
        int index = (int)(name - 0x800000);
        // The compact byte only holds five unsigned bits. The WIP writer
        // encoded negative/large indices here and lost their sign/high bits.
        // Use the reader's full signed representation for every other index.
        if (index >= 0 && index < 32)
            Scr_WriteSaveByte(memFile, (unsigned char)((unsigned int)index << 3));
        else
        {
            Scr_WriteSaveByte(memFile, 3);
            Scr_WriteSaveInt(memFile, index);
        }
    }
}

static void Scr_WriteSaveObject(scriptInstance_t inst, unsigned int id, MemoryFile *memFile)
{
    VariableValueInternal *list = gScrVarGlob[inst].variableList;
    const VariableValueInternal &object = list[id + VARIABLELIST_PARENT_BEGIN];
    unsigned int type = object.w.type & VAR_MASK;
    switch (type)
    {
    case VAR_THREAD:
        Scr_WriteObjectReference(inst, object.u.o.u.self, 1, memFile);
        break;
    case VAR_NOTIFY_THREAD:
    {
        Scr_WriteObjectReference(inst, object.u.o.u.self, 2, memFile);
        unsigned short name = (unsigned short)(object.w.notifyName >> VAR_NAME_BITS);
        Scr_WriteSaveByte(memFile, name != 0);
        if (name)
            Scr_WriteSaveString(inst, name, memFile);
        break;
    }
    case VAR_TIME_THREAD:
        Scr_WriteObjectReference(inst, object.u.o.u.self, 3, memFile);
        Scr_WriteSaveInt(memFile, object.w.waitTime >> VAR_NAME_BITS);
        break;
    case VAR_CHILD_THREAD:
        Scr_WriteObjectReference(inst, object.u.o.u.self, 4, memFile);
        Scr_WriteObjectReference(inst, (unsigned short)(object.w.parentLocalId >> VAR_NAME_BITS), 0, memFile);
        break;
    case VAR_DEAD_ENTITY:
        Scr_WriteObjectReference(inst, object.u.o.u.self, 5, memFile);
        break;
    case VAR_ENTITY:
        Scr_WriteSaveByte(memFile, VAR_ENTITY << 3);
        Scr_WriteSaveShort(memFile, object.u.o.u.entnum);
        Scr_WriteSaveShort(memFile, (unsigned short)(object.w.classnum >> VAR_NAME_BITS));
        break;
    default:
        Scr_WriteSaveByte(memFile, (unsigned char)(type << 3));
        break;
    }
    unsigned int count = 0;
    for (unsigned int child = FindFirstSibling(inst, id); child; child = FindNextSibling(inst, child))
        ++count;
    Scr_WriteSaveShort(memFile, (unsigned short)count);
    for (unsigned int index = FindLastSibling(inst, id); index;
         index = list[SCR_SAVE_CHILD_BEGIN + index].hash.u.prevSibling)
    {
        const VariableValueInternal &child = list[SCR_SAVE_CHILD_BEGIN +
            list[SCR_SAVE_CHILD_BEGIN + index].hash.id];
        Scr_WriteSaveVariable(inst, child.w.type & VAR_MASK, child.u.u,
            child.w.name >> VAR_NAME_BITS, type == VAR_ARRAY, memFile);
    }
}

void Scr_SaveVariables(scriptInstance_t inst, MemoryFile *memFile)
{
    scrVarPub_t &pub = gScrVarPub[inst];
    memset(s_writeSaveIdHistory, 0, sizeof(s_writeSaveIdHistory));
    s_writeSaveIdHead = 0;
    Scr_WriteSaveInt(memFile, pub.time);
    Scr_WriteSaveShort(memFile, pub.savecount);
    for (unsigned int i = 1; i <= pub.savecount; ++i)
        Scr_WriteSaveObject(inst, pub.saveIdMapRev[i], memFile);
    const VariableValueInternal &game = gScrVarGlob[inst].variableList[SCR_SAVE_CHILD_BEGIN + pub.gameId];
    Scr_WriteSaveValue(inst, game.w.type & VAR_MASK, game.u.u, memFile);
    Scr_WriteObjectReference(inst, pub.levelId, 0, memFile);
    Scr_WriteObjectReference(inst, pub.animId, 0, memFile);
    Scr_WriteObjectReference(inst, pub.timeArrayId, 0, memFile);
    Scr_WriteObjectReference(inst, pub.pauseArrayId, 0, memFile);
    Scr_WriteObjectReference(inst, pub.freeEntList, 0, memFile);
    for (unsigned int i = 0; i < CLASS_NUM_COUNT; ++i)
    {
        Scr_WriteObjectReference(inst, gScrClassMap[inst][i].id, 0, memFile);
        Scr_WriteObjectReference(inst, gScrClassMap[inst][i].entArrayId, 0, memFile);
    }
}
#endif
