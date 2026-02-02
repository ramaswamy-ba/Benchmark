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

int main() {
    // Helper lambda to open a cache miss counter
    auto open_cache_miss = [](int cache_id) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(pe));
        pe.type = PERF_TYPE_HW_CACHE;
        pe.size = sizeof(struct perf_event_attr);
        pe.config = cache_id |
                    (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                    (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
        pe.disabled = 0;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;
        return perf_event_open(&pe, 0, -1, -1, 0);
    };

    // Open counters for L1, L2, and LLC (L3)
    int fd_l1 = open_cache_miss(PERF_COUNT_HW_CACHE_L1D);
    int fd_l2 = open_cache_miss(PERF_COUNT_HW_CACHE_L2);
    int fd_llc = open_cache_miss(PERF_COUNT_HW_CACHE_LL);

    if (fd_l1 == -1 || fd_l2 == -1 || fd_llc == -1) {
        std::cerr << "Error opening one of the perf events\n";
        return -1;
    }

    // Allocate data
    constexpr size_t N = 1 << 20; // 1M integers
    std::vector<int> data(N, 1);
    volatile int sum = 0;

    // Warm up cache
    for (size_t i = 0; i < N; i++) sum += data[i];

    // Read before
    uint64_t l1_before, l2_before, llc_before;
    read(fd_l1, &l1_before, sizeof(l1_before));
    read(fd_l2, &l2_before, sizeof(l2_before));
    read(fd_llc, &llc_before, sizeof(llc_before));
    uint64_t start_cycles = __rdtsc();

    // Access pattern to generate misses
    for (size_t i = 0; i < N; i += 64) { // stride to skip cache lines
        sum += data[i];
    }

    // Read after
    uint64_t end_cycles = __rdtsc();
    uint64_t l1_after, l2_after, llc_after;
    read(fd_l1, &l1_after, sizeof(l1_after));
    read(fd_l2, &l2_after, sizeof(l2_after));
    read(fd_llc, &llc_after, sizeof(llc_after));

    std::cout << "Cycles elapsed: " << (end_cycles - start_cycles) << "\n";
    std::cout << "L1 cache misses: " << (l1_after - l1_before) << "\n";
    std::cout << "L2 cache misses: " << (l2_after - l2_before) << "\n";
    std::cout << "L3 (LLC) cache misses: " << (llc_after - llc_before) << "\n";
    std::cout << "Sum = " << sum << "\n";

    close(fd_l1);
    close(fd_l2);
    close(fd_llc);
    return 0;
}
/*
Eg: output
Cycles elapsed: 1234567
L1 cache misses: 54321
L2 cache misses: 12345
L3 (LLC) cache misses: 6789
Sum = 1234

How It Works
Three counters: One each for L1D, L2, and LLC (L3) cache misses.

perf_event_open: Configures counters in per-thread mode (pid=0, cpu=-1).

read(): Returns aggregated counts across all cores for your thread → migration-safe.

__rdtsc(): Measures elapsed cycles for timing.

Stride loop: Forces cache line skips to generate misses.
*/

//Read all together

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

int main() {
    // Helper lambda to configure a cache miss counter
    auto make_attr = [](int cache_id) {
        struct perf_event_attr pe;
        memset(&pe, 0, sizeof(pe));
        pe.type = PERF_TYPE_HW_CACHE;
        pe.size = sizeof(struct perf_event_attr);
        pe.config = cache_id |
                    (PERF_COUNT_HW_CACHE_OP_READ << 8) |
                    (PERF_COUNT_HW_CACHE_RESULT_MISS << 16);
        pe.disabled = 0;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;
        return pe;
    };

    // Open group leader (L1 misses)
    struct perf_event_attr pe_l1 = make_attr(PERF_COUNT_HW_CACHE_L1D);
    int fd_l1 = perf_event_open(&pe_l1, 0, -1, -1, 0);
    if (fd_l1 == -1) {
        std::cerr << "Error opening L1 perf event\n";
        return -1;
    }

    // Open group members (L2 and LLC misses)
    struct perf_event_attr pe_l2 = make_attr(PERF_COUNT_HW_CACHE_L2);
    int fd_l2 = perf_event_open(&pe_l2, 0, -1, fd_l1, 0);

    struct perf_event_attr pe_llc = make_attr(PERF_COUNT_HW_CACHE_LL);
    int fd_llc = perf_event_open(&pe_llc, 0, -1, fd_l1, 0);

    if (fd_l2 == -1 || fd_llc == -1) {
        std::cerr << "Error opening L2/LLC perf events\n";
        return -1;
    }

    // Allocate data
    constexpr size_t N = 1 << 20; // 1M integers
    std::vector<int> data(N, 1);
    volatile int sum = 0;

    // Warm up cache
    for (size_t i = 0; i < N; i++) sum += data[i];

    // Read before (group read)
    struct {
        uint64_t nr;       // number of events
        uint64_t values[3]; // values for L1, L2, LLC
    } counts_before;

    read(fd_l1, &counts_before, sizeof(counts_before));
    uint64_t start_cycles = __rdtsc();

    // Access pattern to generate misses
    for (size_t i = 0; i < N; i += 64) { // stride to skip cache lines
        sum += data[i];
    }

    // Read after (group read)
    struct {
        uint64_t nr;
        uint64_t values[3];
    } counts_after;

    uint64_t end_cycles = __rdtsc();
    read(fd_l1, &counts_after, sizeof(counts_after));

    std::cout << "Cycles elapsed: " << (end_cycles - start_cycles) << "\n";
    std::cout << "L1 cache misses: " << (counts_after.values[0] - counts_before.values[0]) << "\n";
    std::cout << "L2 cache misses: " << (counts_after.values[1] - counts_before.values[1]) << "\n";
    std::cout << "L3 (LLC) cache misses: " << (counts_after.values[2] - counts_before.values[2]) << "\n";
    std::cout << "Sum = " << sum << "\n";

    close(fd_l1);
    close(fd_l2);
    close(fd_llc);
    return 0;
}

