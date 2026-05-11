#pragma once
#include "ring_buffer.hpp"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>

namespace snap {

/**
 * Shared Memory Communication Link.
 * I built this using a file-backed mmap'd RingBuffer. It's the absolute 
 * fastest way to talk between two different processes on the same machine.
 */
template<typename T, size_t Cap = 65536>
class ShmLink final : public ILink<T> {
    RingBuffer<T, Cap>* _rb = nullptr;
    std::string _name;
    size_t _sz = 0;
    bool _owner = false;

public:
    ShmLink(const char* name) : _name(name) {
        _sz = sizeof(RingBuffer<T, Cap>);
        int fd = shm_open(_name.c_str(), O_RDWR | O_CREAT, 0666);
        if (fd < 0) return;

        // I resize the SHM segment to our RingBuffer size
        ftruncate(fd, _sz);
        void* ptr = mmap(nullptr, _sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (ptr == MAP_FAILED) return;
        _rb = static_cast<RingBuffer<T, Cap>*>(ptr);
        
        // I use mlock so the OS doesn't swap our buffer to disk
        mlock(_rb, _sz);
        madvise(_rb, _sz, MADV_WILLNEED | MADV_HUGEPAGE);
    }

    ~ShmLink() {
        if (_rb) {
            munlock(_rb, _sz);
            munmap(_rb, _sz);
        }
        // I only unlink if we're the one who created it
        if (_owner) shm_unlink(_name.c_str());
    }

    // Direct access to our RingBuffer methods. Super low latency.
    SNAP_HOT bool send(const T& m) noexcept override { return _rb && _rb->push(m); }
    SNAP_HOT bool recv(T& m) noexcept override       { return _rb && _rb->pop(m);  }
    
    // Batch operations are great for throughput
    size_t send_n(const T* msgs, size_t n) noexcept { return _rb ? _rb->push_n(msgs, n) : 0; }
    size_t recv_n(T* msgs, size_t n) noexcept       { return _rb ? _rb->pop_n(msgs, n) : 0;  }

    void set_owner(bool own) { _owner = own; }
    SNAP_FORCE_INLINE size_t size() const noexcept { return _rb ? _rb->size() : 0; }
};

} // namespace snap