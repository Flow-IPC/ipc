#include "../snap.hpp"
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

struct RawTick { double bid; double ask; };
struct Spread  { double value; };
struct Alert   { bool   wide; double spread; };

int main() {
    snap::Stage<RawTick, Spread>  stage1;
    snap::Stage<Spread,  Alert>   stage2;
    snap::SinkStage<Alert>        sink;

    std::atomic<int> alerts{0};

    sink.start([&](const Alert& a) {
        ++alerts;
        if (alerts.load() <= 5)
            std::cout << "[Sink] Alert: wide=" << a.wide << " spread=" << a.spread << "\n";
    }, 3);

    stage2.start([&](const Spread& s) -> Alert {
        while (!sink.inbox.push({s.value > 0.5, s.value})) { snap::cpu_relax(); }
        return {s.value > 0.5, s.value};
    }, 2);

    stage1.start([](const RawTick& t) -> Spread {
        return {t.ask - t.bid};
    }, 1);

    snap::pin_thread(0);
    for (int i = 0; i < 200; ++i) {
        RawTick tick{100.0 + i * 0.01, 100.1 + i * 0.01 + (i % 7) * 0.1};
        while (!stage1.inbox.push(tick)) { snap::cpu_relax(); }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    stage1.stop();
    stage2.stop();
    sink.stop();

    std::cout << "Total alerts: " << alerts.load() << "\n";
    return 0;
}
