#include "../snap.hpp"
#include <thread>
#include <vector>
#include <iostream>
#include <atomic>

struct Item { int id; char pad[60]; };
static constexpr size_t POOL_SIZE = 4096;

int main() {
    snap::MemoryPool<Item, POOL_SIZE> pool;
    std::atomic<bool> errors{false};

    std::vector<Item*> ptrs;
    ptrs.reserve(POOL_SIZE);

    for (size_t i = 0; i < POOL_SIZE; ++i) {
        Item* p = pool.allocate();
        if (!p) { std::cerr << "FAIL: alloc returned null at i=" << i << "\n"; errors = true; break; }
        p->id = static_cast<int>(i);
        ptrs.push_back(p);
    }

    Item* overflow = pool.allocate();
    if (overflow != nullptr) {
        std::cerr << "FAIL: expected null on overflow, got non-null\n";
        errors = true;
    }

    for (auto* p : ptrs) pool.deallocate(p);

    std::vector<std::thread> threads;
    std::atomic<size_t> alloc_count{0};
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 10000; ++i) {
                Item* p = pool.allocate();
                if (p) { alloc_count.fetch_add(1, std::memory_order_relaxed); pool.deallocate(p); }
            }
        });
    }
    for (auto& t : threads) t.join();

    bool ok = !errors.load() && (alloc_count.load() > 0);
    std::cout << "Pool Test: allocs=" << alloc_count.load()
              << "\n" << (ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
