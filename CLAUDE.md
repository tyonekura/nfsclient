# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

`nfsclient` sends NFSv3 packets directly over TCP to an NFS server without mounting a
filesystem. It is a userspace testing tool. Written in C++17, built with CMake, tested
with GTest — all inside a Linux Docker container.

**Initial scope:** MOUNT (root file handle), LOOKUP, READ, WRITE over TCP.

## Build & Test Commands

All build and test commands run inside a Docker container (Ubuntu 22.04).

```sh
make docker-image   # build the Docker image (run once, or after Dockerfile changes)
make build          # cmake configure + compile inside Docker
make test           # build then run all unit tests inside Docker
make shell          # open an interactive shell inside the build container
make clean          # remove the local build/ directory
```

Run a single test suite by name:
```sh
docker run --rm -v "$(pwd)":/src nfsclient-build \
    ./build/tests/nfsclient_tests --gtest_filter=XdrEncoder.*
```

Manual CMake workflow (inside `make shell`):
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/tests/nfsclient_tests
```

## Architecture

```
src/
  xdr/          XDR encode/decode (no deps, no network)
  rpc/          ONC RPC over TCP with record marking (RFC 5531)
  nfs/          NFSv3 operations in namespace nfs3
  nfs_client.hpp/cpp   High-level NFSClient facade
```

### Layer dependencies (bottom → top)

```
xdr/xdr.hpp  ←  rpc/rpc_client.hpp  ←  nfs/*.hpp  ←  nfs_client.hpp
              ↖  nfs/nfs3_types.hpp  ↗
```

### XDR layer (`src/xdr/`)
- `XdrEncoder`: appends values to an internal `std::vector<uint8_t>` buffer.
- `XdrDecoder`: reads from a `const uint8_t*` cursor; throws `std::runtime_error` on underflow.
- `put_opaque` / `get_opaque` handle variable-length opaque (4-byte length prefix + padding).
- `put_fixed_opaque` / `get_fixed_opaque` handle fixed-length opaque (padding only, no length prefix); used for `writeverf3`.

### RPC layer (`src/rpc/`)
- `TcpRpcClient`: opens a persistent TCP connection; `call()` sends a complete CALL frame and returns the procedure result bytes.
- Three pure static helpers (`buildCallMessage`, `addRecordMark`, `parseReply`) contain all serialization logic and are tested directly without a network.
- AUTH_NONE only; single-fragment records only.

### NFS layer (`src/nfs/`)
All functions are in `namespace nfs3`.

| File | Responsibility |
|------|---------------|
| `nfs3_types.hpp` | `Fh3`, `Stable3`, `WriteResult`; inline XDR helpers (`encode_fh3`, `skip_post_op_attr`, etc.) |
| `portmap` | RPCBIND GETPORT (prog 100000 v2, port 111) |
| `mount` | MOUNT MNT3 (prog 100005 v3); calls portmap internally |
| `lookup/read/write` | Each exposes `encode_*_args()`, `decode_*_reply()`, and a `lookup/read/write(TcpRpcClient&, ...)` wrapper |

The encode/decode split makes unit testing without a real server straightforward.

### NFSClient facade (`src/nfs_client.hpp`)
Owns a persistent `TcpRpcClient` to the NFS port (resolved once via portmap in the constructor). `mount()` creates short-lived connections to mountd internally.
