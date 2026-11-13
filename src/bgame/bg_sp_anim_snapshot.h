#pragma once

// OpenBLOPS SP protocol extension, not the retail wire layout. No pointers,
// script-string handles, or native structure padding cross the connection.
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <utility>

namespace SpAnimSnapshot
{
constexpr unsigned MaxBytes = 49152;
constexpr unsigned MaxEntities = 1024;
constexpr unsigned MaxNodes = 2048;
constexpr unsigned Version = 3;
constexpr unsigned MaxCommands = 1024;
struct Node
{
    // One-based parent instance in this entity's preorder list; zero is root.
    // Animation indices may repeat while an animation restart blends out.
    unsigned index = 0, notifyChild = 0, parent = 0;
    float time = 0, goalTime = 0, goalWeight = 0, weight = 0, rate = 0;
    std::int16_t cycle = 0;
};
struct Attachment { unsigned model = 0; std::string tag; };
struct Entity
{
    unsigned number = 0, treeSize = 0, ignoreCollision = 0;
    std::string tree;
    Attachment attachments[6];
    std::vector<Node> nodes;
};
constexpr unsigned MaxActors = 32;
struct Actor
{
    unsigned actorNum = 0, entityNum = 0;
    std::int32_t team = 0, modelindex = 0;
    std::int32_t attachModelIndex[6] = {}, attachTagIndex[6] = {};
    std::int32_t attachIgnoreCollision = 0;
    std::string name;
    unsigned attachedEntNum = 1023;
    std::int32_t attachedEntTime = 0, attachedTagIndex = 0;
    std::int32_t animScriptedAnim = 0, animScriptedAnimTime = 0;
    std::int32_t hudwarningType = 0;
    unsigned lookAtEntNum = 1023;
};

inline bool ValidActor(const Actor &actor)
{
    return actor.actorNum < MaxActors && actor.entityNum < MaxEntities-1
        && actor.attachedEntNum < MaxEntities && actor.lookAtEntNum < MaxEntities
        && actor.name.size() < 32 && actor.name.find('\0') == std::string::npos;
}


struct Command
{
    std::int32_t index = 0, type = 0, serverTime = 0, sequence = 0;
    std::int32_t entNum = 0, animIndex = 0, rootAnimIndex = 0;
    float weight = 0, rate = 0, goalTime = 0;
    std::int32_t flags = 0;
};
inline bool ValidCommand(const Command &command)
{
    return command.index >= 0 && command.index < static_cast<int>(MaxCommands)
        && command.type >= 1 && command.type <= 8
        && command.entNum >= 0 && command.entNum < static_cast<int>(MaxEntities-1)
        && command.animIndex >= 0 && command.animIndex <= 0xFFFF
        && command.rootAnimIndex >= 0 && command.rootAnimIndex <= 0xFFFF
        && std::isfinite(command.weight) && std::isfinite(command.rate)
        && std::isfinite(command.goalTime);
}

inline Command QuantizeCommand(Command command)
{
    command.flags &= 3;
    switch (command.type)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 7:
    case 8:
        if (!(std::fabs(command.goalTime - 0.2f) < 0.005f))
            command.goalTime = static_cast<float>((static_cast<int>(command.goalTime * 1000.0f + 0.5f) / 50) & 31) * 50.0f * 0.001f;
        else
            command.goalTime = 0.2f;
        break;
    case 6:
        command.goalTime = static_cast<float>(static_cast<int>(command.goalTime * 1000.0f + 0.5f) & 2047) * 0.001f;
        break;
    }
    if (command.type == 3 || command.type == 4 || command.type == 5 || command.type == 7)
    {
        if (std::fabs(command.weight - 1.0f) < 0.0079f)
            command.weight = 1.0f;
        else if (std::fabs(command.weight) < 0.0079f)
            command.weight = 0.0f;
        else
            command.weight = static_cast<float>(static_cast<int>(command.weight * 127.0f) & 127) * (1.0f / 127.0f);
        if (std::fabs(command.rate - 1.0f) < 0.01f)
            command.rate = 1.0f;
        else
            command.rate = static_cast<float>(static_cast<int>(command.rate * (127.0f / 3.0f)) & 127) * (3.0f / 127.0f);
    }
    return command;
}

struct Frame { int serverTime = 0; std::vector<Entity> entities; std::vector<Actor> actors; std::vector<Command> commands; };

// The temporary script VM may disappear while its map-hunk trees remain live.
// Keep a bounded map-lifetime catalog, separate from packet/clock resets.
template <typename Tree> struct TreeCatalog
{
    struct Entry { std::string name; unsigned size; Tree *tree; };
    std::vector<Entry> entries;
    void Clear() { entries.clear(); }
    void Add(const char *name, unsigned size, Tree *tree)
    {
        if (name && tree && entries.size() < 256) entries.push_back({name, size, tree});
    }
    Tree *Find(const std::string &name, unsigned size) const
    {
        for (const Entry &entry : entries)
            if (entry.name == name && entry.size == size) return entry.tree;
        return nullptr;
    }
};

inline bool Valid(const Frame &frame)
{
    if (frame.serverTime < 0 || frame.entities.size() > MaxEntities) return false;
    unsigned nodes = 0;
    int previousEntity = -1;
    for (const Entity &entity : frame.entities)
    {
        if (entity.number >= MaxEntities-1 || static_cast<int>(entity.number) <= previousEntity
            || entity.tree.empty() || entity.tree.size() > 127 || entity.tree.find('\0') != std::string::npos || !entity.treeSize
            || entity.treeSize > 65535 || entity.ignoreCollision > 63 || entity.nodes.size() > MaxNodes) return false;
        previousEntity = static_cast<int>(entity.number);
        // An empty tag means shared-bone model melding, even with a model.
        for (const Attachment &attachment : entity.attachments)
            if (attachment.model >= 512 || attachment.tag.size() > 127 || attachment.tag.find('\0') != std::string::npos) return false;
        unsigned instance = 0;
        std::vector<unsigned> ancestors;
        std::vector<int> lastChild(entity.nodes.size()+1, -1);
        for (const Node &node : entity.nodes)
        {
            if (++nodes > MaxNodes || node.index >= entity.treeSize
                || node.parent > instance || node.notifyChild >= entity.treeSize
                || !std::isfinite(node.time) || node.time < 0 || node.time > 1
                || !std::isfinite(node.goalTime) || node.goalTime < 0
                || !std::isfinite(node.goalWeight) || node.goalWeight < 0
                || !std::isfinite(node.weight) || node.weight < 0
                || !std::isfinite(node.rate)) return false;
            // Parents precede children, subtrees are contiguous, siblings retain
            // the engine's ascending (but not unique) animation-index order.
            while (!ancestors.empty() && ancestors.back() != node.parent) ancestors.pop_back();
            if (node.parent && ancestors.empty()) return false;
            if (static_cast<int>(node.index) < lastChild[node.parent]) return false;
            lastChild[node.parent] = static_cast<int>(node.index);
            ancestors.push_back(++instance);
        }
    }
    if (frame.actors.size() > MaxActors || frame.commands.size() > MaxCommands) return false;
    int previousActor = -1;
    for (const Actor &actor : frame.actors)
    {
        if (!ValidActor(actor) || static_cast<int>(actor.actorNum) <= previousActor) return false;
        previousActor = static_cast<int>(actor.actorNum);
    }
    for (const Command &command : frame.commands)
        if (!ValidCommand(command)) return false;
    return true;
}

struct Writer
{
    std::vector<unsigned char> bytes;
    void U8(unsigned value) { bytes.push_back(static_cast<unsigned char>(value)); }
    void U16(unsigned value) { U8(value); U8(value >> 8); }
    void U32(unsigned value) { for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<unsigned char>(value >> (8*i))); }
    void Float(float value) { unsigned bits; std::memcpy(&bits, &value, 4); U32(bits); }
    void String(const std::string &value) { U32(static_cast<unsigned>(value.size())); bytes.insert(bytes.end(), value.begin(), value.end()); }
};
struct Reader
{
    const unsigned char *bytes; unsigned size, offset = 0; bool ok = true;
    unsigned U8() { if (offset >= size) { ok = false; return 0; } return bytes[offset++]; }
    unsigned U16() { unsigned lo = U8(); unsigned hi = U8(); return lo | (hi << 8); }
    unsigned U32() { if (offset > size || size-offset < 4) { ok = false; return 0; } unsigned v = 0; for (unsigned i=0;i<4;++i) v |= unsigned(bytes[offset++]) << (8*i); return v; }
    float Float() { unsigned bits = U32(); float v; std::memcpy(&v, &bits, 4); return v; }
    std::string String() { unsigned n = U32(); if (!ok || n > 127 || offset > size || n > size-offset) { ok=false; return {}; } std::string v(reinterpret_cast<const char *>(bytes+offset), n); offset+=n; if (v.find('\0') != std::string::npos) ok=false; return v; }
};
inline std::int32_t ReadActorInt(Reader &in)
{
    const std::uint32_t bits = in.U32();
    std::int32_t value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline bool WriteActor(Writer &out, const Actor &actor)
{
    if (!ValidActor(actor)) return false;
    out.U32(actor.actorNum); out.U32(actor.entityNum);
    out.U32(static_cast<unsigned>(actor.team));
    out.U32(static_cast<unsigned>(actor.modelindex));
    for (std::int32_t value : actor.attachModelIndex) out.U32(static_cast<unsigned>(value));
    for (std::int32_t value : actor.attachTagIndex) out.U32(static_cast<unsigned>(value));
    out.U32(static_cast<unsigned>(actor.attachIgnoreCollision)); out.String(actor.name);
    out.U32(actor.attachedEntNum); out.U32(static_cast<unsigned>(actor.attachedEntTime));
    out.U32(static_cast<unsigned>(actor.attachedTagIndex));
    out.U32(static_cast<unsigned>(actor.animScriptedAnim));
    out.U32(static_cast<unsigned>(actor.animScriptedAnimTime));
    out.U32(static_cast<unsigned>(actor.hudwarningType)); out.U32(actor.lookAtEntNum);
    return true;
}

inline bool ReadActor(Reader &in, Actor &actor)
{
    Actor candidate;
    candidate.actorNum = in.U32(); candidate.entityNum = in.U32();
    candidate.team = ReadActorInt(in); candidate.modelindex = ReadActorInt(in);
    for (std::int32_t &value : candidate.attachModelIndex) value = ReadActorInt(in);
    for (std::int32_t &value : candidate.attachTagIndex) value = ReadActorInt(in);
    candidate.attachIgnoreCollision = ReadActorInt(in); candidate.name = in.String();
    candidate.attachedEntNum = in.U32(); candidate.attachedEntTime = ReadActorInt(in);
    candidate.attachedTagIndex = ReadActorInt(in);
    candidate.animScriptedAnim = ReadActorInt(in);
    candidate.animScriptedAnimTime = ReadActorInt(in);
    candidate.hudwarningType = ReadActorInt(in); candidate.lookAtEntNum = in.U32();
    if (!in.ok || !ValidActor(candidate)) { in.ok = false; return false; }
    actor = std::move(candidate);
    return true;
}


inline void WriteCommand(Writer &out, const Command &command)
{
    out.U16(command.index); out.U8(command.type); out.U8(command.flags);
    out.U32(static_cast<unsigned>(command.serverTime)); out.U32(static_cast<unsigned>(command.sequence));
    out.U16(command.entNum); out.U16(command.animIndex); out.U16(command.rootAnimIndex);
    out.Float(command.weight); out.Float(command.rate); out.Float(command.goalTime);
}
inline bool ReadCommand(Reader &in, Command &command)
{
    Command candidate;
    candidate.index = in.U16(); candidate.type = in.U8(); candidate.flags = in.U8();
    candidate.serverTime = ReadActorInt(in); candidate.sequence = ReadActorInt(in);
    candidate.entNum = in.U16(); candidate.animIndex = in.U16(); candidate.rootAnimIndex = in.U16();
    candidate.weight = in.Float(); candidate.rate = in.Float(); candidate.goalTime = in.Float();
    if (!in.ok || !ValidCommand(candidate)) { in.ok = false; return false; }
    command = candidate;
    return true;
}

inline bool Encode(const Frame &frame, std::vector<unsigned char> &bytes)
{
    if (!Valid(frame)) return false;
    Writer out;
    out.U32(Version); out.U32(static_cast<unsigned>(frame.serverTime)); out.U32(static_cast<unsigned>(frame.entities.size()));
    for (const Entity &entity : frame.entities)
    {
        out.U32(entity.number); out.U32(entity.treeSize); out.String(entity.tree); out.U32(entity.ignoreCollision);
        for (const Attachment &attachment : entity.attachments) { out.U32(attachment.model); out.String(attachment.tag); }
        out.U32(static_cast<unsigned>(entity.nodes.size()));
        for (const Node &node : entity.nodes)
        {
            out.U32(node.index); out.U32(node.notifyChild); out.U32(node.parent); out.U32(static_cast<std::uint16_t>(node.cycle));
            out.Float(node.time); out.Float(node.goalTime); out.Float(node.goalWeight); out.Float(node.weight); out.Float(node.rate);
        }
        if (out.bytes.size() > MaxBytes) return false;
    }
    out.U32(static_cast<unsigned>(frame.actors.size()));
    for (const Actor &actor : frame.actors) if (!WriteActor(out, actor)) return false;
    out.U32(static_cast<unsigned>(frame.commands.size()));
    for (const Command &command : frame.commands) WriteCommand(out, command);
    if (out.bytes.size() > MaxBytes) return false;
    bytes = std::move(out.bytes); return true;
}
inline bool Decode(const unsigned char *bytes, unsigned size, Frame &frame)
{
    if (!bytes || size > MaxBytes) return false;
    Reader in{bytes,size}; Frame candidate;
    if (in.U32() != Version) return false;
    candidate.serverTime = static_cast<int>(in.U32());
    unsigned count = in.U32(), totalNodes = 0;
    if (!in.ok || count > MaxEntities) return false;
    for (unsigned i = 0; i < count; ++i)
    {
        Entity entity;
        entity.number = in.U32(); entity.treeSize = in.U32(); entity.tree = in.String(); entity.ignoreCollision = in.U32();
        for (Attachment &attachment : entity.attachments) { attachment.model = in.U32(); attachment.tag = in.String(); }
        unsigned nodeCount = in.U32();
        if (!in.ok || nodeCount > MaxNodes-totalNodes) return false;
        totalNodes += nodeCount;
        for (unsigned j = 0; j < nodeCount; ++j)
        {
            Node node;
            node.index = in.U32(); node.notifyChild = in.U32(); node.parent = in.U32(); unsigned cycle = in.U32();
            if (cycle > 65535) return false;
            node.cycle = static_cast<std::int16_t>(cycle);
            node.time = in.Float(); node.goalTime = in.Float(); node.goalWeight = in.Float(); node.weight = in.Float(); node.rate = in.Float();
            entity.nodes.push_back(node);
        }
        candidate.entities.push_back(std::move(entity));
    }
    unsigned actorCount = in.U32();
    if (!in.ok || actorCount > MaxActors) return false;
    for (unsigned i = 0; i < actorCount; ++i)
    {
        Actor actor;
        if (!ReadActor(in, actor)) return false;
        candidate.actors.push_back(std::move(actor));
    }
    unsigned commandCount = in.U32();
    if (!in.ok || commandCount > MaxCommands) return false;
    for (unsigned i = 0; i < commandCount; ++i)
    {
        Command command;
        if (!ReadCommand(in, command)) return false;
        candidate.commands.push_back(command);
    }
    if (!in.ok || in.offset != size || !Valid(candidate)) return false;
    frame = std::move(candidate); return true;
}
}
