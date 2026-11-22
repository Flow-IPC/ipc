# 📜 SNAP API REFERENCE
**Version 3.0.0**

## NAMESPACE: `snap`

### CONSTANTS
- `snap::VERSION`: `"3.0.0"`
- `snap::SNAP_CACHE_LINE`: `64` (bytes)

---

## 🔗 `ILink<T>` (snap.hpp)
Base interface for all transport links.

- `virtual bool send(const T& m)`
  - Push message `m`. Non-blocking. Returns `false` if srv/link is full.
- `virtual bool recv(T& m)`
  - Pull message into `m`. Non-blocking. Returns `false` if empty.

---

## 🏭 `snap::connect<T, Cap>(uri)` (snap.hpp)
Factory that returns a smart pointer to an `ILink<T>`.

**Template Parameters:**
- `T`: Message struct. Must be trivially copyable.
- `Cap`: Buffer capacity (SPSC). **MUST be a power of 2**.

**URI Schemes:**
- `"inproc://..."`: Internal thread-to-thread piping.
- `"shm://name"`: Shared memory for inter-process communication (IPC).
- `"udp://IP:PORT"`: UDP Transmitter.
- `"udp://@IP:PORT"`: UDP Receiver (Listener).
- `"tcp://IP:PORT"`: TCP Connector (Client).
- `"tcp://@IP:PORT"`: TCP Reactor (Server).
- `"ipc://PATH"`: Unix Domain Socket (Path).
- `"ipc://@PATH"`: Unix Domain Socket Listener.

---

## 🌐 `HttpServer<Handler>` (includes/http_server.hpp)
Polling HTTP/1.1 Reactor with zero-allocation parsing.

- `HttpServer(port, handler)`
- `start(core)`: Starts the reactor on a specific CPU core.
- `stop()`: Halts the server.

**Structures:**
- `HttpReq`: Contains `m` (Method), `p` (Path), `hdrs` (Headers), `b` (Body).
- `HttpRes`: Contains `s` (Status), `b` (Body).

---

## 💬 `WsServer<Handler>` (includes/ws_server.hpp)
WebSocket Reactor with AVX2-accelerated framing.

- `WsServer(port, handler)`
- `start(core)`: Starts the worker thread.
- `stop()`: Halts the server.

**Structures:**
- `WsFrame`: Contains `op` (Opcode), `pay` (Payload), `len` (Length).

---

## 🔒 `SslLink<T>` (includes/ssl_link.hpp)
Non-blocking OpenSSL link for encrypted messaging.

- `send(const T& m)` / `recv(T& m)`: Messaging.
- `send_raw(d, l)` / `recv_raw(d, l)`: Raw byte transfer.
- `shake()`: Handshake polling.

---

## 📦 `RingBuffer<T, Cap>` (includes/ring_buffer.hpp)
LMAX Disruptor-style lock-free ring buffer.

- `push(const T& d)` / `pop(T& out)`
- `push_n(data, n)` / `pop_n(out, n)`
- `full()` / `empty()` / `size()`

---

## 🏊 `MemoryPool<T, Slots, HugePages>` (includes/memory_pool.hpp)
Lock-free slab allocator for ultra-fast object recycling.

- `T* alloc()`: Grab an object.
- `void free(T* p)`: Return to pool.

---

## ⚡ UTILITIES (includes/utils.hpp)
- `relax()`: CPU pause/yield.
- `spin(n)`: Busy-wait for n cycles.
- `ts_ns()`: Current monotonic time in nanoseconds.
- `cycles()`: Raw CPU cycle counter (RDTSC).
- `pin_thread(core)`: Pin current thread to a CPU core.

---

### 🛡 Performance Macros
- `SNAP_HOT`: Mark function for optimization.
- `SNAP_COLD`: Mark for slow-path.
- `SNAP_LIKELY(x)` / `SNAP_UNLIKELY(x)`: Branch hints.
- `SNAP_CACHE_LINE`: 64.
