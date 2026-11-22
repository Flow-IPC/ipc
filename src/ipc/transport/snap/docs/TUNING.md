# 🛠 SNAP OS TUNING GUIDE
**Version 3.0.0**

This guide describes the OS-level "voodoo" I use to achieve hardware-limited latency with Snap. These settings are **NOT** required for basic use, but if you're building an HFT engine or a 10Gbps+ hub, I highly recommend following these steps.

---

## 1. 🎯 CPU ISOLATION
The kernel is your enemy during a hot-loop. I always reserve dedicated CPU cores for my Snap worker threads to prevent the OS scheduler from interrupting us.

**In `/etc/default/grub`:**
```bash
# Isolate cores 2 and 3 from the general scheduler
GRUB_CMDLINE_LINUX="isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3"
```
Then run: `sudo update-grub && reboot`

In your Snap code, use `pin_thread(2)` or `pin_thread(3)` to claim these "clean" cores.

---

## ⚡ 2. IRQ AFFINITY
I recommend pinning your Network Interface (NIC) interrupts **AWAY** from your isolated Snap cores. If the NIC fires an interrupt on Core 2 while your Snap worker is processing a packet, you'll see a massive latency spike (jitter).

```bash
# Find your NIC's IRQ (e.g., eth0)
grep eth0 /proc/interrupts | awk '{print $1}' | tr -d ':'

# Force affinity to CPU 0 (bitmask 0x1) to keep our worker cores silent
echo 1 > /proc/irq/<IRQ_NUMBER>/smp_affinity
```

---

## 📑 3. HUGEPAGES (2MB)
I use HugePages to reduce TLB (Translation Lookaside Buffer) misses. For large buffers (like 1GB ShmLink segments), this is mandatory for consistent speed.

```bash
# Allocate 512 x 2MB hugepages (1GB total)
echo 512 > /proc/sys/vm/nr_hugepages

# Persist in /etc/sysctl.conf:
vm.nr_hugepages = 512
```
When building Snap, I use: `cmake .. -DSNAP_ENABLE_HUGEPAGES=ON`

---

## 🛣 4. KERNEL NETWORKING
Add these to `/etc/sysctl.conf` to maximize your socket headroom:

```ini
# I maximize the socket buffer to handle huge bursts
net.core.rmem_max = 536870912
net.core.wmem_max = 536870912
net.core.netdev_max_backlog = 250000

# Enable TCP Fast Open (Snap's TcpLink loves this)
net.ipv4.tcp_fastopen = 3

# Global Busy Poll settings (I set this to 100µs)
net.core.busy_poll = 100
net.core.busy_read = 100
```

---

## 🏃 5. REAL-TIME SCHEDULING
I always run my performance-critical binaries with `SCHED_FIFO`. This tells the Linux kernel "Don't touch this thread until I say so."

```bash
sudo chrt -f 99 ./snap_bench
```

---

## ❄ 6. POWER (C-STATES)
CPU frequency scaling and "Deep Sleep" states are the \#1 cause of random latency. I force the CPU into "Performance" mode to keep it at its max clock speed at all times.

```bash
sudo cpupower frequency-set -g performance
# I also disable C-states in the grub cmdline:
processor.max_cstate=1 intel_idle.max_cstate=0
```

---

## 🛑 7. MSG_ZEROCOPY
I integrated `MSG_ZEROCOPY` support into Snap's network links. This eliminates the user→kernel memory copies. 

*   Requires Linux >= 4.14.
*   Enable with: `cmake .. -DSNAP_ENABLE_ZEROCOPY=ON`
*   Verify NIC support: `ethtool -k eth0 | grep scatter-gather`
