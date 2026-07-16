#include "../snap.hpp"
#include <thread>
#include <memory>

struct MarketData {
    int symbol_id;
    double price;
};

int main() {
    // Simple, clean, and now compiles perfectly
    auto link = snap::connect<MarketData>("inproc://ticker_feed");

    std::thread worker([&]() {
        MarketData msg;
        while(true) {
            if (link->recv(msg)) {
                std::cout << "[Engine] Price Received: " << msg.price << std::endl;
            }
            snap::cpu_relax();
        }
    });

    for(int i=0; i<5; ++i) {
        link->send({101, 1500.50 + i});
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "Demo finished." << std::endl;
    worker.detach();
    return 0;
}