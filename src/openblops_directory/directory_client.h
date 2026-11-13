#pragma once
#include "directory_protocol.h"
#include <functional>
namespace bo1dir
{
// One instance per actual UDP transport. This port uses one shared ip_socket
// for listen-server and remote-client traffic. All state below is main-thread only.
class Client
{
  public:
    using Send = std::function<void(const Endpoint &, const std::string &)>;
    using Random = std::function<std::string()>;
    Client(Endpoint endpoint, std::string cohort, Send send, Random random, const char *platform = "pc",
           GameMode mode = GameMode::Multiplayer);
    void Tick(uint64_t now, bool nativeConnected = false);
    void Receive(Endpoint from, const char *data, size_t length, uint64_t now);
    void Refresh(uint64_t now);
    void CancelRefresh();
    bool Join(size_t row, bool allowRelay, bool forceRelay, uint64_t now);
    void CancelJoin();
    void ConnectionStarted(const Endpoint &);
    void SetHost(bool enabled, const Row &row, uint64_t now);
    void Stop();
    bool TakeConnect(Endpoint &);
    bool TakeDisconnect();
    const std::vector<Row> &Rows() const
    {
        return rows;
    }
    bool Refreshing() const
    {
        return refreshing;
    }
    bool Joining() const
    {
        return phase != Idle;
    }
    unsigned Revision() const
    {
        return revision;
    }
    const std::string &Status() const
    {
        return status;
    }

  private:
    struct Request
    {
        std::string id, op, fields, wire;
        uint64_t sent = 0;
        int tries = 0;
        bool active = false;
    };
    struct Binding
    {
        Endpoint endpoint;
        std::string token, role;
        uint64_t start = 0, sent = 0;
    };
    enum Phase
    {
        Idle,
        AwaitDirect,
        ConnectingDirect,
        AwaitRelay,
        BindingRelay,
        ConnectingRelay
    };
    Endpoint endpoint, connectEndpoint;
    std::string compat, platform, cookie, previousCookie, status = "Ready", joinId;
    GameMode gameMode;
    Send send;
    Random random;
    std::vector<Row> rows;
    std::vector<Binding> bindings;
    Request hello, list, heartbeat, join;
    Phase phase = Idle;
    bool refreshing = false, publishing = false, announced = false, allowRelay = false;
    bool connectReady = false, disconnectReady = false, nativeStarted = false;
    unsigned revision = 0;
    int cursor = 0;
    uint64_t cookieAt = 0, refreshAt = 0, pageAt = 0, joinAt = 0, phaseAt = 0, heartbeatAt = 0,
             helloBackoff = 0;
    bool listReauthed = false, joinReauthed = false;
    Row host, joinRow;
    void Start(Request &, const char *, std::string fields, uint64_t now);
    void Poll(Request &, uint64_t now);
    void Transmit(Request &, uint64_t now);
    void Fail(Request &, const std::string &, uint64_t now);
    void RequestJoin(bool relay, uint64_t now);
    void Connect(Endpoint e, Phase next, uint64_t now);
};
} // namespace bo1dir
