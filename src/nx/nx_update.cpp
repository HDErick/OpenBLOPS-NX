// The in-game updater (nx_update.h), after the BO2 Switch port's
// nx/updater/update.cpp and net.cpp: HTTPS through the system's ssl service
// (the portlibs have no TLS library), the GitHub releases API, and the zip
// unpacked with the engine's zlib.
#include "nx_update.h"
#include <switch.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include <zlib/zlib.h>
#include "nx_install.h"

#ifndef NX_APP_VERSION
#define NX_APP_VERSION "0.0.0"
#endif

namespace {
// releases/latest skips pre-releases (a 404 when there are only those): then
// the newest release. The fallback repository only when the main one cannot
// be reached, is gone, or has no release.
constexpr const char *Repos[] = {"HDErick/OpenBLOPS-NX", "HDErick/rqm-updater-nx"};
constexpr char ZipName[] = "OpenBLOPS-NX.zip";
constexpr char ZipPart[] = NX_GAME_DIR "/OpenBLOPS-NX.zip.part";
constexpr char SdRoot[] = "sdmc:/";
constexpr char PendingList[] = NX_GAME_DIR "/update_pending.txt";
constexpr size_t MaxJson = 512 * 1024;
constexpr int TimeoutMs = 15000;
constexpr int MaxRedirects = 5;
constexpr size_t MaxHeaders = 16384;

// ----- HTTPS ---------------------------------------------------------------

struct HttpSink {
    virtual ~HttpSink() = default;
    virtual void length(uint64_t) {}
    virtual bool data(const char *bytes, size_t size) = 0;
};

void set_error(char *error, size_t size, const char *text)
{
    if (size) std::snprintf(error, size, "%s", text);
}

int connect_host(const char *host)
{
    addrinfo hints{};
    addrinfo *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "443", &hints, &res) != 0 || !res) return -1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd >= 0) {
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        int rc = connect(fd, res->ai_addr, res->ai_addrlen);
        if (rc != 0 && errno == EINPROGRESS) {
            pollfd p = {fd, POLLOUT, 0};
            int err = 0;
            socklen_t len = sizeof(err);
            rc = poll(&p, 1, TimeoutMs) == 1 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0 ? 0 : -1;
        }
        if (rc != 0) {
            close(fd);
            fd = -1;
        } else {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
            timeval tv = {TimeoutMs / 1000, (TimeoutMs % 1000) * 1000};
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }
    }
    freeaddrinfo(res);
    return fd;
}

bool split_url(const char *url, std::string &host, std::string &path)
{
    if (strncasecmp(url, "https://", 8) != 0) return false;
    const char *start = url + 8;
    const char *slash = std::strchr(start, '/');
    host.assign(start, slash ? (size_t)(slash - start) : std::strlen(start));
    path = slash ? slash : "/";
    return !host.empty();
}

struct TlsStream {
    SslContext ctx{};
    SslConnection conn{};
    int fd = -1;
    bool ctxOpen = false, connOpen = false;

    bool open(const char *host)
    {
        fd = connect_host(host);
        if (fd < 0) return false;
        if (R_FAILED(sslCreateContext(&ctx, SslVersion_Auto))) return false;
        ctxOpen = true;
        if (R_FAILED(sslContextCreateConnection(&ctx, &conn))) return false;
        connOpen = true;
        // The ssl service takes the socket; what comes back (-1 with ENOENT:
        // nothing) is closed before the connection.
        const int out = socketSslConnectionSetSocketDescriptor(&conn, fd);
        if (out < 0 && errno != ENOENT) return false;
        fd = out;
        return R_SUCCEEDED(sslConnectionSetHostName(&conn, host, (u32)std::strlen(host) + 1)) &&
               R_SUCCEEDED(sslConnectionSetIoMode(&conn, SslIoMode_Blocking)) &&
               R_SUCCEEDED(sslConnectionDoHandshake(&conn, nullptr, nullptr, nullptr, 0));
    }
    bool write(const char *data, size_t size)
    {
        u32 written = 0;
        return R_SUCCEEDED(sslConnectionWrite(&conn, data, (u32)size, &written)) && written == size;
    }
    int read(char *buffer, size_t size)
    {
        s32 pending = 0;
        if (fd >= 0 && (R_FAILED(sslConnectionPending(&conn, &pending)) || pending <= 0)) {
            pollfd p = {fd, POLLIN, 0};
            if (poll(&p, 1, TimeoutMs) != 1) return -1;
        }
        u32 got = 0;
        if (R_FAILED(sslConnectionRead(&conn, buffer, (u32)size, &got))) return -1;
        return (int)got;
    }
    ~TlsStream()
    {
        if (fd >= 0) close(fd);
        if (connOpen) sslConnectionClose(&conn);
        if (ctxOpen) sslContextClose(&ctx);
    }
};

std::string header_value(const std::string &headers, const char *name)
{
    size_t length = std::strlen(name);
    size_t pos = 0;
    while ((pos = headers.find("\r\n", pos)) != std::string::npos) {
        pos += 2;
        if (strncasecmp(headers.c_str() + pos, name, length) == 0 && headers[pos + length] == ':') {
            size_t start = pos + length + 1;
            while (start < headers.size() && headers[start] == ' ') ++start;
            size_t end = headers.find("\r\n", start);
            return headers.substr(start, end == std::string::npos ? std::string::npos : end - start);
        }
    }
    return "";
}

bool net_connected()
{
    NifmInternetConnectionType type;
    NifmInternetConnectionStatus status;
    u32 strength = 0;
    if (R_FAILED(nifmInitialize(NifmServiceType_User))) return false;
    const bool connected = R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &strength, &status)) &&
                           status == NifmInternetConnectionStatus_Connected;
    nifmExit();
    return connected;
}

// GET over HTTPS (HTTP/1.0: never chunked), following redirects (release
// downloads go to another host). The final HTTP status, or -1 and error.
int https_get(const char *url, HttpSink &sink, const std::atomic<bool> *cancel, char *error, size_t errorSize)
{
    if (R_FAILED(sslInitialize(3))) {
        set_error(error, errorSize, "The console's SSL service is unavailable.");
        return -1;
    }
    struct SslGuard { ~SslGuard() { sslExit(); } } sslGuard;
    std::string current = url;
    static char buffer[64 * 1024];
    for (int redirect = 0; redirect <= MaxRedirects; ++redirect) {
        std::string host, path;
        if (!split_url(current.c_str(), host, path)) {
            set_error(error, errorSize, "The server sent an address that is not HTTPS.");
            return -1;
        }
        TlsStream tls;
        if (!tls.open(host.c_str())) {
            std::snprintf(error, errorSize, "Could not connect securely to %s.", host.c_str());
            return -1;
        }
        std::string request = "GET " + path + " HTTP/1.0\r\nHost: " + host +
                              "\r\nUser-Agent: openblops-nx-updater\r\nAccept: */*\r\nConnection: close\r\n\r\n";
        if (!tls.write(request.data(), request.size())) {
            std::snprintf(error, errorSize, "Could not send the request to %s.", host.c_str());
            return -1;
        }
        std::string headers;
        size_t bodyStart = std::string::npos;
        while (bodyStart == std::string::npos) {
            int got = tls.read(buffer, sizeof(buffer));
            if (got <= 0 || headers.size() > MaxHeaders) {
                std::snprintf(error, errorSize, "%s did not answer.", host.c_str());
                return -1;
            }
            headers.append(buffer, got);
            bodyStart = headers.find("\r\n\r\n");
        }
        std::string body = headers.substr(bodyStart + 4);
        headers.resize(bodyStart + 2);
        int status = 0;
        if (std::sscanf(headers.c_str(), "HTTP/%*d.%*d %d", &status) != 1) {
            std::snprintf(error, errorSize, "%s sent an answer that is not HTTP.", host.c_str());
            return -1;
        }
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            std::string location = header_value(headers, "Location");
            if (location.empty()) {
                set_error(error, errorSize, "A redirect without a destination.");
                return -1;
            }
            if (location[0] == '/') location = "https://" + host + location;
            current = location;
            continue;
        }
        std::string lengthText = header_value(headers, "Content-Length");
        uint64_t length = lengthText.empty() ? 0 : std::strtoull(lengthText.c_str(), nullptr, 10);
        sink.length(status == 200 ? length : 0);
        uint64_t received = 0;
        if (!body.empty()) {
            if (status == 200 && !sink.data(body.data(), body.size())) {
                set_error(error, errorSize, "The transfer stopped.");
                return -1;
            }
            received += body.size();
        }
        while (length == 0 || received < length) {
            if (cancel && cancel->load()) {
                set_error(error, errorSize, "Cancelled.");
                return -1;
            }
            int got = tls.read(buffer, sizeof(buffer));
            if (got == 0) break;
            if (got < 0) {
                std::snprintf(error, errorSize, "The connection to %s was lost.", host.c_str());
                return -1;
            }
            if (status == 200 && !sink.data(buffer, (size_t)got)) {
                set_error(error, errorSize, "The transfer stopped.");
                return -1;
            }
            received += (uint64_t)got;
        }
        if (status == 200 && length != 0 && received < length) {
            std::snprintf(error, errorSize, "The download from %s ended early.", host.c_str());
            return -1;
        }
        return status;
    }
    set_error(error, errorSize, "Too many redirects.");
    return -1;
}

// ----- the release JSON ------------------------------------------------------

struct MemorySink : HttpSink {
    std::string body;
    bool data(const char *bytes, size_t size) override
    {
        if (body.size() + size > MaxJson) return false;
        body.append(bytes, size);
        return true;
    }
};

struct FileSink : HttpSink {
    FILE *file;
    std::atomic<uint64_t> &done, &total;
    FileSink(FILE *f, std::atomic<uint64_t> &d, std::atomic<uint64_t> &t) : file(f), done(d), total(t) {}
    void length(uint64_t size) override { total.store(size); }
    bool data(const char *bytes, size_t size) override
    {
        if (std::fwrite(bytes, 1, size, file) != size) return false;
        done.fetch_add(size);
        return true;
    }
};

void append_utf8(std::string &out, unsigned code)
{
    if (code < 0x80) out += (char)code;
    else if (code < 0x800) { out += (char)(0xC0 | (code >> 6)); out += (char)(0x80 | (code & 0x3F)); }
    else if (code < 0x10000) { out += (char)(0xE0 | (code >> 12)); out += (char)(0x80 | ((code >> 6) & 0x3F)); out += (char)(0x80 | (code & 0x3F)); }
    else { out += (char)(0xF0 | (code >> 18)); out += (char)(0x80 | ((code >> 12) & 0x3F)); out += (char)(0x80 | ((code >> 6) & 0x3F)); out += (char)(0x80 | (code & 0x3F)); }
}

bool parse_string(const std::string &json, size_t &pos, std::string &out)
{
    if (pos >= json.size() || json[pos] != '"') return false;
    out.clear();
    for (++pos; pos < json.size(); ++pos) {
        char c = json[pos];
        if (c == '"') { ++pos; return true; }
        if (c != '\\') { if (c != '\r') out += c; continue; }
        if (++pos >= json.size()) return false;
        switch (json[pos]) {
        case 'n': out += '\n'; break;
        case 't': out += ' '; break;
        case 'r': case 'b': case 'f': break;
        case 'u': {
            if (pos + 4 >= json.size()) return false;
            unsigned code = (unsigned)std::strtoul(json.substr(pos + 1, 4).c_str(), nullptr, 16);
            pos += 4;
            if (code >= 0xD800 && code < 0xDC00 && pos + 6 < json.size() && json[pos + 1] == '\\' && json[pos + 2] == 'u') {
                unsigned low = (unsigned)std::strtoul(json.substr(pos + 3, 4).c_str(), nullptr, 16);
                if (low >= 0xDC00 && low < 0xE000) {
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    pos += 6;
                }
            }
            append_utf8(out, code);
            break;
        }
        default: out += json[pos]; break;
        }
    }
    return false;
}

std::string string_value(const std::string &json, const char *key, size_t from, size_t *end = nullptr)
{
    std::string quoted = std::string("\"") + key + "\"";
    for (size_t pos = json.find(quoted, from); pos != std::string::npos; pos = json.find(quoted, pos + 1)) {
        size_t value = pos + quoted.size();
        while (value < json.size() && std::strchr(" \n\r\t", json[value])) ++value;
        if (value >= json.size() || json[value] != ':') continue;
        ++value;
        while (value < json.size() && std::strchr(" \n\r\t", json[value])) ++value;
        std::string out;
        if (parse_string(json, value, out)) {
            if (end) *end = value;
            return out;
        }
    }
    return "";
}

// "v0.1.0" and "0.1.0" are the same version.
std::string normal_version(const std::string &v)
{
    size_t start = 0;
    while (start < v.size() && (v[start] == 'v' || v[start] == 'V' || v[start] == ' ')) ++start;
    size_t end = v.size();
    while (end > start && v[end - 1] == ' ') --end;
    return v.substr(start, end - start);
}


void make_parents(const std::string &path)
{
    for (size_t slash = path.find('/', std::strlen(SdRoot)); slash != std::string::npos; slash = path.find('/', slash + 1))
        mkdir(path.substr(0, slash).c_str(), 0777);
}

std::string safe_entry_path(const std::string &raw)
{
    size_t skip = 0;
    while (skip < raw.size() && (raw[skip] == '/' || raw[skip] == '\\')) ++skip;
    std::string path = raw.substr(skip);
    for (char &c : path) if (c == '\\') c = '/';
    if (path.empty() || path.find(':') != std::string::npos) return "";
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find('/', start);
        if (path.compare(start, end == std::string::npos ? std::string::npos : end - start, "..") == 0) return "";
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return std::string(SdRoot) + path;
}

// ----- the zip ---------------------------------------------------------------

uint32_t le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t le16(const unsigned char *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

struct ZipEntry {
    std::string name;
    uint16_t method;
    uint32_t crc, compressed, size, local;
};

// The central directory of a zip (no ZIP64: the release is well under 4 GB).
bool zip_entries(FILE *zip, std::vector<ZipEntry> &entries, char *error, size_t errorSize)
{
    if (std::fseek(zip, 0, SEEK_END) != 0) return false;
    const long fileSize = std::ftell(zip);
    const long tail = fileSize < 65557 ? fileSize : 65557;
    std::vector<unsigned char> end((size_t)tail);
    if (std::fseek(zip, fileSize - tail, SEEK_SET) != 0 || std::fread(end.data(), 1, end.size(), zip) != end.size())
        return false;
    long eocd = -1;
    for (long i = tail - 22; i >= 0; --i)
        if (le32(&end[(size_t)i]) == 0x06054b50) { eocd = i; break; }
    if (eocd < 0) { set_error(error, errorSize, "The download is not a zip."); return false; }
    const unsigned count = le16(&end[(size_t)eocd + 10]);
    const uint32_t dirSize = le32(&end[(size_t)eocd + 12]), dirOffset = le32(&end[(size_t)eocd + 16]);
    if (dirOffset == 0xffffffffu || (long)dirOffset + (long)dirSize > fileSize) {
        set_error(error, errorSize, "The zip's directory is damaged.");
        return false;
    }
    std::vector<unsigned char> dir(dirSize);
    if (std::fseek(zip, (long)dirOffset, SEEK_SET) != 0 || std::fread(dir.data(), 1, dir.size(), zip) != dir.size())
        return false;
    size_t at = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (at + 46 > dir.size() || le32(&dir[at]) != 0x02014b50) {
            set_error(error, errorSize, "The zip's directory is damaged.");
            return false;
        }
        ZipEntry e;
        e.method = le16(&dir[at + 10]);
        e.crc = le32(&dir[at + 16]);
        e.compressed = le32(&dir[at + 20]);
        e.size = le32(&dir[at + 24]);
        const unsigned nameLength = le16(&dir[at + 28]), extra = le16(&dir[at + 30]), comment = le16(&dir[at + 32]);
        e.local = le32(&dir[at + 42]);
        if (at + 46 + nameLength > dir.size()) return false;
        e.name.assign((const char *)&dir[at + 46], nameLength);
        entries.push_back(e);
        at += 46 + nameLength + extra + comment;
    }
    return true;
}

// One entry to out, checking its CRC.
bool zip_extract(FILE *zip, const ZipEntry &e, FILE *out, std::atomic<uint64_t> &done)
{
    unsigned char local[30];
    if (std::fseek(zip, (long)e.local, SEEK_SET) != 0 || std::fread(local, 1, 30, zip) != 30 || le32(local) != 0x04034b50)
        return false;
    if (std::fseek(zip, (long)e.local + 30 + le16(local + 26) + le16(local + 28), SEEK_SET) != 0) return false;
    static unsigned char in[64 * 1024], outBuf[256 * 1024];
    uLong crc = crc32(0L, Z_NULL, 0);
    uint32_t left = e.compressed;
    if (e.method == 0) {
        while (left) {
            const size_t n = left < sizeof(in) ? left : sizeof(in);
            if (std::fread(in, 1, n, zip) != n || std::fwrite(in, 1, n, out) != n) return false;
            crc = crc32(crc, in, (uInt)n);
            done.fetch_add(n);
            left -= (uint32_t)n;
        }
        return crc == e.crc;
    }
    if (e.method != 8) return false;
    z_stream z{};
    if (inflateInit2(&z, -MAX_WBITS) != Z_OK) return false;
    int rc = Z_OK;
    while (rc != Z_STREAM_END) {
        if (!z.avail_in && left) {
            const size_t n = left < sizeof(in) ? left : sizeof(in);
            if (std::fread(in, 1, n, zip) != n) { rc = Z_ERRNO; break; }
            z.next_in = in;
            z.avail_in = (uInt)n;
            left -= (uint32_t)n;
        }
        // With the input used up, inflate may still hold output a full
        // buffer left behind: call it until the stream ends or nothing moves
        // (Z_BUF_ERROR: a truncated entry).
        z.next_out = outBuf;
        z.avail_out = sizeof(outBuf);
        rc = inflate(&z, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_STREAM_END) break;
        const size_t produced = sizeof(outBuf) - z.avail_out;
        if (produced) {
            if (std::fwrite(outBuf, 1, produced, out) != produced) { rc = Z_ERRNO; break; }
            crc = crc32(crc, outBuf, (uInt)produced);
            done.fetch_add(produced);
        }
    }
    inflateEnd(&z);
    return rc == Z_STREAM_END && crc == e.crc;
}

// ----- the updater -----------------------------------------------------------

struct Updater {
    Thread worker{};
    bool threadActive = false;
    std::atomic<int> phase{(int)NxUpdatePhase::Idle};
    std::atomic<bool> cancel{false};
    std::atomic<uint64_t> done{0}, total{0};
    bool install = false;
    char version[64]{}, title[256]{}, notes[2048]{}, error[256]{}, zipUrl[1024]{};

    NxUpdatePhase state() const { return (NxUpdatePhase)phase.load(); }
    bool busy() const
    {
        const NxUpdatePhase p = state();
        return p == NxUpdatePhase::Checking || p == NxUpdatePhase::Downloading || p == NxUpdatePhase::Extracting;
    }
    void finish(NxUpdatePhase p) { phase.store((int)p); }
    void join()
    {
        if (!threadActive) return;
        threadWaitForExit(&worker);
        threadClose(&worker);
        threadActive = false;
    }
    static void entry(void *self)
    {
        auto *u = static_cast<Updater *>(self);
        if (u->install) u->run_install();
        else u->run_check();
    }
    bool start(bool installJob, NxUpdatePhase first)
    {
        join();
        cancel.store(false);
        install = installJob;
        error[0] = '\0';
        phase.store((int)first);
        // Any of the three application cores, below the game's priority: a
        // thread made on the creator's core gets only that core, and Horizon
        // does not time-slice threads of equal priority.
        if (R_FAILED(threadCreate(&worker, entry, this, nullptr, 0x20000, 0x2D, -2))) return false;
        svcSetThreadCoreMask(worker.handle, -1, 0x7);
        if (R_FAILED(threadStart(&worker))) {
            threadClose(&worker);
            return false;
        }
        threadActive = true;
        return true;
    }

    void run_check()
    {
        if (!net_connected()) {
            std::snprintf(error, sizeof(error), "No internet connection.");
            return finish(NxUpdatePhase::CheckFailed);
        }
        std::string json, tag, report;
        size_t tagEnd = 0;
        for (const char *repo : Repos) {
            if (cancel.load()) break;
            const std::string api = std::string("https://api.github.com/repos/") + repo + "/releases";
            MemorySink latest;
            int status = https_get((api + "/latest").c_str(), latest, &cancel, error, sizeof(error));
            json = latest.body;
            if (status == 404) {
                MemorySink newest;
                status = https_get((api + "?per_page=1").c_str(), newest, &cancel, error, sizeof(error));
                json = newest.body;
            }
            // What each repository answered (a private one answers 404: GitHub
            // shows its releases only to signed-in members).
            std::string result;
            if (status == 200) {
                tag = string_value(json, "tag_name", 0, &tagEnd);
                if (!tag.empty()) break;
                result = "no release";
            } else if (status == 404) {
                result = "not found or private";
            } else if (status > 0) {
                result = "HTTP " + std::to_string(status);
            } else {
                result = error;
            }
            std::printf("[nx-update] %s: %s\n", repo, result.c_str());
            report += (report.empty() ? "" : "; ") + std::string(repo) + ": " + result;
        }
        if (tag.empty()) {
            std::snprintf(error, sizeof(error), "No release found (%s).", report.c_str());
            return finish(NxUpdatePhase::CheckFailed);
        }
        std::snprintf(version, sizeof(version), "%s", tag.c_str());
        std::snprintf(title, sizeof(title), "%s", string_value(json, "name", tagEnd).c_str());
        std::snprintf(notes, sizeof(notes), "%s", string_value(json, "body", 0).c_str());
        zipUrl[0] = '\0';
        const std::string suffix = std::string("/") + ZipName;
        size_t from = 0, end = 0;
        for (std::string url; !(url = string_value(json, "browser_download_url", from, &end)).empty(); from = end)
            if (url.size() > suffix.size() && url.compare(url.size() - suffix.size(), suffix.size(), suffix) == 0) {
                std::snprintf(zipUrl, sizeof(zipUrl), "%s", url.c_str());
                break;
            }
        // As in the BO2 port: the newest release is the current version, so a
        // tag that differs from this build's ("v0.1.0" and "0.1.0" match) is
        // offered, older or newer.
        if (normal_version(tag) == normal_version(NX_APP_VERSION)) return finish(NxUpdatePhase::UpToDate);
        if (!zipUrl[0]) {
            std::snprintf(error, sizeof(error), "Version %s is out, but its release has no %s.", version, ZipName);
            return finish(NxUpdatePhase::CheckFailed);
        }
        finish(NxUpdatePhase::Available);
    }

    void run_install()
    {
        make_parents(ZipPart);
        FILE *file = std::fopen(ZipPart, "wb");
        if (!file) {
            std::snprintf(error, sizeof(error), "Could not write %s (is the SD card full or locked?).", ZipPart);
            return finish(NxUpdatePhase::Failed);
        }
        done.store(0);
        total.store(0);
        FileSink sink(file, done, total);
        const int status = https_get(zipUrl, sink, &cancel, error, sizeof(error));
        const bool written = std::fclose(file) == 0;
        if (status != 200 || !written) {
            std::remove(ZipPart);
            if (cancel.load()) {   // back to the offer
                error[0] = '\0';
                return finish(NxUpdatePhase::Available);
            }
            if (status > 0) std::snprintf(error, sizeof(error), "The download answered HTTP %d.", status);
            else if (!written) std::snprintf(error, sizeof(error), "Could not finish writing the download (SD card full?).");
            return finish(NxUpdatePhase::Failed);
        }
        phase.store((int)NxUpdatePhase::Extracting);
        FILE *zip = std::fopen(ZipPart, "rb");
        std::vector<ZipEntry> entries;
        if (!zip || !zip_entries(zip, entries, error, sizeof(error))) {
            if (zip) std::fclose(zip);
            std::remove(ZipPart);
            if (!error[0]) std::snprintf(error, sizeof(error), "The downloaded %s is not a valid zip.", ZipName);
            return finish(NxUpdatePhase::Failed);
        }
        uint64_t bytes = 0;
        for (const auto &e : entries) bytes += e.size;
        done.store(0);
        total.store(bytes);
        bool ok = true;
        for (const auto &e : entries) {
            const std::string path = safe_entry_path(e.name);
            if (path.empty()) continue;   // outside the SD root: skipped
            make_parents(path);
            if (path.back() == '/') continue;   // a folder
            const std::string staged = path + ".new";
            FILE *out = std::fopen(staged.c_str(), "wb");
            if (!out) {
                std::snprintf(error, sizeof(error), "Could not write %s.", path.c_str());
                ok = false;
                break;
            }
            const bool extracted = zip_extract(zip, e, out, done);
            const bool closed = std::fclose(out) == 0;
            if (!extracted || !closed) {
                std::remove(staged.c_str());
                std::snprintf(error, sizeof(error), "Could not unpack %s (SD card full, or a damaged download).", path.c_str());
                ok = false;
                break;
            }
            std::remove(path.c_str());
            if (std::rename(staged.c_str(), path.c_str()) != 0) {
                // In use: the next start moves it into place.
                FILE *list = std::fopen(PendingList, "a");
                bool listed = list && std::fprintf(list, "%s\n", path.c_str()) > 0;
                if (list) listed = std::fclose(list) == 0 && listed;
                if (!listed) {
                    std::snprintf(error, sizeof(error), "Could not replace %s.", path.c_str());
                    ok = false;
                    break;
                }
            }
        }
        std::fclose(zip);
        std::remove(ZipPart);
        finish(ok ? NxUpdatePhase::Installed : NxUpdatePhase::Failed);
    }
};

Updater *s_updater;
Mutex s_lock;
NxUpdatePhase s_logged = NxUpdatePhase::Idle;

const char *phase_name(NxUpdatePhase p)
{
    static const char *const names[] = {"idle", "checking", "available", "up to date", "check failed",
                                        "downloading", "unpacking", "installed", "failed"};
    const unsigned i = (unsigned)p;
    return i < sizeof(names) / sizeof(names[0]) ? names[i] : "?";
}

Updater &updater()
{
    if (!s_updater) s_updater = new Updater();
    return *s_updater;
}
}

const char *NX_UpdateCurrentVersion() { return NX_APP_VERSION; }

void NX_UpdateApplyPending()
{
    FILE *list = std::fopen(PendingList, "r");
    if (!list) return;
    std::string left;
    char line[600];
    while (std::fgets(line, sizeof(line), list)) {
        line[std::strcspn(line, "\r\n")] = '\0';
        if (std::strncmp(line, SdRoot, std::strlen(SdRoot)) != 0) continue;
        const std::string staged = std::string(line) + ".new";
        struct stat st{};
        if (stat(staged.c_str(), &st) != 0) continue;
        std::remove(line);
        if (std::rename(staged.c_str(), line) != 0) left += std::string(line) + "\n";
        else std::printf("[nx-update] moved the pending %s into place\n", line);
    }
    std::fclose(list);
    std::remove(PendingList);
    if (left.empty()) return;
    if (FILE *rest = std::fopen(PendingList, "w")) {
        std::fwrite(left.data(), 1, left.size(), rest);
        std::fclose(rest);
    }
}

void NX_UpdateStartCheck()
{
    mutexLock(&s_lock);
    Updater &u = updater();
    if (!u.busy()) {
        std::printf("[nx-update] checking for an update (this build %s)\n", NX_APP_VERSION);
        if (!u.start(false, NxUpdatePhase::Checking)) {
            std::snprintf(u.error, sizeof(u.error), "Could not start the update check.");
            u.finish(NxUpdatePhase::CheckFailed);
        }
    }
    mutexUnlock(&s_lock);
}

void NX_UpdateStartInstall()
{
    mutexLock(&s_lock);
    Updater &u = updater();
    if (!u.busy() && (u.state() == NxUpdatePhase::Available || u.state() == NxUpdatePhase::Failed) && u.zipUrl[0]) {
        std::printf("[nx-update] installing %s\n", u.version);
        if (!u.start(true, NxUpdatePhase::Downloading)) {
            std::snprintf(u.error, sizeof(u.error), "Could not start the download.");
            u.finish(NxUpdatePhase::Failed);
        }
    }
    mutexUnlock(&s_lock);
}

void NX_UpdateCancel()
{
    mutexLock(&s_lock);
    if (s_updater) s_updater->cancel.store(true);
    mutexUnlock(&s_lock);
}

void NX_UpdateStop()
{
    mutexLock(&s_lock);
    if (s_updater) {
        s_updater->cancel.store(true);
        s_updater->join();
    }
    mutexUnlock(&s_lock);
}

void NX_UpdateGetInfo(NxUpdateInfo &info)
{
    std::memset(&info, 0, sizeof(info));
    std::snprintf(info.current, sizeof(info.current), "%s", NX_APP_VERSION);
    mutexLock(&s_lock);
    if (s_updater) {
        const Updater &u = *s_updater;
        info.phase = u.state();
        info.done = u.done.load();
        info.total = u.total.load();
        // The text is the worker's, written before it leaves the check.
        if (info.phase != NxUpdatePhase::Checking) {
            std::snprintf(info.version, sizeof(info.version), "%s", u.version);
            std::snprintf(info.title, sizeof(info.title), "%s", u.title);
            std::snprintf(info.notes, sizeof(info.notes), "%s", u.notes);
            std::snprintf(info.error, sizeof(info.error), "%s", u.error);
        }
        if (info.phase != s_logged) {
            s_logged = info.phase;
            std::printf("[nx-update] %s%s%s%s%s\n", phase_name(info.phase), info.version[0] ? ": " : "", info.version,
                        info.error[0] ? ": " : "", info.error);
        }
    }
    mutexUnlock(&s_lock);
}
