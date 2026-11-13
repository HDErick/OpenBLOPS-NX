#include "memfile.h"
#include "com_memory.h"
#include <qcommon/common.h>
#include <live/live.h>
#include "physicalmemory.h"
#include <minilzo/minilzo.h>

bool s_isLzoInitialized;

void __cdecl MemFile_CommonInit(
                MemoryFile *memFile,
                int size,
                unsigned __int8 *buffer,
                bool errorOnOverflow,
                bool compress_enabled)
{
    if ( !s_isLzoInitialized )
    {
        lzo_init();
        s_isLzoInitialized = 1;
    }
    if ( !memFile
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp", 26, 0, "%s", "memFile") )
    {
        __debugbreak();
    }
    if ( !buffer && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp", 27, 0, "%s", "buffer") )
        __debugbreak();
    if ( size <= 0
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    28,
                    0,
                    "%s\n\t(size) = %i",
                    "(size > 0)",
                    size) )
    {
        __debugbreak();
    }
    memFile->buffer = buffer;
    memFile->bufferSize = size;
    memFile->bytesUsed = 0;
    memFile->errorOnOverflow = errorOnOverflow;
    memFile->memoryOverflow = 0;
    memFile->segmentIndex = -1;
    memFile->segmentStart = 0;
    memFile->compress_enabled = compress_enabled;
    memFile->rle_enabled = 1;
    memFile->cacheBufferUsed = 0;
    if ( compress_enabled )
        memFile->rle_enabled = 0;
}

void __cdecl MemFile_InitForReading(MemoryFile *memFile, int size, unsigned __int8 *buffer, bool compress_enabled)
{
    MemFile_CommonInit(memFile, size, buffer, 1, compress_enabled);
    memFile->is_writing = 0;
    memFile->zeroCount = 0;
    memFile->nonZeroCount = 0;
    memFile->bytesUsed = 0;
    MemFile_MoveToSegment(memFile, 0);
}

void __cdecl MemFile_InitForWriting(
                MemoryFile *memFile,
                int size,
                unsigned __int8 *buffer,
                bool errorOnOverflow,
                bool compress_enabled)
{
    MemFile_CommonInit(memFile, size, buffer, errorOnOverflow, compress_enabled);
    memFile->is_writing = 1;
    memFile->zeroCount = 0;
    memFile->nonZeroCount = 0;
    memFile->bytesUsed = 0;
    MemFile_StartSegment(memFile, 0);
}

void __cdecl MemFile_StartSegment(MemoryFile *memFile, int index)
{
    int segmentIndex; // [esp+0h] [ebp-4h]

    if ( (index < -1 || index >= 6)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    360,
                    0,
                    "%s\n\t(index) = %i",
                    "((index >= -1) && (index < SAVE_SEGMENT_COUNT))",
                    index) )
    {
        __debugbreak();
    }
    if ( !memFile->memoryOverflow )
    {
        segmentIndex = memFile->segmentIndex;
        if ( segmentIndex < 0 || (MemFile_EndSegment(memFile), !memFile->memoryOverflow) )
        {
            memFile->segmentIndex = index;
            if ( index >= 0 )
            {
                if ( segmentIndex + 1 != index
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                                383,
                                0,
                                "%s",
                                "segmentIndex + 1 == index") )
                {
                    __debugbreak();
                }
                memFile->segmentStart = memFile->bytesUsed;
                memFile->bytesUsed += 4;
                if ( memFile->memoryOverflow
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                                399,
                                0,
                                "%s",
                                "!memFile->memoryOverflow") )
                {
                    __debugbreak();
                }
                //BLOPS_NULLSUB();
            }
            else
            {
                memFile->bufferSize = memFile->bytesUsed;
            }
        }
    }
}

static bool MemFile_WriteCompressedCache(MemoryFile *memFile, unsigned __int8 *workmem)
{
    static_assert(sizeof(unsigned int) == 4, "MemoryFile lengths are four bytes");
    if (memFile->bytesUsed < 0 || memFile->bytesUsed > memFile->bufferSize
        || memFile->cacheBufferUsed < 0
        || memFile->cacheBufferUsed > sizeof(memFile->cacheBuffer)
        || memFile->bufferSize - memFile->bytesUsed < 4)
    {
        if (memFile->errorOnOverflow)
            Com_Error(ERR_DROP, "MemFile_EndSegment: Out of memory");
        Com_Printf(10, "MemFile_EndSegment: Out of memory");
        memFile->memoryOverflow = 1;
        return false;
    }

    LargeLocal compressed(sizeof(memFile->cacheBuffer) * 2 + 64);
    lzo_uint outputLength = 0;
    const int result = lzo1x_1_compress(memFile->cacheBuffer,
        memFile->cacheBufferUsed, compressed.GetBuf(), &outputLength, workmem);
    if (result != LZO_E_OK || outputLength > UINT_MAX
        || outputLength > static_cast<lzo_uint>(memFile->bufferSize - memFile->bytesUsed - 4))
    {
        if (memFile->errorOnOverflow)
            Com_Error(ERR_DROP, "MemFile_EndSegment: Out of memory");
        Com_Printf(10, "MemFile_EndSegment: Out of memory");
        memFile->memoryOverflow = 1;
        return false;
    }

    const unsigned int blockLength = static_cast<unsigned int>(outputLength);
    memcpy(&memFile->buffer[memFile->bytesUsed], &blockLength, sizeof(blockLength));
    memFile->bytesUsed += sizeof(blockLength);
    memcpy(&memFile->buffer[memFile->bytesUsed], compressed.GetBuf(), blockLength);
    memFile->bytesUsed += static_cast<int>(blockLength);
    memFile->cacheBufferUsed = 0;
    return true;
}

void __cdecl MemFile_EndSegment(MemoryFile *memFile)
{
    LargeLocal workmem_large_local(memFile->compress_enabled ? LZO1X_1_MEM_COMPRESS : 0x2000);
    int err; // [esp+10h] [ebp-Ch]
    int len; // [esp+14h] [ebp-8h]
    int index; // [esp+18h] [ebp-4h]

    if ( memFile->memoryOverflow
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    410,
                    0,
                    "%s",
                    "!memFile->memoryOverflow") )
    {
        __debugbreak();
    }
    if ( !memFile->memoryOverflow )
    {
        index = memFile->segmentIndex;
        if ( (unsigned int)index >= 6
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                        417,
                        0,
                        "%s\n\t(index) = %i",
                        "((index >= 0) && (index < SAVE_SEGMENT_COUNT))",
                        index) )
        {
            __debugbreak();
        }
        if ( memFile->compress_enabled )
        {
            if ( memFile->cacheBufferUsed > 0 )
            {
                if (!MemFile_WriteCompressedCache(memFile, workmem_large_local.GetBuf()))
                    return;
            }
        }
        else
        {
            MemFile_WriteDataFlushInternal(memFile);
        }
        err = Live_CountGuestsInUse();
        if ( err
            && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp", 492, 0, "%s", "err == Z_OK") )
        {
            __debugbreak();
        }
        if ( memFile->memoryOverflow
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                        493,
                        0,
                        "%s\n\t%s",
                        "!memFile->memoryOverflow",
                        "Save Buffer Overflow") )
        {
            __debugbreak();
        }
        memFile->segmentIndex = -1;
        len = memFile->bytesUsed - memFile->segmentStart;
        *(unsigned int *)&memFile->buffer[memFile->segmentStart] = len;
    }
}

void __cdecl MemFile_MoveToSegment(MemoryFile *memFile, int index)
{
    if ( (index < -1 || index >= 6)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    570,
                    0,
                    "%s\n\t(index) = %i",
                    "((index >= -1) && (index < SAVE_SEGMENT_COUNT))",
                    index) )
    {
        __debugbreak();
    }
    if ( !memFile->memoryOverflow )
    {
        if ( memFile->segmentIndex >= 0
            && Live_CountGuestsInUse()
            && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp", 578, 0, "%s", "err == Z_OK") )
        {
            __debugbreak();
        }
        memFile->segmentIndex = index;
        if ( index >= 0 )
        {
            memFile->nonZeroCount = 0;
            memFile->zeroCount = 0;
            memFile->bytesUsed = MemFile_GetSegmentAddess(memFile, index) - memFile->buffer + 4;
            MemFile_inflateInit(memFile);
        }
    }
}

void __cdecl MemFile_inflateInit(MemoryFile *memFile)
{
    memFile->cacheBufferAvail = 0;
    memFile->cacheBufferUsed = 0;
}

unsigned __int8 *__cdecl MemFile_GetSegmentAddess(MemoryFile *memFile, unsigned int index)
{
    int segmentStart; // [esp+0h] [ebp-4h]

    if ( index >= 6
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    542,
                    0,
                    "%s\n\t(index) = %i",
                    "((index >= 0) && (index < SAVE_SEGMENT_COUNT))",
                    index) )
    {
        __debugbreak();
    }
    if ( memFile->memoryOverflow
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    543,
                    0,
                    "%s",
                    "!memFile->memoryOverflow") )
    {
        __debugbreak();
    }
    segmentStart = 0;
    while ( index )
    {
        if ( segmentStart + 4 > memFile->bufferSize
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                        548,
                        0,
                        "%s",
                        "segmentStart + static_cast< int >( sizeof( int ) ) <= memFile->bufferSize") )
        {
            __debugbreak();
        }
        segmentStart += *(unsigned int *)&memFile->buffer[segmentStart];
        --index;
    }
    if ( segmentStart + 4 > memFile->bufferSize
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    553,
                    0,
                    "%s",
                    "segmentStart + static_cast< int >( sizeof( int ) ) <= memFile->bufferSize") )
    {
        __debugbreak();
    }
    return &memFile->buffer[segmentStart];
}

bool __cdecl MemFile_IsReading(MemoryFile *memFile)
{
    if ( !memFile
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp", 623, 0, "%s", "memFile") )
    {
        __debugbreak();
    }
    return !memFile->is_writing;
}

void __cdecl MemFile_WriteDataFlushInternal(MemoryFile *memFile)
{
    LargeLocal workmem_large_local(memFile->compress_enabled ? LZO1X_1_MEM_COMPRESS : 0x2000);
    if ( !memFile->compress_enabled )
    {
        if ( memFile->cacheBufferUsed + memFile->bytesUsed <= memFile->bufferSize )
        {
            memcpy(&memFile->buffer[memFile->bytesUsed], memFile->cacheBuffer, memFile->cacheBufferUsed);
            memFile->bytesUsed += memFile->cacheBufferUsed;
        }
        else
        {
            if ( memFile->errorOnOverflow )
                Com_Error(ERR_DROP, "Trying to write corrupted file");
            Com_Printf(10, "Trying to write corrupted file");
            memFile->memoryOverflow = 1;
        }
        goto LABEL_14;
    }
    if ( memFile->cacheBufferUsed <= 0 )
    {
LABEL_14:
        memFile->cacheBufferUsed = 0;
        return;
    }
    MemFile_WriteCompressedCache(memFile, workmem_large_local.GetBuf());
}

void __cdecl MemFile_WriteDataInternal(MemoryFile *memFile, unsigned int byteCount, unsigned __int8 *p)
{
    unsigned int v3; // [esp+0h] [ebp-Ch]
    int bytesLeft; // [esp+8h] [ebp-4h]

    MemFile_WriteDataFlushInternal(memFile);
    if ( memFile->compress_enabled )
    {
        bytesLeft = byteCount;
        while ( bytesLeft > 0 )
        {
            if ( bytesLeft > 32760 )
                v3 = 32760;
            else
                v3 = bytesLeft;
            memcpy(&memFile->cacheBuffer[memFile->cacheBufferUsed], &p[byteCount - bytesLeft], v3);
            memFile->cacheBufferUsed += v3;
            bytesLeft -= v3;
            if ( bytesLeft > 0 )
                MemFile_WriteDataFlushInternal(memFile);
        }
    }
    else if ( (signed int)(byteCount + memFile->bytesUsed) <= memFile->bufferSize )
    {
        memcpy(&memFile->buffer[memFile->bytesUsed], p, byteCount);
        memFile->bytesUsed += byteCount;
    }
    else
    {
        if ( memFile->errorOnOverflow )
            Com_Error(ERR_DROP, "Trying to write corrupted file");
        Com_Printf(10, "Trying to write corrupted file");
        memFile->memoryOverflow = 1;
    }
}


static char buffer[1024];
char *__cdecl MemFile_ReadCString(MemoryFile *memFile)
{
    char *string; // [esp+0h] [ebp-4h]

    if ( !memFile
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp", 781, 0, "%s", "memFile") )
    {
        __debugbreak();
    }
    if ( !memFile->buffer
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp", 782, 0, "%s", "memFile->buffer") )
    {
        __debugbreak();
    }
    if ( (memFile->bytesUsed < 0 || memFile->bytesUsed > memFile->bufferSize)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    783,
                    0,
                    "memFile->bytesUsed not in [0, memFile->bufferSize]\n\t%i not in [%i, %i]",
                    memFile->bytesUsed,
                    0,
                    memFile->bufferSize) )
    {
        __debugbreak();
    }
    string = buffer;
    while ( 1 )
    {
        MemFile_ReadData(memFile, 1, (unsigned __int8 *)string);
        if ( memFile->memoryOverflow )
            return (char *)"";
        if ( !*string )
            break;
#ifdef KISAK_SP
        if ( ++string >= buffer + sizeof(buffer) )
#else
        if ( ++string >= (char *)&g_mem )
#endif
            Com_Error(ERR_DROP, "Trying to read corrupted file");
    }
    return buffer;
}

void __cdecl MemFile_ReadData(MemoryFile *memFile, int byteCount, unsigned __int8 *p)
{
    int cacheBufferAvail; // [esp+0h] [ebp-18h]
    unsigned int in_len; // [esp+8h] [ebp-10h]
    lzo_uint len;
    int size; // [esp+10h] [ebp-8h]
    int bytesLeft; // [esp+14h] [ebp-4h]

    if ( !memFile
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp", 803, 0, "%s", "memFile") )
    {
        __debugbreak();
    }
    if ( !MemFile_IsReading(memFile)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    804,
                    0,
                    "%s",
                    "MemFile_IsReading( memFile )") )
    {
        __debugbreak();
    }
    if ( !memFile->buffer
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp", 805, 0, "%s", "memFile->buffer") )
    {
        __debugbreak();
    }
    if ( (memFile->bytesUsed < 0 || memFile->bytesUsed > memFile->bufferSize)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    806,
                    0,
                    "memFile->bytesUsed not in [0, memFile->bufferSize]\n\t%i not in [%i, %i]",
                    memFile->bytesUsed,
                    0,
                    memFile->bufferSize) )
    {
        __debugbreak();
    }
    if ( byteCount < 0
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                    807,
                    0,
                    "%s\n\t(byteCount) = %i",
                    "(byteCount >= 0)",
                    byteCount) )
    {
        __debugbreak();
    }
    if ( byteCount && !memFile->memoryOverflow )
    {
        if ( memFile->compress_enabled )
        {
            bytesLeft = byteCount;
            while ( bytesLeft > 0 )
            {
                if ( memFile->cacheBufferUsed == memFile->cacheBufferAvail )
                {
                    if (memFile->bufferSize - memFile->bytesUsed < 4)
                    {
                        if (memFile->errorOnOverflow)
                            Com_Error(ERR_DROP, "Trying to read corrupted file");
                        Com_Printf(10, "Trying to read corrupted file");
                        memFile->memoryOverflow = 1;
                        return;
                    }
                    memcpy(&in_len, &memFile->buffer[memFile->bytesUsed], sizeof(in_len));
                    if (in_len > static_cast<unsigned int>(memFile->bufferSize - memFile->bytesUsed - 4))
                    {
                        if (memFile->errorOnOverflow)
                            Com_Error(ERR_DROP, "Trying to read corrupted file");
                        Com_Printf(10, "Trying to read corrupted file");
                        memFile->memoryOverflow = 1;
                        return;
                    }
                    len = sizeof(memFile->cacheBuffer);
                    const int result = lzo1x_decompress_safe(&memFile->buffer[memFile->bytesUsed + 4],
                        in_len, memFile->cacheBuffer, &len, NULL);
                    if (result != LZO_E_OK || len == 0 || len > sizeof(memFile->cacheBuffer))
                    {
                        if (memFile->errorOnOverflow)
                            Com_Error(ERR_DROP, "Trying to read corrupted file");
                        Com_Printf(10, "Trying to read corrupted file");
                        memFile->memoryOverflow = 1;
                        return;
                    }
                    memFile->cacheBufferAvail = static_cast<int>(len);
                    memFile->cacheBufferUsed = 0;
                    memFile->bytesUsed += 4 + static_cast<int>(in_len);
                }
                if ( memFile->cacheBufferUsed >= memFile->cacheBufferAvail
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                                849,
                                0,
                                "%s",
                                "memFile->cacheBufferUsed<memFile->cacheBufferAvail") )
                {
                    __debugbreak();
                }
                if ( memFile->cacheBufferAvail < bytesLeft )
                    cacheBufferAvail = memFile->cacheBufferAvail;
                else
                    cacheBufferAvail = bytesLeft;
                size = cacheBufferAvail;
                if ( cacheBufferAvail + memFile->cacheBufferUsed > memFile->cacheBufferAvail
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\universal\\memfile.cpp",
                                851,
                                0,
                                "%s",
                                "memFile->cacheBufferUsed + size <= memFile->cacheBufferAvail") )
                {
                    __debugbreak();
                }
                memcpy(&p[byteCount - bytesLeft], &memFile->cacheBuffer[memFile->cacheBufferUsed], size);
                bytesLeft -= size;
                memFile->cacheBufferUsed += size;
            }
        }
        else if ( byteCount + memFile->bytesUsed <= memFile->bufferSize )
        {
            memcpy(p, &memFile->buffer[memFile->bytesUsed], byteCount);
            memFile->bytesUsed += byteCount;
        }
        else
        {
            if ( memFile->errorOnOverflow )
                Com_Error(ERR_DROP, "Trying to read corrupted file");
            Com_Printf(10, "Trying to read corrupted file");
            memFile->memoryOverflow = 1;
        }
    }
}

void __cdecl MemFile_WriteInt(MemoryFile *memFile, int value)
{
    if ((unsigned int)(memFile->cacheBufferUsed + 4) >= 0x7FF8)
        MemFile_WriteDataFlushInternal(memFile);
    *(unsigned int *)&memFile->cacheBuffer[memFile->cacheBufferUsed] = value;
    memFile->cacheBufferUsed += 4;
}

void __cdecl MemFile_WriteFloat(MemoryFile *memFile, float valuef)
{
    if ((unsigned int)(memFile->cacheBufferUsed + 4) >= 0x7FF8)
        MemFile_WriteDataFlushInternal(memFile);
    *(float *)&memFile->cacheBuffer[memFile->cacheBufferUsed] = valuef;
    memFile->cacheBufferUsed += 4;
}