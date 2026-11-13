#include "directory_client.h"
#include <algorithm>
#include <cstring>
namespace bo1dir
{
Client::Client(Endpoint e, std::string c, Send s, Random r, const char *p, GameMode mode)
    : endpoint(e), compat(std::move(c)), platform(p), gameMode(mode), send(std::move(s)), random(std::move(r))
{
}
void Client::Start(Request &r, const char *op, std::string fields, uint64_t now)
{
    r = {};
    r.id = random();
    r.op = op;
    r.fields = std::move(fields);
    r.active = true;
    if (r.id.empty())
    {
        Fail(r, "Random request ID unavailable", now);
        return;
    }
    if (op == std::string("hello") || !cookie.empty())
        Transmit(r, now);
}
void Client::Transmit(Request &r, uint64_t now)
{
    r.wire = "T5IX1 {\"op\":" + Quote(r.op) + ",\"request\":" + Quote(r.id);
    if (r.op == "hello")
        r.wire += ",\"padding\":" + Quote(std::string(160, ' '));
    else
        r.wire += ",\"cookie\":" + Quote(cookie);
    if (!r.fields.empty())
        r.wire += "," + r.fields;
    r.wire += "}";
    if (r.wire.size() > 1200)
    {
        Fail(r, "Directory request exceeds protocol limit", now);
        return;
    }
    send(endpoint, r.wire);
    r.sent = now;
    ++r.tries;
}
void Client::Poll(Request &r, uint64_t now)
{
    if (!r.active)
        return;
    if (r.op != "hello" && cookie.empty())
        return;
    if (!r.tries)
    {
        Transmit(r, now);
        return;
    }
    if (now - r.sent < 1600)
        return;
    if (r.tries >= 3)
    {
        Fail(r, "Directory request timed out", now);
        return;
    }
    Transmit(r, now);
}
void Client::Fail(Request &r, const std::string &why, uint64_t now)
{
    const std::string op = r.op;
    // A router can change its public mapping without reopening the local UDP
    // socket. Re-hello once on silence instead of reusing a dead port cookie.
    bool *retried = op == "list" ? &listReauthed : op == "join" ? &joinReauthed : nullptr;
    if (why == "Directory request timed out" && retried && !*retried)
    {
        *retried = true;
        previousCookie = cookie;
        cookie.clear();
        hello = {};
        helloBackoff = now;
        r.id = random();
        r.tries = 0;
        r.sent = 0;
        r.active = !r.id.empty();
        status = "Refreshing directory authentication...";
        return;
    }
    r.active = false;
    status = why;
    if (op == "hello")
    {
        cookie.clear();
        list.active = heartbeat.active = join.active = false;
        refreshing = false;
        CancelJoin();
        helloBackoff = now + 10000;
    }
    if (op == "list")
        refreshing = false;
    if (op == "join")
        CancelJoin();
    if (op == "heartbeat")
    {
        cookie.clear();
        heartbeatAt = now + 10000;
    }
    status = why;
}
void Client::Refresh(uint64_t now)
{
    CancelJoin();
    list = {};
    rows.clear();
    ++revision;
    refreshing = true;
    cursor = 0;
    listReauthed = false;
    refreshAt = now;
    pageAt = now;
    status = "Contacting directory...";
}
void Client::CancelRefresh()
{
    refreshing = false;
    list = {};
}
bool Client::Join(size_t index, bool relay, bool force, uint64_t now)
{
    if (index >= rows.size())
        return false;
    CancelRefresh();
    CancelJoin();
    joinId = rows[index].id;
    joinRow = rows[index];
    joinReauthed = false;
    allowRelay = relay || force;
    joinAt = now;
    RequestJoin(force, now);
    return true;
}
void Client::RequestJoin(bool relay, uint64_t now)
{
    phase = relay ? AwaitRelay : AwaitDirect;
    phaseAt = now;
    status = relay ? "Requesting relay..." : "Requesting direct connection...";
    Start(join, "join",
          Context(compat, gameMode) + ",\"id\":" + Quote(joinId) + ",\"via\":" + Quote(relay ? "relay" : "direct"),
          now);
}
void Client::CancelJoin()
{
    if (phase != Idle)
        status = "Connection canceled";
    phase = Idle;
    join = {};
    connectReady = false;
    bindings.erase(
        std::remove_if(bindings.begin(), bindings.end(), [](const Binding &b) { return b.role == "client"; }),
        bindings.end());
}
void Client::ConnectionStarted(const Endpoint &e)
{
    if (phase == ConnectingDirect || phase == ConnectingRelay)
    {
        if (e != connectEndpoint)
            CancelJoin();
        else
            nativeStarted = true;
    }
    else if (phase != Idle)
        CancelJoin();
}
void Client::Connect(Endpoint e, Phase next, uint64_t now)
{
    connectEndpoint = e;
    connectReady = true;
    nativeStarted = false;
    phase = next;
    phaseAt = now;
    status = next == ConnectingRelay ? "Connecting through relay..." : "Connecting directly...";
}
bool Client::TakeConnect(Endpoint &out)
{
    if (!connectReady)
        return false;
    connectReady = false;
    out = connectEndpoint;
    return true;
}
bool Client::TakeDisconnect()
{
    bool b = disconnectReady;
    disconnectReady = false;
    return b;
}
void Client::SetHost(bool enabled, const Row &row, uint64_t now)
{
    if (!enabled && publishing)
    {
        if (!cookie.empty() && announced)
        {
            Request bye;
            Start(bye, "bye", "", now);
        }
        announced = false;
        heartbeat = {};
        bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
                                      [](const Binding &b) { return b.role == "host"; }),
                       bindings.end());
    }
    if (enabled && !publishing)
    {
        heartbeatAt = now;
        if (!refreshing && phase == Idle)
            status = "Publishing multiplayer host...";
    }
    host = row;
    publishing = enabled;
}
void Client::Stop()
{
    SetHost(false, {}, 0);
    CancelRefresh();
    CancelJoin();
}
void Client::Tick(uint64_t now, bool connected)
{
    if (cookie.size() && now - cookieAt >= 240000)
    {
        previousCookie = cookie;
        cookie.clear();
    }
    const bool needed = refreshing || publishing || phase != Idle;
    if (needed && cookie.empty() && !hello.active && now >= helloBackoff)
        Start(hello, "hello", "", now);
    Poll(hello, now);
    Poll(list, now);
    Poll(heartbeat, now);
    Poll(join, now);
    if (refreshing && now - refreshAt >= 20000)
    {
        list.active = false;
        refreshing = false;
        status = "Directory refresh timed out";
    }
    if (refreshing && !list.active && now >= pageAt && !cookie.empty())
        Start(list, "list", Context(compat, gameMode) + ",\"cursor\":" + std::to_string(cursor), now);
    if (publishing && !heartbeat.active && now >= heartbeatAt && !cookie.empty())
    {
        Start(heartbeat, "heartbeat", "\"host\":" + HostJson(host, compat, platform.c_str(), gameMode), now);
        heartbeatAt = now + 10000;
    }
    if ((phase == ConnectingDirect || phase == ConnectingRelay) && connected && nativeStarted)
    {
        phase = Idle;
        status = "Connected";
    }
    if (phase == ConnectingDirect && now - phaseAt >= 8000)
    {
        if (allowRelay)
        {
            disconnectReady = true;
            RequestJoin(true, now);
        }
        else
        {
            disconnectReady = true;
            CancelJoin();
            status = "Direct connection failed; relay is disabled";
        }
    }
    if (phase != Idle && now - joinAt >= 40000)
    {
        disconnectReady = true;
        CancelJoin();
        status = "Directory connection timed out";
    }
    for (size_t i = 0; i < bindings.size();)
    {
        auto &b = bindings[i];
        if (now - b.start >= 15000)
        {
            bool guest = b.role == "client";
            bindings.erase(bindings.begin() + i);
            if (guest)
            {
                CancelJoin();
                status = "Relay setup timed out";
            }
            continue;
        }
        if (!b.sent || now - b.sent >= 500)
        {
            send(b.endpoint, "T5RB1 " + b.role + " " + b.token);
            b.sent = now;
        }
        ++i;
    }
}
void Client::Receive(Endpoint from, const char *data, size_t length, uint64_t now)
{
    if (length >= 6 && !std::memcmp(data, "T5RA1 ", 6))
    {
        if (length > 160)
            return;
        std::string ack(data + 6, length - 6);
        for (size_t i = 0; i < bindings.size(); ++i)
        {
            const auto &b = bindings[i];
            if (from != b.endpoint)
                continue;
            if (ack == "ready " + b.token)
            {
                bool guest = b.role == "client";
                Endpoint e = b.endpoint;
                bindings.erase(bindings.begin() + i);
                if (guest && phase == BindingRelay)
                    Connect(e, ConnectingRelay, now);
                return;
            }
            if (ack == "waiting " + b.token)
                return;
        }
        return;
    }
    if (from != endpoint || length < 6 || length > 1200 || std::memcmp(data, "T5IX1 ", 6))
        return;
    Json j;
    if (!Parse(data + 6, length - 6, j))
        return;
    std::string op;
    if (!j.Str("op", op))
        return;
    if (op == "punch" || op == "relay")
    {
        std::string auth;
        if (!publishing || !announced || !j.Str("auth", auth) ||
            (auth != cookie && (previousCookie.empty() || auth != previousCookie)))
            return;
        if (op == "punch")
        {
            const auto &peer = j.Get("peer");
            if (peer.type != Json::Array || peer.items.size() != 2 || peer.items[0].type != Json::String ||
                peer.items[1].type != Json::Integer)
                return;
            Endpoint e;
            if (!Endpoint::Parse(peer.items[0].text + ":" + std::to_string(peer.items[1].number), e))
                return;
            for (int i = 0; i < 3; ++i)
                send(e, "T5P1");
            return;
        }
        int port;
        std::string token;
        if (!j.Int("port", port) || port < 1 || port > 65535 || !j.Str("token", token) || !Token(token, 96))
            return;
        Endpoint e = endpoint;
        e.port = (unsigned short)port;
        for (auto &b : bindings)
            if (b.endpoint == e)
            {
                if (b.token == token && b.role == "host")
                    return;
                else
                    return;
            }
        if (bindings.size() < 17)
            bindings.push_back({e, token, "host", now, 0});
        return;
    }
    std::string request;
    if (!j.Str("request", request))
        return;
    Request *r = nullptr;
    for (auto *candidate : {&hello, &list, &heartbeat, &join})
        if (candidate->active && candidate->id == request)
        {
            r = candidate;
            break;
        }
    if (!r)
        return;
    if (op == "error")
    {
        std::string error;
        if (j.Str("error", error) && Token(error, 64))
            Fail(*r, "Directory: " + error, now);
        return;
    }
    if (r == &hello)
    {
        std::string c;
        if (op != "cookie" || !j.Str("cookie", c))
            return;
        auto opaque = c;
        std::replace(opaque.begin(), opaque.end(), '.', '_');
        if (!Token(opaque, 128))
            return;
        previousCookie = cookie.empty() ? previousCookie : cookie;
        cookie = c;
        cookieAt = now;
        r->active = false;
        return;
    }
    if (r == &heartbeat)
    {
        std::string id;
        if (op != "ok" || !j.Str("id", id) || id.size() != 16)
            return;
        const bool first = !announced;
        announced = true;
        if (first && !refreshing && phase == Idle)
            status = "Public host registered: " + host.map;
        r->active = false;
        return;
    }
    if (r == &list)
    {
        if (op != "list" || j.Get("rows").type != Json::Array || j.Get("rows").items.size() > 1)
            return;
        const auto &next = j.Get("next");
        if (next.type != Json::Null &&
            (next.type != Json::Integer || next.number <= cursor || next.number > 4096))
            return;
        Row row;
        for (const auto &value : j.Get("rows").items)
        {
            if (!ReadRow(value, row, compat, true, gameMode))
                return;
            auto old = std::find_if(rows.begin(), rows.end(), [&](const Row &v) { return v.id == row.id; });
            if (old != rows.end())
                *old = row;
            else if (rows.size() < 128)
                rows.push_back(row);
            ++revision;
        }
        r->active = false;
        if (next.type == Json::Null || rows.size() >= 128)
        {
            refreshing = false;
            status = rows.empty() ? "No public multiplayer hosts"
                                  : std::to_string(rows.size()) + " public multiplayer hosts";
        }
        else
        {
            cursor = next.number;
            pageAt = now + 80;
        }
        return;
    }
    if (r == &join)
    {
        std::string via;
        Row row;
        if (op != "join" || !j.Str("via", via) || !ReadRow(j.Get("host"), row, compat, false, gameMode))
            return;
        if (row.mod != joinRow.mod)
        {
            Fail(*r, "Directory: host changed its mod; refresh the browser", now);
            return;
        }
        if (via == "direct" && phase == AwaitDirect)
        {
            const auto &a = j.Get("endpoint");
            if (a.type != Json::Array || a.items.size() != 2 || a.items[0].type != Json::String ||
                a.items[1].type != Json::Integer)
                return;
            Endpoint e;
            if (!Endpoint::Parse(a.items[0].text + ":" + std::to_string(a.items[1].number), e))
                return;
            r->active = false;
            Connect(e, ConnectingDirect, now);
        }
        else if (via == "relay" && phase == AwaitRelay)
        {
            int port;
            std::string token;
            if (!j.Int("port", port) || port < 1 || port > 65535 || !j.Str("token", token) ||
                !Token(token, 96))
                return;
            Endpoint e = endpoint;
            e.port = (unsigned short)port;
            r->active = false;
            phase = BindingRelay;
            phaseAt = now;
            bindings.push_back({e, token, "client", now, 0});
            status = "Binding relay...";
        }
    }
}
} // namespace bo1dir
