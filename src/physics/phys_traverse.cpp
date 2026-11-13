#include "phys_gjk.h"
#include "phys_traverse.h"

#include <qcommon/cm_test.h>
#include <qcommon/common.h>
#include <qcommon/cm_load.h>

#include <gfx_d3d/r_material.h>


#ifdef KISAK_SP
static void CapsuleLeafnums_r(const float *start, const float *end,
    const float *extents, int nodeNum, int *leafs, int &count, int capacity)
{
    float p0[3] = { start[0], start[1], start[2] };
    while (nodeNum >= 0)
    {
        const cNode_t *node = &cm.nodes[nodeNum];
        const cplane_s *plane = node->plane;
        float normal[3] = { 0.0f, 0.0f, 0.0f };
        float d0, d1;
        if (plane->type < 3)
        {
            d0 = p0[plane->type] - plane->dist;
            d1 = end[plane->type] - plane->dist;
            normal[plane->type] = 1.0f;
        }
        else
        {
            for (int i = 0; i < 3; ++i)
                normal[i] = plane->normal[i];
            d0 = (normal[2] * p0[2] + normal[0] * p0[0]) + normal[1] * p0[1];
            d1 = (normal[1] * end[1] + normal[2] * end[2]) + normal[0] * end[0];
            d0 -= plane->dist;
            d1 -= plane->dist;
        }
        const float x = normal[0] >= 0.0f ? extents[0] : -extents[0];
        const float y = normal[1] >= 0.0f ? extents[1] : -extents[1];
        const float z = normal[2] >= 0.0f ? extents[2] : -extents[2];
        const float offset = (z * normal[2] + y * normal[1]) + x * normal[0];
        const float difference = d1 - d0;
        const float lower = difference >= 0.0f ? d0 : d1;
        if (lower >= offset)
        {
            nodeNum = node->children[0];
            continue;
        }
        const float upper = d0 - d1 >= 0.0f ? d0 : d1;
        if (-offset >= upper)
        {
            nodeNum = node->children[1];
            continue;
        }
        int side = 0;
        float first = 1.0f;
        float second = 0.0f;
        if (fabsf(difference) > 4.76837158203125e-7f)
        {
            if (difference >= 0.0f)
            {
                d0 = -d0;
                side = 1;
            }
            const float inverse = 1.0f / fabsf(difference);
            second = (d0 - offset) * inverse;
            first = (d0 + offset) * inverse;
        }
        if (!(1.0f - first >= 0.0f))
            first = 1.0f;
        float middle[3];
        for (int i = 0; i < 3; ++i)
            middle[i] = (end[i] - p0[i]) * first + p0[i];
        CapsuleLeafnums_r(p0, middle, extents, node->children[side], leafs, count, capacity);
        if (!(second >= 0.0f))
            second = 0.0f;
        for (int i = 0; i < 3; ++i)
            p0[i] = (end[i] - p0[i]) * second + p0[i];
        nodeNum = node->children[side ^ 1];
    }
    if (count < capacity)
        leafs[count++] = -1 - nodeNum;
}

bool colgeom_visitor_t::capsule_intersects_box(const float *mins, const float *maxs) const
{
    for (int i = 0; i < 3; ++i)
    {
        if (!(maxs[i] >= m_mn.vec.v[i]) || !(m_mx.vec.v[i] >= mins[i]))
            return false;
    }
    float relative[3], extents[3];
    for (int i = 0; i < 3; ++i)
    {
        const float lower = mins[i] * 0.5f;
        const float upper = maxs[i] * 0.5f;
        relative[i] = m_p0.vec.v[i] - (upper + lower);
        extents[i] = m_rvec.vec.v[i] + (upper - lower);
    }
    const float *delta = m_delta.vec.v;
    const float xy = extents[1] * fabsf(delta[0]) + extents[0] * fabsf(delta[1])
        - fabsf(delta[1] * relative[0] - delta[0] * relative[1]);
    const float yz = extents[2] * fabsf(delta[1]) + extents[1] * fabsf(delta[2])
        - fabsf(relative[1] * delta[2] - delta[1] * relative[2]);
    return xy >= 0.0f && yz >= 0.0f;
}

void colgeom_visitor_t::intersect_capsule_brushnode(cLeafBrushNode_s *node,
    const float *start, const float *end)
{
    float p0[4] = { start[0], start[1], start[2], start[3] };
    while (node->contents & m_mask)
    {
        if (node->leafBrushCount > 0)
        {
            for (int i = 0; i < node->leafBrushCount; ++i)
            {
                const cbrush_t *brush = &cm.brushes[node->data.leaf.brushes[i]];
                if ((brush->contents & m_mask) && capsule_intersects_box(brush->mins, brush->maxs))
                    visit(brush);
            }
            return;
        }
        if (node->leafBrushCount < 0)
            intersect_capsule_brushnode(node + 1, p0, end);
        float d0 = p0[node->axis] - node->data.children.dist;
        const float d1 = end[node->axis] - node->data.children.dist;
        const float offset = m_rvec.vec.v[node->axis] + 0.125f - node->data.children.range;
        const float lower = d1 - d0 >= 0.0f ? d0 : d1;
        const float upper = d0 - d1 >= 0.0f ? d0 : d1;
        if (lower >= offset)
        {
            if (-offset >= upper)
                return;
            node += node->data.children.childOffset[0];
            continue;
        }
        if (-offset >= upper)
        {
            node += node->data.children.childOffset[1];
            continue;
        }
        if (p0[3] >= 1.0f)
            return;
        const float difference = d1 - d0;
        int side = 0;
        float first = 1.0f;
        float second = 0.0f;
        if (fabsf(difference) > 4.76837158203125e-7f)
        {
            if (difference >= 0.0f)
            {
                d0 = -d0;
                side = 1;
            }
            const float inverse = 1.0f / fabsf(difference);
            second = (d0 - offset) * inverse;
            first = (d0 + offset) * inverse;
        }
        if (!(1.0f - first >= 0.0f))
            first = 1.0f;
        float middle[4];
        for (int i = 0; i < 4; ++i)
            middle[i] = (end[i] - p0[i]) * first + p0[i];
        intersect_capsule_brushnode(node + node->data.children.childOffset[side], p0, middle);
        if (!(second >= 0.0f))
            second = 0.0f;
        for (int i = 0; i < 4; ++i)
            p0[i] = (end[i] - p0[i]) * second + p0[i];
        node += node->data.children.childOffset[side ^ 1];
    }
}

void colgeom_visitor_t::intersect_capsule_brushes(cLeaf_s *leaf)
{
    if (!leaf->leafBrushNode || !capsule_intersects_box(leaf->mins, leaf->maxs))
        return;
    const float start[4] = { m_p0.vec.v[0], m_p0.vec.v[1], m_p0.vec.v[2], 0.0f };
    const float end[4] = { m_p1.vec.v[0], m_p1.vec.v[1], m_p1.vec.v[2], 1.0f };
    intersect_capsule_brushnode(&cm.leafbrushNodes[leaf->leafBrushNode], start, end);
}

void colgeom_visitor_t::intersect_capsule_partitions_r(CollisionAabbTree *tree)
{
    float mins[3], maxs[3];
    for (int i = 0; i < 3; ++i)
    {
        mins[i] = tree->origin[i] - tree->halfSize[i];
        maxs[i] = tree->origin[i] + tree->halfSize[i];
    }
    if (!capsule_intersects_box(mins, maxs))
        return;
    if (tree->childCount)
    {
        for (int i = 0; i < tree->childCount; ++i)
            intersect_capsule_partitions_r(&cm.aabbTrees[tree->u.firstChildIndex + i]);
    }
    else
    {
        const int index = tree->u.partitionIndex;
        if (m_threadInfo->checkcount.partitions[index] != m_threadInfo->checkcount.global)
        {
            m_threadInfo->checkcount.partitions[index] = m_threadInfo->checkcount.global;
            visit(tree);
        }
    }
}

void colgeom_visitor_t::intersect_capsule_partitions(cLeaf_s *leaf)
{
    for (int i = 0; i < leaf->collAabbCount; ++i)
    {
        CollisionAabbTree *tree = &cm.aabbTrees[leaf->firstCollAabbIndex + i];
        if (cm.materials[tree->materialIndex].contentFlags & m_mask)
            intersect_capsule_partitions_r(tree);
    }
}

void colgeom_visitor_t::intersect_capsule(const float *start, const float *end,
    const float *mins, const float *maxs, int mask)
{
    m_mask = mask;
    for (int i = 0; i < 3; ++i)
    {
        m_rvec.vec.v[i] = (maxs[i] - mins[i]) * 0.5f;
        const float offset = (mins[i] + maxs[i]) * 0.5f;
        m_p0.vec.v[i] = start[i] + offset;
        m_p1.vec.v[i] = end[i] + offset;
    }
    int leafs[256], count = 0;
    CapsuleLeafnums_r(m_p0.vec.v, m_p1.vec.v, m_rvec.vec.v, 0, leafs, count, 256);
    if (!count)
        return;
    TraceThreadInfo threadInfo;
    m_threadInfo = &threadInfo;
    CM_GetTraceThreadInfo(m_threadInfo);
    if (!m_threadInfo->checkcount.partitions && cm.partitionCount)
        return;
    for (int i = 0; i < 4; ++i)
        m_delta.vec.v[i] = m_p1.vec.v[i] - m_p0.vec.v[i];
    for (int i = 0; i < 4; ++i)
    {
        m_mn.vec.v[i] = (m_p1.vec.v[i] - m_p0.vec.v[i] >= 0.0f ? m_p0.vec.v[i] : m_p1.vec.v[i]) - m_rvec.vec.v[i];
        m_mx.vec.v[i] = (m_p0.vec.v[i] - m_p1.vec.v[i] >= 0.0f ? m_p0.vec.v[i] : m_p1.vec.v[i]) + m_rvec.vec.v[i];
    }
    for (int i = 0; i < count; ++i)
    {
        cLeaf_s *leaf = &cm.leafs[leafs[i]];
        if (leaf->brushContents & m_mask)
            intersect_capsule_brushes(leaf);
        if (leaf->terrainContents & m_mask)
            intersect_capsule_partitions(leaf);
    }
}
#endif

colgeom_visitor_t::colgeom_visitor_t()
{
    //this->__vftable = (colgeom_visitor_t_vtbl *)&visitor_base_t::`vftable';
    //this->__vftable = (colgeom_visitor_t_vtbl *)&colgeom_visitor_t::`vftable';
    this->m_mn.vec.u[0] = 0;
    this->m_mn.vec.u[1] = 0;
    this->m_mn.vec.u[2] = 0;
    this->m_mn.vec.u[3] = 0;
    this->m_mx.vec.u[0] = 0;
    this->m_mx.vec.u[1] = 0;
    this->m_mx.vec.u[2] = 0;
    this->m_mx.vec.u[3] = 0;
    this->m_p0.vec.u[0] = 0;
    this->m_p0.vec.u[1] = 0;
    this->m_p0.vec.u[2] = 0;
    this->m_p0.vec.u[3] = 0;
    this->m_p1.vec.u[0] = 0;
    this->m_p1.vec.u[1] = 0;
    this->m_p1.vec.u[2] = 0;
    this->m_p1.vec.u[3] = 0;
    this->m_delta.vec.u[0] = 0;
    this->m_delta.vec.u[1] = 0;
    this->m_delta.vec.u[2] = 0;
    this->m_delta.vec.u[3] = 0;
    this->m_rvec.vec.u[0] = 0;
    this->m_rvec.vec.u[1] = 0;
    this->m_rvec.vec.u[2] = 0;
    this->m_rvec.vec.u[3] = 0;
}

colgeom_visitor_t::colgeom_visitor_t(const colgeom_visitor_t *__that)
{
    //this->__vftable = (colgeom_visitor_t_vtbl *)&visitor_base_t::`vftable';
    //    this->__vftable = (colgeom_visitor_t_vtbl *)&colgeom_visitor_t::`vftable';
    this->m_mn = __that->m_mn;
    this->m_mx = __that->m_mx;
    this->m_p0 = __that->m_p0;
    this->m_p1 = __that->m_p1;
    this->m_delta = __that->m_delta;
    this->m_rvec = __that->m_rvec;
    this->m_radius = __that->m_radius;
    this->m_mask = __that->m_mask;
    this->m_threadInfo = __that->m_threadInfo;
}


void static_colgeom_visitor_t::visit(const CollisionAabbTree *tree)
{
    if ( this->ntrees < 512 )
        this->trees[this->ntrees++] = tree;
}

void static_colgeom_visitor_t::visit(const cbrush_t *brush)
{
    int i; // [esp+4h] [ebp-4h]

    if ( this->nbrushes < 512 )
    {
        for ( i = 0; i < this->nbrushes; ++i )
        {
            if ( brush == this->brushes[i] )
                return;
        }
        this->brushes[this->nbrushes++] = brush;
    }
}

void static_colgeom_visitor_t::update(
                const float *_mn,
                const float *_mx,
                int mask,
                const float *expand_vec)
{
    float result[3]; // [esp+10h] [ebp-40h] BYREF
    float b[3]; // [esp+1Ch] [ebp-34h] BYREF
    float a[3]; // [esp+28h] [ebp-28h] BYREF
    bool inside; // [esp+37h] [ebp-19h]
    float mx[3]; // [esp+38h] [ebp-18h] BYREF
    float mn[3]; // [esp+44h] [ebp-Ch] BYREF

    a[0] = this->m_mn.vec.v[0] - _mn[0];
    a[1] = this->m_mn.vec.v[1] - _mn[1];
    a[2] = this->m_mn.vec.v[2] - _mn[2];

    b[0] = _mx[0] - this->m_mx.vec.v[0];
    b[1] = _mx[1] - this->m_mx.vec.v[1];
    b[2] = _mx[2] - this->m_mx.vec.v[2];

    Vec3Max(a, b, result);
    inside = result[0] < 0.0 && result[1] < 0.0 && result[2] < 0.0;

    if ( this->m_mask != mask || !inside )
    {
        mn[0] = _mn[0] - expand_vec[0];
        mn[1] = _mn[1] - expand_vec[1];
        mn[2] = _mn[2] - expand_vec[2];

        mx[0] = _mx[0] + expand_vec[0];
        mx[1] = _mx[1] + expand_vec[1];
        mx[2] = _mx[2] + expand_vec[2];

        this->prolog();

        colgeom_visitor_t::intersect_box(mn, mx, mask);
    }
}


void colgeom_visitor_t::intersect_box(float *mn, float *mx, int mask)
{

    this->m_mn.vec.v[0] = mn[0];
    this->m_mn.vec.v[1] = mn[1];
    this->m_mn.vec.v[2] = mn[2];

    this->m_mx.vec.v[0] = mx[0];

    this->m_mx.vec.v[1] = mx[1];
    this->m_mx.vec.v[2] = mx[2];
    this->m_mask = mask;

    leafList_s ll; // [esp+20h] [ebp-1048h] BYREF
    _WORD leaflist[2048]; // [esp+60h] [ebp-1008h] BYREF

    ll.bounds[0][0] = this->m_mn.vec.v[0];
    ll.bounds[0][1] = this->m_mn.vec.v[1];
    ll.bounds[0][2] = this->m_mn.vec.v[2];
    ll.bounds[1][0] = this->m_mx.vec.v[0];
    ll.bounds[1][1] = this->m_mx.vec.v[1];
    ll.bounds[1][2] = this->m_mx.vec.v[2];

    ll.count = 0;
    ll.maxcount = 2048;
    ll.list = leaflist;
    ll.lastLeaf = 0;
    ll.overflowed = 0;
    CM_BoxLeafnums_r(&ll, 0);

    if (!ll.count)
    {
        return;
    }

    if (ll.overflowed)
    {
#ifdef KISAK_SP
        // The retail walk bounds-checks the 2048-entry array, so an overflow
        // here is truncation rather than a write past leaflist. Preserve the
        // warning and capture the exact query that preceded the SP crash.
        Com_Printf(
            0,
            "colgeom_visitor_t::intersect_box: leafList overflow (max %d) count=%d mask=0x%08X mn=(%.3f %.3f %.3f) mx=(%.3f %.3f %.3f)\n",
            ll.maxcount,
            ll.count,
            mask,
            mn[0],
            mn[1],
            mn[2],
            mx[0],
            mx[1],
            mx[2]);
#else
        Com_Printf(0, "colgeom_visitor_t::intersect_box: leafList overflow (max %d)\n", 2048);
#endif
    }

    TraceThreadInfo threadInfo;
    this->m_threadInfo = &threadInfo;
    CM_GetTraceThreadInfo(this->m_threadInfo);

    if (!this->m_threadInfo->checkcount.partitions && cm.partitionCount)
    {
        return;
    }

    for (int i = 0; i < ll.count; ++i)
    {
        cLeaf_s *leaf = &cm.leafs[(unsigned __int16)leaflist[i]];

        if ((this->m_mask & leaf->brushContents) != 0)
            colgeom_visitor_t::intersect_box_brushes(leaf);

        if ((this->m_mask & leaf->terrainContents) != 0)
            colgeom_visitor_t::intersect_box_partitions(leaf);
    }
}


void colgeom_visitor_t::intersect_box_brushes(cLeaf_s *leaf)
{
    if (leaf->leafBrushNode)
    {
        if (this->m_mx.vec.v[0] >= leaf->mins[0]
            && this->m_mx.vec.v[1] >= leaf->mins[1]
            && this->m_mx.vec.v[2] >= leaf->mins[2]
            && leaf->maxs[0] >= this->m_mn.vec.v[0]
            && leaf->maxs[1] >= this->m_mn.vec.v[1]
            && leaf->maxs[2] >= this->m_mn.vec.v[2])
        {
            colgeom_visitor_t::intersect_box_brushnode(&cm.leafbrushNodes[leaf->leafBrushNode]);
        }
    }
}

void colgeom_visitor_t::intersect_box_partitions(cLeaf_s *leaf)
{
    CollisionAabbTree *aabbTree; // [esp+8h] [ebp-8h]
    int k; // [esp+Ch] [ebp-4h]

    for (k = 0; k < leaf->collAabbCount; ++k)
    {
        aabbTree = &cm.aabbTrees[k + leaf->firstCollAabbIndex];
        if ((cm.materials[aabbTree->materialIndex].contentFlags & this->m_mask) != 0)
            colgeom_visitor_t::intersect_box_partitions_r(aabbTree);
    }
}

void colgeom_visitor_t::intersect_box_brushnode(cLeafBrushNode_s *node)
{
    cbrush_t *b; // [esp+14h] [ebp-Ch]
    int i; // [esp+1Ch] [ebp-4h]

    while ((this->m_mask & node->contents) != 0)
    {
        if (node->leafBrushCount)
        {
            if (node->leafBrushCount > 0) // KISAKTODO: redundant if()?
            {
                for (i = 0; i < node->leafBrushCount; ++i)
                {
                    b = &cm.brushes[node->data.leaf.brushes[i]];
                    if ((this->m_mask & b->contents) != 0
                        && this->m_mx.vec.v[0] >= b->mins[0]
                        && this->m_mx.vec.v[1] >= b->mins[1]
                        && this->m_mx.vec.v[2] >= b->mins[2]
                        && b->maxs[0] >= this->m_mn.vec.v[0]
                        && b->maxs[1] >= this->m_mn.vec.v[1]
                        && b->maxs[2] >= this->m_mn.vec.v[2])
                    {
                        this->visit(b);
                    }
                }
                return;
            }
            colgeom_visitor_t::intersect_box_brushnode(node + 1);
        }
        if (this->m_mn.vec.v[node->axis] <= node->data.children.dist)
        {
            if (this->m_mx.vec.v[node->axis] >= node->data.children.dist)
                colgeom_visitor_t::intersect_box_brushnode(&node[node->data.children.childOffset[0]]);
            node += node->data.children.childOffset[1];
        }
        else
        {
            node += node->data.children.childOffset[0];
        }
    }
}

void colgeom_visitor_t::intersect_box_partitions_r(CollisionAabbTree *aabbTree)
{
    bool v3; // [esp+7h] [ebp-25h]
    int childIndex; // [esp+18h] [ebp-14h]
    int partitionIndex; // [esp+1Ch] [ebp-10h]
    CollisionAabbTree *child; // [esp+20h] [ebp-Ch]
    int checkStamp; // [esp+24h] [ebp-8h]

    if (this->m_mn.vec.v[0] <= (float)(aabbTree->origin[0] + aabbTree->halfSize[0]))
    {
        if (this->m_mn.vec.v[1] <= (float)(aabbTree->origin[1] + aabbTree->halfSize[1]))
        {
            if (this->m_mn.vec.v[2] <= (float)(aabbTree->origin[2] + aabbTree->halfSize[2]))
                v3 = (float)(aabbTree->origin[0] - aabbTree->halfSize[0]) <= this->m_mx.vec.v[0]
                && (float)(aabbTree->origin[1] - aabbTree->halfSize[1]) <= this->m_mx.vec.v[1]
                && (float)(aabbTree->origin[2] - aabbTree->halfSize[2]) <= this->m_mx.vec.v[2];
            else
                v3 = 0;
        }
        else
        {
            v3 = 0;
        }
    }
    else
    {
        v3 = 0;
    }
    if (v3)
    {
        if (aabbTree->childCount)
        {
            childIndex = 0;
            child = &cm.aabbTrees[aabbTree->u.firstChildIndex];
            while (childIndex < aabbTree->childCount)
            {
                colgeom_visitor_t::intersect_box_partitions_r(child);
                ++childIndex;
                ++child;
            }
        }
        else
        {
            partitionIndex = aabbTree->u.firstChildIndex;
            checkStamp = this->m_threadInfo->checkcount.global;
            if (this->m_threadInfo->checkcount.partitions[partitionIndex] != checkStamp)
            {
                this->m_threadInfo->checkcount.partitions[partitionIndex] = checkStamp;
                this->visit(aabbTree);
            }
        }
    }
}