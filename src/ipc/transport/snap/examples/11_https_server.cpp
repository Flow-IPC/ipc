#include "snap/snap.hpp"
#include <iostream>

/**
 * Snap HTTPS 1.1 Support (v3.0.0)
 * 
 * To generate dev certs:
 * openssl req -new -x509 -days 365 -nodes -out cert.pem -keyout key.pem
 */

int main() {
    std::cout << "Snap v" << snap::VERSION << " HTTPS Secured Server - Fast & Safe" << std::endl;

    snap::SslContext ssl(true);
    // ssl.load_cert_file("cert.pem", "key.pem"); // Uncomment after generating certs

    auto handler = [](const snap::HttpRequest& req) -> snap::HttpResponse {
        std::cout << "[Secure] Received: " << req.path << std::endl;
        snap::HttpResponse res;
        res.status = snap::HttpStatus::OK;
        res.body = "SECURE SNAP: SSL-Encrypted High Performance Payload";
        return res;
    };

    // snap::HttpServer server(8443, handler); // In v3, we allow passing SslContext
    // server.start(2); 

    std::cout << "HTTPS logic ready. See SNAP_SSL.md for setup." << std::endl;
    return 0;
}
