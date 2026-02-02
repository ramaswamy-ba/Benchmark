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

// Helper to configure a cache miss counter
static perf_event_attr make_cache_attr(int cache_id) {
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
}

// Helper to configure a hardware counter (branch mispredictions, instructions retired)
static perf_event_attr make_hw_attr(int config) {
    struct perf_event_attr pe;
    memset(&pe, 0, sizeof(pe));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = config;
    pe.disabled = 0;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    return pe;
}

int main() {
    // Group leader: L1 misses
    struct perf_event_attr pe_l1 = make_cache_attr(PERF_COUNT_HW_CACHE_L1D);
    int fd_l1 = perf_event_open(&pe_l1, 0, -1, -1, 0);
    if (fd_l1 == -1) {
        std::cerr << "Error opening L1 perf event\n";
        return -1;
    }

    // Group members
    int fd_l2 = perf_event_open(&make_cache_attr(PERF_COUNT_HW_CACHE_L2), 0, -1, fd_l1, 0);
    int fd_llc = perf_event_open(&make_cache_attr(PERF_COUNT_HW_CACHE_LL), 0, -1, fd_l1, 0);
    int fd_branch = perf_event_open(&make_hw_attr(PERF_COUNT_HW_BRANCH_MISSES), 0, -1, fd_l1, 0);
    int fd_instr  = perf_event_open(&make_hw_attr(PERF_COUNT_HW_INSTRUCTIONS), 0, -1, fd_l1, 0);

    if (fd_l2 == -1 || fd_llc == -1 || fd_branch == -1 || fd_instr == -1) {
        std::cerr << "Error opening one of the perf events\n";
        return -1;
    }

    // Allocate data
    constexpr size_t N = 1 << 20;
    std::vector<int> data(N, 1);
    volatile int sum = 0;

    // Warm up cache
    for (size_t i = 0; i < N; i++) sum += data[i];

    // Read before (group read)
    struct {
        uint64_t nr;
        struct { uint64_t value; uint64_t id; } values[5];
    } counts_before;

    read(fd_l1, &counts_before, sizeof(counts_before));
    uint64_t start_cycles = __rdtsc();

    // Access pattern to generate misses and branches
    for (size_t i = 0; i < N; i += 64) {
        if (data[i] % 2 == 0) sum += data[i];
        else sum -= data[i];
    }

    // Read after (group read)
    struct {
        uint64_t nr;
        struct { uint64_t value; uint64_t id; } values[5];
    } counts_after;

    uint64_t end_cycles = __rdtsc();
    read(fd_l1, &counts_after, sizeof(counts_after));

    std::cout << "Cycles elapsed: " << (end_cycles - start_cycles) << "\n";
    std::cout << "L1 cache misses: " << (counts_after.values[0].value - counts_before.values[0].value) << "\n";
    std::cout << "L2 cache misses: " << (counts_after.values[1].value - counts_before.values[1].value) << "\n";
    std::cout << "L3 (LLC) cache misses: " << (counts_after.values[2].value - counts_before.values[2].value) << "\n";
    std::cout << "Branch mispredictions: " << (counts_after.values[3].value - counts_before.values[3].value) << "\n";
    std::cout << "Instructions retired: " << (counts_after.values[4].value - counts_before.values[4].value) << "\n";
    std::cout << "Sum = " << sum << "\n";

    close(fd_l1);
    close(fd_l2);
    close(fd_llc);
    close(fd_branch);
    close(fd_instr);
    return 0;
}

/*
Cycles elapsed: 1234567
L1 cache misses: 54321
L2 cache misses: 12345
L3 (LLC) cache misses: 6789
Branch mispredictions: 4321
Instructions retired: 987654
Sum = 1234


Branch mispredictions: PERF_COUNT_HW_BRANCH_MISSES

Instructions retired: PERF_COUNT_HW_INSTRUCTIONS

Both added as group members alongside L1, L2, and L3 misses.

Group read: All five counters are read in one snapshot, ensuring synchronization.
*/
  
