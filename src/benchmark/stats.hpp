#ifndef INCLUDE_BENCHMARK_STATS_HPP_
#define INCLUDE_BENCHMARK_STATS_HPP_

#include <stdlib.h>
#include <cstring> // for memset
#include <chrono>

typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> Time;
typedef std::chrono::system_clock::duration Duration;

class Stats {
 private: 
  uint64_t _count;
  uint64_t _limit;
  uint64_t _min;
  uint64_t _max;
  uint64_t *_data;
  uint64_t _finished;
  Time _start;
  Time _end;
  uint64_t _count_on_end;
  
 public:
  Stats(uint64_t max): _limit(max+1) {
    this->_data = new uint64_t[this->_limit];
    this->reset();
  }

  ~Stats() {
    delete[] this->_data;
  }

  void reset();

  int record(uint64_t);
  void correct(int64_t);
  void add_finished();

  uint64_t num();
  uint64_t min();
  uint64_t max();
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
