#pragma once
#include <thread>
#include <functional>
#include <atomic>
#include <tuple>
#include "ring_buffer.hpp"
#include "utils.hpp"

namespace snap {

template<typename In, typename Out, size_t Cap = 4096>
struct Stage {
    using input_type  = In;
    using output_type = Out;

    RingBuffer<In,  Cap> inbox;
    RingBuffer<Out, Cap> outbox;
    std::atomic<bool>    running{false};
    std::thread          worker;

    template<typename Fn>
    void start(Fn&& fn, int pin_core = -1) {
        running.store(true, std::memory_order_relaxed);
        worker = std::thread([this, fn = std::forward<Fn>(fn), pin_core]() mutable {
            if (pin_core >= 0) pin_thread(pin_core);
            In  in_msg;
            Out out_msg;
            while (running.load(std::memory_order_acquire)) {
                if (SNAP_LIKELY(inbox.pop(in_msg))) {
                    out_msg = fn(in_msg);
                    while (!outbox.push(out_msg)) { relax(); }
                } else {
                    relax();
                }
            }
        });
    }

    void stop() {
        running.store(false, std::memory_order_release);
        if (worker.joinable()) worker.join();
    }

    ~Stage() { stop(); }
};

template<typename T, size_t Cap = 4096>
struct SinkStage {
    using input_type = T;

    RingBuffer<T, Cap> inbox;
    std::atomic<bool>  running{false};
    std::thread        worker;

    template<typename Fn>
    void start(Fn&& fn, int pin_core = -1) {
        running.store(true, std::memory_order_relaxed);
        worker = std::thread([this, fn = std::forward<Fn>(fn), pin_core]() mutable {
            if (pin_core >= 0) pin_thread(pin_core);
            T msg;
            while (running.load(std::memory_order_acquire)) {
                if (SNAP_LIKELY(inbox.pop(msg))) {
                    fn(msg);
                } else {
                    relax();
                }
            }
        });
    }

    void stop() {
        running.store(false, std::memory_order_release);
        if (worker.joinable()) worker.join();
    }

    ~SinkStage() { stop(); }
};

} // namespace snap
