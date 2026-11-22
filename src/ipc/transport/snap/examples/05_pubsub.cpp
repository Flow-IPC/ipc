#include "../snap.hpp"
#include <iostream>
#include <atomic>
#include <thread>

struct Event {
    int type;
    double value;
};

int main() {
    std::atomic<int> received_a{0}, received_b{0};

    auto disp_a = snap::make_dispatcher<Event>([&](const Event& e) {
        ++received_a;
        if (received_a.load() <= 3)
            std::cout << "[HandlerA] type=" << e.type << " val=" << e.value << "\n";
    });

    auto disp_b = snap::make_dispatcher<Event>([&](const Event& e) {
        ++received_b;
        if (received_b.load() <= 3)
            std::cout << "[HandlerB] type=" << e.type << " val=" << e.value << "\n";
    });

    std::thread producer([&]() {
        for (int i = 0; i < 1000; ++i) {
            Event e{i % 5, i * 1.5};
            while (!disp_a.send(e)) { snap::cpu_relax(); }
            while (!disp_b.send(e)) { snap::cpu_relax(); }
        }
    });

    std::thread consumer([&]() {
        int total = 0;
        while (total < 2000) {
            disp_a.poll();
            disp_b.poll();
            total = received_a.load() + received_b.load();
        }
    });

    producer.join();
    consumer.join();

    std::cout << "HandlerA received: " << received_a.load() << "\n";
    std::cout << "HandlerB received: " << received_b.load() << "\n";
    return 0;
}
