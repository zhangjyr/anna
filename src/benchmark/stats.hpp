#ifndef INCLUDE_BENCHMARK_STATS_HPP_
#define INCLUDE_BENCHMARK_STATS_HPP_

#include <stdlib.h>
#include <cstring> // for memset
#include <chrono>
#include <vector>

typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::system_clock::duration> Time;
typedef std::chrono::system_clock::duration Duration;

class Stats {
 private: 
  uint64_t _count;
  uint64_t _limit;
  uint64_t _min;
  uint64_t _max;
  uint64_t *_data;        // An array of counters, which stored the histogram of the data.
  uint64_t _finished;     // The number of threads that have finished.
  Time _start;            // The time when the first record is received. We assume all threads start at the same time.
  Time _end;              // The time when the first thread finishes.
  uint64_t _count_on_end; // The throughput will be calculated on this value, which will not be updated after the first thread finishes.
  
  // Add support to hierarchy stats
  std::vector<Stats*> _children;
  Stats *_parent;
  
 public:
  Stats(uint64_t max): _limit(max+1), _parent(NULL) {
    this->_data = new uint64_t[this->_limit];
    this->reset();
  }

  ~Stats() {
    this->_parent = NULL;
    delete[] this->_data;
    for (auto child: this->_children) {
      delete child;
    }
    this->_children.clear();
  }

  void reset();

  int record(uint64_t);
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

  // Hierarchical stats methods.
  void add_stats(Stats *stats);
  Stats* operator[](int index);

  uint64_t popcount();
  uint64_t value_at(uint64_t, uint64_t *);
};

#endif  // INCLUDE_BENCHMARK_STATS_HPP_
