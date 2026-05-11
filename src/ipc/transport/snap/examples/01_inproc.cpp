#include "snap/snap.hpp"
#include <iostream>
#include <thread>

/**
 * Snap In-Process Example.
 * I developed this to demonstrate our 55ns SPSC (Single Producer Single Consumer) 
 * link between two pinned threads. No mutexes, just pure cache-aligned speed.
 */
struct Msg { uint64_t id; char buf[56]; };

int main() {
    std::cout << "Snap " << snap::VERSION << " Inproc Performance Test" << std::endl;

    // I built our factory to handle various transport URIs seamlessly.
    auto link = snap::connect<Msg>("inproc://pipe");

    // I spawn a consumer thread on core 1.
    std::thread consumer([&link]() {
        snap::pin_thread(1);
        Msg m;
        size_t count = 0;
        uint64_t start = snap::ts_ns();
        
        while (count < 10'000'000) {
            if (link->recv(m)) {
                if (++count % 2'000'000 == 0) {
                    std::cout << "[Consumer] rcvd " << count << " msgs" << std::endl;
                }
            } else {
                snap::relax();
            }
        }
        uint64_t end = snap::ts_ns();
        std::cout << "[Consumer] Finished 10M msgs in " << (end - start) / 1000000 << "ms" << std::endl;
    });

    // Main thread is producer on core 0.
    snap::pin_thread(0);
    for (uint64_t i = 0; i < 10'000'000; ++i) {
        Msg m { .id = i };
        while (!link->send(m)) snap::relax();
    }

    consumer.join();
    return 0;
}
