#include "../snap.hpp"
#include <thread>
#include <atomic>
#include <iostream>

static constexpr size_t N = 1'000'000;

int main() {
    std::atomic<size_t> count{0};

    auto disp = snap::make_dispatcher<int>([&](const int& v) {
        count.fetch_add(1, std::memory_order_relaxed);
        (void)v;
    });

    std::thread prod([&]() {
        snap::pin_thread(0);
        for (size_t i = 0; i < N; ++i) {
            while (!disp.send(static_cast<int>(i))) { snap::cpu_relax(); }
        }
    });

    std::thread cons([&]() {
        snap::pin_thread(1);
        while (count.load(std::memory_order_relaxed) < N) {
            disp.poll();
        }
    });

    prod.join();
    cons.join();

    bool ok = (count.load() == N);
    std::cout << "Dispatch Test: sent=" << N << " received=" << count.load()
              << "\n" << (ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
