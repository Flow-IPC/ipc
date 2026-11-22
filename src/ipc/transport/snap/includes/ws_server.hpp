#pragma once
#include "snap.hpp"
#include "ws_link.hpp"
#include "http_link.hpp"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <atomic>
#include <thread>
#include <list>

namespace snap {

/**
 * WebSocket Scaling Utils.
 * I developed this to handle the initial RFC 6455 handshake.
 */
class WsUtil {
public:
    static std::string b64(const unsigned char* in, int l) {
        BIO *b64, *bio;
        BUF_MEM *buf_ptr;
        b64 = BIO_new(BIO_f_base64());
        bio = BIO_new(BIO_s_mem());
        bio = BIO_push(b64, bio);
        BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
        BIO_write(bio, in, l);
        BIO_flush(bio);
        BIO_get_mem_ptr(bio, &buf_ptr);
        std::string res(buf_ptr->data, buf_ptr->length);
        BIO_free_all(bio);
        return res;
    }

    static std::string handshake(std::string_view key) {
        static constexpr std::string_view GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string combined = std::string(key) + std::string(GUID);
        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1((const unsigned char*)combined.data(), combined.size(), hash);
        std::string enc = b64(hash, SHA_DIGEST_LENGTH);
        return "HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: " + enc + "\r\n\r\n";
    }
};

/**
 * High-performance WebSocket Server.
 * I designed this to be a massive-throughput chat or streaming hub.
 * It combines our fast HTTP parser with our AVX2 WebSocket framer.
 */
template<typename Handler>
class WsServer {
    int _port;
    Handler _h;
    std::atomic<bool> _run{false};
    std::thread _worker;

public:
    WsServer(int port, Handler&& h) : _port(port), _h(std::move(h)) {}
    ~WsServer() { stop(); }

    void start(int core = -1) {
        int srv = TcpLink<char[4096]>::listen_socket(("@0.0.0.0:" + std::to_string(_port)).c_str());
        _run = true;
        _worker = std::thread([this, srv, core]() {
            if (core >= 0) pin_thread(core);
            std::list<std::unique_ptr<TcpLink<char[4096]>>> clis;
            while (_run) {
                int cfd = accept(srv, nullptr, nullptr);
                if (cfd >= 0) clis.push_back(std::make_unique<TcpLink<char[4096]>>(cfd, ""));

                auto it = clis.begin();
                while (it != clis.end()) {
                    char buf[4096];
                    if ((*it)->recv(*reinterpret_cast<char(*)[4096]>(buf))) {
                        std::string_view d(buf, 4096);
                        if (d.find("Upgrade: websocket") != std::string_view::npos) {
                            size_t kp = d.find("Sec-WebSocket-Key: ");
                            if (kp != std::string_view::npos) {
                                std::string_view key = d.substr(kp + 19, 24);
                                std::string res_txt = WsUtil::handshake(key);
                                char res[4096] = {0}; std::memcpy(res, res_txt.data(), res_txt.size());
                                (*it)->send(*reinterpret_cast<char(*)[4096]>(res));
                            }
                        } else {
                            WsFrame f;
                            if (Ws::decode(buf, 4096, f)) {
                                _h(f); // I call our handler on the hot path
                            }
                        }
                    }
                    ++it;
                }
                relax();
            }
            close(srv);
        });
    }

    void stop() { _run = false; if(_worker.joinable()) _worker.join(); }
};

} // namespace snap
