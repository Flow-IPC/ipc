#include "../snap.hpp"
#include <iostream>

struct Task {
    int id;
    char data[64];
};

int main() {
    snap::MemoryPool<Task, 1024> pool;

    std::cout << "Pool capacity: " << snap::MemoryPool<Task, 1024>::capacity() << "\n";

    Task* tasks[10];
    for (int i = 0; i < 10; ++i) {
        tasks[i] = pool.allocate();
        if (tasks[i]) {
            tasks[i]->id = i;
            std::cout << "[Alloc] Task id=" << tasks[i]->id << " @ " << tasks[i] << "\n";
        }
    }

    for (int i = 0; i < 5; ++i) {
        pool.deallocate(tasks[i]);
        std::cout << "[Free] Released task id=" << i << "\n";
    }

    Task* reused = pool.allocate();
    if (reused) {
        reused->id = 999;
        std::cout << "[Reuse] Got recycled slot, setting id=" << reused->id << "\n";
        pool.deallocate(reused);
    }

    for (int i = 5; i < 10; ++i) {
        pool.deallocate(tasks[i]);
    }

    std::cout << "Done.\n";
    return 0;
}
