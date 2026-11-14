#include "huffman.h"
#include <universal/com_shared.h>
#include <cstdlib>
#include <universal/q_shared.h>

thread_local int bloc;

int __cdecl get_bit(const unsigned __int8 *fin)
{
    int t; // [esp+0h] [ebp-4h]

    t = ((int)fin[bloc >> 3] >> (bloc & 7)) & 1; 
    bloc = bloc + 1;
    return t;
}

void __cdecl Huff_offsetReceive(nodetype *node, int *ch, const unsigned __int8 *fin, int *offset)
{
    bloc = *offset;
    while (node && node->symbol == 257)
    {
        if (get_bit(fin))
            node = node->right;
        else
            node = node->left;
    }
    if (node)
    {
        *ch = node->symbol;
        *offset = bloc;
    }
    else
    {
        *ch = 0;
    }
}

void __cdecl huffman_send(nodetype *node, nodetype *child, unsigned __int8 *fout)
{
    if ( node->parent )
        huffman_send(node->parent, node, fout);
    if ( child )
    {
        if ( node->right == child )
            add_bit(1, fout);
        else
            add_bit(0, fout);
    }
}

void __cdecl add_bit(char bit, unsigned __int8 *fout)
{
    if ((bloc & 7) == 0)
        fout[bloc >> 3] = 0;
    fout[bloc >> 3] = (bit << (bloc & 7)) | fout[bloc >> 3];
    bloc = bloc + 1;
}

void __cdecl Huff_offsetTransmit(huff_t *huff, int ch, unsigned __int8 *fout, int *offset)
{
    bloc = *offset;
    huffman_send(huff->loc[ch], 0, fout);
    *offset = bloc;
}

void __cdecl Huff_Init(huffman_t *huff)
{
    Com_Memset((unsigned int *)huff, 0, 19476);
    huff->compressDecompress.loc[256] = &huff->compressDecompress.nodeList[huff->compressDecompress.blocNode++];
    huff->compressDecompress.tree = huff->compressDecompress.loc[256];
    huff->compressDecompress.tree->symbol = 256;
    huff->compressDecompress.tree->weight = 0;
    huff->compressDecompress.tree->parent = 0;
    huff->compressDecompress.tree->left = 0;
    huff->compressDecompress.tree->right = 0;
}

nodetype *__cdecl Huff_initNode(huff_t *huff, int ch, int weight)
{
    nodetype *tnode; // [esp+0h] [ebp-4h]

    tnode = &huff->nodeList[huff->blocNode++];
    tnode->symbol = ch;
    tnode->weight = weight;
    tnode->left = 0;
    tnode->right = 0;
    tnode->parent = 0;
    huff->loc[ch] = tnode;
    return tnode;
}

int __cdecl nodeCmp(const void *left, const void *right)
{
    // nx-port: sorts an array of nodetype*, so each argument is a nodetype** --
    // read it as one. The decompiled form went through `unsigned int`, which
    // truncated the element pointer to its low 32 bits, and reached the weight
    // at the hardcoded x86 offset 12. Neither survives LP64: nodetype carries
    // three pointers before its scalar tail, so sizeof goes 20 -> 32 and weight
    // 12 -> 24, and 12 now lands in the upper half of `right`. Same defect as
    // BG_UnlockablesCompareItemsBySortKey (c12c1b9).
    const nodetype *lnode = *(const nodetype *const *)left;
    const nodetype *rnode = *(const nodetype *const *)right;

    return lnode->weight - rnode->weight;
}

// Cross-platform: the tree is built by sorting nodes on weight, and several
// weights tie. qsort leaves tied elements in an order its C library chooses,
// so the PC (MSVC CRT) and Switch (newlib) trees differed: bytes 155 and 205
// swapped codes, and a PC client of a Switch host (or the reverse) lost sync
// in the first gamestate that contained one. Every platform now runs the MSVC
// CRT algorithm, so the tree stays the one PC builds always produced (checked
// against the CRT qsort for all 256 codes).
template <class T>
static void Huff_MsvcSwap(T *a, T *b) { if (a != b) { T t = *a; *a = *b; *b = t; } }
template <class T>
static void Huff_MsvcShortSort(T *lo, T *hi, int (*comp)(const void *, const void *))
{
    while (hi > lo)
    {
        T *max = lo;
        for (T *p = lo + 1; p <= hi; ++p)
            if (comp(p, max) > 0)
                max = p;
        Huff_MsvcSwap(max, hi);
        --hi;
    }
}
template <class T>
static void Huff_MsvcQsort(T *base, size_t num, int (*comp)(const void *, const void *))
{
    enum { CUTOFF = 8, STKSIZ = 8 * sizeof(void *) - 2 };
    if (num < 2)
        return;
    T *lostk[STKSIZ], *histk[STKSIZ];
    int stkptr = 0;
    T *lo = base, *hi = base + (num - 1);
recurse:
    {
        const size_t size = size_t(hi - lo) + 1;
        if (size <= CUTOFF)
        {
            Huff_MsvcShortSort(lo, hi, comp);
        }
        else
        {
            T *mid = lo + size / 2;
            if (comp(lo, mid) > 0) Huff_MsvcSwap(lo, mid);
            if (comp(lo, hi) > 0) Huff_MsvcSwap(lo, hi);
            if (comp(mid, hi) > 0) Huff_MsvcSwap(mid, hi);
            T *loguy = lo, *higuy = hi;
            for (;;)
            {
                if (mid > loguy)
                    do { ++loguy; } while (loguy < mid && comp(loguy, mid) <= 0);
                if (mid <= loguy)
                    do { ++loguy; } while (loguy <= hi && comp(loguy, mid) <= 0);
                do { --higuy; } while (higuy > mid && comp(higuy, mid) > 0);
                if (higuy < loguy)
                    break;
                Huff_MsvcSwap(loguy, higuy);
                if (mid == higuy)
                    mid = loguy;
            }
            ++higuy;
            if (mid < higuy)
                do { --higuy; } while (higuy > mid && comp(higuy, mid) == 0);
            if (mid >= higuy)
                do { --higuy; } while (higuy > lo && comp(higuy, mid) == 0);
            if (higuy - lo >= hi - loguy)
            {
                if (lo < higuy) { lostk[stkptr] = lo; histk[stkptr] = higuy; ++stkptr; }
                if (loguy < hi) { lo = loguy; goto recurse; }
            }
            else
            {
                if (loguy < hi) { lostk[stkptr] = loguy; histk[stkptr] = hi; ++stkptr; }
                if (lo < higuy) { hi = higuy; goto recurse; }
            }
        }
    }
    --stkptr;
    if (stkptr >= 0)
    {
        lo = lostk[stkptr];
        hi = histk[stkptr];
        goto recurse;
    }
}

void __cdecl Huff_BuildFromData(huff_t *huff, const int *msg_hData)
{
    nodetype *inited; // eax
    nodetype *v3; // eax
    nodetype *v4; // eax
    int numNodes; // [esp+0h] [ebp-410h]
    nodetype *heap[256]; // [esp+8h] [ebp-408h] BYREF
    int i; // [esp+408h] [ebp-8h]
    int heapHead; // [esp+40Ch] [ebp-4h]

    numNodes = 256;
    heapHead = 0;
    for ( i = 0; i < 256; ++i )
    {
        inited = Huff_initNode(huff, i, msg_hData[i]);
        heap[i] = inited;
    }
    Huff_MsvcQsort(heap, 0x100u, nodeCmp);
    v3 = Huff_initNode(huff, 257, 1);
    v3->left = huff->tree;
    v3->right = heap[0];
    v3->left->parent = v3;
    v3->right->parent = v3;
    v3->weight = v3->right->weight + v3->left->weight;
    heap[0] = v3;
    while ( numNodes > 1 )
    {
        Huff_MsvcQsort(&heap[heapHead], 256 - heapHead, nodeCmp);
        v4 = Huff_initNode(huff, 257, 1);
        v4->left = heap[heapHead];
        v4->right = heap[heapHead + 1];
        v4->left->parent = v4;
        v4->right->parent = v4;
        v4->weight = v4->right->weight + v4->left->weight;
        heap[++heapHead] = v4;
        --numNodes;
    }
    huff->tree = heap[heapHead];
}

