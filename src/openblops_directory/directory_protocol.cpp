#include "directory_protocol.h"
#include <climits>
#include <cstdio>
#include <cstring>
namespace bo1dir
{
std::string Endpoint::Text() const
{
    char b[32];
    std::snprintf(b, sizeof(b), "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3], port);
    return b;
}
bool Endpoint::Parse(const std::string &s, Endpoint &out)
{
    Endpoint e;
    size_t p = 0;
    for (int n = 0; n < 5; ++n)
    {
        if (p >= s.size() || s[p] < '0' || s[p] > '9')
            return false;
        unsigned v = 0;
        size_t start = p;
        while (p < s.size() && s[p] >= '0' && s[p] <= '9')
        {
            v = v * 10 + (s[p++] - '0');
            if (v > (n == 4 ? 65535u : 255u))
                return false;
        }
        if (p - start > 1 && s[start] == '0')
            return false;
        if (n < 4)
        {
            if (n < 3)
            {
                e.ip[n] = (unsigned char)v;
                if (p >= s.size() || s[p++] != '.')
                    return false;
            }
            else
            {
                e.ip[3] = (unsigned char)v;
                if (p >= s.size() || s[p++] != ':')
                    return false;
            }
        }
        else
            e.port = (unsigned short)v;
    }
    if (p != s.size() || !e.port || e.ip[0] == 0 || e.ip[0] >= 224 ||
        e.ip == std::array<unsigned char, 4>{255, 255, 255, 255})
        return false;
    out = e;
    return true;
}
const Json &Json::Get(const char *key) const
{
    static const Json nil;
    for (const auto &p : fields)
        if (p.first == key)
            return p.second;
    return nil;
}
bool Json::Int(const char *key, int &n) const
{
    const auto &v = Get(key);
    if (v.type != Integer)
        return false;
    n = v.number;
    return true;
}
bool Json::Str(const char *key, std::string &s) const
{
    const auto &v = Get(key);
    if (v.type != String)
        return false;
    s = v.text;
    return true;
}
namespace
{
struct Parser
{
    const char *p, *end;
    int nodes = 0;
    void Space()
    {
        while (p < end && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t'))
            ++p;
    }
    bool Eat(char c)
    {
        Space();
        if (p == end || *p != c)
            return false;
        ++p;
        return true;
    }
    bool Text(std::string &s)
    {
        if (!Eat('"'))
            return false;
        while (p < end)
        {
            unsigned char c = *p++;
            if (c == '"')
                return true;
            if (c < 32 || c > 126)
                return false;
            if (c == '\\')
            {
                if (p == end)
                    return false;
                c = *p++;
                if (c == 'u')
                {
                    unsigned v = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        if (p == end)
                            return false;
                        char h = *p++;
                        int d = h >= '0' && h <= '9'   ? h - '0'
                                : h >= 'a' && h <= 'f' ? h - 'a' + 10
                                : h >= 'A' && h <= 'F' ? h - 'A' + 10
                                                       : -1;
                        if (d < 0)
                            return false;
                        v = v * 16 + d;
                    }
                    if (v < 32 || v > 126)
                        return false;
                    c = (unsigned char)v;
                }
                else if (c != '"' && c != '\\' && c != '/')
                    return false;
            }
            s += (char)c;
        }
        return false;
    }
    bool Value(Json &v, int depth)
    {
        if (++nodes > 192 || depth > 8)
            return false;
        Space();
        if (p == end)
            return false;
        if (*p == '"')
        {
            v.type = Json::String;
            return Text(v.text);
        }
        if (*p == '{' || *p == '[')
        {
            bool object = *p++ == '{';
            v.type = object ? Json::Object : Json::Array;
            Space();
            char close = object ? '}' : ']';
            if (Eat(close))
                return true;
            do
            {
                Json child;
                std::string key;
                if (object)
                {
                    if (!Text(key) || key.size() > 64 || !Eat(':'))
                        return false;
                    for (auto &f : v.fields)
                        if (f.first == key)
                            return false;
                }
                if (!Value(child, depth + 1))
                    return false;
                if (object)
                    v.fields.emplace_back(key, std::move(child));
                else
                    v.items.push_back(std::move(child));
                if (Eat(close))
                    return true;
            } while (Eat(','));
            return false;
        }
        for (const auto &literal : {std::string("true"), std::string("false"), std::string("null")})
        {
            if (size_t(end - p) >= literal.size() && !std::memcmp(p, literal.data(), literal.size()))
            {
                p += literal.size();
                v.type = literal == "null" ? Json::Null : Json::Bool;
                v.boolean = literal == "true";
                return true;
            }
        }
        bool neg = false;
        if (*p == '-')
        {
            neg = true;
            ++p;
        }
        if (p == end || *p < '0' || *p > '9')
            return false;
        const char *start = p;
        unsigned long long n = 0;
        while (p < end && *p >= '0' && *p <= '9')
        {
            n = n * 10 + (*p++ - '0');
            if (n > 2147483647ULL)
                return false;
        }
        if (p - start > 1 && *start == '0')
            return false;
        v.type = Json::Integer;
        v.number = neg ? -(int)n : (int)n;
        return true;
    }
};
} // namespace
bool Parse(const char *data, size_t size, Json &out)
{
    if (!data || size < 2 || size > 1194)
        return false;
    Json result;
    Parser p{data, data + size};
    if (!p.Value(result, 0))
        return false;
    p.Space();
    if (p.p != p.end || result.type != Json::Object)
        return false;
    out = std::move(result);
    return true;
}
std::string Quote(const std::string &s)
{
    std::string r = "\"";
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\')
            r += '\\';
        if (c < 32 || c > 126)
            r += '?';
        else
            r += (char)c;
    }
    return r + '"';
}
bool Token(const std::string &s, size_t max, bool empty)
{
    if (s.empty())
        return empty;
    if (s.size() > max)
        return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
              c == '-'))
            return false;
    return true;
}
std::string Context(const std::string &compat, GameMode mode)
{
    return std::string(mode == GameMode::Zombies ? "\"mode\":\"zm\",\"protocol\":1044,\"direct\":1,\"anim\":4,\"compat\":"
                                                : "\"mode\":\"mp\",\"protocol\":1044,\"direct\":1,\"anim\":0,\"compat\":") + Quote(compat);
}
bool ReadRow(const Json &j, Row &r, const std::string &compat, bool requireId, GameMode gameMode)
{
    std::string mode, cohort, state, platform;
    int protocol, direct, anim;
    const bool zombies = gameMode == GameMode::Zombies;
    if (j.type != Json::Object || !j.Str("mode", mode) || mode != (zombies ? "zm" : "mp") || !j.Str("compat", cohort) ||
        cohort != compat || !j.Int("protocol", protocol) || protocol != 1044 || !j.Int("direct", direct) ||
        direct != 1 || !j.Int("anim", anim) || anim != (zombies ? 4 : 0) || !j.Str("state", state) || state != "match" ||
        !j.Str("platform", platform) || (platform != "pc" && platform != "switch"))
        return false;
    if (requireId)
    {
        if (!j.Str("id", r.id) || r.id.size() != 16)
            return false;
        for (char c : r.id)
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                return false;
    }
    if (!j.Str("name", r.name) || r.name.empty() || r.name.size() > 48 || !j.Str("map", r.map) ||
        !Token(r.map, 48) || (zombies ? r.map.compare(0, 7, "zombie_") : r.map.compare(0, 3, "mp_")) || !j.Str("gametype", r.gametype) ||
        !Token(r.gametype, 24) || !j.Str("mod", r.mod) || !Token(r.mod, 32, true))
        return false;
    for (char c : r.name)
        if (c < 32 || c > 126 || c == '\\' || c == '"' || c == ';')
            return false;
    if (!j.Int("players", r.players) || !j.Int("maxplayers", r.capacity) || r.players < 0 || r.capacity < 1 ||
        r.capacity > (zombies ? 4 : 18) || r.players > r.capacity || j.Get("password").type != Json::Bool)
        return false;
    r.password = j.Get("password").boolean;
    return true;
}
std::string HostJson(const Row &r, const std::string &compat, const char *platform, GameMode mode)
{
    return "{" + Context(compat, mode) + ",\"name\":" + Quote(r.name) + ",\"map\":" + Quote(r.map) +
           ",\"gametype\":" + Quote(r.gametype) + ",\"mod\":" + Quote(r.mod) +
           ",\"players\":" + std::to_string(r.players) + ",\"maxplayers\":" + std::to_string(r.capacity) +
           ",\"password\":" + (r.password ? "true" : "false") + ",\"platform\":" + Quote(platform) +
           ",\"state\":\"match\"}";
}
} // namespace bo1dir
