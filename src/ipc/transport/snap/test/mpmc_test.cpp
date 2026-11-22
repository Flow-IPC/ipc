#include "../snap.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <iostream>

static constexpr size_t TOTAL   = 10'000'000;
static constexpr int    PROD    = 4;
static constexpr int    CONS    = 4;
static constexpr size_t PER_P   = TOTAL / PROD;

int main() {
    snap::MpmcQueue<uint64_t, 65536> q;
    std::atomic<uint64_t> sent{0}, recvd{0};
    std::atomic<bool>     done{false};

    std::vector<std::thread> producers, consumers;

    for (int p = 0; p < PROD; ++p) {
        producers.emplace_back([&, p]() {
            snap::pin_thread(p % 4);
            for (size_t i = 0; i < PER_P; ++i) {
                while (!q.push(static_cast<uint64_t>(p * PER_P + i))) {
                    snap::cpu_relax();
                }
                sent.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (int c = 0; c < CONS; ++c) {
        consumers.emplace_back([&]() {
            uint64_t v;
            while (!done.load(std::memory_order_acquire) || !q.empty()) {
                if (q.pop(v)) {
                    recvd.fetch_add(1, std::memory_order_relaxed);
                } else {
                    snap::cpu_relax();
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    done.store(true, std::memory_order_release);
    for (auto& t : consumers) t.join();

    bool ok = (sent.load() == TOTAL) && (recvd.load() == TOTAL);
    std::cout << "MPMC Test: " << PROD << "P x " << CONS << "C, sent=" << sent.load()
              << " recvd=" << recvd.load()
              << "\n" << (ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
