// Exercise the production native client over real UDP without starting the game.
#include "openblops_directory/directory_client.h"
#include <chrono>
#include <cstring>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
using namespace bo1dir;
uint64_t Now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
void Check(bool ok, const char *what)
{
    if (!ok)
        throw std::runtime_error(what);
}
sockaddr_in Address(Endpoint e)
{
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(e.port);
    std::memcpy(&a.sin_addr, e.ip.data(), 4);
    return a;
}
struct Peer
{
    SOCKET socket = INVALID_SOCKET;
    unsigned serial = 0;
    Endpoint hostRelay;
    std::deque<std::string> native;
    Client client;
    Peer(Endpoint directory, const char *seed, GameMode gameMode)
        : client(
              directory, "directory-native-smoke",
              [this](const Endpoint &e, const std::string &s) {
                  Send(e, s);
                  if (s.compare(0, 11, "T5RB1 host ") == 0)
                      hostRelay = e;
              },
              [this, seed]() { return std::string(seed) + std::to_string(++serial); }, "pc", gameMode)
    {
        socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        Check(socket != INVALID_SOCKET, "UDP socket");
        sockaddr_in local{};
        local.sin_family = AF_INET;
        Check(bind(socket, (sockaddr *)&local, sizeof(local)) == 0, "UDP bind");
        u_long mode = 1;
        Check(ioctlsocket(socket, FIONBIO, &mode) == 0, "nonblocking UDP");
    }
    ~Peer()
    {
        if (socket != INVALID_SOCKET)
            closesocket(socket);
    }
    void Send(Endpoint to, const std::string &bytes)
    {
        auto a = Address(to);
        Check(sendto(socket, bytes.data(), (int)bytes.size(), 0, (sockaddr *)&a, sizeof(a)) ==
                  (int)bytes.size(),
              "UDP send");
    }
    void Poll()
    {
        for (int i = 0; i < 64; ++i)
        {
            char bytes[16384];
            sockaddr_in from{};
            int length = sizeof(from);
            int n = recvfrom(socket, bytes, sizeof(bytes), 0, (sockaddr *)&from, &length);
            if (n < 0)
            {
                Check(WSAGetLastError() == WSAEWOULDBLOCK, "UDP receive");
                break;
            }
            Endpoint e;
            std::memcpy(e.ip.data(), &from.sin_addr, 4);
            e.port = ntohs(from.sin_port);
            if (n >= 6 && (!std::memcmp(bytes, "T5IX1 ", 6) || !std::memcmp(bytes, "T5RA1 ", 6)))
                client.Receive(e, bytes, n, Now());
            else if (n >= 4 && !std::memcmp(bytes, "T5P1", 4))
            {
            }
            else
                native.emplace_back(bytes, n);
        }
        client.Tick(Now());
    }
};
int main(int argc, char **argv)
{
    WSADATA wsa{};
    Check(WSAStartup(MAKEWORD(2, 2), &wsa) == 0, "Winsock startup");
    try
    {
        Check(argc >= 2 && argc <= 4, "Expected directory IPv4:port [mp|zm] [fallback]");
        const bool zombies = argc >= 3 && !std::strcmp(argv[2], "zm");
        const bool fallback = argc == 4 && !std::strcmp(argv[3], "fallback");
        const auto gameMode = zombies ? GameMode::Zombies : GameMode::Multiplayer;
        Endpoint endpoint;
        Check(Endpoint::Parse(argv[1], endpoint), "numeric endpoint");
        Peer host(endpoint, "nativehost", gameMode), guest(endpoint, "nativeguest", gameMode);
        auto pump = [&]() {
            host.Poll();
            guest.Poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        };
        Row row;
        row.name = "PROTOCOL TEST - NOT PLAYABLE";
        row.map = zombies ? "zombie_theater" : "mp_nuked";
        row.gametype = zombies ? "zom" : "dm";
        row.capacity = zombies ? 4 : 8;
        row.players = 1;
        host.client.SetHost(true, row, Now());
        struct Cleanup
        {
            Peer &h;
            ~Cleanup()
            {
                h.client.Stop();
            }
        } cleanup{host};
        uint64_t deadline = Now() + 18000, refreshAt = 0;
        while (guest.client.Rows().empty() && Now() < deadline)
        {
            if (!guest.client.Refreshing() && Now() >= refreshAt)
            {
                guest.client.Refresh(Now());
                refreshAt = Now() + 1000;
            }
            pump();
        }
        Check(guest.client.Rows().size() == 1, "Native public registration/discovery");
        std::cout << "PASS: native C++ hello, heartbeat and browser list\n";
        Check(guest.client.Join(0, true, false, Now()), "row-zero Join");
        Endpoint destination;
        deadline = Now() + 8000;
        bool connected = false;
        while (Now() < deadline)
        {
            pump();
            if (guest.client.TakeConnect(destination))
            {
                connected = true;
                break;
            }
        }
        Check(connected && destination.port != 0, "native direct rendezvous");
        std::cout << "PASS: native C++ direct rendezvous and validated endpoint\n";
        if (fallback)
            guest.client.ConnectionStarted(destination); // Intentionally leave native direct connection incomplete.
        else
        {
            guest.client.CancelJoin();
            Check(guest.client.Join(0, true, true, Now()), "relay Join");
        }
        deadline = Now() + 18000;
        connected = false;
        while (Now() < deadline)
        {
            pump();
            if (guest.client.TakeConnect(destination))
            {
                connected = true;
                break;
            }
        }
        Check(connected && destination.ip == endpoint.ip && destination.port >= 31000 &&
                  destination.port <= 31015,
              "native authenticated relay READY");
        if (fallback) Check(guest.client.TakeDisconnect(), "automatic direct failure requested disconnect before relay");
        guest.client.ConnectionStarted(destination);
        guest.client.Tick(Now(), true);
        std::string payload("\xff\xff\xff\xffnative-client\0binary", 25);
        guest.Send(destination, payload);
        deadline = Now() + 3000;
        while (host.native.empty() && Now() < deadline)
            pump();
        Check(!host.native.empty() && host.native.front() == payload, "native client-to-host binary relay");
        host.native.clear();
        std::string response("\0\x01\x02native-host\xff", 15);
        Check(host.hostRelay.port != 0, "host relay bound on its own UDP socket");
        host.Send(host.hostRelay, response);
        deadline = Now() + 3000;
        while (guest.native.empty() && Now() < deadline)
            pump();
        Check(!guest.native.empty() && guest.native.front() == response,
              "native host-to-client binary relay");
        std::cout << "PASS: native C++ host/client relay binding and unchanged binary datagrams both ways\n";
        host.client.SetHost(false, {}, Now());
        guest.client.Refresh(Now());
        deadline = Now() + 6000;
        uint64_t nextRefresh = Now() + 250;
        while (Now() < deadline)
        {
            pump();
            if (!guest.client.Refreshing())
            {
                if (guest.client.Rows().empty())
                    break;
                if (Now() >= nextRefresh)
                {
                    guest.client.Refresh(Now());
                    nextRefresh = Now() + 250;
                }
            }
        }
        Check(!guest.client.Refreshing() && guest.client.Rows().empty(), "published test host cleanup");
        std::cout << "PASS: native C++ bye and listing cleanup; no game launched\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << "\n";
        WSACleanup();
        return 1;
    }
    WSACleanup();
}
