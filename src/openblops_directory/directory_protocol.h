#pragma once
// T5IX1 contains text and fixed-width values only, never engine ABI structures.
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
namespace bo1dir
{
struct Endpoint
{
    std::array<unsigned char, 4> ip{};
    unsigned short port = 0;
    bool operator==(const Endpoint &b) const
    {
        return ip == b.ip && port == b.port;
    }
    bool operator!=(const Endpoint &b) const
    {
        return !(*this == b);
    }
    std::string Text() const;
    static bool Parse(const std::string &, Endpoint &);
};
struct Json
{
    enum Type
    {
        Null,
        Bool,
        Integer,
        String,
        Object,
        Array
    } type = Null;
    std::string text;
    int number = 0;
    bool boolean = false;
    std::vector<std::pair<std::string, Json>> fields;
    std::vector<Json> items;
    const Json &Get(const char *) const;
    bool Int(const char *, int &) const;
    bool Str(const char *, std::string &) const;
};
bool Parse(const char *data, size_t size, Json &out);
std::string Quote(const std::string &);
bool Token(const std::string &, size_t maximum, bool empty = false);
struct Row
{
    std::string id, name, map, gametype, mod;
    int players = 0, capacity = 0;
    bool password = false;
};
enum class GameMode { Multiplayer, Zombies };
bool ReadRow(const Json &, Row &, const std::string &compat, bool requireId = true,
             GameMode mode = GameMode::Multiplayer);
std::string Context(const std::string &compat, GameMode mode = GameMode::Multiplayer);
std::string HostJson(const Row &, const std::string &compat, const char *platform,
                     GameMode mode = GameMode::Multiplayer);
} // namespace bo1dir
