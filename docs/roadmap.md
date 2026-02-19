# nfsclient Roadmap

## Goals

1. **RFC Compliance Testing** — Verify that an NFSv3 server correctly implements RFC 1813:
   correct error codes, write consistency data (WCC), COMMIT semantics, attribute accuracy,
   and proper handling of edge cases and malformed inputs.

2. **Performance Benchmarking** — Measure NFS server performance: per-operation latency,
   read/write throughput, IOPS, and how each scales with request size and concurrency.

---

## Current State

| Area | Status |
|------|--------|
| NFSv3 procedures | 3 of 22: LOOKUP, READ, WRITE |
| MOUNT protocol | MNT only |
| Authentication | AUTH_NONE only |
| RPC records | Single-fragment only |
| Connections | One blocking TCP connection per NFSClient |
| Error handling | `std::runtime_error` — no NFS status code exposed |
| Timing / statistics | None |

---

## Phase 1 — Complete NFSv3 Coverage

*Prerequisite for both goals. Items are ordered by dependency and usefulness.*

### 1.1 `NfsError` exception type  *(all other work depends on this)*
Replace the bare `std::runtime_error` with a typed exception that carries the
`nfsstat3` code. Compliance tests must be able to assert specific error codes
(e.g., `NFS3ERR_NOENT` vs `NFS3ERR_ACCES`), not just "some error".

```cpp
struct NfsError : std::runtime_error {
    uint32_t status;  // nfsstat3
};
```

### 1.2 GETATTR (proc 1)
Retrieve all file attributes (type, mode, size, timestamps, nlink, uid, gid).
Required by almost every compliance and benchmark scenario — size verification
after write, timestamp testing, permission checks.

### 1.3 CREATE (proc 8)
Create a regular file in three modes:
- `UNCHECKED` — overwrite if exists
- `GUARDED` — fail with `NFS3ERR_EXIST` if exists
- `EXCLUSIVE` — idempotent creation using a client-supplied verifier

`EXCLUSIVE` mode is a common source of server bugs and is explicitly tested in
the RFC compliance suite (Phase 2).

### 1.4 REMOVE / MKDIR / RMDIR (procs 12, 9, 13)
File and directory lifecycle operations needed to set up and tear down test
fixtures independently from the server's initial export state.

### 1.5 AUTH\_SYS
Add `AUTH_SYS` credential encoding (uid, gid, gids array) to `TcpRpcClient`.
Most production NFS servers reject AUTH_NONE after the mount step; without this,
access-control compliance tests cannot run.

### 1.6 SETATTR (proc 2)
Set mode, uid/gid, size (truncate), and timestamps. Required for:
- Permission-denied tests (set mode 0000, verify NFSPROC3_READ returns
  `NFS3ERR_ACCES`)
- Timestamp accuracy tests

### 1.7 READDIR (proc 16)
List directory entries with cookie-based pagination. Required to verify that
CREATE/REMOVE/RENAME have taken effect from the server's perspective, and to
test `cookieverf` consistency.

### 1.8 COMMIT (proc 21)
Flush unstable writes to stable storage and return a `writeverf3` verifier.
Required to test the write-consistency model: an `UNSTABLE` write followed by
COMMIT must return the same verifier as a `FILE_SYNC` write; a server restart
between them must return a different verifier.

### 1.9 RENAME (proc 14)
Rename a file or directory. Required for correctness tests (source disappears,
destination appears, old FH behaviour post-rename).

### 1.10 ACCESS (proc 4)
Check file/directory access permissions without actually performing the
operation. Useful for negative-testing permission enforcement.

### 1.11 FSSTAT / FSINFO / PATHCONF (procs 18, 19, 20)
Server capability and capacity queries. `FSINFO` returns `rtmax`, `wtmax`,
`dtpref` — the maximum block sizes that drive benchmark buffer sizing.

### 1.12 Multi-fragment RPC record reassembly
Large responses (e.g., a full-sized READDIRPLUS reply) can be split across
multiple TCP record-mark fragments. The current `recvRecord()` throws on a
non-last-fragment header. Fix before adding READDIRPLUS or any operation that
may produce replies larger than ~64 KB.

### 1.13 READDIRPLUS (proc 17)
Directory listing with inline attributes and file handles — avoids a GETATTR
roundtrip per entry. Required for the metadata benchmark workload and for
testing WCC attribute accuracy.

### 1.14 READLINK / SYMLINK / LINK (procs 5, 10, 15)
Symlink read/create and hard-link creation. Needed to test symlink-loop
detection, cross-directory links, and hard-link count accuracy in GETATTR.

### 1.15 MKNOD (proc 11)
Create special files (block/char device, FIFO, socket). Lower priority; mainly
useful to verify the server returns `NFS3ERR_NOTSUPP` or handles the call
correctly.

### 1.16 MOUNTPROC3\_UMNT / EXPORT (procs 3, 5)
Unmount notification and export list query. EXPORT is useful for discovering
what paths to target without hard-coding the export path in tests.

---

## Phase 2 — RFC Compliance Test Suite

*Implements the first goal: structured, repeatable RFC 1813 compliance checks.*

### Framework

A standalone binary (`nfsclient_compliance`) that:
- Accepts a server hostname and export path as arguments
- Runs named test cases and reports PASS / FAIL / SKIP with RFC section
  references
- Distinguishes "server bug" (wrong status code) from "test infrastructure
  failure" (network error)
- Can target specific test categories with a filter flag

```
./build/nfsclient_compliance --server nfsd --export / --filter WCC
```

### 2.1 Basic operations
| Test | What is checked |
|------|----------------|
| LOOKUP existing file | Returns correct FH; obj_attributes matches GETATTR |
| LOOKUP non-existent | Returns `NFS3ERR_NOENT` |
| LOOKUP on non-directory | Returns `NFS3ERR_NOTDIR` |
| READ exact size | Returns `count` bytes and correct `eof` flag |
| READ past EOF | Returns 0 bytes with `eof=true` |
| WRITE FILE\_SYNC | `count` in reply matches bytes sent |
| WRITE then GETATTR | `size` attribute reflects written data |
| CREATE GUARDED duplicate | Returns `NFS3ERR_EXIST` |
| CREATE EXCLUSIVE idempotency | Two calls with same verifier return same FH |
| REMOVE non-existent | Returns `NFS3ERR_NOENT` |
| RENAME across directories | Source gone, destination present |

### 2.2 Write consistency (WCC) — RFC 1813 §2.6
All mutating operations (WRITE, CREATE, SETATTR, REMOVE, RENAME, …) must
return `wcc_data` with valid `before` and `after` attributes.

| Test | What is checked |
|------|----------------|
| WRITE pre-op attrs | `before.size` matches GETATTR size before call |
| WRITE post-op attrs | `after.size` matches GETATTR size after call |
| CREATE post-op dir attrs | Parent directory `mtime` advanced |
| REMOVE post-op dir attrs | Parent directory `nlink` decremented |

### 2.3 COMMIT / write verifier — RFC 1813 §3.3.7 and §3.3.21
| Test | What is checked |
|------|----------------|
| UNSTABLE write + COMMIT | COMMIT returns same `writeverf3` as FSINFO |
| Verifier consistency | Multiple COMMITs return identical `writeverf3` |
| FILE\_SYNC vs DATA\_SYNC | Both return `committed` ≥ requested stability |

### 2.4 EXCLUSIVE CREATE verifier — RFC 1813 §3.3.8
| Test | What is checked |
|------|----------------|
| Same verifier, same FH | Idempotent re-creation returns the original FH |
| Different verifier | Returns `NFS3ERR_EXIST` |

### 2.5 Attribute accuracy — RFC 1813 §2.5
| Test | What is checked |
|------|----------------|
| `mtime` after WRITE | Advances by at least the write duration |
| `ctime` after SETATTR | Advances after any metadata change |
| `nlink` after LINK | Increments by 1 |
| `nlink` after REMOVE | Decrements by 1; drops to 0 on final remove |
| `size` after truncate | SETATTR `size=0` confirmed by GETATTR |

### 2.6 Permission / access control — RFC 1813 §3.3.4
| Test | What is checked |
|------|----------------|
| SETATTR mode 0000 + READ | Returns `NFS3ERR_ACCES` |
| ACCESS on unreadable file | Returns `ACCESS3_READ = 0` |
| Root bypass | uid=0 ignores read-only mode (if server enforces AUTH_SYS) |

### 2.7 Stale file handle — RFC 1813 §2.5
Obtain a FH, delete the underlying file/directory, then use the FH:

| Test | What is checked |
|------|----------------|
| READ on deleted file | Returns `NFS3ERR_STALE` |
| LOOKUP in deleted dir | Returns `NFS3ERR_STALE` |

### 2.8 Edge cases
| Test | What is checked |
|------|----------------|
| Zero-byte READ | Returns 0 bytes, no error |
| Zero-byte WRITE | Returns 0 bytes written, no error |
| Max filename length (255) | CREATE succeeds |
| Filename length 256 | Returns `NFS3ERR_NAMETOOLONG` |
| READ count > `rtmax` | Server clips to `rtmax` or returns full count |
| READDIR with expired `cookieverf` | Returns `NFS3ERR_BAD_COOKIE` |

---

## Phase 3 — Performance Benchmark Suite

*Implements the second goal: reproducible, configurable NFS benchmarks.*

### Framework

A standalone binary (`nfsclient_bench`) that:
- Creates and cleans up its own test files (no manual server setup)
- Reports per-operation latency (min / p50 / p95 / p99 / max) and
  aggregate throughput
- Accepts block size, file size, concurrency, and duration as parameters
- Outputs in human-readable and machine-readable (CSV) format

```
./build/nfsclient_bench --server nfsd --export / \
    --workload seqread --bs 65536 --size 1G --threads 4 --duration 30
```

### 3.1 Timing infrastructure
Wrap each `TcpRpcClient::call()` with `std::chrono::steady_clock` to record
start/end timestamps. Collect samples into a reservoir for percentile
computation.

### 3.2 Concurrent connection pool
Allow `NFSClient` (or a `BenchClient` wrapper) to open N independent TCP
connections to the server. Each connection is driven by a dedicated thread.
This is the minimal change needed to saturate a multi-threaded server.

### 3.3 Sequential read benchmark
Measure read throughput (MB/s) and per-call latency:
1. Pre-create a large file (e.g., 1 GB) using FILE_SYNC writes
2. Read it sequentially in configurable block sizes (4 KB – 1 MB)
3. Repeat for N seconds; report throughput and latency distribution

### 3.4 Sequential write benchmark
Measure write throughput and latency. Parameterise stability mode
(UNSTABLE / FILE_SYNC) to show the cost of synchronous writes.

### 3.5 Random read / write benchmark
Issue reads and writes at uniformly random offsets within a pre-created file.
Reports IOPS alongside throughput. Reveals caching and seek behaviour.

### 3.6 Metadata benchmark
Repeatedly CREATE and REMOVE small files in a directory. Measures metadata
IOPS — typically the bottleneck for workloads with many small files.
Reports GETATTR latency separately as a baseline.

### 3.7 Mixed read/write benchmark
Configurable read/write ratio (e.g., 70% read / 30% write) at random offsets.
Simulates database-like access patterns.

### 3.8 Latency vs. concurrency curve
Run a fixed workload (e.g., random 4 KB reads) while sweeping the number of
concurrent connections from 1 to N. Plot how latency and throughput change.
Identifies the server's saturation point.

---

## Summary Table

| # | Item | Goal | Priority |
|---|------|------|----------|
| 1.1 | `NfsError` with status code | Both | **P0** |
| 1.2 | GETATTR | Both | **P1** |
| 1.3 | CREATE (all modes) | Both | **P1** |
| 1.4 | REMOVE / MKDIR / RMDIR | Both | **P1** |
| 1.5 | AUTH\_SYS | Both | **P1** |
| 1.6 | SETATTR | Compliance | P2 |
| 1.7 | READDIR | Compliance | P2 |
| 1.8 | COMMIT | Compliance | P2 |
| 1.9 | RENAME | Compliance | P2 |
| 1.10 | ACCESS | Compliance | P2 |
| 1.11 | FSSTAT / FSINFO / PATHCONF | Bench | P2 |
| 1.12 | Multi-fragment records | Both | P2 |
| 1.13 | READDIRPLUS | Both | P3 |
| 1.14 | READLINK / SYMLINK / LINK | Compliance | P3 |
| 1.15 | MKNOD | Compliance | P3 |
| 1.16 | UMNT / EXPORT | Both | P3 |
| 2.1 | Compliance framework + basic ops | Compliance | P2 |
| 2.2 | WCC attribute tests | Compliance | P2 |
| 2.3 | COMMIT / write verifier tests | Compliance | P2 |
| 2.4 | EXCLUSIVE CREATE tests | Compliance | P3 |
| 2.5 | Attribute accuracy tests | Compliance | P3 |
| 2.6 | Permission / AUTH\_SYS tests | Compliance | P3 |
| 2.7 | Stale FH tests | Compliance | P3 |
| 2.8 | Edge case tests | Compliance | P3 |
| 3.1 | Timing infrastructure | Bench | P2 |
| 3.2 | Concurrent connection pool | Bench | P2 |
| 3.3 | Sequential read benchmark | Bench | P2 |
| 3.4 | Sequential write benchmark | Bench | P2 |
| 3.5 | Random read/write benchmark | Bench | P3 |
| 3.6 | Metadata benchmark | Bench | P3 |
| 3.7 | Mixed read/write benchmark | Bench | P3 |
| 3.8 | Latency vs. concurrency curve | Bench | P3 |
