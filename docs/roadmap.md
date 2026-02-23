# nfsclient Roadmap

## Goals

1. **RFC Compliance Testing** — Verify that an NFS server correctly implements the
   relevant RFC: error codes, consistency guarantees, attribute accuracy, and proper
   handling of edge cases. Covers both NFSv3 (RFC 1813) and NFSv4.0 (RFC 7530).

2. **Performance Benchmarking** — Measure NFS server performance: per-operation latency,
   read/write throughput, IOPS, and how each scales with request size and concurrency.
   Benchmarks run against both protocol versions so results can be compared directly.

---

## Current State

| Area | Status |
|------|--------|
| NFSv3 procedures | ✅ All 22 + MOUNT protocol (MNT, UMNT, EXPORT) |
| NFSv4 support | None |
| Authentication | ✅ AUTH_NONE and AUTH_SYS |
| RPC records | ✅ Multi-fragment reassembly |
| Connections | One blocking TCP connection per NFSClient |
| Error handling | ✅ `NfsError` with `nfsstat3` status code |
| RFC 1813 compliance | ✅ 36 tests (34 pass, 2 skip — infrastructure) |
| Timing / statistics | ✅ Reservoir with min/p50/p95/p99/max |
| Benchmark workloads | ✅ seqread, seqwrite, randread, randwrite, meta, mixed |

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

---

## Phase 4 — NFSv4.0 Client

*Adds NFSv4.0 (RFC 7530) support. The target server implements NFSv4.0 on the same
port 2049 as NFSv3 with no separate MOUNT step. All NFSv4 traffic is a single RPC
procedure — COMPOUND (proc 1, prog 100003, vers 4) — that carries a sequence of
sub-operations in one call.*

### Protocol differences from NFSv3

| Aspect | NFSv3 | NFSv4.0 |
|--------|-------|---------|
| Transport | MOUNT + NFS on port 2049 | NFS only on port 2049; no MOUNT |
| RPC structure | One procedure per operation | COMPOUND: N sub-ops per call |
| File handles | Obtained via MOUNT MNT3 | Obtained via PUTROOTFH + LOOKUP |
| Attributes | Fixed `fattr3` struct | Bitmap-selected `fattr4` in opaque |
| State | Stateless | Stateful: client ID, open stateids, leases |
| Locking | NLM side protocol | Built-in LOCK/LOCKT/LOCKU |
| Delegations | None | Read + write, server-initiated recall |

### 4.1 COMPOUND frame builder

The foundation of all NFSv4 work. A `CompoundBuilder` composes sub-operations
into a single COMPOUND call and decodes the matched reply sequence.

```cpp
auto reply = compound(client)
    .putrootfh()
    .lookup("dir")
    .getfh()
    .getattr({FATTR4_SIZE, FATTR4_MODE})
    .call();
```

Internally encodes:
- Header: `minorversion=0`, `tag` (arbitrary label), `argarray` length
- Each op: `op_code (uint32)` + op-specific XDR arguments
- Decodes reply array positionally; stops on first non-OK status per RFC 7530 §15.2.3

### 4.2 fattr4 attribute codec

NFSv4 attributes are bitmap-selected: the request carries a `bitmap4` (variable-length
array of uint32 bitmasks) and the reply packs only the requested attributes into a
single opaque, in bitmap order.

Two attribute classes:
- **Mandatory** (server must support): `type`, `change`, `size`, `fsid`, `fileid`,
  `mode`, `numlinks`, `owner`, `owner_group`, `space_used`, `time_access`,
  `time_metadata`, `time_modify`, `mounted_on_fileid`
- **Recommended** (server may support): `time_create`, `acl`, `hidden`, `system`, etc.

The codec needs:
- `Bitmap4` — encode/decode a set of attribute IDs
- `Fattr4Encoder` — given a set of attr values, encode into the opaque
- `Fattr4Decoder` — given a bitmap + opaque, decode each present attribute

### 4.3 Client ID and lease management

NFSv4 is stateful. Before any OPEN the client must establish a client ID:

```
COMPOUND([SETCLIENTID(verifier, client_string)]) → clientid4 + confirm_verifier
COMPOUND([SETCLIENTID_CONFIRM(clientid4, confirm_verifier)]) → OK
```

The server expires the client ID if no operation is received within the lease period
(90 s on the target server). The `Nfs4Client` class must:
- Store `clientid4` and renew it by sending any operation (or explicit RENEW)
  before the lease expires
- Regenerate a 64-bit verifier on each fresh mount (so the server detects restarts)
- Handle `NFS4ERR_STALE_CLIENTID` by re-running SETCLIENTID/CONFIRM

### 4.4 File handle operations and GETATTR

The core navigation operations, all usable inside a COMPOUND:

| Op | What it does |
|----|-------------|
| `PUTROOTFH` | Sets current FH to the export root (replaces MOUNT MNT3) |
| `PUTFH(fh)` | Sets current FH to a known FH |
| `GETFH` | Captures current FH into the reply (use after LOOKUP to get a stored FH) |
| `LOOKUP(name)` | Traverses one component; updates current FH |
| `LOOKUPP` | Traverses to parent directory |
| `SAVEFH` / `RESTOREFH` | Save and restore current FH within a COMPOUND |
| `GETATTR(bitmap)` | Returns selected `fattr4` attributes for current FH |
| `ACCESS(mask)` | Returns which access rights are permitted |

### 4.5 OPEN / CLOSE and stateid lifecycle

NFSv4 READ and WRITE require a `stateid4` obtained from OPEN:

```
COMPOUND([PUTFH dir_fh, OPEN(seqid, access, deny, owner, UNCHECKED name, attrs)])
  → current FH = file FH, stateid4, open_flags (RESULT_FLAGS_*)

COMPOUND([PUTFH file_fh, OPEN_CONFIRM(stateid, seqid)])   # if new stateid
  → confirmed stateid4

COMPOUND([PUTFH file_fh, READ(stateid, offset, count)])
COMPOUND([PUTFH file_h,  WRITE(stateid, offset, stable, data)])

COMPOUND([PUTFH file_fh, CLOSE(seqid, stateid)])
  → invalidated stateid4
```

The `Nfs4Client` manages the open state table so callers receive a simple
`Nfs4FileHandle` with the stateid embedded.

### 4.6 Remaining file system operations

Once OPEN/CLOSE and GETATTR work, the remaining ops follow the same COMPOUND pattern:

| Op | NFSv4 name | Notes |
|----|-----------|-------|
| Create file | `OPEN(..., CREATE, UNCHECKED/GUARDED/EXCLUSIVE4)` | Folded into OPEN |
| Create dir/symlink | `CREATE(type, name, attrs)` | Separate from OPEN |
| Remove | `REMOVE(name)` | On parent directory FH |
| Rename | `SAVEFH`, `PUTFH dst_dir`, `RENAME(old, new)` | Two-FH operation via SAVEFH |
| SETATTR | `SETATTR(stateid, bitmap, attrlist)` | Bitmap-based |
| READDIR | `READDIR(cookie, cookieverf, dircount, maxcount, attr_request)` | Returns `dirlist4` |
| READLINK | `READLINK` | On symlink FH |
| COMMIT | `COMMIT(offset, count)` | Returns `writeverf4` |

### 4.7 Byte-range locking

The target server implements LOCK, LOCKT, LOCKU, and RELEASE_LOCKOWNER with
cross-protocol conflict detection against NLM. The NFSv4 lock flow:

```
# First lock on a file requires a lock-owner stateid derived from the open stateid
COMPOUND([PUTFH fh, LOCK(WRITE_LT, reclaim=false, offset, length,
                         new_lock_owner{seqid, open_stateid, lock_owner})])
  → lock_stateid4

COMPOUND([PUTFH fh, LOCKU(WRITE_LT, seqid, lock_stateid, offset, length)])
  → updated lock_stateid4

COMPOUND([RELEASE_LOCKOWNER(lock_owner)])
```

Useful for verifying conflict detection, range splitting, and the shared/exclusive
semantics defined in RFC 7530 §16.10.

### 4.8 Delegations

The target server grants read and write delegations and recalls them via the
callback channel (CB_RECALL). Testing delegations requires a client that:

1. Advertises a callback address in SETCLIENTID (`cb_client4`)
2. Listens for incoming CB_COMPOUND calls on that address
3. Processes CB_RECALL and calls DELEGRETURN

This is the highest-complexity item. It involves an inbound TCP listener on the
client side and is best deferred until the rest of the NFSv4 client is stable.

### 4.9 `Nfs4Client` facade

```cpp
class Nfs4Client {
public:
    explicit Nfs4Client(const std::string& host, uint16_t port = 2049);

    // Establish client ID with the server (call once before any file operation).
    void setup();

    Nfs4Fh  root();
    Nfs4Fh  lookup(const Nfs4Fh& dir, const std::string& name);
    Fattr4  getattr(const Nfs4Fh& fh, const Bitmap4& attrs);

    Nfs4File open(const Nfs4Fh& dir, const std::string& name, OpenAccess access);
    std::vector<uint8_t> read(Nfs4File& file, uint64_t offset, uint32_t count);
    WriteResult4         write(Nfs4File& file, uint64_t offset, Stable4 stable,
                               const uint8_t* data, size_t size);
    void close(Nfs4File& file);

    void create_dir(const Nfs4Fh& parent, const std::string& name);
    void remove(const Nfs4Fh& parent, const std::string& name);
    void rename(const Nfs4Fh& src_dir, const std::string& src_name,
                const Nfs4Fh& dst_dir, const std::string& dst_name);

    std::vector<DirEntry4> readdir(const Nfs4Fh& dir);

    LockStateid lock(Nfs4File& file, LockType type, uint64_t offset, uint64_t length);
    void unlock(Nfs4File& file, const LockStateid& sid, uint64_t offset, uint64_t length);
};
```

---

## Phase 5 — NFSv4 Compliance Tests

*RFC 7530 compliance checks. Uses `nfsclient_compliance --version 4`.*

### 5.1 COMPOUND semantics — RFC 7530 §14.2

| Test | What is checked |
|------|----------------|
| Error stops compound | Op N fails → ops N+1…end not executed, their status = `NFS4ERR_OP_ILLEGAL` (actually not executed, reply has results only up to failed op) |
| Tag echoed | Reply `tag` equals request `tag` |
| Empty compound | Zero ops → `NFS4_OK` |

### 5.2 Client ID lifecycle — RFC 7530 §16.33–16.34

| Test | What is checked |
|------|----------------|
| SETCLIENTID + CONFIRM | Returns valid `clientid4`; subsequent ops accepted |
| Duplicate SETCLIENTID (same verifier) | Returns same `clientid4` |
| Fresh verifier | Returns new `clientid4`; old one invalidated |
| Stale clientid in RENEW | Returns `NFS4ERR_STALE_CLIENTID` |

### 5.3 Open/close stateid lifecycle — RFC 7530 §16.16–16.2

| Test | What is checked |
|------|----------------|
| OPEN read-only | Stateid valid for READ, rejected for WRITE |
| OPEN write-only | Stateid valid for WRITE, rejected for READ |
| OPEN read-write | Stateid valid for both |
| OPEN GUARDED duplicate | Returns `NFS4ERR_EXIST` |
| OPEN EXCLUSIVE idempotency | Same verifier → same stateid |
| READ with wrong stateid | Returns `NFS4ERR_BAD_STATEID` |
| READ after CLOSE | Returns `NFS4ERR_BAD_STATEID` |

### 5.4 fattr4 mandatory attributes — RFC 7530 §5.8

| Test | What is checked |
|------|----------------|
| `type` after CREATE | Regular file = `NF4REG`; directory = `NF4DIR` |
| `size` after WRITE | Matches bytes written |
| `change` advances | Increments after each mutation |
| `time_modify` after WRITE | Advances |
| `numlinks` after hard link / remove | Correct count |
| `owner` / `owner_group` | Non-empty strings returned |
| `supported_attrs` | Contains all mandatory IDs |

### 5.5 Stale file handle — RFC 7530 §4.2.4

| Test | What is checked |
|------|----------------|
| GETATTR on deleted file | Returns `NFS4ERR_STALE` |
| READ with stale stateid | Returns `NFS4ERR_STALE` or `NFS4ERR_BAD_STATEID` |
| LOOKUP in deleted dir | Returns `NFS4ERR_STALE` |

### 5.6 Byte-range locking — RFC 7530 §16.10–16.12

| Test | What is checked |
|------|----------------|
| Exclusive lock, same owner retry | Succeeds (upgrade) |
| Exclusive lock conflict | Returns `NFS4ERR_LOCK_NOTSUPP` or `NFS4ERR_DENIED` |
| LOCKT on free range | Returns lock denied = false |
| LOCKT on locked range | Returns `NFS4ERR_DENIED` with conflicting range |
| LOCKU releases range | Subsequent LOCKT shows free |
| Write with active lock | Allowed for lock holder |

### 5.7 RENAME two-directory atomicity — RFC 7530 §16.24

| Test | What is checked |
|------|----------------|
| Source disappears | LOOKUP(src_name) → `NFS4ERR_NOENT` |
| Destination appears | LOOKUP(dst_name) → correct FH |
| `change` on both dirs | Both parent directories show updated `change` attr |

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
| 4.1 | COMPOUND frame builder | Both | P2 |
| 4.2 | fattr4 bitmap codec | Both | P2 |
| 4.3 | Client ID + lease management | Both | P2 |
| 4.4 | FH ops + GETATTR + ACCESS | Both | P2 |
| 4.5 | OPEN / CLOSE / READ / WRITE | Both | P2 |
| 4.6 | CREATE / REMOVE / RENAME / READDIR / SETATTR / COMMIT | Both | P3 |
| 4.7 | Byte-range locking (LOCK/LOCKT/LOCKU) | Compliance | P3 |
| 4.8 | Delegations + callback listener | Compliance | P4 |
| 4.9 | `Nfs4Client` facade | Both | P2 |
| 5.1 | COMPOUND semantics tests | Compliance | P3 |
| 5.2 | Client ID lifecycle tests | Compliance | P3 |
| 5.3 | Open/close stateid tests | Compliance | P3 |
| 5.4 | fattr4 mandatory attribute tests | Compliance | P3 |
| 5.5 | Stale FH tests (NFSv4) | Compliance | P3 |
| 5.6 | Byte-range locking tests | Compliance | P4 |
| 5.7 | RENAME atomicity tests | Compliance | P4 |
