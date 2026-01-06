#include <benchmark/benchmark.h>
#include <array>
#include <unordered_map>
#include <boost/container/flat_map.hpp>
#include <algorithm>
#include <random>

// Abseil
#include "absl/container/flat_hash_map.h"

// Robin Hood
#include "robin_hood.h"

// Dummy handler function
using HandlerFn = void(*)(int&);
void dummyHandler(int& counter) { ++counter; }

// Generate 50 FIDs (sparse values)
constexpr std::array<int, 50> fid_values = {
    22,25,28,29,1000,9999,101,202,303,404,
    505,606,707,808,909,1111,1234,1500,1600,1700,
    1800,1900,2000,2100,2200,2300,2400,2500,2600,2700,
    2800,2900,3000,3100,3200,3300,3400,3500,3600,3700,
    3800,3900,4000,4100,4200,4300,4400,4500,4600,4700
};

// -------------------- std::array --------------------
constexpr std::array<std::pair<int, HandlerFn>, fid_values.size()> arrayHandlers = []{
    std::array<std::pair<int, HandlerFn>, fid_values.size()> arr{};
    for (size_t i=0; i<fid_values.size(); ++i)
        arr[i] = {fid_values[i], dummyHandler};
    return arr;
}();

static void BM_ArrayLookup(benchmark::State& state) {
    int counter = 0;
    for (auto _ : state) {
        int fid = fid_values[state.iterations() % fid_values.size()];
        auto it = std::find_if(arrayHandlers.begin(), arrayHandlers.end(),
                               [fid](auto& p){ return p.first == fid; });
        if (it != arrayHandlers.end()) it->second(counter);
        benchmark::DoNotOptimize(counter);
    }
}
BENCHMARK(BM_ArrayLookup);

// -------------------- boost::flat_map --------------------
static void BM_FlatMapLookup(benchmark::State& state) {
    boost::container::flat_map<int, HandlerFn> fm;
    for (auto fid : fid_values) fm.emplace(fid, dummyHandler);

    int counter = 0;
    for (auto _ : state) {
        int fid = fid_values[state.iterations() % fid_values.size()];
        auto it = fm.find(fid);
        if (it != fm.end()) it->second(counter);
        benchmark::DoNotOptimize(counter);
    }
}
BENCHMARK(BM_FlatMapLookup);

// -------------------- std::unordered_map --------------------
static void BM_UnorderedMapLookup(benchmark::State& state) {
    std::unordered_map<int, HandlerFn> um;
    for (auto fid : fid_values) um.emplace(fid, dummyHandler);

    int counter = 0;
    for (auto _ : state) {
        int fid = fid_values[state.iterations() % fid_values.size()];
        auto it = um.find(fid);
        if (it != um.end()) it->second(counter);
        benchmark::DoNotOptimize(counter);
    }
}
BENCHMARK(BM_UnorderedMapLookup);

// -------------------- absl::flat_hash_map --------------------
static void BM_AbslFlatHashMapLookup(benchmark::State& state) {
    absl::flat_hash_map<int, HandlerFn> am;
    for (auto fid : fid_values) am.emplace(fid, dummyHandler);

    int counter = 0;
    for (auto _ : state) {
        int fid = fid_values[state.iterations() % fid_values.size()];
        auto it = am.find(fid);
        if (it != am.end()) it->second(counter);
        benchmark::DoNotOptimize(counter);
    }
}
BENCHMARK(BM_AbslFlatHashMapLookup);

// -------------------- robin_hood::unordered_map --------------------
static void BM_RobinHoodLookup(benchmark::State& state) {
    robin_hood::unordered_map<int, HandlerFn> rh;
    for (auto fid : fid_values) rh.emplace(fid, dummyHandler);

    int counter = 0;
    for (auto _ : state) {
        int fid = fid_values[state.iterations() % fid_values.size()];
        auto it = rh.find(fid);
        if (it != rh.end()) it->second(counter);
        benchmark::DoNotOptimize(counter);
    }
}
BENCHMARK(BM_RobinHoodLookup);

// -------------------- main --------------------
BENCHMARK_MAIN();
