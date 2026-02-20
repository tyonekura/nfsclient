#include "mount.hpp"
#include "nfs_error.hpp"
#include "portmap.hpp"
#include "../rpc/rpc_client.hpp"
#include "../xdr/xdr.hpp"

#include <stdexcept>

namespace nfs3 {

static constexpr uint32_t MOUNT_PROG        = 100005;
static constexpr uint32_t MOUNT_VERS        = 3;
static constexpr uint32_t MOUNTPROC3_MNT    = 1;
static constexpr uint32_t MOUNTPROC3_UMNT   = 3;
static constexpr uint32_t MOUNTPROC3_EXPORT = 5;

// ── MNT ──────────────────────────────────────────────────────────────────────

Fh3 mnt(const std::string& host, const std::string& export_path) {
    const uint16_t port = getport(host, MOUNT_PROG, MOUNT_VERS);
    TcpRpcClient client(host, port);

    XdrEncoder args;
    args.put_string(export_path);

    const auto reply = client.call(MOUNT_PROG, MOUNT_VERS, MOUNTPROC3_MNT, args.bytes());

    XdrDecoder dec(reply);
    const uint32_t status = dec.get_uint32();
    if (status != 0)
        throw NfsError(status, "MOUNT MNT3 mountstat3");

    // fhandle3: variable-length opaque (RFC 1813 Appendix I)
    return Fh3{dec.get_opaque()};
    // auth_flavors array follows but we don't need it
}

// ── UMNT ─────────────────────────────────────────────────────────────────────

void umnt(const std::string& host, const std::string& export_path) {
    const uint16_t port = getport(host, MOUNT_PROG, MOUNT_VERS);
    TcpRpcClient client(host, port);

    XdrEncoder args;
    args.put_string(export_path);

    // UMNT3 returns void — the reply body is empty.
    client.call(MOUNT_PROG, MOUNT_VERS, MOUNTPROC3_UMNT, args.bytes());
}

// ── EXPORT ───────────────────────────────────────────────────────────────────

std::vector<ExportEntry> export_list(const std::string& host) {
    const uint16_t port = getport(host, MOUNT_PROG, MOUNT_VERS);
    TcpRpcClient client(host, port);

    // EXPORT3 takes no arguments.
    const auto reply = client.call(MOUNT_PROG, MOUNT_VERS, MOUNTPROC3_EXPORT, {});

    // exports: XDR linked list of exportnode
    // exportnode: ex_dir (string) + ex_groups (XDR linked list of groupnode)
    //             + value_follows bool for next entry
    std::vector<ExportEntry> entries;
    XdrDecoder dec(reply);

    while (dec.get_uint32() != 0) {  // value_follows for export list
        ExportEntry entry;
        entry.path = dec.get_string();

        // groups: linked list of { name (string), value_follows }
        while (dec.get_uint32() != 0) {
            entry.groups.push_back(dec.get_string());
        }

        entries.push_back(std::move(entry));
    }

    return entries;
}

}  // namespace nfs3
