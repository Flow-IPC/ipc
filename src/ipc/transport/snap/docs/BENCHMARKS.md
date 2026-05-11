# SNAP BENCHMARKS
**Version 2.0.0**

All measurements are round-trip latency (send + recv) with producer on Core 0 and consumer on Core 1.
**Hardware:** x86-64 Linux, performance governor, no RT scheduling.

---

## INPROC (SPSC RingBuffer)
**Command:** `./snap_bench`

| Percentile | Typical Latency |
| :--- | :--- |
| **Min** | 15–30 ns |
| **Avg** | 35–80 ns |
| **P50** | 30–60 ns |
| **P95** | 80–120 ns |
| **P99** | 120–200 ns |
| **P999** | 300–600 ns |

*Snap is competitive with LMAX Disruptor (~50–100ns) and significantly faster than ZeroMQ (700ns+).*

---

## THROUGHPUT
**Command:** `./snap_bench tput`

*   **Expected:** 150–400 million messages/second (depends on payload size).

---

## OTHER TRANSPORTS

### Shared Memory (SHM)
- **Avg:** 100–400 ns (cross-process)
- **HugePages:** 80–300 ns

### Network (UDP Loopback)
- **Avg:** 1–8 µs
- **SO_BUSY_POLL:** 2–6 µs

---

## METHODOLOGY
1.  **Warmup:** 10,000 iterations discarded.
2.  **Measure:** 1,000,000 iterations timed with `CLOCK_MONOTONIC_RAW`.
3.  **Stats:** Percentiles calculated from exact sorted array.
4.  **Flags:** `-O3 -march=native -flto=auto -fno-exceptions -fno-rtti`.

## Full Test Suite
```bash
./snap_bench          # SPSC latency
./snap_bench tput     # throughput
./snap_spsc_test      # correctness
./snap_mpmc_test      # MPMC correctness
./snap_pool_test      # pool correctness
./snap_dispatch_test  # dispatcher correctness
```
