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

## Benchmark Suite

Run all benchmark workloads against a live NFS server:

```sh
make bench-test
```

Or run a specific workload manually with custom parameters:

```sh
docker run --rm -v "$(pwd)":/src nfsclient-build \
    ./build/tools/bench/nfsclient_bench \
    --server nfsd --export / --workload seqread \
    --bs 65536 --size 1G --threads 4 --duration 30
```

Supported workloads:

| Workload | Description |
|----------|-------------|
| `seqread` | Sequential read throughput — pre-fills a file, reads it end-to-end |
| `seqwrite` | Sequential write throughput — each thread writes its own file |
| `randread` | Random read IOPS — uniform random block-aligned offsets |
| `randwrite` | Random write IOPS — each thread writes random offsets into its file |
| `meta` | Metadata IOPS — repeated CREATE+REMOVE pairs |
| `mixed` | Mixed read/write — configurable ratio via `--rw-ratio` |

Options:

```
--bs <bytes>       Block size (default 65536; supports K/M/G suffixes)
--size <bytes>     Data file size (default 1G)
--threads <n>      Concurrent connections/threads (default 1)
--duration <s>     Run time in seconds (default 30)
--stable <mode>    Write stability: unstable, datasync, filesync (default unstable)
--rw-ratio <0-1>   Read fraction for 'mixed' workload (default 0.7)
--csv <path>       Append results to a CSV file
```

Example output:

```
Workload : seqread
bs       : 64.0 KiB
size     : 256.0 MiB
threads  : 1
duration : 10.0 s

Ops          Throughput     lat_min    lat_p50    lat_p95    lat_p99    lat_max
───────────  ─────────────  ─────────  ─────────  ─────────  ─────────  ─────────
3981         824.6 MB/s     0.34 ms    0.62 ms    1.21 ms    1.89 ms    4.32 ms
```

Latency vs. concurrency sweep — run the same workload at increasing thread counts and
collect results into a single CSV for plotting:

```sh
for t in 1 2 4 8 16; do
  docker run --rm -v "$(pwd)":/src nfsclient-build \
    ./build/tools/bench/nfsclient_bench \
    --server nfsd --export / --workload randread \
    --bs 4096 --size 256M --threads $t --duration 30 --csv curve.csv
done
```

## NFSv4.0 Client

`Nfs4Client` provides the same style of facade as `NFSClient` but speaks NFSv4.0
(RFC 7530). It uses the COMPOUND procedure and manages the stateful OPEN/CLOSE lifecycle.

### Quick-start

```cpp
#include "nfs4_client.hpp"

// AUTH_NONE (for servers that allow unauthenticated access)
Nfs4Client client("nfsserver.example.com");

// AUTH_SYS — required by Linux kernel nfsd and most production servers
AuthSys auth{};
auth.uid = 1000; auth.gid = 1000; auth.machinename = "myclient";
Nfs4Client client("nfsserver.example.com", auth);

// Root file handle is obtained automatically in the constructor.
Nfs4Fh root = client.root_fh();

// Lookup + read
Nfs4Fh fh   = client.lookup(root, "hello.txt");
Nfs4File f  = client.open_read(root, "hello.txt");
auto data   = client.read(f, 0, 4096);
client.close(f);

// Write
Nfs4File wf = client.open_write(root, "out.txt");  // creates if absent
client.write(wf, 0, Stable4::FILE_SYNC,
             data.data(), static_cast<uint32_t>(data.size()));
client.close(wf);

// Directory ops
Nfs4Fh dir = client.mkdir(root, "newdir");
client.rename(root, "out.txt", dir, "moved.txt");
client.remove(dir, "moved.txt");

// List a directory
for (const auto& e : client.readdir(root)) {
    // e.name, e.cookie, e.attrs.size, e.attrs.type, ...
}
```

### Nfs4Client API

| Method | Description |
|--------|-------------|
| `root_fh()` | Root file handle (PUTROOTFH+GETFH, done in constructor) |
| `lookup(dir, name)` | Resolve a name to a file handle |
| `getattr(fh)` | Get file attributes (returns `Fattr4`) |
| `access(fh, mask)` | Check access permissions |
| `open_read(dir, name)` | Open existing file for reading |
| `open_write(dir, name, create)` | Open or create file for writing |
| `close(f)` | Close an open file |
| `read(f, offset, count)` | Read file data |
| `write(f, offset, stable, data, len)` | Write file data |
| `commit(f, offset, count)` | Flush unstable writes |
| `mkdir(dir, name)` | Create a directory |
| `remove(dir, name)` | Delete a file or empty directory |
| `rename(src_dir, src, dst_dir, dst)` | Rename / move |
| `symlink(dir, name, target)` | Create a symbolic link |
| `readlink(fh)` | Read symlink target |
| `setattr(fh, attrs)` | Set file attributes |
| `readdir(dir)` | List all directory entries (auto-paginated) |
| `renew()` | Renew the client lease |

### RFC 7530 Compliance Suite

Run the NFSv4.0 compliance suite against a Linux kernel NFS server:

```sh
make compliance4-test
```

This spins up an `erichough/nfs-server` Linux kernel NFSv4 server and the compliance
binary in Docker Compose, runs `nfsclient_compliance4`, then tears everything down.

Run manually with a filter:

```sh
docker run --rm -v "$(pwd)":/src nfsclient-build \
    ./build/tools/compliance4/nfsclient_compliance4 \
    --server nfsd --export / --filter Attr4
```

### Architecture (`src/nfs4/`)

Each NFSv4 operation exposes a pair of pure functions:

- `encode_<op>(XdrEncoder& enc, ...)` — appends into a shared COMPOUND encoder
- `decode_<op>_result(XdrDecoder& dec)` — reads the per-op result from the reply

The COMPOUND wire format (RFC 7530 §14.2):
```
[tag:string] [minorversion:u32=0] [numops:u32] [op₁ op₂ … opₙ]
```
Note: **tag comes before minorversion**.

## Roadmap

- **Phase 1** ✅ Complete NFSv3 coverage (all 22 procedures + MOUNT protocol)
- **Phase 2** ✅ RFC 1813 compliance test suite
- **Phase 3** ✅ Performance benchmark suite
- **Phase 4** ✅ NFSv4.0 client
- **Phase 5** 🔄 RFC 7530 compliance tests

See [`docs/roadmap.md`](docs/roadmap.md) for details.
