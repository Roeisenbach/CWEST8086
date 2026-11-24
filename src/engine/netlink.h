// =============================================================================
//  netlink -- the wire under a hosted tournament, shared by the app and cwnode.
//
//  A host listens on 127.0.0.1 only; `cloudflared` (a Cloudflare quick
//  tunnel, started by the app) gives it a public https://....trycloudflare.com
//  address and hands every visitor's connection to that local port as an
//  HTTP request. So the host speaks two ways on its port:
//
//    * a WebSocket, which is what comes through the tunnel -- the joiner dials
//      the https address and upgrades (WinHTTP does the TLS);
//    * the plain line protocol, for a joiner on this same machine.
//
//  Inside either, the conversation is the same text blocks as always (see
//  gui_net.inl), plus a first exchange that decides who gets in:
//
//    host -> joiner   cwest-host 1 / auth open|password|ask / salt / nonce / end
//    joiner -> host   hello 1 / name <n> [/ proof <hmac>] / end
//    host -> joiner   welcome 1 / end          in -- a seat follows
//                     waiting 1 / end          "ask": the host has to accept you
//                     denied 1 / why <w> / end refused, and the line closes
//
//  With a password, the key is PBKDF2-SHA256 of it and the host's salt; the
//  joiner proves it knows the password (HMAC over the host's nonce, so the
//  password itself never crosses), and from then on every block both ways is
//  sealed with AES-256-GCM -- the tunnel's operator can carry the traffic but
//  not read or change it. Without a password nothing is sealed.
// =============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace cw {

// ---- text helpers ---------------------------------------------------------
std::string netHex(const void* p, size_t n);
bool        netUnhex(const std::string& s, std::vector<uint8_t>& out);
std::string netBase64(const void* p, size_t n);
bool        netUnbase64(const std::string& s, std::vector<uint8_t>& out);
bool        netRandom(void* p, size_t n);
std::string netRandomHex(size_t bytes);

// Blocks of lines ending in a line that is exactly "end".
bool        netTakeBlock(std::string& in, std::string& block);
std::string netFirstLine(const std::string& block);
std::string netField(const std::string& block, const char* key);   // "key value" line, or ""

// ---- password protection --------------------------------------------------
struct NetKey { uint8_t k[32] = {}; bool set = false; };
bool        netDeriveKey(const std::string& password, const std::string& saltHex, NetKey& key);
std::string netProof(const NetKey& key, const std::string& nonceHex);
std::string netSeal(const NetKey& key, const std::string& block);         // -> "sealed 1 / data / end"
bool        netUnseal(const NetKey& key, const std::string& sealed, std::string& block);

// ---- WebSocket, the host's side -------------------------------------------
// The HTTP request at the front of `in`, once it has fully arrived: removed
// from `in`, answered in `response` -- 101 for a WebSocket upgrade, or a short
// page for a browser -- and true. `upgrade` says which; `clientIp` is the
// visitor's address when the tunnel passed it on (Cf-Connecting-Ip).
bool        wsServerHandshake(std::string& in, std::string& response, bool& upgrade, std::string& clientIp);
// Frames from a joiner: data goes to `out`, a ping's pong to `reply`.
// False when the joiner closed or sent something that is not a frame.
bool        wsServerDecode(std::string& in, std::string& out, std::string& reply);
// Data to a joiner, as frames of at most 64 KB.
std::string wsServerFrames(const std::string& data);
std::string wsServerPing();

// ---- the joiner's side ----------------------------------------------------
// Whether an address goes through the tunnel (https / wss / ws / http, or a
// *.trycloudflare.com name) rather than straight to host:port.
bool        netIsWebAddress(const std::string& address);

// A connection to a host, dialled and read on threads of its own so the
// caller only ever polls. Keeps redialling for `retryFor` seconds while the
// host is not there yet. close() never waits: a dial stuck in a slow lookup
// finishes in the background and lets go of its share by itself.
struct NetLinkCore;
class NetLink {
public:
    enum State { Idle, Connecting, Open, Failed };
    NetLink() = default;
    NetLink(const NetLink&) = delete;
    NetLink& operator=(const NetLink&) = delete;
    ~NetLink() { close(); }

    void open(const std::string& address, int defaultPort, double retryFor = 600.0);
    void close();

    State       state() const;
    std::string where() const;
    std::string why() const;            // the last failure (kept while retrying)
    int         attempts() const;
    void        send(const std::string& data);
    void        take(std::string& appendTo);

private:
    std::shared_ptr<NetLinkCore> core() const;
    mutable std::mutex m_mu;
    std::shared_ptr<NetLinkCore> m_core;
};

// The joiner's half of the first exchange. Feed it every block from the
// host; it answers the greeting (with the proof when there is a password),
// unseals what is sealed, and hands the rest on.
struct NetJoinAuth {
    std::string name, password;
    NetKey key;
    bool   greeted = false;
    bool   sealed = false;

    enum { Consumed = 0, Pass = 1, Refused = -1 };
    // Pass: `block` is a plain block for the caller (unsealed if it came
    // sealed). `reply` is what to send back (already wrapped). Refused: `why`.
    int         onBlock(std::string& block, std::string& reply, std::string& why);
    std::string wrap(const std::string& block) const { return sealed ? netSeal(key, block) : block; }
};

} // namespace cw
