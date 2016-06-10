//
// Created by dpantele on 5/25/16.
//

#ifndef ACC_ACC_WORKER_STATS_H
#define ACC_ACC_WORKER_STATS_H

#include <thread>
#include "acc_class.h"
#include "boost_filtering_stream.h"
#include "stopwatch.h"
#include <crag/multithreading/SharedQueue.h>

struct MoveStats {

  ClickTimer total_time{};

  std::array<ClickTimer, 5> timers{};
  static constexpr char timers_order[] =
      "graph_construct_time, "
      "harvest_time, "
      "whitehead_reduce_time, "
      "whitehead_extend_time, "
      "conj_normalize_time";

  auto GraphConstructClick() {
    return timers[0].Click();
  }
  auto HarvestClick() {
    return timers[1].Click();
  }
  auto WhiteheadReduceClick() {
    return timers[2].Click();
  }
  auto WhiteheadExtendClick() {
    return timers[3].Click();
  }
  auto ConjNormalizeClick() {
    return timers[4].Click();
  }

  auto TotalClick() {
    return total_time.Click();
  }

  std::array<size_t, 7> num_stats{};
  static constexpr char stats_order[] =
      "graph_size, "
      "graph_modulus, "
      "max_weight, "
      "harvested_pairs, "
      "unique_pairs, "
      "aut_orbits_size, "
      "added_pairs";

  void SetGraphSize(size_t s) {
    num_stats[0] = s;
  }
  void SetGraphModulus(size_t s) {
    num_stats[1] = s;
  }
  void SetGraphMaxWeight(size_t s) {
    num_stats[2] = s;
  }
  void SetHarvestedPairs(size_t s) {
    num_stats[3] = s;
  }
  auto GetHarvestedPairs() const {
    return num_stats[3];
  }
  void SetUniquePairs(size_t s) {
    num_stats[4] = s;
  }
  void SetAutOrbitSize(size_t s) {
    num_stats[5] = s;
  }
  void SetAddedPairs(size_t s) {
    num_stats[6] = s;
  }
  auto GetAddedPairs() const {
    return num_stats[6];
  }
};

struct ACWorkerStats {
  BoostFilteringOStream move_stats_out_;

  ACWorkerStats(const Config& c);
  ACWorkerStats(ACWorkerStats&&)=default;
  ACWorkerStats& operator=(ACWorkerStats&&)=default;
  ~ACWorkerStats();

  template<typename F>
  void Write(std::ostream* to, bool tee_to_stdout, F&& f) {
    fmt::MemoryWriter data;
    f(data);
    write_tasks_.Push(WriteTask{to, std::move(data), tee_to_stdout});
  }

 private:
  struct WriteTask {
    std::ostream* out_ = nullptr;
    fmt::MemoryWriter data_;
    bool tee_to_stdout_ = false;

    WriteTask() = default;

    WriteTask(const WriteTask&)=delete;
    WriteTask& operator=(const WriteTask&)=delete;

    WriteTask& operator=(WriteTask&& other)=default;
    WriteTask(WriteTask&& other)=default;

    WriteTask(std::ostream* out, fmt::MemoryWriter data, bool tee_to_stdout)
        : out_(out)
        , data_(std::move(data))
        , tee_to_stdout_(tee_to_stdout)
    { }
  };

  crag::multithreading::SharedQueue<WriteTask> write_tasks_;
  std::thread writer_;
};

#endif //ACC_ACC_WORKER_STATS_H
