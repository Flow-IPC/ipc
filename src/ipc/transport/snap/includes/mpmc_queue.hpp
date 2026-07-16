#pragma once
#include "utils.hpp"
#include <atomic>

namespace snap {

/**
 * Multi-Producer Multi-Consumer Bounded Queue.
 * I developed this based on the Rigtorp design. Every slot has its own 
 * sequence number to keep producers and consumers in sync without a global lock.
 */
template<typename T, size_t Cap = 4096>
class MpmcQueue {
    static_assert(is_p2(Cap), "I need capacity to be a power of 2.");

    struct alignas(SNAP_CACHE_LINE) Slot {
        std::atomic<size_t> _seq; // Sequence number for sync
        T _data;                  // The actual payload
    };

    alignas(SNAP_CACHE_LINE) Slot _slots[Cap];
    alignas(SNAP_CACHE_LINE) std::atomic<size_t> _p_pos; // Producer position
    alignas(SNAP_CACHE_LINE) std::atomic<size_t> _c_pos; // Consumer position

    static constexpr size_t MASK = Cap - 1;

public:
    MpmcQueue() : _p_pos(0), _c_pos(0) {
        // I initialize all sequence numbers to their respective index
        for (size_t i = 0; i < Cap; ++i) _slots[i]._seq.store(i, std::memory_order_relaxed);
    }

    // Producer side: I use a CAS loop on the slot sequence 
    // to find the next available spot.
    SNAP_HOT bool push(const T& data) noexcept {
        Slot* s;
        size_t p = _p_pos.load(std::memory_order_relaxed);
        for (;;) {
            s = &_slots[p & MASK];
            size_t seq = s->_seq.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)p;
            if (diff == 0) {
                if (_p_pos.compare_exchange_weak(p, p + 1, std::memory_order_relaxed)) break;
            } else if (diff < 0) {
                return false; // Queue full
            } else {
                p = _p_pos.load(std::memory_order_relaxed);
            }
        }
        s->_data = data;
        s->_seq.store(p + 1, std::memory_order_release);
        return true;
    }

    // Consumer side: same CAS logic to claim the next filled slot.
    SNAP_HOT bool pop(T& out) noexcept {
        Slot* s;
        size_t c = _c_pos.load(std::memory_order_relaxed);
        for (;;) {
            s = &_slots[c & MASK];
            size_t seq = s->_seq.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)(c + 1);
            if (diff == 0) {
                if (_c_pos.compare_exchange_weak(c, c + 1, std::memory_order_relaxed)) break;
            } else if (diff < 0) {
                return false; // Queue empty
            } else {
                c = _c_pos.load(std::memory_order_relaxed);
            }
        }
        out = s->_data;
        s->_seq.store(c + MASK + 1, std::memory_order_release);
        return true;
    }

    SNAP_FORCE_INLINE bool empty() const noexcept {
        return (intptr_t)_p_pos.load(std::memory_order_relaxed) - (intptr_t)_c_pos.load(std::memory_order_relaxed) <= 0;
    }
    
    SNAP_FORCE_INLINE size_t size() const noexcept {
        return _p_pos.load(std::memory_order_relaxed) - _c_pos.load(std::memory_order_relaxed);
    }
    
    static constexpr size_t capacity() noexcept { return Cap; }
};

} // namespace snap
