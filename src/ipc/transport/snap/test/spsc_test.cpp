#include "../snap.hpp"
#include <thread>
#include <atomic>
#include <iostream>
#include <cassert>

static constexpr size_t N = 10'000'000;

int main() {
    snap::RingBuffer<uint64_t, 131072> rb;
    std::atomic<uint64_t> checksum_send{0}, checksum_recv{0};
    std::atomic<size_t>   recv_count{0};

    std::thread producer([&]() {
        snap::pin_thread(0);
        snap::set_thread_name("spsc_prod");
        for (uint64_t i = 0; i < N; ++i) {
            while (!rb.push(i)) { snap::cpu_relax(); }
            checksum_send.fetch_add(i, std::memory_order_relaxed);
        }
    });

    std::thread consumer([&]() {
        snap::pin_thread(1);
        snap::set_thread_name("spsc_cons");
        uint64_t v;
        while (recv_count.load(std::memory_order_relaxed) < N) {
            if (rb.pop(v)) {
                checksum_recv.fetch_add(v, std::memory_order_relaxed);
                recv_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                snap::cpu_relax();
            }
        }
    });

    producer.join();
    consumer.join();

    bool ok = (recv_count.load() == N) && (checksum_send.load() == checksum_recv.load());
    std::cout << "SPSC Test: " << N << " messages, recv=" << recv_count.load()
              << ", checksum_match=" << (checksum_send.load() == checksum_recv.load() ? "YES" : "NO")
              << "\n" << (ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
