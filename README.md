# nfsclient

A userspace NFSv3 client library that sends NFS packets directly over TCP —
no kernel mount required. Designed for black-box testing and benchmarking of
NFS server implementations.

## What it does

- Speaks **NFSv3 over TCP** (ONC RPC with record marking, RFC 5531)
- Implements **all 22 NFSv3 NFS procedures** plus the full MOUNT protocol
- Handles **AUTH\_NONE** and **AUTH\_SYS** credentials
- Reassembles **multi-fragment RPC records**
- Provides a clean C++17 `NFSClient` facade, plus lower-level
  `encode_*_args` / `decode_*_reply` pure functions for unit testing without a server

## Requirements

- **Docker** (all builds and tests run inside a Linux container)
- macOS or Linux host

No other dependencies needed on the host machine.

## Build & Test

```sh
make build            # build the Docker image + compile with CMake
make test             # build then run all 99 unit tests (no server needed)
make shell            # open an interactive shell inside the build container
make clean            # remove build/
```

Run integration tests against a live NFS server (requires `../project`):

```sh
make integration-test
```

This spins up the NFS server and the test binary in Docker Compose, waits
for the server to be ready, runs `nfsclient_integration_tests`, then tears
everything down.

Run the RFC 1813 compliance suite against a live NFS server:

```sh
make compliance-test
```

Or run it manually with a filter:

```sh
docker run --rm -v "$(pwd)":/src nfsclient-build \
    ./build/tools/compliance/nfsclient_compliance \
    --server nfsd --export / --filter WCC
```

Run a single test suite by name:

```sh
docker run --rm -v "$(pwd)":/src nfsclient-build \
    ./build/tests/nfsclient_tests --gtest_filter=GetattrDecode.*
```

## Supported NFSv3 Operations

### MOUNT protocol (prog 100005 v3)

| Method | Description |
|--------|-------------|
| `mount(path)` | Obtain the root file handle for an export |
| `umnt(path)` | Notify the server of an unmount |
| `export_list()` | Retrieve the server's export list |

### File operations

| Method | NFS proc | Description |
|--------|----------|-------------|
| `getattr(fh)` | 1 | Read file attributes (type, mode, size, timestamps, …) |
| `setattr(fh, attrs)` | 2 | Set mode, uid/gid, size, timestamps |
| `lookup(dir, name)` | 3 | Resolve a name to a file handle |
| `access(fh, mask)` | 4 | Check access permissions; returns granted bitmask |
| `readlink(fh)` | 5 | Read symlink target path |
| `read(fh, offset, count)` | 6 | Read file data |
| `write(fh, offset, stable, data)` | 7 | Write file data |
| `create(dir, name, mode, attrs)` | 8 | Create a file (UNCHECKED / GUARDED / EXCLUSIVE) |
| `mkdir(dir, name, attrs)` | 9 | Create a directory |
| `symlink(dir, name, target)` | 10 | Create a symbolic link |
| `mknod_fifo / mknod_socket` | 11 | Create a named pipe or Unix socket |
| `mknod_chr / mknod_blk` | 11 | Create a character or block device |
| `remove(dir, name)` | 12 | Delete a file |
| `rmdir(dir, name)` | 13 | Remove an empty directory |
| `rename(from_dir, from_name, to_dir, to_name)` | 14 | Rename / move |
| `link(file, link_dir, link_name)` | 15 | Create a hard link |
| `readdir(dir)` | 16 | List directory entries (auto-paginated) |
| `readdirplus(dir)` | 17 | List entries with inline attributes and file handles |
| `fsstat(root)` | 18 | Filesystem capacity and usage |
| `fsinfo(root)` | 19 | Server capabilities and preferred I/O sizes |
| `pathconf(fh)` | 20 | POSIX pathconf values |
| `commit(fh)` | 21 | Flush unstable writes to stable storage |

## Quick-start Example

```cpp
#include "nfs_client.hpp"

NFSClient client("nfsserver.example.com");

// 1. Get a root file handle
Fh3 root = client.mount("/export");

// 2. Look up a file
Fh3 fh = client.lookup(root, "hello.txt");

// 3. Read its contents
std::vector<uint8_t> data = client.read(fh, 0, 4096);
std::string content(data.begin(), data.end());

// 4. Write to a file
std::string payload = "hello from nfsclient";
client.write(fh, 0, Stable3::FILE_SYNC,
             reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

// 5. Create a new file and write
Fh3 new_fh = client.create(root, "new.txt", nfs3::CreateMode3::GUARDED);
client.write(new_fh, 0, Stable3::UNSTABLE,
             reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
client.commit(new_fh);  // flush unstable write

// 6. List a directory
for (const auto& entry : client.readdir(root)) {
    // entry.name, entry.fileid, entry.cookie
}

// 7. Get file attributes
Fattr3 attrs = client.getattr(fh);
// attrs.size, attrs.mode, attrs.uid, attrs.mtime.seconds, ...
```

## AUTH_SYS

By default the client uses AUTH\_NONE. To authenticate as a specific user:

```cpp
AuthSys auth;
auth.stamp       = static_cast<uint32_t>(time(nullptr));
auth.machinename = "myclient";
auth.uid         = 1000;
auth.gid         = 1000;
auth.gids        = {100, 200};   // supplemental groups

client.set_auth_sys(auth);
// all subsequent calls use AUTH_SYS
client.clear_auth();             // revert to AUTH_NONE
```

## Error Handling

All operations throw `NfsError` (a subclass of `std::runtime_error`) on
failure. The `status` field carries the exact `nfsstat3` code:

```cpp
try {
    Fh3 fh = client.lookup(root, "missing.txt");
} catch (const NfsError& e) {
    if (e.is(Nfsstat3::NFS3ERR_NOENT)) {
        // file not found
    }
    // e.what() contains a human-readable message
    // e.status contains the raw nfsstat3 uint32
}
```

## Architecture

```
src/
  xdr/            XDR encode/decode primitives (no network, no deps)
  rpc/            ONC RPC over TCP with record marking (RFC 5531)
                  TcpRpcClient — AUTH_NONE and AUTH_SYS, multi-fragment reassembly
  nfs/            NFSv3 operations in namespace nfs3
                  One file per operation: encode_*_args + decode_*_reply + wrapper
  nfs_client.hpp  NFSClient facade — owns a persistent TCP connection to nfsd
```

```
tools/
  compliance/     RFC 1813 compliance suite (nfsclient_compliance binary)
                  runner.hpp/cpp — TestRunner, TestCtx, PASS/FAIL/SKIP logic
                  test_helpers.hpp — CHECK / EXPECT_NFS_ERR macros
                  test_*.cpp — one file per RFC section (2.1 – 2.8)
```

Each NFS operation exposes pure `encode_*` / `decode_*` functions that are
tested entirely with hand-crafted byte buffers, without a real server.

## Roadmap

- **Phase 1** ✅ Complete NFSv3 coverage (all 22 procedures + MOUNT protocol)
- **Phase 2** ✅ RFC 1813 compliance test suite
- **Phase 3** Performance benchmark suite
- **Phase 4** NFSv4.0 client
- **Phase 5** RFC 7530 compliance tests

See [`docs/roadmap.md`](docs/roadmap.md) for details.
