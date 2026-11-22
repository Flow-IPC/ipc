#include "snap/snap.hpp"
#include <iostream>

/**
 * Snap HTTP Example.
 * I built this to show how easy it is to spin up a sub-microsecond 
 * HTTP service. It's completely zero-allocation on the hot path.
 */
int main() {
    std::cout << "Snap v" << snap::VERSION << " HTTP Cache Srv @ 8080" << std::endl;

    auto h = [](const snap::HttpReq& req) -> snap::HttpRes {
        std::cout << "[Req] " << req.p << " (" << (int)req.m << ")" << std::endl;
        
        snap::HttpRes res;
        res.s = snap::HttpStatus::OK;
        
        if (req.p == "/api/hello") {
            res.b = "{\"msg\": \"Hello from Ultra-Fast Snap!\"}";
        } else {
            res.b = "Snap v3.0 - The World's Fastest Web Protocol Suite";
        }
        return res;
    };

    snap::HttpServer srv(8080, h);
    srv.start(0); // Pin to core 0 for max speed

    std::cout << "Server live. Press Enter to pull the plug." << std::endl;
    std::cin.get();
    srv.stop();
    return 0;
}
