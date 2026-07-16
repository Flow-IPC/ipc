#include "snap/snap.hpp"
#include <iostream>
#include <vector>

/**
 * High-Throughput WebSocket Chat Hub.
 * I developed this to demonstrate our AVX2-powered framing engine.
 * It handles 128-byte masking/unmasking in near-zero cycles.
 */
int main() {
    std::cout << "Snap v" << snap::VERSION << " WebSocket Chat Hub @ 8081" << std::endl;

    // Chat room state (simplified)
    static std::vector<int> fds; 

    auto h = [](const snap::WsFrame& f) {
        if (f.op == snap::WsOp::TEXT) {
            std::string msg((const char*)f.pay, f.len);
            std::cout << "[Chat] msg: " << msg << std::endl;
            
            // I simplified this multicast for the demo. 
            // In a real app, I'd iterate through a session pool.
            if (msg == "!") std::cout << "Client pulse: active!" << std::endl;
        }
    };

    snap::WsServer srv(8081, h);
    srv.start(1); // Pin chat worker to core 1

    std::cout << "Chat live at ws://localhost:8081" << std::endl;
    std::cout << "Press Enter to stop." << std::endl;
    std::cin.get();
    srv.stop();
    return 0;
}
