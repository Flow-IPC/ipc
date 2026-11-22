#include "../snap.hpp"
#include <vector>
#include <numeric>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <cstring>
#include <thread>
#include <atomic>

struct BenchMsg {
    uint64_t id;
    uint64_t ts_ns;
};

struct Stats {
    uint64_t avg, p50, p90, p95, p99, p999, min, max;
};

Stats compute(std::vector<uint64_t>& lats) {
    if (lats.empty()) return {0,0,0,0,0,0,0,0};
    std::sort(lats.begin(), lats.end());
    size_t n = lats.size();
    uint64_t sum = 0;
    for (auto v : lats) sum += v;
    return {
        sum / n,
        lats[n * 50 / 100],
        lats[n * 90 / 100],
        lats[n * 95 / 100],
        lats[n * 99 / 100],
        lats[n * 999 / 1000],
        lats.front(),
        lats.back()
    };
}

void print_stats(std::string_view label, const Stats& s) {
    std::cout << "\nResults for " << label << ":\n";
    std::cout << "  avg=" << s.avg << "ns p50=" << s.p50 << "ns p95=" << s.p95
              << "ns p99=" << s.p99 << "ns p999=" << s.p999 << "ns min=" << s.min << "ns max=" << s.max << "ns\n";
}

void bench_uri(const char* uri, size_t warmup, size_t iterations) {
    std::cout << "[Bench] Starting " << uri << "... " << std::flush;
    
    std::string s_uri(uri);
    std::unique_ptr<snap::ILink<BenchMsg>> link;

    std::atomic<bool> listener_running{true};
    std::thread listener_thread;

    bool needs_echo = (s_uri.find("udp://") != std::string::npos || 
                       s_uri.find("tcp://") != std::string::npos ||
                       s_uri.find("ipc://") != std::string::npos);

    if (needs_echo) {
        std::string listen_uri;
        if (s_uri.find("udp://") != std::string::npos) listen_uri = "udp://@" + s_uri.substr(6);
        else if (s_uri.find("tcp://") != std::string::npos) listen_uri = "tcp://@" + s_uri.substr(6);
        else if (s_uri.find("ipc://") != std::string::npos) listen_uri = "ipc://@" + s_uri.substr(6);

        listener_thread = std::thread([listen_uri, &listener_running]() {
            snap::pin_thread(1);
            std::unique_ptr<snap::ILink<BenchMsg>> listener;
            while (listener_running.load(std::memory_order_relaxed) && !listener) {
                listener = snap::connect<BenchMsg>(listen_uri);
                if (!listener) snap::cpu_relax();
            }
            if (!listener) return;
            BenchMsg m;
            while (listener_running.load(std::memory_order_relaxed)) {
                if (listener->recv(m)) {
                    while (!listener->send(m)) { 
                      if (!listener_running.load(std::memory_order_relaxed)) return;
                      snap::cpu_relax(); 
                    }
                } else {
                    snap::cpu_relax();
                }
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        link = snap::connect<BenchMsg>(uri);
    } else {
        link = snap::connect<BenchMsg>(uri);
    }

    if (!link) {
        std::cerr << "FAIL: Could not create link for " << uri << "\n";
        if (listener_thread.joinable()) { listener_running.store(false); listener_thread.join(); }
        return;
    }

    std::vector<uint64_t> lats;
    lats.reserve(iterations);

    for (size_t i = 0; i < warmup; ++i) {
        if (link->send({i, 0})) {
            BenchMsg m; 
            auto start = snap::timestamp_ns();
            while (!link->recv(m)) { 
                if (snap::timestamp_ns() - start > 100'000'000) break; // 100ms timeout
                snap::cpu_relax(); 
            }
        }
    }

    for (size_t i = 0; i < iterations; ++i) {
        uint64_t t0 = snap::timestamp_ns();
        while (!link->send({i, t0})) { snap::cpu_relax(); }
        BenchMsg m; 
        while (!link->recv(m)) { 
            if (snap::timestamp_ns() - t0 > 100'000'000) break; // timeout
            snap::cpu_relax(); 
        }
        lats.push_back(snap::timestamp_ns() - t0);
    }

    if (listener_thread.joinable()) {
        listener_running.store(false);
        listener_thread.join();
    }

    print_stats(uri, compute(lats));
}

int main(int argc, char** argv) {
    std::cout << "Snap v" << snap::VERSION << " Standard Metrics Suite\n";
    std::cout << "=========================================\n";

    if (argc > 1) {
        bench_uri(argv[1], 1000, 10000);
    } else {
        bench_uri("inproc://bench",   10000, 1000000);
        bench_uri("shm://bench",      10000, 1000000);
        bench_uri("ipc:///tmp/b.ipc", 2000,  20000);
        bench_uri("udp://127.0.0.1:9005", 2000, 20000);
        bench_uri("tcp://127.0.0.1:9006", 2000, 20000);
    }

    return 0;
}