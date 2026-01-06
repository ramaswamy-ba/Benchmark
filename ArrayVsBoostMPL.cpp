#include <benchmark/benchmark.h>
#include <array>
#include <algorithm>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/int.hpp>

// Dummy handler
using HandlerFn = void(*)(int&);
void dummyHandler(int& counter) { ++counter; }

// Generate 50 FIDs
constexpr std::array<int, 50> fid_values = {
    22,25,28,29,1000,9999,101,202,303,404,
    505,606,707,808,909,1111,1234,1500,1600,1700,
    1800,1900,2000,2100,2200,2300,2400,2500,2600,2700,
    2800,2900,3000,3100,3200,3300,3400,3500,3600,3700,
    3800,3900,4000,4100,4200,4300,4400,4500,4600,4700
};

// -------------------- Boost.MPL --------------------
using bid_fids = boost::mpl::vector<
    boost::mpl::int_<22>, boost::mpl::int_<28>, boost::mpl::int_<29>,
    boost::mpl::int_<1000>, boost::mpl::int_<9999>
>;

template<int Fid>
struct is_bid_fid : boost::mpl::contains<bid_fids, boost::mpl::int_<Fid>>::type {};

static void BM_BoostMPL(benchmark::State& state) {
    int counter = 0;
    for (auto _ : state) {
        int fid = fid_values[state.iterations() % fid_values.size()];

        // Runtime check using compile-time trait
        switch(fid) {
            case 22: case 28: case 29: case 1000: case 9999:
                dummyHandler(counter);
                break;
            default:
                break;
        }
        benchmark::DoNotOptimize(counter);
    }
}
BENCHMARK(BM_BoostMPL);

// -------------------- std::array (linear scan) --------------------
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

// -------------------- std::array (binary search) --------------------
constexpr auto sortedHandlers = []{
    auto arr = arrayHandlers;
    std::sort(arr.begin(), arr.end(),
              [](auto& a, auto& b){ return a.first < b.first; });
    return arr;
}();

static void BM_ArrayBinarySearch(benchmark::State& state) {
    int counter = 0;
    for (auto _ : state) {
        int fid = fid_values[state.iterations() % fid_values.size()];
        auto it = std::lower_bound(sortedHandlers.begin(), sortedHandlers.end(), fid,
                                   [](auto& p, int val){ return p.first < val; });
        if (it != sortedHandlers.end() && it->first == fid) it->second(counter);
        benchmark::DoNotOptimize(counter);
    }
}
BENCHMARK(BM_ArrayBinarySearch);

// -------------------- main --------------------
BENCHMARK_MAIN();
