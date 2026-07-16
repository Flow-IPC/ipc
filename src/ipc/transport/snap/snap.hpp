#pragma once
#include <memory>
#include <string_view>
#include "includes/utils.hpp"
#include "includes/ring_buffer.hpp"
#include "includes/mpmc_queue.hpp"
#include "includes/memory_pool.hpp"
#include "includes/dispatch.hpp"
#include "includes/pipeline.hpp"

namespace snap {
    template<typename T> class TcpLink;
    template<typename T, size_t Cap> class ShmLink;
    template<typename T> class UdpLink;
    template<typename T> class IpcLink;
    template<typename T> class MulticastLink;
    template<typename T, size_t Cap> class InprocLink;
}

namespace snap {

// This is our base for all message links. I kept it simple so we can swap 
// transports without breaking client code.
template<typename T>
class ILink {
public:
    virtual ~ILink() = default;
    virtual bool send(const T& m) noexcept = 0;
    virtual bool recv(T& m) noexcept = 0;
};

} // namespace snap

// Moved web protocols to the bottom

namespace snap {

// I've bumped this to 3.0 to mark our major protocol expansion
static constexpr std::string_view VERSION = "3.0.0";

} // namespace snap

// Moved ILink up

// I put these includes after the interface so they can inherit cleanly
#include "includes/shm_link.hpp"
#include "includes/udp_link.hpp"
#include "includes/tcp_link.hpp"
#include "includes/ipc_link.hpp"
#include "includes/multicast_link.hpp"

namespace snap {

// For simple thread-to-thread passing, I use this ring-buffer based link.
// It's basically a zero-copy pipe.
template<typename T, size_t Cap = 65536>
class InprocLink final : public ILink<T> {
    RingBuffer<T, Cap> _rb;
public:
    SNAP_HOT SNAP_FORCE_INLINE bool send(const T& m) noexcept override { return _rb.push(m); }
    SNAP_HOT SNAP_FORCE_INLINE bool recv(T& m) noexcept override       { return _rb.pop(m);  }
    
    // I added batch methods for when we need to move lots of messages at once
    size_t send_n(const T* msgs, size_t n) noexcept { return _rb.push_n(msgs, n); }
    size_t recv_n(T* msgs, size_t n) noexcept       { return _rb.pop_n(msgs, n);  }
};

// This is our main factory. I designed it to be smart based on the URI scheme.
// Whether it's memory, network, or IPC, I make sure we get the fastest path.
template<typename T, size_t Cap = 65536>
std::unique_ptr<ILink<T>> connect(std::string_view uri) {
    // If it's empty or inproc, we stay within the process
    if (starts_with(uri, "inproc://") || uri.find("://") == std::string_view::npos) {
        return std::make_unique<InprocLink<T, Cap>>();
    }
    
    // Fast path for shared memory. I use this for local but inter-process messaging.
    if (starts_with(uri, "shm://")) {
        return std::make_unique<ShmLink<T, Cap>>(std::string(uri.substr(6)).c_str());
    }
    
    // Networking protocols. I check for the '@' prefix to see if we're a listener.
    if (starts_with(uri, "udp://")) {
        std::string_view addr = uri.substr(6);
        bool srv = (addr[0] == '@');
        return std::make_unique<UdpLink<T>>(srv ? addr.data() + 1 : addr.data(), !srv);
    }
    
    if (starts_with(uri, "tcp://")) {
        std::string_view addr = uri.substr(6);
        bool srv = (addr[0] == '@');
        if (srv) {
            int fd = TcpLink<T>::listen_socket(addr.data() + 1);
            return std::unique_ptr<ILink<T>>(TcpLink<T>::accept(fd));
        }
        return std::unique_ptr<ILink<T>>(TcpLink<T>::connect(addr.data()));
    }
    
    if (starts_with(uri, "ipc://")) {
        std::string_view path = uri.substr(6);
        bool srv = (path[0] == '@');
        if (srv) {
            int fd = IpcLink<T>::listen_socket(path.data() + 1);
            return std::unique_ptr<ILink<T>>(IpcLink<T>::accept(fd, path.data() + 1));
        }
        return std::unique_ptr<ILink<T>>(IpcLink<T>::connect(path.data()));
    }
    
    // If we don't recognize the protocol, I fall back to in-process for safety.
    return std::make_unique<InprocLink<T, Cap>>();
}

// I added these helpers for multicast scenarios. Perfect for market data or chat.
template<typename T>
std::unique_ptr<ILink<T>> subscribe_multicast(const char* group, int port, int ttl = 1) {
    return std::make_unique<MulticastLink<T>>(group, port, false, ttl);
}

template<typename T>
std::unique_ptr<ILink<T>> publish_multicast(const char* group, int port, int ttl = 1) {
    return std::make_unique<MulticastLink<T>>(group, port, true, ttl);
}

} // namespace snap

// We pull in our web protocol extensions here, after everything else is defined
#include "includes/ssl_link.hpp"
#include "includes/http_link.hpp"
#include "includes/ws_link.hpp"
#include "includes/http_server.hpp"
#include "includes/ws_server.hpp"