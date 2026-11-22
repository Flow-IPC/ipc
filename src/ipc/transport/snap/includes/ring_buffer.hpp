#pragma once
#include "utils.hpp"
#include <atomic>
#include <cstring>

namespace snap {

// I built this using the SPSC (Single Producer Single Consumer) pattern 
// with monotonic cursors. It's the fastest way to pass messages between 
// two pinned threads. No locks, no mutexes, just pure cache-aligned speed.
template<typename T, size_t Cap = 4096>
struct alignas(SNAP_CACHE_LINE) RingBuffer {
    static_assert(is_p2(Cap), "I need capacity to be a power of 2 for fast masking.");

    // Monotonic cursors. I keep them on separate cache lines so 
    // we don't kill performance through false sharing.
    alignas(SNAP_CACHE_LINE) std::atomic<size_t> _h{0}; // Shared head (consumer reads)
    alignas(SNAP_CACHE_LINE) size_t _h_local{0};        // Local head cache (producer writes)

    alignas(SNAP_CACHE_LINE) std::atomic<size_t> _t{0}; // Shared tail (producer writes)
    alignas(SNAP_CACHE_LINE) size_t _t_local{0};        // Local tail cache (consumer reads)

    // Using a mask is way faster than the modulo (%) operator. 
    // It's just a bitwise AND.
    static constexpr size_t MASK = Cap - 1;
    alignas(SNAP_CACHE_LINE) T _buf[Cap];

    // Producer side: I only update the shared tail when we actually push.
    SNAP_HOT bool push(const T& data) noexcept {
        if (SNAP_UNLIKELY(full())) {
            // I refresh the head cache from memory before giving up.
            _h_local = _h.load(std::memory_order_acquire);
            if (full()) return false;
        }
        _buf[_t.load(std::memory_order_relaxed) & MASK] = data;
        _t.store(_t.load(std::memory_order_relaxed) + 1, std::memory_order_release);
        return true;
    }

    // Consumer side: same logic but for head.
    SNAP_HOT bool pop(T& out) noexcept {
        if (SNAP_UNLIKELY(empty())) {
            _t_local = _t.load(std::memory_order_acquire);
            if (empty()) return false;
        }
        out = _buf[_h.load(std::memory_order_relaxed) & MASK];
        _h.store(_h.load(std::memory_order_relaxed) + 1, std::memory_order_release);
        return true;
    }

    // Bulk operations. I use these for batching to save on atomic overhead.
    size_t push_n(const T* s, size_t n) noexcept {
        size_t pushed = 0;
        while (pushed < n && push(s[pushed])) pushed++;
        return pushed;
    }

    size_t pop_n(T* d, size_t n) noexcept {
        size_t popped = 0;
        while (popped < n && pop(d[popped])) popped++;
        return popped;
    }

    // Checking if we're full or empty using local caches first.
    SNAP_FORCE_INLINE bool full() const noexcept {
        return (_t.load(std::memory_order_relaxed) - _h_local) >= Cap;
    }
    SNAP_FORCE_INLINE bool empty() const noexcept {
        return _h.load(std::memory_order_relaxed) == _t_local;
    }
    
    SNAP_FORCE_INLINE size_t size() const noexcept { 
        return _t.load(std::memory_order_relaxed) - _h.load(std::memory_order_relaxed); 
    }
    static constexpr size_t capacity() noexcept { return Cap; }
};

} // namespace snap