#include <numa.h>
#include <numaif.h>
#include <unistd.h>
#include <iostream>
#include <chrono>

using namespace std::chrono;

void benchmark_node(int node, size_t size, size_t iterations) {
    // Allocate memory bound to a specific NUMA node
    void* mem = numa_alloc_onnode(size, node);
    if (!mem) {
        std::cerr << "Failed to allocate on node " << node << "\n";
        return;
    }

    // Touch memory to ensure pages are faulted in
    for (size_t i = 0; i < size; i += 4096) {
        static_cast<char*>(mem)[i] = 0;
    }

    // Benchmark: repeatedly read/write memory
    auto start = high_resolution_clock::now();
    volatile char sink = 0;
    for (size_t it = 0; it < iterations; ++it) {
        for (size_t i = 0; i < size; i += 64) { // stride by cache line
            sink += static_cast<char*>(mem)[i];
        }
    }
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start).count();
    std::cout << "Node " << node << " benchmark: " << duration << " ms\n";

    numa_free(mem, size);
}

int main() {
    size_t size = 64 * 1024 * 1024; // 64 MB
    size_t iterations = 100;

    // Benchmark local node (where this thread runs)
    int cpu = sched_getcpu();
    int local_node = numa_node_of_cpu(cpu);
    benchmark_node(local_node, size, iterations);

    // Benchmark remote node (pick another node if available)
    int remote_node = (local_node == 0) ? 1 : 0;
    benchmark_node(remote_node, size, iterations);

    return 0;
}
