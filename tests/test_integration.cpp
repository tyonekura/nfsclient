// Integration tests for nfsclient against a live NFSv3 server.
//
// Requires:
//   - A running nfsd with rpcbind, exporting a directory at port 2049.
//   - NFS_SERVER env var set to the server hostname (default: "localhost").
//   - The export must contain:
//       hello.txt      — readable file, content starts with "Hello from NFS"
//       subdir/nested.txt — readable file, content contains "Nested"
//       writable.txt   — writable file (any initial content)
//
// Run via: make integration-test

#include "nfs_client.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

static const char* server_host() {
    const char* h = std::getenv("NFS_SERVER");
    return h ? h : "localhost";
}

class NfsIntegration : public ::testing::Test {
protected:
    void SetUp() override {
        client_   = std::make_unique<NFSClient>(server_host());
        root_fh_  = client_->mount("/");
    }

    std::unique_ptr<NFSClient> client_;
    Fh3 root_fh_;
};

TEST_F(NfsIntegration, MountReturnsFileHandle) {
    EXPECT_FALSE(root_fh_.data.empty());
}

TEST_F(NfsIntegration, LookupFile) {
    const Fh3 fh = client_->lookup(root_fh_, "hello.txt");
    EXPECT_FALSE(fh.data.empty());
}

TEST_F(NfsIntegration, ReadFile) {
    const Fh3 fh = client_->lookup(root_fh_, "hello.txt");
    const auto data = client_->read(fh, 0, 4096);
    const std::string content(data.begin(), data.end());
    EXPECT_NE(content.find("Hello from NFS"), std::string::npos);
}

TEST_F(NfsIntegration, ReadAtOffset) {
    // "Hello from NFS\n" — bytes 6 onward: "from NFS\n"
    const Fh3 fh    = client_->lookup(root_fh_, "hello.txt");
    const auto full = client_->read(fh, 0, 4096);
    const auto tail = client_->read(fh, 6, 4096);
    ASSERT_LT(tail.size(), full.size());
    EXPECT_EQ(tail, std::vector<uint8_t>(full.begin() + 6, full.end()));
}

TEST_F(NfsIntegration, LookupSubdirectory) {
    const Fh3 subdir_fh = client_->lookup(root_fh_, "subdir");
    EXPECT_FALSE(subdir_fh.data.empty());

    const Fh3 nested_fh = client_->lookup(subdir_fh, "nested.txt");
    EXPECT_FALSE(nested_fh.data.empty());

    const auto data = client_->read(nested_fh, 0, 4096);
    const std::string content(data.begin(), data.end());
    EXPECT_NE(content.find("Nested"), std::string::npos);
}

TEST_F(NfsIntegration, WriteAndReadBack) {
    const Fh3 fh = client_->lookup(root_fh_, "writable.txt");

    const std::string payload = "nfsclient integration test";
    const auto result = client_->write(
        fh, 0, Stable3::FILE_SYNC,
        reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    EXPECT_EQ(result.count, static_cast<uint32_t>(payload.size()));

    const auto data = client_->read(fh, 0, static_cast<uint32_t>(payload.size()));
    EXPECT_EQ(std::string(data.begin(), data.end()), payload);
}

TEST_F(NfsIntegration, LookupNonExistentReturnsError) {
    EXPECT_THROW(client_->lookup(root_fh_, "does_not_exist.txt"), std::runtime_error);
}
