#pragma once
#include <cstddef>
#include "ring_buffer.hpp"
#include "utils.hpp"

namespace snap {

template<typename MsgType, typename Handler, size_t Cap = 4096>
class Dispatcher {
    static_assert(is_p2(Cap), "Dispatcher capacity must be a power of two");

    RingBuffer<MsgType, Cap> _queue;
    Handler _handler;

public:
    explicit Dispatcher(Handler h = {}) noexcept : _handler(std::move(h)) {}

    void subscribe(Handler h) noexcept { _handler = std::move(h); }

    SNAP_HOT SNAP_FORCE_INLINE bool send(const MsgType& msg) noexcept {
        return _queue.push(msg);
    }

    SNAP_HOT SNAP_FORCE_INLINE void poll() noexcept {
        MsgType msg;
        if (SNAP_LIKELY(_queue.pop(msg))) {
            _handler(msg);
        }
    }

    SNAP_HOT void drain() noexcept {
        MsgType msg;
        while (_queue.pop(msg)) {
            _handler(msg);
        }
    }

    SNAP_HOT size_t drain_n(size_t max) noexcept {
        MsgType msg;
        size_t n = 0;
        while (n < max && _queue.pop(msg)) {
            _handler(msg);
            ++n;
        }
        return n;
    }

    bool empty() const noexcept { return _queue.empty(); }
    size_t size() const noexcept { return _queue.size(); }
};

template<typename MsgType, typename Handler, size_t Cap = 4096>
SNAP_FORCE_INLINE auto make_dispatcher(Handler&& h) {
    return Dispatcher<MsgType, Handler, Cap>(std::forward<Handler>(h));
}

} // namespace snap