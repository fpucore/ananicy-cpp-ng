#include <benchmark/benchmark.h>

#include "utility/synchronized_queue.hpp"

#include <algorithm>
#include <execution>

struct SomeStruct {
  std::uint64_t first;
  std::uint64_t second;
};

static void PushStructData(benchmark::State &state) {
  const int bytes = 1 << state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
#ifdef USE_EXPERIMENTAL_IMPL
    synchronized_queue<SomeStruct> sync_queue{INT16_MAX};
#else
    synchronized_queue<SomeStruct>    sync_queue;
#endif

    std::vector<std::size_t> input_data(state.range(0));
    std::iota(input_data.begin(), input_data.end(), 0);

    state.ResumeTiming();
    std::for_each(std::execution::par_unseq, input_data.begin(), input_data.end(),
                  [&](auto i) {
                    sync_queue.push(SomeStruct{.first = i, .second = i * 2});
                  });
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          long(bytes));
}
BENCHMARK(PushStructData)->DenseRange(13, 26)->ReportAggregatesOnly(1);

static void PushSmallData(benchmark::State &state) {
  const int bytes = 1 << state.range(0);
  for (auto _ : state) {
    state.PauseTiming();
#ifdef USE_EXPERIMENTAL_IMPL
    synchronized_queue<std::uint64_t> sync_queue{INT16_MAX};
#else
    synchronized_queue<std::uint64_t> sync_queue;
#endif

    std::vector<std::size_t> input_data(state.range(0));
    std::iota(input_data.begin(), input_data.end(), 0);

    state.ResumeTiming();
    std::for_each(std::execution::par_unseq, input_data.begin(), input_data.end(),
                  [&](auto i) { sync_queue.push(i); });
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          long(bytes));
}
BENCHMARK(PushSmallData)->DenseRange(13, 26)->ReportAggregatesOnly(1);

BENCHMARK_MAIN();
