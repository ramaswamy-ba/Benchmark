#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <stdint.h>
#include <string.h>
#include <x86intrin.h>   // for __rdtsc

// perf_event_open syscall wrapper
static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

// Inline assembly wrapper for RDPMC
static inline uint64_t rdpmc(uint32_t ecx) {
    uint32_t a, d;
    asm volatile("rdpmc" : "=a"(a), "=d"(d) : "c"(ecx));
    return ((uint64_t)d << 32) | a;
}

int main() {
    // Configure perf_event_attr for L1 data cache load misses
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(struct perf_event_attr));
    pe.type = PERF_TYPE_HW_CACHE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_CACHE_L1D |
                (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
    pe.disabled = 0;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;

    // Open perf event
    int fd = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd == -1) {
        std::cerr << "Error opening perf event for L1 cache misses\n";
        return -1;
    }

    // Map perf event to PMC index
    uint32_t index;
    if (ioctl(fd, PERF_EVENT_IOC_ID, &index) == -1) {
        std::cerr << "Error getting PMC index\n";
        return -1;
    }

    // Allocate data
    constexpr size_t N = 1 << 20; // 1M integers
    std::vector<int> data(N, 1);
    volatile int sum = 0;

    // Warm up cache
    for (size_t i = 0; i < N; i++) sum += data[i];

    // Read before
    uint64_t start_cycles = __rdtsc();
    uint64_t start_misses = rdpmc(index);

    // Access pattern to generate misses
    for (size_t i = 0; i < N; i += 64) { // stride to skip cache lines
        sum += data[i];
    }

    // Read after
    uint64_t end_cycles = __rdtsc();
    uint64_t end_misses = rdpmc(index);

    std::cout << "Cycles elapsed: " << (end_cycles - start_cycles) << "\n";
    std::cout << "L1 cache misses: " << (end_misses - start_misses) << "\n";
    std::cout << "Sum = " << sum << "\n";

    close(fd);
    return 0;
}

/*
How It Works
perf_event_open: Configures a hardware counter for L1D cache load misses.

PERF_TYPE_HW_CACHE + PERF_COUNT_HW_CACHE_L1D + READ + MISS.

ioctl(PERF_EVENT_IOC_ID): Retrieves the PMC index that Linux assigned.

rdpmc(index): Reads the counter directly from user space.

Access pattern: Stride loop forces cache line skips → generates misses.

Output: Prints elapsed cycles and number of L1 cache misses.
*/
