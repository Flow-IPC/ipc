#pragma once
#include "snap.hpp"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

namespace snap {

/**
 * Snap SslLink - v3.0.0
 * I built this as a high-speed SSL/TLS wrapper. It uses non-blocking BIOs 
 * to ensure that encryption doesn't kill our latency.
 */
struct SslCtx {
    SSL_CTX* ctx;
    bool srv;

    SslCtx(bool is_srv = false) : srv(is_srv) {
        static bool init = false;
        if (!init) {
            SSL_library_init();
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
            init = true;
        }
        ctx = SSL_CTX_new(srv ? TLS_server_method() : TLS_client_method());
        if (!ctx) { perror("SslCtx init fail"); exit(1); }
    }

    void load(const char* cert, const char* key) {
        if (SSL_CTX_use_certificate_chain_file(ctx, cert) <= 0) exit(1);
        if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) <= 0) exit(1);
        if (!SSL_CTX_check_private_key(ctx)) exit(1);
    }

    ~SslCtx() { if (ctx) SSL_CTX_free(ctx); }
};

template<typename T>
class SslLink final : public ILink<T> {
    int _fd = -1;
    SSL* _ssl = nullptr;
    bool _shaked = false;
    bool _srv;

public:
    using msg_t = T;

    SslLink(int fd, SSL_CTX* ctx, bool is_srv) : _fd(fd), _srv(is_srv) {
        _ssl = SSL_new(ctx);
        SSL_set_fd(_ssl, _fd);
        if (_srv) SSL_set_accept_state(_ssl);
        else SSL_set_connect_state(_ssl);
        
        int flags = fcntl(_fd, F_GETFL, 0);
        fcntl(_fd, F_SETFL, flags | O_NONBLOCK);
    }

    ~SslLink() {
        if (_ssl) { SSL_shutdown(_ssl); SSL_free(_ssl); }
        if (_fd >= 0) close(_fd);
    }

    // Encrypted send. Standard Snap ILink compliant.
    SNAP_HOT bool send(const T& m) noexcept override {
        if (!_shaked) if (!shake()) return false;
        int n = SSL_write(_ssl, &m, sizeof(T));
        if (n <= 0) return (SSL_get_error(_ssl, n) == SSL_ERROR_WANT_WRITE);
        return n == sizeof(T);
    }

    // Encrypted recv. Low-latency non-blocking read.
    SNAP_HOT bool recv(T& m) noexcept override {
        if (!_shaked) if (!shake()) return false;
        int n = SSL_read(_ssl, &m, sizeof(T));
        if (n <= 0) return (SSL_get_error(_ssl, n) == SSL_ERROR_WANT_READ);
        return n == sizeof(T);
    }

    // TLS Handshake polling. I kept this in the hot path to avoid 
    // any initialization blocking.
    bool shake() {
        int r = SSL_do_handshake(_ssl);
        if (r == 1) { _shaked = true; return true; }
        int e = SSL_get_error(_ssl, r);
        return (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE);
    }

    // Raw byte I/O for HTTPS/WSS. Super fast.
    ssize_t send_raw(const void* d, size_t l) {
        if (!_shaked) if (!shake()) return -1;
        return SSL_write(_ssl, d, l);
    }

    ssize_t recv_raw(void* d, size_t l) {
        if (!_shaked) if (!shake()) return -1;
        return SSL_read(_ssl, d, l);
    }
    
    int fd() const { return _fd; }
};

} // namespace snap
