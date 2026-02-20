#include "rpc_client.hpp"
#include "rpc_types.hpp"
#include "../xdr/xdr.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

// ── Construction / Destruction ───────────────────────────────────────────────

TcpRpcClient::TcpRpcClient(const std::string& host, uint16_t port)
    : sock_(-1), xid_(1) {
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    const auto port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0)
        throw std::runtime_error("getaddrinfo failed for " + host);

    sock_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock_ < 0) {
        freeaddrinfo(res);
        throw std::runtime_error("socket() failed");
    }

    if (connect(sock_, res->ai_addr, res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        throw std::runtime_error("connect() to " + host + ":" + port_str + " failed");
    }

    freeaddrinfo(res);
}

TcpRpcClient::~TcpRpcClient() {
    if (sock_ >= 0) close(sock_);
}

// ── Auth management ──────────────────────────────────────────────────────────

void TcpRpcClient::set_auth_sys(const AuthSys& auth) {
    auth_sys_ = std::make_unique<AuthSys>(auth);
}

void TcpRpcClient::clear_auth() {
    auth_sys_.reset();
}

// ── Pure helpers (also used by unit tests) ───────────────────────────────────

std::vector<uint8_t> TcpRpcClient::buildCallMessage(uint32_t xid,
                                                      uint32_t prog,
                                                      uint32_t vers,
                                                      uint32_t proc,
                                                      const std::vector<uint8_t>& args,
                                                      const AuthSys* auth) {
    XdrEncoder enc;
    enc.put_uint32(xid);
    enc.put_uint32(static_cast<uint32_t>(MsgType::CALL));
    enc.put_uint32(RPC_VERSION);
    enc.put_uint32(prog);
    enc.put_uint32(vers);
    enc.put_uint32(proc);

    if (auth) {
        // AUTH_SYS credential body (RFC 5531 §8.1)
        XdrEncoder cred_body;
        cred_body.put_uint32(auth->stamp);
        cred_body.put_string(auth->machinename);
        cred_body.put_uint32(auth->uid);
        cred_body.put_uint32(auth->gid);
        cred_body.put_uint32(static_cast<uint32_t>(auth->gids.size()));
        for (uint32_t g : auth->gids) cred_body.put_uint32(g);

        enc.put_uint32(AUTH_SYS_FLAV);
        enc.put_opaque(cred_body.release());
    } else {
        // AUTH_NONE credential: flavor=0, body_len=0
        enc.put_uint32(AUTH_NONE);
        enc.put_uint32(0);
    }

    // Verifier is always AUTH_NONE
    enc.put_uint32(AUTH_NONE);
    enc.put_uint32(0);

    auto buf = enc.release();
    buf.insert(buf.end(), args.begin(), args.end());
    return buf;
}

std::vector<uint8_t> TcpRpcClient::addRecordMark(const std::vector<uint8_t>& payload) {
    const uint32_t len  = static_cast<uint32_t>(payload.size());
    const uint32_t mark = (1u << 31) | len;  // last-fragment bit always set

    std::vector<uint8_t> result;
    result.reserve(4 + payload.size());
    result.push_back(static_cast<uint8_t>((mark >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((mark >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((mark >>  8) & 0xFF));
    result.push_back(static_cast<uint8_t>( mark        & 0xFF));
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

std::vector<uint8_t> TcpRpcClient::parseReply(const std::vector<uint8_t>& record) {
    XdrDecoder dec(record);
    /* xid        */ dec.get_uint32();

    const auto msg_type = dec.get_uint32();
    if (msg_type != static_cast<uint32_t>(MsgType::REPLY))
        throw std::runtime_error("RPC: expected REPLY message type");

    const auto reply_stat = dec.get_uint32();
    if (reply_stat != static_cast<uint32_t>(ReplyStat::MSG_ACCEPTED))
        throw std::runtime_error("RPC: message denied (reply_stat=" +
                                 std::to_string(reply_stat) + ")");

    // verifier: auth_flavor + variable-length body
    /* verf_flavor */ dec.get_uint32();
    /* verf_body   */ dec.get_opaque();

    const auto accept_stat = dec.get_uint32();
    if (accept_stat != static_cast<uint32_t>(AcceptStat::SUCCESS))
        throw std::runtime_error("RPC: not accepted (accept_stat=" +
                                 std::to_string(accept_stat) + ")");

    return dec.get_remaining();
}

// ── Network I/O ──────────────────────────────────────────────────────────────

void TcpRpcClient::sendAll(const std::vector<uint8_t>& data) {
    size_t total = 0;
    while (total < data.size()) {
        const ssize_t n = send(sock_, data.data() + total, data.size() - total, 0);
        if (n <= 0)
            throw std::runtime_error("send() failed");
        total += static_cast<size_t>(n);
    }
}

std::vector<uint8_t> TcpRpcClient::recvRecord() {
    // RFC 5531 §11: a record may be split across multiple fragments.
    // Each fragment is prefixed by a 4-byte mark: bit 31 = last-fragment,
    // bits 30-0 = fragment length.  Reassemble until last-fragment is set.
    std::vector<uint8_t> record;

    for (;;) {
        // Read the 4-byte record mark.
        uint8_t mark_buf[4] = {};
        size_t received = 0;
        while (received < 4) {
            const ssize_t n = recv(sock_, mark_buf + received, 4 - received, 0);
            if (n <= 0)
                throw std::runtime_error("recv() record mark failed");
            received += static_cast<size_t>(n);
        }

        const uint32_t mark =
            (static_cast<uint32_t>(mark_buf[0]) << 24) |
            (static_cast<uint32_t>(mark_buf[1]) << 16) |
            (static_cast<uint32_t>(mark_buf[2]) <<  8) |
             static_cast<uint32_t>(mark_buf[3]);

        const bool     last_fragment = (mark & 0x80000000u) != 0;
        const uint32_t frag_len      = mark & 0x7FFFFFFFu;

        // Append this fragment's data to the reassembly buffer.
        const size_t offset = record.size();
        record.resize(offset + frag_len);
        received = 0;
        while (received < frag_len) {
            const ssize_t n = recv(sock_, record.data() + offset + received,
                                   frag_len - received, 0);
            if (n <= 0)
                throw std::runtime_error("recv() record data failed");
            received += static_cast<size_t>(n);
        }

        if (last_fragment) break;
    }

    return record;
}

// ── Public call ──────────────────────────────────────────────────────────────

std::vector<uint8_t> TcpRpcClient::call(uint32_t prog, uint32_t vers, uint32_t proc,
                                         const std::vector<uint8_t>& args) {
    const uint32_t my_xid = xid_++;
    const auto msg    = buildCallMessage(my_xid, prog, vers, proc, args,
                                         auth_sys_.get());
    const auto framed = addRecordMark(msg);
    sendAll(framed);
    const auto record = recvRecord();
    return parseReply(record);
}
