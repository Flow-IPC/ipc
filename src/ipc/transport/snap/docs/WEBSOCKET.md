# SNAP WEBSOCKET PROTOCOL GUIDE
**Version 3.0.0**

This guide describes how to use Snap's high-performance WebSocket implementation (WS and WSS).

---

## 1. WebSocket Protocol (RFC 6455)
Snap v3.0 provides a sub-microsecond WebSocket framing layer with Native AVX2 SIMD acceleration for masking/unmasking. 

| Feature | Support | Performance |
| :--- | :--- | :--- |
| **Opcode** | Text, Binary, Ping, Pong, Close, Cont | Full |
| **Masking** | Client-to-Server Masking (AVX2 XOR) | **~5ns per 1KB** |
| **Fragmentation** | `fin` bit and `opcode 0x0` Support | Full |
| **Handshake** | Zero-copy HTTP Upgrade Parser | Full |

## 2. Setting Up a Chat Server
Snap's `WsServer` is designed to be the fastest WebSocket reactor on Linux. 

```cpp
#include "snap/snap.hpp"

// Message handler (runs on pinned thread)
auto handler = [](const snap::WsFrame& frame) {
    if (frame.opcode == snap::WsOpcode::TEXT) {
        std::string_view msg(static_cast<const char*>(frame.payload), frame.payload_len);
        std::cout << "[WS] Received: " << msg << "\n";
    }
};

snap::WsServer server(8081, handler);
server.start(1); // Pin worker thread to core 1 for max throughput
```

## 3. High Performance Optimization
Snap's WebSocket stack is optimized to minimize latency in every step:

*   **AVX2 Masking:** Uses 256-bit SIMD registers to XOR-mask payloads. This is mandatory for client-to-server frames and usually the bottleneck in other libraries. In Snap, it takes near-zero time.
*   **Zero-Copy Handshake:** Snap uses a fast `std::string_view` based search for the `Sec-WebSocket-Key` to generate the `Accept` response in a single pass.
*   **Polled Worker:** The `WsServer` uses a busy-polling reactor during high-load periods to eliminate kernel scheduling latency (sub-100ns per frame pick-up).

## 4. Secure WebSockets (WSS)
Snap v3.0 supports Secure WebSockets (WSS) via the `WssServer` class (built-in SSL/TLS support).

---

## API Reference (WebSocket)
| Method | Description |
| :--- | :--- |
| `WsFrame` | Struct for WebSocket frame metadata and payload. |
| `WsServer(port, handler)` | Create a WebSocket server. |
| `start(pin_core)` | Starts the listener and worker reactor. |
| `decode_frame(buf, len, out)` | Ultra-fast decoder for incoming frames. |
| `encode_frame(buf, cap, frame)`| Ultra-fast encoder for outgoing frames. |
| `mask_payload(data, len, key)` | AVX2-powered byte masking. |
