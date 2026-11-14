#include "directory_engine.h"
#if defined(OPENBLOPS_NO_STEAM_AUTH)
#include "directory_client.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <client/cl_main.h>
#include <cgame/cg_main.h>
#include <client/client.h>
#include <client/splitscreen.h>
#include <client_mp/cl_main_mp.h>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <qcommon/cmd.h>
#include <qcommon/common.h>
#include <qcommon/net_chan_mp.h>
#include <server_mp/sv_init_mp.h>
#include <server_mp/sv_main_mp.h>
#include <ui/ui_feeders.h>
#include <ui/keycodes.h>
#include <ui/ui_main.h>
#include <ui/ui_server.h>
#include <ui/ui_shared.h>
#include <ui/ui_utils.h>
#include <ui_mp/ui_main_mp.h>
#include <universal/dvar.h>
#include <win32/win_net.h>
#ifdef KISAK_NX
#include <switch.h>
#else
#include <wincrypt.h>
#endif
#ifndef BO1_DIRECTORY_DEFAULT
#define BO1_DIRECTORY_DEFAULT ""
#endif
namespace
{
using namespace bo1dir;
#ifdef KISAK_SP
constexpr GameMode gameMode = GameMode::Zombies;
#else
constexpr GameMode gameMode = GameMode::Multiplayer;
#endif
std::unique_ptr<Client> client;
const dvar_t *enabled, *publish, *address, *cohort, *relay, *forceRelay, *publicName;
std::string selectedAddress, selectedCohort, lastStatus;
std::string passwordJoinId;
unsigned passwordJoinGeneration = 0;
unsigned lastRevision = ~0u, lastGeneration = 0;
std::atomic<unsigned> generation{1};
std::atomic<uint64_t> pinnedDirectory{0};
uint64_t Packed(Endpoint e)
{
    return (uint64_t(e.ip[0]) << 40) | (uint64_t(e.ip[1]) << 32) | (uint64_t(e.ip[2]) << 24) |
           (uint64_t(e.ip[3]) << 16) | e.port;
}
struct Packet
{
    Endpoint from;
    std::string bytes;
    unsigned generation;
};
std::mutex inboxMutex;
std::deque<Packet> inbox;
std::deque<std::string> connectionNotices;
bool browserRequested = false;
bool frameRunning = false;
std::string lastHostSummary;
uint64_t browserRequestedAt = 0;
cmd_function_s browserCommand, cancelCommand, statusCommand;
uint64_t Now();
void OpenBrowser()
{
    browserRequested = true;
    browserRequestedAt = Now();
}
void Cancel()
{
    Directory_CancelJoin();
    Directory_CancelRefresh();
}
void PrintStatus()
{
    Com_Printf(14, "Directory: %s\n", Directory_Status());
    Com_Printf(14, "Directory state: enabled=%d publish=%d server=%d state=%d map='%s' source=%d rows=%d\n",
               enabled ? enabled->current.enabled : 0, publish ? publish->current.enabled : 0,
               com_sv_running->current.enabled, (int)sv.state, Dvar_GetString("mapname"),
               Dvar_GetInt("ui_netSource"), client ? (int)client->Rows().size() : 0);
}
uint64_t Now()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
netadr_t Native(Endpoint e)
{
    netadr_t n{};
    n.type = NA_IP;
    std::memcpy(n.ip, e.ip.data(), 4);
    n.port = BigShort(e.port);
    n.addrHandleIndex = -1;
    return n;
}
Endpoint Portable(const netadr_t &n)
{
    Endpoint e;
    std::memcpy(e.ip.data(), n.ip, 4);
    e.port = (unsigned short)BigShort(n.port);
    return e;
}
std::string Random()
{
    unsigned char bytes[12];
#ifdef KISAK_NX
    randomGet(bytes, sizeof(bytes));
#else
    HCRYPTPROV provider = 0;
    if (!CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return {};
    const bool ok = CryptGenRandom(provider, sizeof(bytes), bytes) != FALSE;
    CryptReleaseContext(provider, 0);
    if (!ok)
        return {};
#endif
    static const char hex[] = "0123456789abcdef";
    std::string s;
    for (unsigned b : bytes)
    {
        s += hex[b >> 4];
        s += hex[b & 15];
    }
    return s;
}
void Send(const Endpoint &e, const std::string &s)
{
    netadr_t n = Native(e);
    NET_SendPacket(NS_CLIENT1, (unsigned)s.size(), (unsigned char *)s.data(), n);
}
std::string ModTag(const char *raw)
{
    std::string value = raw;
    if (value.compare(0, 5, "mods/") == 0 || value.compare(0, 5, "mods\\") == 0)
        value.erase(0, 5);
    return value;
}
std::string DisplayName(const char *s)
{
    std::string name;
    for (; *s && name.size() < 48; ++s)
        if (*s >= 32 && *s <= 126 && *s != '\\' && *s != '"' && *s != ';')
            name += *s;
    return name.empty() ? "OpenBLOPS match" : name;
}
void Initialize()
{
    if (enabled)
        return;
    enabled = _Dvar_RegisterBool("bo1_directory_enabled", true, 1,
                                 "Use the community multiplayer server directory");
    address = _Dvar_RegisterString("bo1_directory", BO1_DIRECTORY_DEFAULT, 1,
                                   "Directory numeric IPv4:UDP-port (no blocking DNS)");
    // config.cfg saves archived dvars, so builds that started with an empty
    // default left "bo1_directory" "" behind, which would hide a default
    // added later. Empty means "use the build's default"; turn discovery off
    // with bo1_directory_enabled 0 instead.
    if (!address->current.string[0] && BO1_DIRECTORY_DEFAULT[0])
        Dvar_SetString(const_cast<dvar_s *>(address), BO1_DIRECTORY_DEFAULT);
#ifdef KISAK_SP
    const char *defaultCohort = "openblops-zombies-v1";
#else
    const char *defaultCohort = "openblops-direct-v1";
#endif
    cohort = _Dvar_RegisterString("bo1_directory_compat", defaultCohort, 1,
                                  "Compatible game/content cohort");
    publish = _Dvar_RegisterBool("bo1_directory_publish", false, 0,
                                 "Opt in to publicly listing this running match");
    relay = _Dvar_RegisterBool("bo1_directory_relay", true, 1, "Fall back to the directory UDP relay");
    forceRelay =
        _Dvar_RegisterBool("bo1_directory_force_relay", false, 0, "Use relay immediately for testing");
    publicName = _Dvar_RegisterString("bo1_directory_name", "OpenBLOPS match", 1,
                                      "Public match label (not a player roster)");
    _Dvar_RegisterString("bo1_directory_status", "Ready", 0x40, "Community directory status");
    Cmd_AddCommandInternal("directory_browser", OpenBrowser, &browserCommand);
    Cmd_AddCommandInternal("directory_cancel", Cancel, &cancelCommand);
    Cmd_AddCommandInternal("directory_status", PrintStatus, &statusCommand);
}
void UpdateRows()
{
    if (!client || lastRevision == client->Revision())
        return;
    lastRevision = client->Revision();
    cls.numunrankedservers = (int)client->Rows().size();
    for (int i = 0; i < cls.numunrankedservers; ++i)
    {
        const Row &r = client->Rows()[i];
        auto &s = cls.unrankedServers[i];
        s = {};
        s.adr.type = NA_BAD;
        s.adr.addrHandleIndex = -1;
        s.ping = -1;
        s.gotInfo = 1;
        s.dirty = 1;
        I_strncpyz(s.hostName, r.name.c_str(), sizeof(s.hostName));
        I_strncpyz(s.mapName, r.map.c_str(), sizeof(s.mapName));
        I_strncpyz(s.gameType, r.gametype.c_str(), sizeof(s.gameType));
        I_strncpyz(s.game, r.mod.empty() ? "" : ("mods/" + r.mod).c_str(), sizeof(s.game));
        s.clients = (unsigned char)r.players;
        s.maxClients = (unsigned char)r.capacity;
        s.bPassword = r.password;
        s.mod = !r.mod.empty();
        s.licensetype = 5;
    }
}
bool TryOpenBrowser()
{
    auto *ui = UI_GetInfo(0);
    // PC MP has a 2D frontend: mapname is empty and sv_running is false.
    // A menu map is required only when a local server is actually running.
    if (!ui || (com_sv_running->current.enabled && !Com_IsMenuLevel(nullptr)))
        return false;
#ifdef KISAK_SP
    if (!Directory_ZombiesPrepareMenus(&ui->uiDC)) return false;
#endif
    if (!Menus_FindByName(&ui->uiDC, "server_browser_unranked"))
        return false;
    for (const char *name : {"server_browser_ranked", "server_browser_wager"})
    {
        auto *menu = Menus_FindByName(&ui->uiDC, name);
        if (menu && Window_IsVisible(ui->uiDC.contextIndex, &menu->window))
            Menus_CloseByName(0, &ui->uiDC, name);
    }
    Dvar_SetIntByName("ui_netSource", 0);
    Menus_OpenByName(0, &ui->uiDC, "server_browser_unranked");
    // Its stock onOpen can already have called PopulateServers.
    if (!client->Refreshing() && client->Rows().empty())
        UI_StartServerRefresh(0, ui->uiDC.contextIndex, 1);
    Com_Printf(14, "Directory: opened community Unranked browser\n");
    return true;
}
} // namespace
void Directory_Init()
{
    Initialize();
}
bool Directory_Source(int source)
{
    return enabled && enabled->current.enabled && source == 0;
}

#ifdef KISAK_NX
static bool EditDirectoryText(const char *dvar, bool password, int limit)
{
    SwkbdConfig keyboard{};
    if (R_FAILED(swkbdCreate(&keyboard, 0)))
    {
        Com_PrintWarning(13, "Could not open the Switch software keyboard.\n");
        return false;
    }
    if (password)
        swkbdConfigMakePresetPassword(&keyboard);
    else
        swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetHeaderText(&keyboard, password ? "Server password" : "Server name");
    swkbdConfigSetInitialText(&keyboard, Dvar_GetString(dvar));
    swkbdConfigSetStringLenMax(&keyboard, limit);
    char text[48 * 4 + 1]{};
    const Result result = swkbdShow(&keyboard, text, sizeof(text));
    swkbdClose(&keyboard);
    if (R_SUCCEEDED(result) && std::strlen(text) <= static_cast<size_t>(limit))
    {
        Dvar_SetStringByName(dvar, text);
        return true;
    }
    return false;
}
#endif

bool Directory_HandleControllerKey(int localClientNum, UiContext *dc, menuDef_t *menu, int key)
{
    if (localClientNum || !menu->window.name)
        return false;
#ifdef KISAK_NX
    if (Directory_UpdateControllerKey(localClientNum, dc, menu, key))
        return true;
#endif

    const char *name = menu->window.name;
    const bool browser = !I_stricmp(name, "server_browser_unranked") && Directory_Source(Dvar_GetInt("ui_netSource"));
    const bool passwordPrompt = !I_stricmp(name, "join_password_popmenu") && Directory_HasPasswordPrompt();
#ifdef KISAK_NX
    if (key == K_BUTTON_A && (passwordPrompt || !I_stricmp(name, "openblops_zombie_host") || !I_stricmp(name, "server_settings")))
    {
        const int cursor = Menu_GetCursorItem(dc->contextIndex, menu);
        if (cursor >= 0 && cursor < menu->itemCount)
        {
            auto *item = menu->items[cursor];
            const char *dvar = item->dvar;
            if (dvar && Item_IsTextField(item) && Window_HasFocus(dc->contextIndex, &item->window))
            {
                const bool password = !I_stricmp(dvar, "ui_directory_joinPassword") || !I_stricmp(dvar, "g_password");
                if (password || !I_stricmp(dvar, "bo1_directory_name"))
                {
                    // In the join prompt, confirming the keyboard joins (no need
                    // to move down to JOIN afterwards).
                    if (EditDirectoryText(dvar, password, password ? 32 : 48) && passwordPrompt
                        && !I_stricmp(dvar, "ui_directory_joinPassword") && Dvar_GetString(dvar)[0])
                        Directory_AcceptJoinPassword();
                    return true;
                }
            }
            // JOIN / CANCEL: the Zombies prompt is built in code and its buttons
            // did not react to A; run their action here.
            if (passwordPrompt && !Item_IsTextField(item)
                && Directory_HandleMenuEvent(localClientNum, dc, item, "action"))
                return true;
        }
    }
#endif
    // The Zombies host setup menu is built in code (directory_zombies.cpp):
    // with a controller nothing focused its items, so A did nothing. Up/down
    // move between them and A runs the focused one; left/right stay native
    // (MAX PLAYERS cycles its values).
#ifdef KISAK_NX
    // MP host settings: the native up/down walks the item list in index order,
    // and the password row (a copy placed where the stock VAC row was drawn,
    // PatchServerSettings) was never reached with a controller. Move by screen
    // position between the visible, selectable items instead.
    if (!I_stricmp(name, "server_settings")
        && (key == K_UPARROW || key == K_DPAD_UP || key == K_APAD_UP
            || key == K_DOWNARROW || key == K_DPAD_DOWN || key == K_APAD_DOWN))
    {
        const bool down = key == K_DOWNARROW || key == K_DPAD_DOWN || key == K_APAD_DOWN;
        std::vector<int> order;
        for (int i = 0; i < menu->itemCount; ++i)
        {
            itemDef_s *item = menu->items[i];
            if (!item || (item->window.staticFlags & 0x100000) || !Window_IsVisible(dc->contextIndex, &item->window)
                || !Item_IsVisible(localClientNum, dc->contextIndex, item))
                continue;
            if (!item->onEvent && !Item_IsTextField(item) && item->type != 10)
                continue;
            order.push_back(i);
        }
        std::stable_sort(order.begin(), order.end(), [menu](int a, int b) {
            const auto &ra = menu->items[a]->window.rect, &rb = menu->items[b]->window.rect;
            return ra.y != rb.y ? ra.y < rb.y : ra.x < rb.x;
        });
        if (order.empty())
            return false;
        int at = -1;
        for (int i = 0; i < int(order.size()); ++i)
            if (Window_HasFocus(dc->contextIndex, &menu->items[order[i]]->window))
                at = i;
        const int count = int(order.size());
        const int next = at < 0 ? 0 : (at + (down ? 1 : count - 1)) % count;
        itemDef_s *target = menu->items[order[next]];
        Menu_ClearFocus(localClientNum, dc, menu);
        Menu_SetCursorItem(dc->contextIndex, menu, order[next]);
        Item_SetFocus(localClientNum, dc, target, target->window.rect.x + 1, target->window.rect.y + 1);
        return true;
    }
#endif
    // Switch only; the PC keeps its mouse/native handling.
#ifdef KISAK_NX
    const bool zombieHost = !I_stricmp(name, "openblops_zombie_host");
#else
    const bool zombieHost = false;
#endif
    if (!browser && !passwordPrompt && !zombieHost)
        return false;
    if (zombieHost)
    {
        if (key == K_BUTTON_A)
        {
            const int cursor = Menu_GetCursorItem(dc->contextIndex, menu);
            if (cursor >= 0 && cursor < menu->itemCount
                && Directory_HandleMenuEvent(localClientNum, dc, menu->items[cursor], "action"))
                return true;
            return false;   // fields and values: native
        }
        const bool zUp = key == K_UPARROW || key == K_DPAD_UP || key == K_APAD_UP;
        const bool zDown = key == K_DOWNARROW || key == K_DPAD_DOWN || key == K_APAD_DOWN;
        if (zUp)
            Menu_SetPrevCursorItem(localClientNum, dc, menu, 1);
        else if (zDown)
            Menu_SetNextCursorItem(localClientNum, dc, menu, 0, 1);
        else
            return false;
        return true;
    }

    if (browser && key == K_BUTTON_X)
    {
        UI_StartServerRefresh(localClientNum, dc->contextIndex, 1);
        return true;
    }
    const bool up = key == K_UPARROW || key == K_DPAD_UP || key == K_APAD_UP;
    const bool down = key == K_DOWNARROW || key == K_DPAD_DOWN || key == K_APAD_DOWN;
    if (browser)
    {
        auto *item = Menu_FindItemByName(menu, "browser");
        if (item && Window_HasFocus(dc->contextIndex, &item->window))
        {
            if (up || down)
            {
                Item_ListBox_HandleKey(localClientNum, dc, item, up ? K_UPARROW : K_DOWNARROW, 1, 1);
                return true;
            }
            if (key == K_BUTTON_A)
            {
                auto *list = Item_GetListBoxDef(item);
                if (!list)
                    return true;
                const int count = UI_FeederCount(localClientNum, dc->contextIndex, list->special, list);
                const int row = list->cursorPos[dc->contextIndex];
                if (row >= 0 && row < count)
                {
                    UI_FeederSelection(localClientNum, dc->contextIndex, list->special, row);
                    UI_RunMenuScript_JoinServer(localClientNum, dc->contextIndex);
                }
                return true;
            }
        }
    }

    if (up || key == K_LEFTARROW || key == K_DPAD_LEFT || key == K_APAD_LEFT)
        Menu_SetPrevCursorItem(localClientNum, dc, menu, 1);
    else if (down || key == K_RIGHTARROW || key == K_DPAD_RIGHT || key == K_APAD_RIGHT)
        Menu_SetNextCursorItem(localClientNum, dc, menu, 0, 1);
    else
        return false; // Native A on a button and B/Back continue through Menu_HandleKey.
    return true;
}

bool Directory_UpdateBrowserCounts()
{
    if (!client || !cls.uiStarted || !Directory_Source(Dvar_GetInt("ui_netSource"))) return false;
    int players = 0;
    for (const auto &row : client->Rows()) players += row.players;
    Dvar_SetStringByName("ui_browserPlayerCount", UI_ReplaceConversionInt(
        UI_SafeTranslateString("PLATFORM_PLAYERS_ONLINE"), players));
    Dvar_SetStringByName("ui_browserDedicatedServerCount", UI_ReplaceConversionInt(
        UI_SafeTranslateString("PLATFORM_ALL_SERVERS"), int(client->Rows().size())));
    return true;
}
bool Directory_IsRow(int source, int index)
{
    return Directory_Source(source) && client && index >= 0 && index < (int)client->Rows().size() &&
           index < cls.numunrankedservers;
}
const char *Directory_Status()
{
    return client ? client->Status().c_str() : lastStatus.c_str();
}
void Directory_TransportReset()
{
    generation.fetch_add(1, std::memory_order_relaxed);
}
bool Directory_Packet(const netadr_t &from, const unsigned char *bytes, int size)
{
    const bool json = size >= 6 && !std::memcmp(bytes, "T5IX1 ", 6);
    const bool ack = size >= 6 && !std::memcmp(bytes, "T5RA1 ", 6);
    if ((size >= 4 && !std::memcmp(bytes, "T5P1", 4)) || (size >= 6 && !std::memcmp(bytes, "T5RB1 ", 6)))
        return true;
    if (!json && !ack)
        return false;
    const uint64_t pinned = pinnedDirectory.load(std::memory_order_relaxed);
    const Endpoint peer = Portable(from);
    const uint64_t packed = Packed(peer);
    if (size <= 1200 && from.type == NA_IP && pinned &&
        ((json && packed == pinned) || (ack && (packed >> 16) == (pinned >> 16))))
    {
        std::lock_guard<std::mutex> lock(inboxMutex);
        if (inbox.size() < 64)
            inbox.push_back({Portable(from), std::string((const char *)bytes, size),
                             generation.load(std::memory_order_relaxed)});
    }
    return true;
}
const char *Directory_BrowserMenu(const char *name)
{
#ifdef KISAK_NX
    // Out of date: the browser and the host menus open the update popup.
    name = Directory_UpdateGateMenu(name);
#endif
    if (!name || !Directory_Source(0))
        return name;
    for (const char *legacy : {"server_browser_ranked", "server_browser_wager", "server_browser_friends",
                               "server_browser_favorites", "server_browser_history"})
        if (!std::strcmp(name, legacy))
            return "server_browser_unranked";
    return name;
}
bool Directory_PopulateServers(int localClientNum, int contextIndex)
{
    if (!Directory_Source(0))
        return false;
    const int source = Dvar_GetInt("ui_netSource");
    if (source == 0)
    {
        // Native focus/reopen scripts also call PopulateServers. They must not
        // discard a completed directory snapshot or restart a pending request.
        if (client && client->Refreshing())
            return true;
        if (client && !client->Rows().empty())
        {
            auto *ui = UI_UIContext_GetInfo(contextIndex);
            if (!sharedUiInfo.serverStatus.numDisplayServers)
                UI_BuildServerDisplayList(localClientNum, ui, 1);
            else
                UI_UpdateDisplayServers(localClientNum, ui);
        }
        else
            UI_StartServerRefresh(localClientNum, contextIndex, 1);
    }
    else if (source == 1 || source == 2)
        OpenBrowser(); // Explicitly move legacy defaults to Unranked.
    else
        Com_Printf(14, "Community directory discovery supports Unranked only.\n");
    return true;
}
// Check session selection as well as guest admission: loading a local match
// must never briefly publish before player two has joined.
static bool Directory_IsSplitScreenHost()
{
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
    if (CL_SplitScreenSessionRequested() || CL_IsSplitGuest(1))
        return true;
#endif
    return Dvar_GetBool("splitscreen");
}
static bool Directory_IsCompatibleHostMap()
{
#ifdef KISAK_SP
    // Campaign, Dead Ops and frontend servers never enter the co-op list.
    return Com_IsZombieMap(Dvar_GetString("mapname")) && !Dvar_GetBool("zombietron");
#else
    return true;
#endif
}

void Directory_Frame()
{
    if (frameRunning)
        return;
    frameRunning = true;
    struct FrameGuard
    {
        ~FrameGuard()
        {
            frameRunning = false;
        }
    } guard;
    Initialize();
#ifdef KISAK_NX
    Directory_UpdateFrame();
#endif
    Directory_FrontendFrame();
    const unsigned gen = generation.load(std::memory_order_relaxed);
    const bool changed = lastGeneration != gen || selectedAddress != address->current.string ||
                         selectedCohort != cohort->current.string;
    if (changed || !enabled->current.enabled)
    {
        pinnedDirectory.store(0, std::memory_order_relaxed);
        if (client)
        {
            if (lastGeneration == gen)
                client->Stop();
            client.reset();
            cls.waitdwfindsessionsresponse = 0;
            cls.numunrankedservers = 0;
        }
        selectedAddress = address->current.string;
        selectedCohort = cohort->current.string;
        lastGeneration = gen;
        lastRevision = ~0u;
    }
    if (!enabled->current.enabled)
        return;
    if (!client)
    {
        Endpoint endpoint;
        if (!Endpoint::Parse(selectedAddress, endpoint) || !Token(selectedCohort, 32))
        {
            if (lastStatus != "Invalid directory IPv4:port or compatibility tag")
            {
                lastStatus = "Invalid directory IPv4:port or compatibility tag";
                Com_PrintWarning(14, "%s\n", lastStatus.c_str());
                Dvar_SetStringByName("bo1_directory_status", lastStatus.data());
            }
            return;
        }
#ifdef KISAK_NX
        const char *platform = "switch";
#else
        const char *platform = "pc";
#endif
        client.reset(new Client(endpoint, selectedCohort, Send, Random, platform, gameMode));
        pinnedDirectory.store(Packed(endpoint), std::memory_order_relaxed);
    }
    const uint64_t now = Now();
    if (browserRequested)
    {
        // Clear before opening: native onOpen scripts may request a refresh.
        browserRequested = false;
        if (!TryOpenBrowser())
        {
            if (now - browserRequestedAt <= 20000)
                browserRequested = true;
            else
            {
                auto *ui = UI_GetInfo(0);
                Com_PrintWarning(
                    14, "Directory: could not open Unranked browser (menus=%d server=%d map='%s')\n",
                    ui ? ui->uiDC.menuCount : 0, com_sv_running->current.enabled, Dvar_GetString("mapname"));
            }
        }
    }
    std::deque<Packet> packets;
    std::deque<std::string> notices;
    {
        std::lock_guard<std::mutex> lock(inboxMutex);
        packets.swap(inbox);
        notices.swap(connectionNotices);
    }
    for (auto &p : packets)
        if (p.generation == gen)
            client->Receive(p.from, p.bytes.data(), p.bytes.size(), now);
    for (const auto &notice : notices)
    {
        Endpoint e;
        if (notice.empty() || !Endpoint::Parse(notice, e))
            client->CancelJoin();
        else
            client->ConnectionStarted(e);
    }
    Row host;
    const bool localSplitScreen = Directory_IsSplitScreenHost();
    bool hosting = !localSplitScreen && publish->current.enabled && com_sv_running->current.enabled && sv.state == SS_GAME &&
                   svs.clients && !Com_IsMenuLevel(nullptr) && Directory_IsCompatibleHostMap();
    if (hosting)
    {
        host.name = DisplayName(publicName->current.string);
        host.map = Dvar_GetString("mapname");
#ifdef KISAK_SP
        host.gametype = "zom";
#else
        host.gametype = Dvar_GetString("g_gametype");
#endif
        host.mod = ModTag(Dvar_GetString("fs_game"));
        host.capacity = sv_maxclients->current.integer;
        host.password = Dvar_GetString("g_password")[0] != 0;
        for (int i = 0; i < host.capacity; ++i)
            if (svs.clients[i].header.state >= CS_CONNECTED)
                ++host.players;
        Json check;
        const std::string raw = HostJson(host, selectedCohort, "pc", gameMode);
        Row validated;
        hosting = Parse(raw.data(), raw.size(), check) && ReadRow(check, validated, selectedCohort, false, gameMode);
    }
    if (publish->current.enabled)
    {
        std::string summary;
        if (localSplitScreen)
            summary = "publication blocked: local split-screen match";
        else if (hosting)
            summary = "advertising " + host.map + " " + host.gametype + " (" + std::to_string(host.players) +
                      "/" + std::to_string(host.capacity) + ")";
        else if (com_sv_running->current.enabled && sv.state == SS_GAME && !Com_IsMenuLevel(nullptr))
            summary = "publication blocked: invalid metadata map='" + host.map + "' gametype='" +
                      host.gametype + "' mod='" + host.mod + "' capacity=" + std::to_string(host.capacity);
        else
            summary = "waiting for a running match";
        if (summary != lastHostSummary)
        {
            lastHostSummary = summary;
            Com_Printf(14, "Directory host: %s\n", summary.c_str());
        }
    }
    else
        lastHostSummary.clear();
    client->SetHost(hosting, host, now);
    // A frontend listen client is already CA_ACTIVE; it must not count as
    // success before the queued remote connect command actually starts.
    bool connected = false;
    if (!com_sv_running->current.enabled && CL_GetLocalClientConnectionState(0) >= CA_CONNECTED)
        connected = true;
    client->Tick(now, connected);
    UpdateRows();
    Directory_UpdateBrowserCounts();
    cls.waitdwfindsessionsresponse = client->Refreshing() ? 1 : 0;
    if (client->Status() != lastStatus)
    {
        lastStatus = client->Status();
        Dvar_SetStringByName("bo1_directory_status", lastStatus.data());
        Com_Printf(14, "Directory: %s\n", lastStatus.c_str());
    }
    if (client->TakeDisconnect() && CL_GetLocalClientConnectionState(0) >= CA_CONNECTING)
        CL_Disconnect(0, 1);
    Endpoint destination;
    if (client->TakeConnect(destination))
    {
        // Text is constructed only from validated numeric address components.
        Cbuf_AddText(0, va("set onlinegame 0; set systemlink 1; set xblive_basictraining 0; connect %s\n",
                           destination.Text().c_str()));
    }
}
bool Directory_Refresh(int source)
{
    Directory_Init();
    if (!client)
        Directory_Frame();
    if (!enabled || !enabled->current.enabled)
        return false;
    cls.waitdwfindsessionsresponse = 0;
    if (source != 0)
    {
        serverInfo_t *servers = nullptr;
        int *count = nullptr;
        if (source >= 0 && source < 6 && CL_GetServerList(source, &servers, &count) && count)
            *count = 0;
        Com_Printf(14, "Community directory is available in Unranked only.\n");
        return true;
    }
    if (client)
    {
        client->Refresh(Now());
        UpdateRows();
        cls.waitdwfindsessionsresponse = 1;
        cls.pingUpdateSource = 0;
    }
    return true;
}
void Directory_CancelRefresh()
{
    if (client)
    {
        client->CancelRefresh();
        cls.waitdwfindsessionsresponse = 0;
    }
}
void Directory_CancelJoin()
{
    std::lock_guard<std::mutex> lock(inboxMutex);
    if (connectionNotices.size() < 16)
        connectionNotices.emplace_back();
}
void Directory_ConnectionStarted(const char *address)
{
    std::lock_guard<std::mutex> lock(inboxMutex);
    if (connectionNotices.size() < 16)
        connectionNotices.emplace_back(address);
}
// The popup owns a directory ID, never a display-list position: refresh/sort
// changes must not redirect the entered password to another game host.
bool Directory_PromptJoinPassword(int source, int index)
{
    if (!Directory_IsRow(source, index) || !client->Rows()[index].password) return false;
    passwordJoinId = client->Rows()[index].id;
    passwordJoinGeneration = generation.load(std::memory_order_relaxed);
    _Dvar_RegisterString("ui_directory_joinPassword", "", 0, "Pending server password");
    Dvar_SetStringByName("ui_directory_joinPassword", "");
    Dvar_SetStringByName("password", "");
    auto *ui = UI_GetInfo(0);
    Menus_OpenByName(0, &ui->uiDC, "join_password_popmenu");
    return true;
}
bool Directory_HasPasswordPrompt() { return !passwordJoinId.empty(); }
void Directory_CancelJoinPassword()
{
    if (passwordJoinId.empty()) return;
    passwordJoinId.clear();
    Dvar_SetStringByName("ui_directory_joinPassword", "");
    Dvar_SetStringByName("password", "");
}
bool Directory_AcceptJoinPassword()
{
    if (passwordJoinId.empty()) return false;
    const std::string id = passwordJoinId, entered = Dvar_GetString("ui_directory_joinPassword");
    const unsigned selectedGeneration = passwordJoinGeneration;
    passwordJoinId.clear(); // Closing an accepted popup must not cancel its password.
    Dvar_SetStringByName("ui_directory_joinPassword", "");
    auto *ui = UI_GetInfo(0);
    Menus_CloseByName(0, &ui->uiDC, "join_password_popmenu");
    if (client && selectedGeneration == generation.load(std::memory_order_relaxed))
    {
        const auto &rows = client->Rows();
        for (size_t i = 0; i < rows.size(); ++i)
            if (rows[i].id == id && Directory_IsRow(0, int(i)))
            {
                Dvar_SetStringByName("password", const_cast<char *>(entered.c_str()));
                CG_SetThirdPerson(0);
                Dvar_SetStringByName("ui_mapname", const_cast<char *>(rows[i].map.c_str()));
                Directory_Join(0, int(i));
                return true;
            }
    }
    Dvar_SetStringByName("password", "");
    Dvar_SetStringByName("ui_errorMessage", "This server is no longer available. Refresh the server list and try again.");
    Menus_OpenByName(0, &ui->uiDC, "error_popmenu");
    return true;
}
bool Directory_Join(int source, int index)
{
    if (!Directory_IsRow(source, index))
        return false;
    const Row &row = client->Rows()[index];
    if (row.map.size() >= sizeof(cls.unrankedServers[0].mapName) ||
        row.gametype.size() >= sizeof(cls.unrankedServers[0].gameType))
    {
        Com_PrintWarning(14, "Directory row cannot fit this build's map/gametype fields\n");
        return true;
    }
    if (row.mod != ModTag(Dvar_GetString("fs_game")))
    {
        Dvar_SetStringByName("ui_errorMessage", "This server requires a different mod.");
        Com_PrintWarning(14, "Directory: select the host's mod before joining\n");
        return true;
    }
    if (!row.password) Dvar_SetStringByName("password", "");
    client->Join(index, relay->current.enabled, forceRelay->current.enabled, Now());
    return true;
}
#endif
