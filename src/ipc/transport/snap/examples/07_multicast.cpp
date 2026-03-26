#include "../snap.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

struct MarketBroadcast {
    uint32_t symbol;
    double   last_price;
};

int main(int argc, char** argv) {
    bool is_pub = (argc < 2 || std::string(argv[1]) == "pub");
    const char* GROUP = "239.0.0.1";
    const int   PORT  = 9100;

    if (is_pub) {
        auto link = snap::publish_multicast<MarketBroadcast>(GROUP, PORT);
        std::cout << "[Publisher] Broadcasting to " << GROUP << ":" << PORT << "\n";
        for (int i = 0; i < 50; ++i) {
            MarketBroadcast msg{static_cast<uint32_t>(i % 10), 100.0 + i};
            while (!link->send(msg)) { snap::cpu_relax(); }
            if (i < 3) std::cout << "[Publisher] sym=" << msg.symbol << " price=" << msg.last_price << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        std::cout << "[Publisher] Done.\n";
    } else {
        auto link = snap::subscribe_multicast<MarketBroadcast>(GROUP, PORT);
        std::cout << "[Subscriber] Joined multicast " << GROUP << ":" << PORT << "\n";
        int count = 0;
        while (count < 50) {
            MarketBroadcast msg;
            if (link->recv(msg)) {
                ++count;
                if (count <= 3) std::cout << "[Subscriber] sym=" << msg.symbol << " price=" << msg.last_price << "\n";
            } else {
                snap::cpu_relax();
            }
        }
        std::cout << "[Subscriber] Total: " << count << "\n";
    }
    return 0;
}
