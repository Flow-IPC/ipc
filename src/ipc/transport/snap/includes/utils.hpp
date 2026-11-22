#pragma once
#include <cstdint>
#include <ctime>
#include <sched.h>
#include <pthread.h>
#include <string_view>
#include <atomic>

#if __has_include(<numa.h>)
#  include <numa.h>
#  define SNAP_HAS_NUMA 1
#endif

namespace snap {

// I've defined these for mechanical sympathy. Aligning to cache lines is 
// the #1 way we stop false sharing and keep the CPU happy.
#define SNAP_CACHE_LINE 64
#define SNAP_FORCE_INLINE __attribute__((always_inline)) inline
#define SNAP_LIKELY(x)   __builtin_expect(!!(x), 1)
#define SNAP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define SNAP_PREFETCH_R(ptr) __builtin_prefetch((ptr), 0, 3)
#define SNAP_PREFETCH_W(ptr) __builtin_prefetch((ptr), 1, 3)
#define SNAP_HOT   __attribute__((hot))
#define SNAP_COLD  __attribute__((cold))
#define SNAP_NODISCARD [[nodiscard]]
#define SNAP_NOINLINE  __attribute__((noinline))
#define SNAP_RESTRICT  __restrict__

// I enforce C++20 because we need those atomic waiting features 
// and constexpr improvements for everything to be this fast.
static_assert(__cplusplus >= 202002L, "Snap requires C++20 or later");

// Sometimes we just need the CPU to take a breather without giving up its time slice.
// I've mapped this to the most efficient architecture-specific instructions.
SNAP_FORCE_INLINE void relax() noexcept {
#if defined(__i386__) || defined(__x86_64__)
    asm volatile("pause" ::: "memory");
#elif defined(__aarch64__)
    asm volatile("isb" ::: "memory");
#elif defined(__arm__)
    asm volatile("yield" ::: "memory");
#elif defined(__riscv)
    asm volatile(".insn i 0x0F, 0, x0, x0, 0x010" ::: "memory");
#else
    asm volatile("" ::: "memory");
#endif
}

// I use this in our spinning loops (like RingBuffer polling).
SNAP_FORCE_INLINE void spin(int count) noexcept {
    for (int i = 0; i < count; ++i) relax();
}

// Low-latency wall-clock. I used CLOCK_MONOTONIC_RAW to avoid 
// any sudden NTP-driven time jumps during our benchmarks.
SNAP_FORCE_INLINE uint64_t ts_ns() noexcept {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1'000'000'000ULL + (uint64_t)ts.tv_nsec;
}

// CPU cycle counter. I recommend using this for hyper-granular profiling.
SNAP_FORCE_INLINE uint64_t cycles() noexcept {
#if defined(__x86_64__)
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    return ts_ns(); // Fallback if we aren't on x86
#endif
}

// Handy for parsing our internal URIs without extra allocations.
constexpr bool starts_with(std::string_view s, std::string_view p) noexcept {
    return s.size() >= p.size() && s.substr(0, p.size()) == p;
}

// I designed this to pin our worker threads to specific cores. 
// Mandatory for sub-microsecond latency.
inline bool pin_thread(int core) noexcept {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &set) == 0;
}

// Helps us find our threads in 'top -H' or 'gdb'.
inline bool set_thread_name(const char* name) noexcept {
    return pthread_setname_np(pthread_self(), name) == 0;
}

// I use this for memory placement. We want threads and their memory 
// to live on the same NUMA node to avoid the QPI/UltraPath penalty.
inline int get_numa(int cpu) noexcept {
#ifdef SNAP_HAS_NUMA
    return numa_available() >= 0 ? ::numa_node_of_cpu(cpu) : 0;
#else
    (void)cpu; return 0;
#endif
}

template<typename T>
constexpr bool is_p2(T v) noexcept { return v > 0 && (v & (v - 1)) == 0; }

template<size_t N>
constexpr size_t next_p2() noexcept {
    size_t v = 1;
    while (v < N) v <<= 1;
    return v;
}

// Atomic helpers I use throughout the codebase to keep things lean.
template<typename T>
SNAP_FORCE_INLINE T load_relaxed(const std::atomic<T>& a) noexcept {
    return a.load(std::memory_order_relaxed);
}

template<typename T>
SNAP_FORCE_INLINE void store_relaxed(std::atomic<T>& a, T v) noexcept {
    a.store(v, std::memory_order_relaxed);
}

} // namespace snap