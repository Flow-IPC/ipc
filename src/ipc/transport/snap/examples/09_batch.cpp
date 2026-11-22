#include "../snap.hpp"
#include <thread>
#include <iostream>
#include <numeric>
#include <atomic>

struct Batch {
    int value;
};

static constexpr size_t BATCH_SZ  = 32;
static constexpr size_t TOTAL_MSG = 100000;

int main() {
    snap::RingBuffer<Batch, 65536> rb;

    std::atomic<size_t> total_recv{0};

    std::thread producer([&]() {
        snap::pin_thread(0);
        Batch batch[BATCH_SZ];
        size_t sent = 0;
        while (sent < TOTAL_MSG) {
            size_t n = BATCH_SZ;
            if (sent + n > TOTAL_MSG) n = TOTAL_MSG - sent;
            for (size_t i = 0; i < n; ++i) batch[i].value = static_cast<int>(sent + i);
            size_t pushed = rb.push_n(batch, n);
            sent += pushed;
            if (!pushed) snap::cpu_relax();
        }
    });

    std::thread consumer([&]() {
        snap::pin_thread(1);
        Batch out[BATCH_SZ];
        while (total_recv.load(std::memory_order_relaxed) < TOTAL_MSG) {
            size_t n = rb.pop_n(out, BATCH_SZ);
            total_recv.fetch_add(n, std::memory_order_relaxed);
            if (!n) snap::cpu_relax();
        }
    });

    producer.join();
    consumer.join();

    std::cout << "Batch send/recv: total messages = " << total_recv.load() << "\n";
    std::cout << (total_recv.load() == TOTAL_MSG ? "PASS\n" : "FAIL\n");
    return 0;
}
