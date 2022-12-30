#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

#include "stats.hpp"

void Stats::reset() {
  this->count = 0;
  memset(this->data, 0, this->limit * sizeof(uint64_t));
  this->min   = UINT64_MAX;
  this->max   = 0;
  this->finished = 0;
  this->start = std::chrono::system_clock::now();
  this->end = this->start;
  this->count_on_end = 0;
}

int Stats::record(uint64_t n) {
    if (n >= this->limit) return 0;
    __sync_fetch_and_add(&this->data[n], 1);
    uint64_t first = !__sync_fetch_and_add(&this->count, 1);
    if (first) {
        this->start = std::chrono::system_clock::now();
    }
    if (this->finished == 0) {
      __sync_fetch_and_add(&this->count_on_end, 1);
    }
    uint64_t min = this->min;
    uint64_t max = this->max;
    while (n < min) min = __sync_val_compare_and_swap(&this->min, min, n);
    while (n > max) max = __sync_val_compare_and_swap(&this->max, max, n);
    return 1;
}

void Stats::correct(int64_t expected) {
    for (uint64_t n = expected * 2; n <= this->max; n++) {
        uint64_t count = this->data[n];
        int64_t m = (int64_t) n - expected;
        while (count && m > expected) {
            this->data[m] += count;
            this->count += count;
            m -= expected;
        }
    }
}

void Stats::add_finished() {
    uint64_t finished = !__sync_fetch_and_add(&this->finished, 1);
     if (finished) {
        this->end = std::chrono::system_clock::now();
    }
}

uint64_t Stats::num() {
    return this->count;
}

long double Stats::mean() {
    if (this->count == 0) return 0.0;

    uint64_t sum = 0;
    for (uint64_t i = this->min; i <= this->max; i++) {
        sum += this->data[i] * i;
    }
    return sum / (long double) this->count;
}

long double Stats::stdev(long double mean) {
    long double sum = 0.0;
    if (this->count < 2) return 0.0;
    for (uint64_t i = this->min; i <= this->max; i++) {
        if (this->data[i]) {
            sum += powl(i - mean, 2) * this->data[i];
        }
    }
    return sqrtl(sum / (this->count - 1));
}

long double Stats::within_stdev(long double mean, long double stdev, uint64_t n) {
    long double upper = mean + (stdev * n);
    long double lower = mean - (stdev * n);
    uint64_t sum = 0;

    for (uint64_t i = this->min; i <= this->max; i++) {
        if (i >= lower && i <= upper) {
            sum += this->data[i];
        }
    }

    return (sum / (long double) this->count) * 100;
}

uint64_t Stats::percentile(long double p) {
    uint64_t rank = round((p / 100.0) * this->count + 0.5);
    uint64_t total = 0;
    for (uint64_t i = this->min; i <= this->max; i++) {
        total += this->data[i];
        if (total >= rank) return i;
    }
    return 0;
}

uint64_t Stats::get_finished() {
    return this->finished;
}

Duration Stats::elapsed() {
    if (this->finished == 0) {
        return std::chrono::system_clock::now() - this->start;
    }
    return this->end - this->start;
}

long double Stats::throughput() {
    long double count = this->count_on_end
    if (this->finished == 0) {
        count = this->count;
    }
    return count / std::chrono::duration_cast<std::chrono::milliseconds>(this->elapsed()).count() * 1000;
}

uint64_t Stats::popcount() {
    uint64_t count = 0;
    for (uint64_t i = this->min; i <= this->max; i++) {
        if (this->data[i]) count++;
    }
    return count;
}

uint64_t Stats::value_at(uint64_t index, uint64_t *count) {
    *count = 0;
    for (uint64_t i = this->min; i <= this->max; i++) {
        if (this->data[i] && (*count)++ == index) {
            *count = this->data[i];
            return i;
        }
    }
    return 0;
}