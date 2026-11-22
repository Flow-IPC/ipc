# SNAP SSL/TLS Setup Guide
**Version 3.0.0**

This guide describes how to use Snap with encrypted transports (HTTPS/WSS/SSL-Chat) using **OpenSSL**.

---

## 1. Prerequisites
Snap requires `libssl-dev` (Linux) and a standard compiler with C++20 support.

```bash
sudo apt-get install -y libssl-dev
```

## 2. Generating Certificates (Dev/Test)
Use the following commands to generate self-signed certificates for local development:

```bash
# Generate CA and Server Certificate
openssl genrsa -out ca.key 4096
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt -subj "/CN=Snap-Dev-CA"

# Server Key & Cert
openssl genrsa -out server.key 4096
openssl req -new -key server.key -out server.csr -subj "/CN=localhost"
openssl x509 -req -days 365 -in server.csr -CA ca.crt -CAkey ca.key -set_serial 01 -out server.crt

# Bundle for Snap
cat server.crt server.key > server.pem
```

## 3. Using HTTPS in Snap
Snap's `HttpServer` and `WsServer` can be upgraded to secure mode by passing an `SslContext`:

```cpp
#include "snap/snap.hpp"

// SSL Server Configuration
snap::SslContext ssl(true); // true = Server mode
ssl.load_cert_file("server.pem", "server.key");

// Secure HttpServer
snap::HttpServer<tls_handler> server(8443, handler);
server.enable_tls(ssl); 
server.start(0); 
```

## 4. Performance Tuning
*   **ALPN:** (Experimental) Snap v3.0 supports ALPN for protocol negotiation (HTTP/2 ready).
*   **Zero-Copy Handshake:** Snap uses non-blocking encrypted BIOs to avoid kernel context switching during SSL handshake.
*   **Cipher Selection:** Use `tls_server_method()` with Snap's optimized cipher list (AES-NI enabled).

---

## API Reference (SSL)
| Method | Description |
| :--- | :--- |
| `SslContext(is_server)` | Initialize OpenSSL context and algorithms. |
| `load_cert_file(cert, key)` | Load PEM certificates and private keys. |
| `SslLink<T>(fd, ctx, is_srv)` | Wrap an existing socket FD into an encrypted link. |
| `handshake()` | Non-blocking TLS handshake state poll. |
| `send_raw() / recv_raw()` | Direct encrypted byte I/O. |
