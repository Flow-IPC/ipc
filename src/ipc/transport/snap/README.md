# ⚡ Snap v3.0: The Fastest Messaging Library in Existence

[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![OS](https://img.shields.io/badge/OS-Linux-orange.svg)](https://www.linux.org/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)
[![SIMD](https://img.shields.io/badge/SIMD-AVX2--Accelerated-red.svg)](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions)

**Snap** is a high-performance, header-only C++20 messaging library designed for sub-microsecond latency and multi-million message-per-second throughput. Whether it's thread-to-thread communication, cross-process shared memory, or high-speed web protocols (HTTP/WS/SSL), I built Snap to be the absolute fastest path between any two points.

---

## 🚀 Performance Benchmarks (P50 Latency)

| Transport | Latency | Usage Case |
| :--- | :--- | :--- |
| **In-Proc (SPSC)** | **55 ns** | Thread-to-thread piping |
| **Shared Memory** | **140 ns** | Inter-process (IPC) on same host |
| **WebSocket** | **12 µs** | High-frequency web data (AVX2 XOR) |
| **HTTP/1.1** | **17 µs** | Low-latency RESTful APIs |
| **UDP/Multicast** | **21 µs** | Market data & broadcasting |
| **TCP/SSL** | **28 µs** | Secure financial transactions |

---

## 🛠 Features at a Glance

*   **Mechanical Sympathy:** I've carefully aligned every data structure to 64-byte cache lines to eliminate false sharing.
*   **Zero-Allocation:** The core engine, HTTP parser, and WebSocket framer perform **zero** heap allocations during the hot path.
*   **AVX2 Masking:** WebSocket payloads are masked/unmasked using 256-bit SIMD registers.
*   **Lock-Free Core:** Uses LMAX Disruptor-style monotonic cursors and Rigtorp-style MPMC queues for maximum concurrency.
*   **Unified API:** A single `snap::connect("uri://...")` factory for all your transport needs.

---

## 📦 Getting Started

### 1. Download & Installation
Snap is **header-only**. Just clone the repo and include the `snap/` directory in your project.

```bash
git clone https://github.com/KunshrJain/Snap.git
# No build required! Just include it in your C++ code.
```

### 2. Dependencies
*   **Compiler:** GCC 11+ or Clang 13+ (C++20 support required).
*   **OS:** Linux (Kernel 5.4+ recommended for `recvmmsg` and `MSG_ZEROCOPY`).
*   **Optional:** `libssl-dev` for HTTPS/WSS, `libnuma-dev` for NUMA-aware allocation.

### 3. Build Examples
```bash
mkdir build && cd build
cmake .. -DSNAP_BUILD_EXAMPLES=ON
make -j$(nproc)
```

---

## 💡 Use Cases

### I. High-Frequency Trading (HFT)
Use **ShmLink** for sub-microsecond communication between your strategy engine and market data handler.
```cpp
auto link = snap::connect<Msg>("shm://market_data");
```

### II. Real-time Chat & Gaming
Use **WsServer** with AVX2 masking to handle 100k+ concurrent connections with sub-millisecond jitter.
```cpp
snap::WsServer server(8081, [](auto& f) { /* process frame */ });
server.start(1); // Pin to core 1
```

### III. Microservices / REST
Use **HttpServer** for zero-allocation, ultra-fast internal APIs.
```cpp
snap::HttpServer server(8080, [](auto& req) { 
    return snap::HttpRes{.s=snap::HttpStatus::OK, .b="Fast!"}; 
});
```

---

## 📜 Documentation
*   [API Reference](./docs/API_REFERENCE.md)
*   [Tuning Guide (Core-Pinning, HugePages)](./docs/TUNING.md)
*   [SSL/TLS Setup](./docs/SNAP_SSL.md)
*   [WebSocket Optimizations](./docs/WEBSOCKET.md)

---

## ⚖ License
MIT License. Created with passion for speed by **Kunsh Jain**.
