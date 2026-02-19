#include "rpc/rpc_client.hpp"
#include "xdr/xdr.hpp"

#include <gtest/gtest.h>

// ── buildCallMessage ──────────────────────────────────────────────────────────

TEST(TcpRpcClient, BuildCallMessageLayout) {
    // 10 uint32s with no args = 40 bytes
    const auto msg = TcpRpcClient::buildCallMessage(0x12345678u, 100003u, 3u, 6u, {});
    ASSERT_EQ(msg.size(), 40u);

    XdrDecoder dec(msg);
    EXPECT_EQ(dec.get_uint32(), 0x12345678u);  // xid
    EXPECT_EQ(dec.get_uint32(), 0u);           // MsgType::CALL
    EXPECT_EQ(dec.get_uint32(), 2u);           // RPC_VERSION
    EXPECT_EQ(dec.get_uint32(), 100003u);      // prog
    EXPECT_EQ(dec.get_uint32(), 3u);           // vers
    EXPECT_EQ(dec.get_uint32(), 6u);           // proc
    EXPECT_EQ(dec.get_uint32(), 0u);           // cred flavor AUTH_NONE
    EXPECT_EQ(dec.get_uint32(), 0u);           // cred body len
    EXPECT_EQ(dec.get_uint32(), 0u);           // verf flavor AUTH_NONE
    EXPECT_EQ(dec.get_uint32(), 0u);           // verf body len
}

TEST(TcpRpcClient, BuildCallMessageWithArgs) {
    XdrEncoder args;
    args.put_uint32(0xAABBCCDDu);
    const auto msg = TcpRpcClient::buildCallMessage(1u, 100000u, 2u, 3u, args.bytes());
    // 40 header bytes + 4 arg bytes = 44
    EXPECT_EQ(msg.size(), 44u);
    XdrDecoder dec(msg);
    for (int i = 0; i < 10; ++i) dec.get_uint32();  // skip header
    EXPECT_EQ(dec.get_uint32(), 0xAABBCCDDu);
}

// ── addRecordMark ────────────────────────────────────────────────────────────

TEST(TcpRpcClient, AddRecordMarkSetsLastFragmentBit) {
    const std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
    const auto framed = TcpRpcClient::addRecordMark(payload);
    ASSERT_EQ(framed.size(), 7u);  // 4-byte mark + 3-byte payload

    // mark = 0x80000003: last-fragment=1, length=3
    EXPECT_EQ(framed[0], 0x80u);
    EXPECT_EQ(framed[1], 0x00u);
    EXPECT_EQ(framed[2], 0x00u);
    EXPECT_EQ(framed[3], 0x03u);

    EXPECT_EQ(framed[4], 0x01u);
    EXPECT_EQ(framed[5], 0x02u);
    EXPECT_EQ(framed[6], 0x03u);
}

TEST(TcpRpcClient, AddRecordMarkEmptyPayload) {
    const auto framed = TcpRpcClient::addRecordMark({});
    ASSERT_EQ(framed.size(), 4u);
    EXPECT_EQ(framed[0], 0x80u);  // last-fragment bit
    EXPECT_EQ(framed[1], 0x00u);
    EXPECT_EQ(framed[2], 0x00u);
    EXPECT_EQ(framed[3], 0x00u);  // length = 0
}

// ── parseReply ───────────────────────────────────────────────────────────────

// Build a valid MSG_ACCEPTED SUCCESS reply with a uint32 result.
static std::vector<uint8_t> makeAcceptedReply(uint32_t xid, uint32_t result) {
    XdrEncoder enc;
    enc.put_uint32(xid);
    enc.put_uint32(1u);   // MsgType::REPLY
    enc.put_uint32(0u);   // ReplyStat::MSG_ACCEPTED
    enc.put_uint32(0u);   // verf flavor AUTH_NONE
    enc.put_uint32(0u);   // verf body len = 0
    enc.put_uint32(0u);   // AcceptStat::SUCCESS
    enc.put_uint32(result);
    return enc.release();
}

TEST(TcpRpcClient, ParseReplyReturnsResultBody) {
    const auto record = makeAcceptedReply(0xABCDu, 0xCAFEBABEu);
    const auto result = TcpRpcClient::parseReply(record);
    ASSERT_EQ(result.size(), 4u);
    XdrDecoder dec(result);
    EXPECT_EQ(dec.get_uint32(), 0xCAFEBABEu);
}

TEST(TcpRpcClient, ParseReplyThrowsOnWrongMsgType) {
    XdrEncoder enc;
    enc.put_uint32(1u);  // xid
    enc.put_uint32(0u);  // MsgType::CALL (not REPLY)
    const auto record = enc.release();
    EXPECT_THROW(TcpRpcClient::parseReply(record), std::runtime_error);
}

TEST(TcpRpcClient, ParseReplyThrowsOnMsgDenied) {
    XdrEncoder enc;
    enc.put_uint32(1u);  // xid
    enc.put_uint32(1u);  // MsgType::REPLY
    enc.put_uint32(1u);  // ReplyStat::MSG_DENIED
    const auto record = enc.release();
    EXPECT_THROW(TcpRpcClient::parseReply(record), std::runtime_error);
}

TEST(TcpRpcClient, ParseReplyThrowsOnNonSuccess) {
    XdrEncoder enc;
    enc.put_uint32(1u);  // xid
    enc.put_uint32(1u);  // REPLY
    enc.put_uint32(0u);  // MSG_ACCEPTED
    enc.put_uint32(0u);  // verf flavor
    enc.put_uint32(0u);  // verf body len
    enc.put_uint32(1u);  // AcceptStat::PROG_UNAVAIL
    const auto record = enc.release();
    EXPECT_THROW(TcpRpcClient::parseReply(record), std::runtime_error);
}
