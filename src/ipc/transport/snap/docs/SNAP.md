# SNAP — High-Performance Messaging Library for C++20
**Version 2.0.0**

## OVERVIEW
Snap is a zero-dependency, header-only C++20 messaging library engineered for sub-100ns intra-process and sub-1µs inter-process communication. It is designed to be faster than ZeroMQ and NanoMSG for point-to-point and fan-out workloads, with hardware as the only bottleneck.

### To use:
```cpp
#include "snap.hpp"
// link with: -lpthread -lrt -lnuma (if available)
```

## TRANSPORTS
Snap uses a URI-based `connect()` factory to select the transport at compile time.

| URI | Examples | Best For |
| :--- | :--- | :--- |
| `inproc://` | `inproc://ticker` | Thread-to-thread in same process |
| `shm://` | `shm://sensor_data` | Process-to-process, same host |
| `udp://` | `udp://127.0.0.1:5555` | Low-latency network, lossy OK |
| | `udp://@127.0.0.1:5555` | (@ = listener/receiver side) |
| `tcp://` | `tcp://127.0.0.1:6000` | Reliable network, ordered |
| | `tcp://@127.0.0.1:6000` | (@ = server/listener side) |
| `ipc://` | `ipc:///tmp/snap.sock` | Local IPC, variable-size msgs |
| | `ipc://@/tmp/snap.sock` | (@ = listener side) |
| `mcast://` | `publish_multicast()` | One-to-many broadcast |
| | `subscribe_multicast()` | |

## DESIGN PRINCIPLES

### 1. LMAX Disruptor SPSC Ring Buffer
- Power-of-2 capacity → bitmask instead of modulo (no division)
- Producer head and consumer tail on separate cache lines (zero false sharing)
- Shadow copies of the remote index cached locally per side
- Minimal memory ordering: acquire/release only where necessary

### 2. Cache-Line Aligned Everything
- All atomic hot paths aligned to 64-byte cache lines
- `SNAP_CACHE_LINE = 64` (all major x86/ARM platforms)

### 3. Zero Dynamic Allocation on Hot Paths
- `InprocLink` stores `RingBuffer` inline (no heap)
- `Dispatcher` uses template `Handler`, not `std::function`
- `MemoryPool` pre-allocates slabs at startup

### 4. Kernel Bypass Optimizations
- **SO_BUSY_POLL**: keeps socket receive in poll mode (avoids sleep/wake cycles)
- **MSG_ZEROCOPY**: eliminates user→kernel copy on send (requires `SNAP_ENABLE_ZEROCOPY`)
- **recvmmsg**: batch receives up to 64 UDP datagrams per syscall
- **SO_PRIORITY + IP_TOS (DSCP EF 0xB8)**: marks packets for low-latency queuing
- **Hugepage-backed SHM and MemoryPool** (requires `SNAP_ENABLE_HUGEPAGES`)

### 5. Compile-Time Optimizations
- `SNAP_FORCE_INLINE` (`__attribute__((always_inline))`)
- `SNAP_LIKELY` / `SNAP_UNLIKELY` (`__builtin_expect`)
- `SNAP_PREFETCH_R / SNAP_PREFETCH_W` (`__builtin_prefetch`)
- `-O3 -march=native -fno-plt -flto=auto` in Release mode

## BUILDING
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Options:
- `-DSNAP_ENABLE_ZEROCOPY=ON`: Enable MSG_ZEROCOPY (Linux >= 4.14)
- `-DSNAP_ENABLE_HUGEPAGES=ON`: Enable 2MB hugepage SHM/Pool
- `-DSNAP_BUILD_EXAMPLES=OFF`: Skip examples
- `-DSNAP_BUILD_TESTS=OFF`: Skip tests

## QUICK START
```cpp
auto link = snap::connect<MyMessage>("inproc://my_channel");
link->send({...});

MyMessage m;
if (link->recv(m)) { /* handle m */ }
```

See `examples/` for full, runnable code for every transport.
