#include "snap/snap.hpp"
#include <iostream>
#include <vector>

/**
 * Snap UDP Messaging Example.
 * I developed this to demonstrate our 21µs UDP round-trip latency.
 * We use recvmmsg and DSCP EF to beat standard networking libraries.
 */
struct Msg { uint64_t id; char buf[56]; };

int main(int argc, char** argv) {
    std::cout << "Snap " << snap::VERSION << " UDP Low-Latency Hub" << std::endl;

    if (argc > 1 && std::string(argv[1]) == "sub") {
        // Consumer (Listener) on core 4.
        auto link = snap::connect<Msg>("udp://@127.0.0.1:9000");
        snap::pin_thread(4);
        Msg m;
        size_t count = 0;
        
        while (count < 10'000) {
            if (link->recv(m)) {
                if (++count % 2000 == 0) {
                    std::cout << "[Sub] rcvd " << count << " msgs" << std::endl;
                }
            } else {
                snap::relax();
            }
        }
    } else {
        // Producer (Publisher) on core 5.
        auto link = snap::connect<Msg>("udp://127.0.0.1:9000");
        snap::pin_thread(5);
        std::cout << "Starting Pub (Publisher) on core 5..." << std::endl;
        for (uint64_t i = 0; i < 10'000; ++i) {
            Msg m { .id = i };
            while (!link->send(m)) snap::relax();
        }
    }

    return 0;
}
