#pragma once

#include "rpc_types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Sends ONC RPC CALL messages over a TCP connection using RFC 5531 record marking.
// Each call() encodes a complete CALL frame, sends it, reads the REPLY, and
// returns the raw XDR bytes of the procedure result body.
class TcpRpcClient {
public:
    TcpRpcClient(const std::string& host, uint16_t port);
    ~TcpRpcClient();

    TcpRpcClient(const TcpRpcClient&) = delete;
    TcpRpcClient& operator=(const TcpRpcClient&) = delete;

    std::vector<uint8_t> call(uint32_t prog, uint32_t vers, uint32_t proc,
                              const std::vector<uint8_t>& args);

    // Switch to AUTH_SYS credentials for all subsequent calls.
    void set_auth_sys(const AuthSys& auth);

    // Revert to AUTH_NONE (the default).
    void clear_auth();

    // Pure functions exposed for unit testing.
    // auth == nullptr → AUTH_NONE; auth != nullptr → AUTH_SYS.
    static std::vector<uint8_t> buildCallMessage(uint32_t xid,
                                                  uint32_t prog, uint32_t vers,
                                                  uint32_t proc,
                                                  const std::vector<uint8_t>& args,
                                                  const AuthSys* auth = nullptr);
    static std::vector<uint8_t> addRecordMark(const std::vector<uint8_t>& payload);
    static std::vector<uint8_t> parseReply(const std::vector<uint8_t>& record);

private:
    void sendAll(const std::vector<uint8_t>& data);
    std::vector<uint8_t> recvRecord();

    int                       sock_;
    uint32_t                  xid_;
    std::unique_ptr<AuthSys>  auth_sys_;  // null = AUTH_NONE
};
