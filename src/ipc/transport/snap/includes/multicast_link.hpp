#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "utils.hpp"

namespace snap {

template<typename T>
class MulticastLink : public ILink<T> {
    static_assert(sizeof(T) <= 65507, "MulticastLink: message type too large for a UDP datagram");

    int                _fd;
    struct sockaddr_in _group_addr;
    bool               _is_publisher;

public:
    MulticastLink(const char* group_ip, int port, bool is_publisher, int ttl = 1, bool loopback = true)
        : _is_publisher(is_publisher) {
        _fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (_fd < 0) { perror("snap: mcast socket"); exit(1); }

        int opt = 1;
        setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

        int busy_poll = 100;
        setsockopt(_fd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll, sizeof(busy_poll));

        memset(&_group_addr, 0, sizeof(_group_addr));
        _group_addr.sin_family      = AF_INET;
        _group_addr.sin_addr.s_addr = inet_addr(group_ip);
        _group_addr.sin_port        = htons(port);

        if (_is_publisher) {
            setsockopt(_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
            int lp = loopback ? 1 : 0;
            setsockopt(_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &lp, sizeof(lp));
        } else {
            struct sockaddr_in bind_addr{};
            bind_addr.sin_family      = AF_INET;
            bind_addr.sin_port        = htons(port);
            bind_addr.sin_addr.s_addr = INADDR_ANY;
            if (bind(_fd, reinterpret_cast<const struct sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
                perror("snap: mcast bind"); exit(1);
            }

            struct ip_mreq mreq{};
            mreq.imr_multiaddr.s_addr = inet_addr(group_ip);
            mreq.imr_interface.s_addr = INADDR_ANY;
            if (setsockopt(_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
                perror("snap: mcast join"); exit(1);
            }
        }

        fcntl(_fd, F_SETFL, O_NONBLOCK);
    }

    ~MulticastLink() override { close(_fd); }

    SNAP_HOT SNAP_FORCE_INLINE bool send(const T& m) noexcept override {
        return sendto(_fd, &m, sizeof(T), MSG_DONTWAIT,
                      reinterpret_cast<const struct sockaddr*>(&_group_addr), sizeof(_group_addr)) == sizeof(T);
    }

    SNAP_HOT SNAP_FORCE_INLINE bool recv(T& m) noexcept override {
        return recvfrom(_fd, &m, sizeof(T), MSG_DONTWAIT, nullptr, nullptr) == sizeof(T);
    }
};

} // namespace snap
