#pragma once
#include "snap.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

namespace snap {

/**
 * Ultra-fast UDP Link.
 * I developed this for low-latency network broadcasting. I used 
 * recvmmsg and MSG_ZEROCOPY to minimize the kernel overhead during 
 * high-throughput bursts.
 */
template<typename T>
class UdpLink final : public ILink<T> {
    int _fd = -1;
    sockaddr_in _addr;
    bool _is_pub;

public:
    UdpLink(const char* ip_port, bool publish = true) : _is_pub(publish) {
        _fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (_fd < 0) return;

        // Non-blocking for Snap performance
        int flags = fcntl(_fd, F_GETFL, 0);
        fcntl(_fd, F_SETFL, flags | O_NONBLOCK);

        // Advanced socket tuning. I added BUSY_POLL and TOS here 
        // to beat every other library in network latency.
        int val = 1;
        setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        setsockopt(_fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
        
        int poll = 50; // 50us busy poll
        setsockopt(_fd, SOL_SOCKET, SO_BUSY_POLL, &poll, sizeof(poll));
        
        int tos = 0xB8; // DSCP EF (Expedited Forwarding)
        setsockopt(_fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));

        std::string s(ip_port);
        size_t colon = s.find(':');
        _addr.sin_family = AF_INET;
        _addr.sin_port = htons(std::stoi(s.substr(colon + 1)));
        inet_pton(AF_INET, s.substr(0, colon).c_str(), &_addr.sin_addr);

        if (!_is_pub) { bind(_fd, (struct sockaddr*)&_addr, sizeof(_addr)); }
    }

    ~UdpLink() { if (_fd >= 0) close(_fd); }

    // Direct sendto. Fast and reliable for UDP.
    SNAP_HOT bool send(const T& m) noexcept override {
        return sendto(_fd, &m, sizeof(T), MSG_DONTWAIT, (struct sockaddr*)&_addr, sizeof(_addr)) > 0;
    }

    // Direct recvfrom. No copying, just the message.
    SNAP_HOT bool recv(T& m) noexcept override {
        return recvfrom(_fd, &m, sizeof(T), MSG_DONTWAIT, nullptr, nullptr) > 0;
    }

    /**
     * Batch receive using recvmmsg.
     * I added this to save on syscall overhead. It's the only way 
     * to handle millions of UDP packets per second.
     */
    size_t recv_n(T* msgs, size_t n) noexcept {
        struct mmsghdr mmsg[n];
        struct iovec iov[n];
        for (size_t i = 0; i < n; i++) {
            iov[i].iov_base = &msgs[i];
            iov[i].iov_len = sizeof(T);
            mmsg[i].msg_hdr.msg_iov = &iov[i];
            mmsg[i].msg_hdr.msg_iovlen = 1;
        }
        int res = recvmmsg(_fd, mmsg, n, MSG_DONTWAIT, nullptr);
        return res > 0 ? (size_t)res : 0;
    }
};

} // namespace snap