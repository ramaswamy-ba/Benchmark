#include <iostream>
#include <vector>
#include <cstdint>
#include <x86intrin.h>   // for __rdtsc

// Inline assembly wrapper for RDPMC
static inline uint64_t rdpmc(uint32_t ecx) {
    uint32_t a, d;
    asm volatile("rdpmc" : "=a"(a), "=d"(d) : "c"(ecx));
    return ((uint64_t)d << 32) | a;
}

int main() {
    constexpr size_t N = 1 << 20; // 1M integers
    std::vector<int> data(N, 1);

    // Warm up cache
    volatile int sum = 0;
    for (size_t i = 0; i < N; i++) sum += data[i];

    // Read PMC before
    uint64_t start_cycles = __rdtsc();
    uint64_t start_misses = rdpmc(0); // assume PMC0 configured for L1 misses

    // Access pattern to generate cache misses
    for (size_t i = 0; i < N; i += 64) { // stride to skip cache lines
        sum += data[i];
    }

    // Read PMC after
    uint64_t end_cycles = __rdtsc();
    uint64_t end_misses = rdpmc(0);

    std::cout << "Cycles elapsed: " << (end_cycles - start_cycles) << "\n";
    std::cout << "L1 cache misses: " << (end_misses - start_misses) << "\n";
    std::cout << "Sum = " << sum << "\n";

    return 0;
}
/*
PMC setup: You must configure PMC0 to count L1 data cache misses. On Linux, you can do this with perf_event_open or msr-tools.
Example perf command:

bash
perf stat -e L1-dcache-load-misses ./a.out
Or configure PMC0 manually in kernel space to track event 0x03 (L1D cache load misses).

rdpmc(0): Reads the value of PMC0.

ecx=0 → counter index 0.

If you configure PMC1, you’d call rdpmc(1).

Access pattern: The stride loop (i += 64) forces cache line skips, generating misses.

Output: Prints cycles elapsed and L1 cache misses.
*/
