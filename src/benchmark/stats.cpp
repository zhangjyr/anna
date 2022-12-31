#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

#include "stats.hpp"

void Stats::reset() {
  this->_count = 0;
  memset(this->_data, 0, this->_limit * sizeof(uint64_t));
  this->_min   = UINT64_MAX;
  this->_max   = 0;
  this->_finished = 0;
  this->_start = std::chrono::system_clock::now();
  this->_end = this->_start;
  this->_count_on_end = 0;

  for (auto child: this->_children) {
    child->reset();
  }
}

int Stats::record(uint64_t n) {
    if (n >= this->_limit) return 0;
    __sync_fetch_and_add(&this->_data[n], 1);
    uint64_t fetched = __sync_fetch_and_add(&this->_count, 1);
    if (fetched == 0) {
        this->_start = std::chrono::system_clock::now();
    }
    if (this->_finished == 0) {
      // Once any thread has finished, we will not update this value again 
      this->_count_on_end = fetched + 1; // Accuracy is not important here, we will verify the value on the first thread that finishes.
    }
    uint64_t min = this->_min;
    uint64_t max = this->_max;
    while (n < min) min = __sync_val_compare_and_swap(&this->_min, min, n);
    while (n > max) max = __sync_val_compare_and_swap(&this->_max, max, n);

    if (this->_parent != NULL) {
        this->_parent->record(n);
    }

    return 1;
}

void Stats::add_finished() {
    uint64_t finished = !__sync_fetch_and_add(&this->_finished, 1);
    // If this is the first thread to finish, we will update the end time.
    if (finished) {
        this->_count_on_end = this->_count;
        this->_end = std::chrono::system_clock::now();
        // Notify the children to conclue, call once only.
        for (auto child: this->_children) {
            child->add_finished();
        }
    }
}

uint64_t Stats::num() {
    return this->_count_on_end;
}

uint64_t Stats::min() {
    return this->_min;
}

uint64_t Stats::max() {
    return this->_max;
}

long double Stats::mean() {
    if (this->_count == 0) return 0.0;

    uint64_t sum = 0;
    for (uint64_t i = this->_min; i <= this->_max; i++) {
        sum += this->_data[i] * i;
    }
    return sum / (long double) this->_count;
}

long double Stats::stdev(long double mean) {
    long double sum = 0.0;
    if (this->_count < 2) return 0.0;
    for (uint64_t i = this->_min; i <= this->_max; i++) {
        if (this->_data[i]) {
            sum += powl(i - mean, 2) * this->_data[i];
        }
    }
    return sqrtl(sum / (this->_count - 1));
}

long double Stats::within_stdev(long double mean, long double stdev, uint64_t n) {
    long double upper = mean + (stdev * n);
    long double lower = mean - (stdev * n);
    uint64_t sum = 0;

    for (uint64_t i = this->_min; i <= this->_max; i++) {
        if (i >= lower && i <= upper) {
            sum += this->_data[i];
        }
    }

    return (sum / (long double) this->_count) * 100;
}

uint64_t Stats::percentile(long double p) {
    uint64_t rank = round((p / 100.0) * this->_count + 0.5);
    uint64_t total = 0;
    for (uint64_t i = this->_min; i <= this->_max; i++) {
        total += this->_data[i];
        if (total >= rank) return i;
    }
    return 0;
}

uint64_t Stats::get_finished() {
    return this->_finished;
}

Duration Stats::elapsed() {
    if (this->_finished == 0) {
        return std::chrono::system_clock::now() - this->_start;
    }
    return this->_end - this->_start;
}

long double Stats::throughput() {
    long double count = this->_count_on_end;
    if (this->_finished == 0) {
        count = this->_count;
    }
    return count / std::chrono::duration_cast<std::chrono::milliseconds>(this->elapsed()).count() * 1000;
}

// add_stats adds the child stats.
void Stats::add_stats(Stats *stats) {
    this->_children.push_back(stats);
    stats->_parent = this;
}

// operation[] overrides the [] operator to return the child stats.
Stats* Stats::operator[](int index) {
    return this->_children.at(index);
}

uint64_t Stats::popcount() {
    uint64_t count = 0;
    for (uint64_t i = this->_min; i <= this->_max; i++) {
        if (this->_data[i]) count++;
    }
    return count;
}

uint64_t Stats::value_at(uint64_t index, uint64_t *count) {
    *count = 0;
    for (uint64_t i = this->_min; i <= this->_max; i++) {
        if (this->_data[i] && (*count)++ == index) {
            *count = this->_data[i];
            return i;
        }
    }
    return 0;
}