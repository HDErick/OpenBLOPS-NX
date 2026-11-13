#include "openblops_directory/directory_client.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace bo1dir;
static GameMode testMode = GameMode::Multiplayer;
struct Fixture
{
    Endpoint directory, other;
    std::vector<std::pair<Endpoint, std::string>> packets;
    unsigned serial = 0;
    Client client;
    Fixture()
        : client(
              Address("192.0.2.10:3075"), "openblops-direct-v1",
              [this](const Endpoint &e, const std::string &s) { packets.emplace_back(e, s); },
              [this]() { return "request" + std::to_string(++serial); }, "pc", testMode)
    {
        directory = Address("192.0.2.10:3075");
        other = Address("127.0.0.1:7777");
    }
    static Endpoint Address(const std::string &s)
    {
        Endpoint e;
        assert(Endpoint::Parse(s, e));
        return e;
    }
    Json Last()
    {
        Json j;
        assert(packets.back().second.compare(0, 6, "T5IX1 ") == 0);
        assert(Parse(packets.back().second.data() + 6, packets.back().second.size() - 6, j));
        return j;
    }
    void Reply(std::string body, uint64_t now)
    {
        std::string s = "T5IX1 " + body;
        client.Receive(directory, s.data(), s.size(), now);
    }
    void Hello(uint64_t now)
    {
        auto req = Last().Get("request").text;
        Reply("{\"op\":\"cookie\",\"request\":" + Quote(req) + ",\"cookie\":\"100.secret_cookie\"}", now);
    }
    Row Host()
    {
        Row row;
        row.id = "0123456789abcdef";
        row.name = "Native integration test";
        row.map = testMode == GameMode::Zombies ? "zombie_theater" : "mp_nuked";
        row.gametype = testMode == GameMode::Zombies ? "zom" : "dm";
        row.capacity = testMode == GameMode::Zombies ? 4 : 8;
        row.players = 1;
        return row;
    }
    std::string HostWire()
    {
        return HostJson(Host(), "openblops-direct-v1", "pc", testMode);
    }
    std::string RowWire()
    {
        auto s = HostWire();
        s.pop_back();
        return s + ",\"id\":\"0123456789abcdef\"}";
    }
    void Listed()
    {
        client.Refresh(0);
        client.Tick(0);
        assert(Last().Get("op").text == "hello");
        Hello(1);
        client.Tick(2);
        auto req = Last().Get("request").text;
        Reply("{\"op\":\"list\",\"request\":" + Quote(req) + ",\"rows\":[" + RowWire() + "],\"next\":null}",
              3);
        assert(!client.Refreshing() && client.Rows().size() == 1);
    }
};
int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "zm") == 0) testMode = GameMode::Zombies;
    Endpoint e;
    assert(Endpoint::Parse("192.0.2.10:3075", e));
    assert(e.Text() == "192.0.2.10:3075");
    for (const char *bad :
         {"localhost:3075", "1.2.3.4:0", "1.2.3.4:65536", "1.2.3.4:3075;quit", "1.2.3.4:3075\n",
          "01.2.3.4:22", "256.2.3.4:22", "224.0.0.1:22", "0.0.0.0:22", "1.2.3:22"})
        assert(!Endpoint::Parse(bad, e));
    Json j;
    for (const char *bad :
         {"{\"op\":\"list\",\"op\":\"join\"}", "{\"a\":2147483648}", "{\"a\":01}", "{\"a\":1.5}",
          "{\"a\":\"\\u0000\"}", "{\"a\":true}junk", "{\"a\":[],,}", "{\"a\":\"bad\nline\"}"})
        assert(!Parse(bad, std::strlen(bad), j));
    std::string nested = "{\"a\":" + std::string(10, '[') + "0" + std::string(10, ']') + "}";
    assert(!Parse(nested.data(), nested.size(), j));
    std::string oversized = "{\"a\":\"" + std::string(1300, 'x') + "\"}";
    assert(!Parse(oversized.data(), oversized.size(), j));
    {
        Fixture f;
        f.Listed();
        auto row = f.RowWire();
        assert(Parse(row.data(), row.size(), j));
        Row r;
        assert(ReadRow(j, r, "openblops-direct-v1", true, testMode));
        assert(!ReadRow(j, r, "wrong-build", true, testMode));
        assert(!ReadRow(j, r, "openblops-direct-v1", true,
                        testMode == GameMode::Zombies ? GameMode::Multiplayer : GameMode::Zombies));
        for (auto &field : j.fields)
            if (field.first == "players")
            {
                field.second.type = Json::Bool;
                field.second.boolean = true;
            }
        assert(!ReadRow(j, r, "openblops-direct-v1", true, testMode));
        assert(!f.client.Join(1, true, false, 10));
        assert(f.client.Join(0, true, false, 10)); // row zero
        auto id = f.Last().Get("request").text;
        std::string reply = "{\"op\":\"join\",\"request\":" + Quote(id) +
                            ",\"via\":\"direct\",\"endpoint\":[\"127.0.0.1\",3074],\"host\":" + f.HostWire() +
                            "}";
        std::string raw = "T5IX1 " + reply;
        f.client.Receive(f.other, raw.data(), raw.size(), 11);
        assert(!f.client.TakeConnect(e));
        f.Reply(reply, 12);
        assert(f.client.TakeConnect(e) && e == Fixture::Address("127.0.0.1:3074"));
        f.client.Tick(13, true);
        assert(f.client.Joining()); // Old frontend state is not remote success.
        f.client.Tick(8013);
        assert(f.client.TakeDisconnect());
        assert(f.Last().Get("via").text == "relay");
        id = f.Last().Get("request").text;
        f.Reply("{\"op\":\"join\",\"request\":" + Quote(id) +
                    ",\"via\":\"relay\",\"port\":31000,\"token\":\"client_token\",\"host\":" + f.HostWire() +
                    "}",
                8014);
        f.client.Tick(8015);
        assert(f.packets.back().second == "T5RB1 client client_token");
        auto re = Fixture::Address("192.0.2.10:31000");
        std::string ack = "T5RA1 ready wrong_token";
        f.client.Receive(re, ack.data(), ack.size(), 8016);
        assert(!f.client.TakeConnect(e));
        ack = "T5RA1 ready client_token";
        f.client.Receive(f.directory, ack.data(), ack.size(), 8017);
        assert(!f.client.TakeConnect(e));
        f.client.Receive(re, ack.data(), ack.size(), 8018);
        assert(f.client.TakeConnect(e) && e == re);
        f.client.ConnectionStarted(re);
        f.client.Tick(8019, true);
        assert(!f.client.Joining());
    }
    {
        Fixture f;
        f.client.Refresh(0);
        f.client.Tick(0);
        auto old = f.Last().Get("request").text;
        f.client.Tick(1600);
        f.client.Tick(3200);
        f.client.Tick(4800);
        assert(!f.client.Refreshing());
        f.client.Refresh(5000);
        f.client.Tick(15000);
        f.Hello(15001);
        f.client.Tick(15002);
        auto stale = f.Last().Get("request").text;
        f.client.Refresh(15003);
        f.client.Tick(15004);
        f.Reply("{\"op\":\"list\",\"request\":" + Quote(stale) + ",\"rows\":[" + f.RowWire() +
                    "],\"next\":null}",
                15005);
        assert(f.client.Rows().empty() && f.client.Refreshing());
        f.client.CancelRefresh();
        assert(!f.client.Refreshing());
    }
    {
        Fixture f;
        f.client.SetHost(true, f.Host(), 1);
        f.client.Tick(1);
        f.Hello(2);
        f.client.Tick(3);
        auto req = f.Last().Get("request").text;
        f.Reply("{\"op\":\"ok\",\"request\":" + Quote(req) + ",\"id\":\"0123456789abcdef\"}", 4);
        size_t before = f.packets.size();
        f.Reply("{\"op\":\"punch\",\"auth\":\"wrong\",\"peer\":[\"127.0.0.1\",7777]}", 5);
        assert(f.packets.size() == before);
        f.Reply("{\"op\":\"punch\",\"auth\":\"100.secret_cookie\",\"peer\":[\"127.0.0.1\",7777]}", 6);
        assert(f.packets.size() == before + 3);
        f.Reply("{\"op\":\"relay\",\"auth\":\"100.secret_cookie\",\"port\":31000,\"token\":\"host_token\"}",
                7);
        f.client.Tick(8);
        assert(f.packets.back().second == "T5RB1 host host_token");
        f.client.SetHost(false, {}, 9);
        assert(f.Last().Get("op").text == "bye");
    }
    {
        Fixture f;
        f.Listed();
        f.client.Refresh(20);
        f.client.Tick(21);
        f.client.Tick(1621);
        f.client.Tick(3221);
        f.client.Tick(4821);
        assert(f.client.Refreshing());
        f.client.Tick(4822);
        assert(f.Last().Get("op").text == "hello");
        f.Hello(4823);
        f.client.Tick(4824);
        assert(f.Last().Get("op").text == "list");
        // Second silent list stops; it cannot loop hello/retry indefinitely.
        f.client.Tick(6424);
        f.client.Tick(8024);
        f.client.Tick(9624);
        assert(!f.client.Refreshing());
    }
    std::cout << (testMode == GameMode::Zombies ? "Zombies: " : "MP: ");
    std::cout << "PASS: protocol bounds/types, mode isolation, row zero, spoof/stale replies, deadlines, direct/relay "
                 "transitions, host auth and cleanup\n";
}
