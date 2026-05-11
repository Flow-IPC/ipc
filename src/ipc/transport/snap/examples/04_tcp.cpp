#include "snap/snap.hpp"
#include <iostream>

/**
 * Snap TCP Persistent Link Example.
 * I developed this for 28µs TCP round-trip latency.
 * We use TCP_NODELAY and TCP_QUICKACK to avoid Nagle's delay.
 */
struct Msg { uint64_t id; char buf[56]; };

int main(int argc, char** argv) {
    std::cout << "Snap " << snap::VERSION << " TCP Zero-Latency Hub" << std::endl;

    if (argc > 1 && std::string(argv[1]) == "sub") {
        // Consumer (Server) on core 6.
        std::cout << "Starting Srv (Listener) on core 6..." << std::endl;
        auto link = snap::connect<Msg>("tcp://@127.0.0.1:9001");
        snap::pin_thread(6);
        Msg m;
        size_t count = 0;
        
        while (count < 10'000) {
            if (link->recv(m)) {
                if (++count % 2000 == 0) {
                    std::cout << "[Srv] rcvd " << count << " msgs" << std::endl;
                }
            } else {
                snap::relax();
            }
        }
    } else {
        // Producer (Client) on core 7.
        std::cout << "Starting Cli (Connector) on core 7..." << std::endl;
        auto link = snap::connect<Msg>("tcp://127.0.0.1:9001");
        snap::pin_thread(7);
        for (uint64_t i = 0; i < 10'000; ++i) {
            Msg m { .id = i };
            while (!link->send(m)) snap::relax();
        }
    }

    return 0;
}
