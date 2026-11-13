#pragma once
#include "checkpoint_fields.h"
#include <array>
namespace checkpoint {
// Owned strings live in the staging Resources context. Saved allocation IDs
// are relocation labels; model-map caches and native pointers are not archived.
struct AnimationNode {
    std::uint32_t parent=0;
    std::uint16_t animation=0,notifyChild=0,notifyType=0,savedInfo=0;
    std::int16_t notifyIndex=0,cycle=0,oldCycle=0;
    const char* notifyName=nullptr;
    const char* streamedParts=nullptr;
    float time=0,oldTime=0,goalTime=0,goalWeight=0,weight=0,rate=0;
    bool instant=false;
};
struct AnimationTree {
    bool present=false;
    const char* asset=nullptr;
    std::uint32_t assetSize=0,hiddenParts[5]={};
    std::uint16_t published=0;std::uint8_t publishedUser=1;
    std::int16_t instance=0;
    std::vector<AnimationNode> nodes;
};
inline void ValidateAnimationTree(const AnimationTree& tree){
    if(tree.nodes.size()>4095||tree.assetSize>65535)throw std::runtime_error("checkpoint animation tree too large");
    if(!tree.present){if(tree.asset||tree.assetSize||!tree.nodes.empty())throw std::runtime_error("absent checkpoint tree has state");return;}
    if(!tree.assetSize)throw std::runtime_error("checkpoint animation asset is empty");
    std::array<bool,4096> seenInfo{};
    std::vector<unsigned> ancestors{0};std::vector<int> lastChild(tree.nodes.size()+1,-1);
    for(std::size_t i=0;i<tree.nodes.size();++i){const auto& node=tree.nodes[i];
        if(!node.savedInfo||node.savedInfo>=seenInfo.size()||seenInfo[node.savedInfo])
            throw std::runtime_error("invalid/duplicate checkpoint animation instance ID");
        seenInfo[node.savedInfo]=true;
        if(node.parent>i||node.animation>=tree.assetSize||node.notifyChild>=tree.assetSize)
            throw std::runtime_error("checkpoint animation index/parent out of range");
        // Restarting a clip clones its entire subtree. Animation asset indices
        // may repeat; each instance retains its own parent, time and weights.
        while(!ancestors.empty()&&ancestors.back()!=node.parent)ancestors.pop_back();
        if(ancestors.empty()||static_cast<int>(node.animation)<lastChild[node.parent])
            throw std::runtime_error("invalid checkpoint animation preorder/sibling order");
        lastChild[node.parent]=node.animation;ancestors.push_back(static_cast<unsigned>(i+1));
    }
}
template<class ParentOf,class IsLeaf>void ValidateAnimationAssetTopology(const AnimationTree& tree,ParentOf parentOf,IsLeaf isLeaf){
    ValidateAnimationTree(tree);
    for(const auto& node:tree.nodes){
        if((!node.parent&&node.animation!=0)||(node.parent&&(node.animation==0||isLeaf(tree.nodes[node.parent-1].animation)||parentOf(node.animation)!=tree.nodes[node.parent-1].animation)))
            throw std::runtime_error("checkpoint animation hierarchy does not match its asset");
    }
}
// The allocator receives an already relocated parent instance, never an asset
// index lookup that could merge two simultaneously active animation restarts.
template<class Allocate>std::vector<unsigned> RebuildAnimationInstances(const AnimationTree& tree,Allocate allocate){
    ValidateAnimationTree(tree);std::vector<unsigned> instances{0};std::array<bool,4096> seen{};
    for(const auto& node:tree.nodes){const unsigned index=allocate(node,instances[node.parent]);
        if(!index||index>=seen.size()||seen[index])throw std::runtime_error("invalid rebuilt animation instance ID");
        seen[index]=true;instances.push_back(index);
    }return instances;
}
template<class IO>void AnimationFields(IO& io,AnimationTree& tree){
    io.Field("published_index",tree.published);io.Field("published_user",tree.publishedUser);io.Field("tree_present",tree.present);io.Text("animation_asset",tree.asset);io.Field("asset_size",tree.assetSize);
    io.Field("hidden_parts",tree.hiddenParts);io.Field("script_instance",tree.instance);
    std::uint32_t count=static_cast<std::uint32_t>(tree.nodes.size());io.Field("node_count",count);
    if(count>4095)throw std::runtime_error("checkpoint animation node count too large");
    tree.nodes.resize(count);
    for(auto& node:tree.nodes){
        io.Field("saved_info",node.savedInfo);io.Field("parent",node.parent);io.Field("animation",node.animation);io.Field("notify_child",node.notifyChild);io.Field("notify_type",node.notifyType);
        io.Field("notify_index",node.notifyIndex);io.Text("notify_name",node.notifyName);io.Text("streamed_parts",node.streamedParts);
        io.Field("time",node.time);io.Field("old_time",node.oldTime);io.Field("cycle",node.cycle);io.Field("old_cycle",node.oldCycle);
        io.Field("goal_time",node.goalTime);io.Field("goal_weight",node.goalWeight);io.Field("weight",node.weight);io.Field("rate",node.rate);io.Field("instant",node.instant);
    }
    ValidateAnimationTree(tree);
}
}
