#include "snap/snap.hpp"
#include <iostream>
#include <unistd.h>

/**
 * Snap Shared Memory Example.
 * I developed this to demonstrate the 140ns SHM (Shared Memory) 
 * link between two processes on the same machine. No syscalls, 
 * no buffers copying, just pure memory mapping.
 */
struct Msg { uint64_t id; char buf[56]; };

int main(int argc, char** argv) {
    std::cout << "Snap " << snap::VERSION << " SHM Inter-Process Hub" << std::endl;

    // I built our factory to handle various transport URIs seamlessly.
    auto link = snap::connect<Msg>("shm://market_data");

    if (argc > 1 && std::string(argv[1]) == "sub") {
        // Consumer process on core 2.
        snap::pin_thread(2);
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
        // Producer process on core 3.
        snap::pin_thread(3);
        std::cout << "Starting Pub (Producer) on core 3..." << std::endl;
        for (uint64_t i = 0; i < 10'000; ++i) {
            Msg m { .id = i };
            while (!link->send(m)) snap::relax();
        }
    }

    return 0;
}
