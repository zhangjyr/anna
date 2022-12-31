//  Copyright 2019 U.C. Berkeley RISE Lab
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include <stdlib.h>

#include "benchmark.pb.h"
#include "client/kvs_client.hpp"
#include "kvs_threads.hpp"
#include "yaml-cpp/yaml.h"
#include "stats.hpp"

unsigned kBenchmarkThreadNum;
unsigned kRoutingThreadCount;
unsigned kDefaultLocalReplication;

ZmqUtil zmq_util;
ZmqUtilInterface *kZmqUtil = &zmq_util;

const int READ_STATS = 0;
const int UPDATE_STATS = 1;

double get_base(unsigned N, double skew) {
  double base = 0;
  for (unsigned k = 1; k <= N; k++) {
    base += pow(k, -1 * skew);
  }
  return (1 / base);
}

double get_zipf_prob(unsigned rank, double skew, double base) {
  return pow(rank, -1 * skew) / base;
}

void receive(KvsClientInterface *client) {
  vector<KeyResponse> responses = client->receive_async();
  while (responses.size() == 0) {
    responses = client->receive_async();
  }
}

int sample(int n, unsigned &seed, double base,
           map<unsigned, double> &sum_probs) {
  double z;           // Uniform random number (0 < z < 1)
  int zipf_value;     // Computed exponential value to be returned
  int i;              // Loop counter
  int low, high, mid; // Binary-search bounds

  // Pull a uniform random number (0 < z < 1)
  do {
    z = rand_r(&seed) / static_cast<double>(RAND_MAX);
  } while ((z == 0) || (z == 1));

  // Map z to the value
  low = 1, high = n;

  do {
    mid = floor((low + high) / 2);
    if (sum_probs[mid] >= z && sum_probs[mid - 1] < z) {
      zipf_value = mid;
      break;
    } else if (sum_probs[mid] >= z) {
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  } while (low <= high);

  // Assert that zipf_value is between 1 and N
  assert((zipf_value >= 1) && (zipf_value <= n));

  return zipf_value;
}

string generate_key(unsigned n) {
  return string(8 - std::to_string(n).length(), '0') + std::to_string(n);
}

void run_control(const unsigned &thread_id, const Address &ip, Stats &latency) {
  string log_file = "bench_log.txt";
  string logger_name = "benchmark_log";
  auto log = spdlog::basic_logger_mt(logger_name, log_file, true);
  log->flush_on(spdlog::level::info);

  // responsible for pulling benchmark commands
  zmq::context_t context(1);
  SocketCache pushers(&context, ZMQ_PUSH);
  zmq::socket_t command_puller(context, ZMQ_PULL);
  command_puller.bind("tcp://*:" +
                      std::to_string(thread_id + kBenchmarkCommandPort));

  vector<zmq::pollitem_t> pollitems = {
      {static_cast<void *>(command_puller), 0, ZMQ_POLLIN, 0}};

  while (true) {
    kZmqUtil->poll(-1, &pollitems);

    if (pollitems[0].revents & ZMQ_POLLIN) {
      string msg = kZmqUtil->recv_string(&command_puller);
      log->info("Received benchmark command: {}", msg);

      vector<string> v;
      split(msg, ':', v);
      string mode = v[0];

      if (mode == "STATS") {
        string type = "";
        if (v.size() > 1) {
          type = v[1];
        }
        
        if (type == "RESET") {
          log->info("Stats: reset.");
          latency.reset();
        } else if (type == "FINISHED") {
          log->info("Stats: finished {}", latency.get_finished());
        } else if (type == "LATENCY") {
          log->info("Stats: average latency {}ms", latency.mean());
        } else if (type == "THROUGHPUT") {
          log->info("Stats: Throughput {}ops", latency.throughput());
        } else if (type == "WAITDONE") {
          string expected_threads = v[2];
          string output = "";
          if (v.size() < 4) {
            log->info("Stats: waiting for {} threads to finish.", expected_threads);
          } else {
            output = v[3];
            log->info("Stats: waiting for {} threads to finish and output data to {}.", expected_threads, output);
          }

          // Wait finish
          while (latency.get_finished() < std::stoi(expected_threads)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
          }
          log->info("Stats: all threads({}) finished.", latency.get_finished());
          
          if (output != "") {
            // Output YCSB compatible summary data
            auto pattern = "%^%v%$";  // %^ and %$ are used to set color
            auto writer = spdlog::basic_logger_mt("output", output, true);
            auto formatter = std::make_shared<spdlog::pattern_formatter>(pattern);
            writer->set_formatter(formatter);
            writer->flush_on(spdlog::level::info);

            Stats *outputing = &latency;
            writer->info("[OVERALL], Runtime(ms), {}", std::chrono::duration_cast<std::chrono::milliseconds>(latency.elapsed()).count());
            writer->info("[OVERALL], Throughput(ops/sec), {}", latency.throughput());
            // Overall statistics
            writer->info("[OVERALL], Total Operations, {}", outputing->num());
            writer->info("[OVERALL], Average, {}", outputing->mean());
            writer->info("[OVERALL], Min, {}", outputing->min());
            writer->info("[OVERALL], Max, {}", outputing->max());
            writer->info("[OVERALL], p1, {}", outputing->percentile(1));
            writer->info("[OVERALL], p5, {}", outputing->percentile(5));
            writer->info("[OVERALL], p50, {}", outputing->percentile(50));
            writer->info("[OVERALL], p90, {}", outputing->percentile(90));
            writer->info("[OVERALL], p95, {}", outputing->percentile(95));
            writer->info("[OVERALL], p99, {}", outputing->percentile(99));
            writer->info("[OVERALL], p99.9, {}", outputing->percentile(99.9));
            writer->info("[OVERALL], p99.99, {}", outputing->percentile(99.99));
            writer->info("[OVERALL], Return=OK, {}", outputing->num());
            // Read statistics
            outputing = latency[READ_STATS];
            writer->info("[READ], Total Operations, {}", outputing->num());
            writer->info("[READ], Average, {}", outputing->mean());
            writer->info("[READ], Min, {}", outputing->min());
            writer->info("[READ], Max, {}", outputing->max());
            writer->info("[READ], p1, {}", outputing->percentile(1));
            writer->info("[READ], p5, {}", outputing->percentile(5));
            writer->info("[READ], p50, {}", outputing->percentile(50));
            writer->info("[READ], p90, {}", outputing->percentile(90));
            writer->info("[READ], p95, {}", outputing->percentile(95));
            writer->info("[READ], p99, {}", outputing->percentile(99));
            writer->info("[READ], p99.9, {}", outputing->percentile(99.9));
            writer->info("[READ], p99.99, {}", outputing->percentile(99.99));
            writer->info("[READ], Return=OK, {}", outputing->num());
            // Write statistics
            outputing = latency[UPDATE_STATS];
            writer->info("[UPDATE], Total Operations, {}", outputing->num());
            writer->info("[UPDATE], Average, {}", outputing->mean());
            writer->info("[UPDATE], Min, {}", outputing->min());
            writer->info("[UPDATE], Max, {}", outputing->max());
            writer->info("[UPDATE], p1, {}", outputing->percentile(1));
            writer->info("[UPDATE], p5, {}", outputing->percentile(5));
            writer->info("[UPDATE], p50, {}", outputing->percentile(50));
            writer->info("[UPDATE], p90, {}", outputing->percentile(90));
            writer->info("[UPDATE], p95, {}", outputing->percentile(95));
            writer->info("[UPDATE], p99, {}", outputing->percentile(99));
            writer->info("[UPDATE], p99.9, {}", outputing->percentile(99.9));
            writer->info("[UPDATE], p99.99, {}", outputing->percentile(99.99));
            writer->info("[UPDATE], Return=OK, {}", outputing->num());

            log->info("Stats: outputed");
          }
        } else {
          unsigned elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(latency.elapsed()).count();
          log->info("Stats: {} finished, avg latency {}ms, throughput {}ops, elasped: {}s.", latency.num(), latency.mean(), latency.throughput(), elapsed_ms / 1000.0);
        }
      } else {
        log->info("{} is an invalid mode.", mode);
      }
    }
  }
}

void run(const unsigned &thread_id,
         const vector<UserRoutingThread> &routing_threads,
         const vector<MonitoringThread> &monitoring_threads,
         const Address &ip,
         Stats &latency) {
  KvsClient client(routing_threads, ip, thread_id, 10000);
  string log_file = "bench_log_" + std::to_string(thread_id) + ".txt";
  string logger_name = "benchmark_log_" + std::to_string(thread_id);
  auto log = spdlog::basic_logger_mt(logger_name, log_file, true);
  log->flush_on(spdlog::level::info);

  client.set_logger(log);
  unsigned seed = client.get_seed();

  // observed per-key avg latency
  map<Key, std::pair<double, unsigned>> observed_latency;

  // responsible for pulling benchmark commands
  zmq::context_t &context = *(client.get_context());
  SocketCache pushers(&context, ZMQ_PUSH);
  zmq::socket_t command_puller(context, ZMQ_PULL);
  command_puller.bind("tcp://*:" +
                      std::to_string(thread_id + kBenchmarkCommandPort));

  vector<zmq::pollitem_t> pollitems = {
      {static_cast<void *>(command_puller), 0, ZMQ_POLLIN, 0}};

  while (true) {
    kZmqUtil->poll(-1, &pollitems);

    if (pollitems[0].revents & ZMQ_POLLIN) {
      string msg = kZmqUtil->recv_string(&command_puller);
      log->info("Received benchmark command: {}", msg);

      vector<string> v;
      split(msg, ':', v);
      string mode = v[0];

      if (mode == "CACHE") {
        unsigned num_keys = stoi(v[1]);
        // warm up cache
        client.clear_cache();
        auto warmup_start = std::chrono::system_clock::now();

        for (unsigned i = 1; i <= num_keys; i++) {
          if (i % 50000 == 0) {
            log->info("Warming up cache for key {}.", i);
          }

          client.get_async(generate_key(i));
        }

        auto warmup_time = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now() - warmup_start)
                               .count();
        log->info("Cache warm-up took {} seconds.", warmup_time);
      } else if (mode == "LOAD") {
        string type = v[1];
        unsigned num_keys = stoi(v[2]);
        unsigned length = stoi(v[3]);
        unsigned report_period = stoi(v[4]);
        unsigned time = stoi(v[5]);
        double zipf = stod(v[6]);
        double read_proportion, update_proportion = 0.0;

        // Parse type that has "|" into READ/UPDAT proportions
        if (type.find('|') != string::npos) {
          // Split the string into parts using "|" as the delimiter
          std::stringstream ss(type);
          string part;
          if (getline(ss, part, '|')) {
              // First segment: read proportion
              read_proportion = stod(part);
          }
          update_proportion = 100.0 - read_proportion;
          log->info("Workload proportions READs:{}, UPDATEs:{}.", read_proportion, update_proportion);
        }

        map<unsigned, double> sum_probs;
        double base;

        if (zipf > 0) {
          log->info("Zipf coefficient is {}.", zipf);
          base = get_base(num_keys, zipf);
          sum_probs[0] = 0;

          for (unsigned i = 1; i <= num_keys; i++) {
            sum_probs[i] = sum_probs[i - 1] + base / pow((double)i, zipf);
          }
        } else {
          log->info("Using a uniform random distribution.");
        }

        size_t count = 0;
        auto benchmark_start = std::chrono::system_clock::now();
        auto benchmark_end = std::chrono::system_clock::now();
        auto epoch_start = std::chrono::system_clock::now();
        auto epoch_end = std::chrono::system_clock::now();
        auto request_start = std::chrono::system_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::seconds>(
                              benchmark_end - benchmark_start)
                              .count();
        unsigned epoch = 1;

        while (true) {
          unsigned k;
          if (zipf > 0) {
            k = sample(num_keys, seed, base, sum_probs);
          } else {
            k = rand_r(&seed) % (num_keys) + 1;
          }

          Key key = generate_key(k);

          request_start = std::chrono::system_clock::now();
          bool is_read = false;

          if (type == "G") {
            is_read = true;
            client.get_async(key);
            receive(&client);
            count += 1;
          } else if (type == "P") {
            unsigned ts = generate_timestamp(thread_id);
            LWWPairLattice<string> val(
                TimestampValuePair<string>(ts, string(length, 'a')));

            client.put_async(key, serialize(val), LatticeType::LWW);
            receive(&client);
            count += 1;
          } else if (type == "M") {
            is_read = true;
            auto req_start = std::chrono::system_clock::now();
            unsigned ts = generate_timestamp(thread_id);
            LWWPairLattice<string> val(
                TimestampValuePair<string>(ts, string(length, 'a')));

            client.put_async(key, serialize(val), LatticeType::LWW);
            receive(&client);
            client.get_async(key);
            receive(&client);
            count += 2;

            auto req_end = std::chrono::system_clock::now();

            double key_latency =
                (double)std::chrono::duration_cast<std::chrono::microseconds>(
                    req_end - req_start)
                    .count() /
                2;

            if (observed_latency.find(key) == observed_latency.end()) {
              observed_latency[key].first = key_latency;
              observed_latency[key].second = 1;
            } else {
              observed_latency[key].first =
                  (observed_latency[key].first * observed_latency[key].second +
                   key_latency) /
                  (observed_latency[key].second + 1);
              observed_latency[key].second += 1;
            }
          } else if (read_proportion > 0 || update_proportion > 0) {
            // Add support to READ/UPDATE proportion
            double z = rand_r(&seed) % 100;
            if (z < read_proportion) {
              // Read
              is_read = true;
              client.get_async(key);
              receive(&client);
              count += 1;
            } else {
              // Update
              unsigned ts = generate_timestamp(thread_id);
              LWWPairLattice<string> val(
                  TimestampValuePair<string>(ts, string(length, 'a')));

              client.put_async(key, serialize(val), LatticeType::LWW);
              receive(&client);
              count += 1;
            }
          } else {
            log->info("{} is an invalid request type.", type);
          }

          epoch_end = std::chrono::system_clock::now();
          auto request_elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                        epoch_end - request_start)
                                        .count();
          if (is_read) {
            latency[READ_STATS]->record(request_elapsed_us);
          } else {
            latency[UPDATE_STATS]->record(request_elapsed_us);
          }
          auto time_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                  epoch_end - epoch_start)
                                  .count();

          // report throughput every report_period seconds
          if (time_elapsed >= report_period) {
            double throughput = (double)count / (double)time_elapsed;
            log->info("[Epoch {}] Throughput is {} ops/seconds.", epoch,
                      throughput);
            epoch += 1;

            auto latency = (double)1000000 / throughput;
            UserFeedback feedback;

            feedback.set_uid(ip + ":" + std::to_string(thread_id));
            feedback.set_latency(latency);
            feedback.set_throughput(throughput);

            for (const auto &key_latency_pair : observed_latency) {
              if (key_latency_pair.second.first > 1) {
                UserFeedback_KeyLatency *kl = feedback.add_key_latency();
                kl->set_key(key_latency_pair.first);
                kl->set_latency(key_latency_pair.second.first);
              }
            }

            string serialized_latency;
            feedback.SerializeToString(&serialized_latency);

            for (const MonitoringThread &thread : monitoring_threads) {
              kZmqUtil->send_string(
                  serialized_latency,
                  &pushers[thread.feedback_report_connect_address()]);
            }

            count = 0;
            observed_latency.clear();
            epoch_start = std::chrono::system_clock::now();
          }

          benchmark_end = std::chrono::system_clock::now();
          total_time = std::chrono::duration_cast<std::chrono::seconds>(
                           benchmark_end - benchmark_start)
                           .count();
          if (total_time > time) {
            break;
          }
        }

        log->info("Finished");
        UserFeedback feedback;

        feedback.set_uid(ip + ":" + std::to_string(thread_id));
        feedback.set_finish(true);

        string serialized_latency;
        feedback.SerializeToString(&serialized_latency);

        for (const MonitoringThread &thread : monitoring_threads) {
          kZmqUtil->send_string(
              serialized_latency,
              &pushers[thread.feedback_report_connect_address()]);
        }
      } else if (mode == "WARM") {
        unsigned num_keys = stoi(v[1]);
        unsigned length = stoi(v[2]);
        unsigned total_threads = stoi(v[3]);
        unsigned range = num_keys / total_threads;
        unsigned start = thread_id * range + 1;
        unsigned end = thread_id * range + 1 + range;

        Key key;
        auto warmup_start = std::chrono::system_clock::now();

        for (unsigned i = start; i < end; i++) {
          if (i % 100 == 0) {
            log->info("Creating key {}.", i);
          }

          unsigned ts = generate_timestamp(thread_id);
          LWWPairLattice<string> val(
              TimestampValuePair<string>(ts, string(length, 'a')));

          client.put_async(generate_key(i), serialize(val), LatticeType::LWW);
          receive(&client);
        }

        auto warmup_time = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now() - warmup_start)
                               .count();
        log->info("Warming up data took {} seconds.", warmup_time);
      } else {
        log->info("{} is an invalid mode.", mode);
      }

      // Mark finishing of the command
      latency.add_finished();
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc > 2) {
    std::cerr << "Usage: " << argv[0] << " [benchmark_threads]" << std::endl;
    return 1;
  }

  // initialize stats
  int slots = 10000000; // 10s. Percentile slots should be aligned with timeouts in us
  Stats latency = Stats(slots); 
  latency.add_stats(new Stats(slots)); // reads
  latency.add_stats(new Stats(slots)); // updates

  // read the YAML conf
  YAML::Node conf = YAML::LoadFile("conf/anna-config.yml");
  YAML::Node user = conf["user"];
  Address ip = user["ip"].as<string>();

  vector<MonitoringThread> monitoring_threads;
  vector<Address> routing_ips;

  YAML::Node monitoring = user["monitoring"];
  for (const YAML::Node &node : monitoring) {
    monitoring_threads.push_back(MonitoringThread(node.as<Address>()));
  }

  YAML::Node threads = conf["threads"];
  kRoutingThreadCount = threads["routing"].as<int>();
  kBenchmarkThreadNum = threads["benchmark"].as<int>();
  if (argc > 1) {
    kBenchmarkThreadNum = atoi(argv[1]);
  }
  kDefaultLocalReplication = conf["replication"]["local"].as<unsigned>();

  vector<std::thread> benchmark_threads;

  if (YAML::Node elb = user["routing-elb"]) {
    routing_ips.push_back(elb.as<string>());
  } else {
    YAML::Node routing = user["routing"];

    for (const YAML::Node &node : routing) {
      routing_ips.push_back(node.as<Address>());
    }
  }

  vector<UserRoutingThread> routing_threads;
  for (const Address &ip : routing_ips) {
    for (unsigned i = 0; i < kRoutingThreadCount; i++) {
      routing_threads.push_back(UserRoutingThread(ip, i));
    }
  }

  // NOTE: We create a new client for every single thread.
  for (unsigned thread_id = 0; thread_id < kBenchmarkThreadNum; thread_id++) {
    benchmark_threads.push_back(
        std::thread(run, thread_id+1, routing_threads, monitoring_threads, ip, std::ref(latency)));
  }

  // Start the control threads.
  run_control(0, ip, latency);
}
