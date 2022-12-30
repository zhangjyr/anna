#ifndef INCLUDE_BENCHMARK_STATS_HPP_
#define INCLUDE_BENCHMARK_STATS_HPP_

#include <stdlib.h>
#include <cstring> // for memset
#include <chrono>

typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> Time;
typedef std::chrono::system_clock::duration Duration;

class Stats {
 private: 
  uint64_t count;
  uint64_t limit;
  uint64_t min;
  uint64_t max;
  uint64_t *data;
  uint64_t finished;
  Time start;
  Time end;
  uint64_t count_on_end;
  
 public:
  Stats(uint64_t max): limit(max+1) {
    this->data = new uint64_t[this->limit];
    this->reset();
  }

  ~Stats() {
    delete[] this->data;
  }

  void reset();

  int record(uint64_t);
  void correct(int64_t);
  void add_finished();

  uint64_t num();
  long double mean();
  long double stdev(long double);
  long double within_stdev(long double, long double, uint64_t);
  uint64_t percentile(long double);
  uint64_t get_finished();
  Duration elapsed();
  long double throughput();

  uint64_t popcount();
  uint64_t value_at(uint64_t, uint64_t *);
};

#endif  // INCLUDE_BENCHMARK_STATS_HPP_
