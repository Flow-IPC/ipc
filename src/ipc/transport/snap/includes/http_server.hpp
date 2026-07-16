#pragma once
#include "snap.hpp"
#include "http_link.hpp"
#include <atomic>
#include <thread>
#include <list>

namespace snap {

/**
 * Polling HTTP Server.
 * I built this as a thin reactor over our TcpLink. It handles 
 * multiple connections via busy-polling, which is the fastest 
 * way for small-to-medium scale services to eliminate jitter.
 */
template<typename Handler>
class HttpServer {
    int _port;
    Handler _h;
    std::atomic<bool> _run{false};
    std::thread _worker;
    int _srv = -1;

public:
    HttpServer(int port, Handler&& h) : _port(port), _h(std::move(h)) {}
    ~HttpServer() { stop(); }

    // Start: I recommend pinning to a dedicated core.
    void start(int core = -1) {
        _srv = TcpLink<char[4096]>::listen_socket(("@0.0.0.0:" + std::to_string(_port)).c_str());
        _run = true;
        _worker = std::thread([this, core]() {
            if (core >= 0) pin_thread(core);
            set_thread_name(("snap_http_" + std::to_string(_port)).c_str());
            
            std::list<std::unique_ptr<TcpLink<char[4096]>>> clis;
            while (_run) {
                // I check for new connections first
                int cfd = accept(_srv, nullptr, nullptr);
                if (cfd >= 0) {
                    clis.push_back(std::make_unique<TcpLink<char[4096]>>(cfd, ""));
                }

                // I poll all active clients. No epoll overhead, just pure cycle-churn.
                auto it = clis.begin();
                while (it != clis.end()) {
                    char buf[4096];
                    if ((*it)->recv(*reinterpret_cast<char(*)[4096]>(buf))) {
                        HttpReq req;
                        std::memcpy(req.buf, buf, 4096);
                        req.l = 4096;
                        if (HttpParser::parse_req(req)) {
                            HttpRes res = _h(req);
                            // Simple response build. I kept it minimal for speed.
                            std::string s = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(res.b.size()) + "\r\n\r\n" + std::string(res.b);
                            char frame[4096] = {0}; 
                            std::memcpy(frame, s.data(), std::min(s.size(), (size_t)4096));
                            (*it)->send(*reinterpret_cast<char(*)[4096]>(frame));
                        }
                    }
                    ++it; // I simplified client removal for the example
                }
                relax();
            }
        });
    }

    void stop() {
        _run = false;
        if (_worker.joinable()) _worker.join();
        if (_srv >= 0) close(_srv);
    }
};

} // namespace snap
