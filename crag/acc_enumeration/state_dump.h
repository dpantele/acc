//
// Created by dpantele on 5/25/16.
//

#ifndef ACC_ACC_CLASS_LOGGER_H
#define ACC_ACC_CLASS_LOGGER_H

#include <thread>
#include "acc_class.h"
#include "boost_filtering_stream.h"
#include <crag/multithreading/SharedQueue.h>

struct ACStateDump {
  BoostFilteringOStream classes_merges_out_;
  BoostFilteringOStream classes_minimums_out_;
  BoostFilteringOStream ac_graph_vertex_harvest_;
  BoostFilteringOStream ac_graph_edges_;

  BoostFilteringOStream pairs_queue_;

  BoostFilteringOStream ac_pair_class_;

  ACStateDump(const Config& c);
  ACStateDump(ACStateDump&&)=default;
  ACStateDump& operator=(ACStateDump&&)=default;
  ~ACStateDump();

  void Merge(const ACClass&, const ACClass&);

  static void DumpPair(const ACPair& p, std::ostream* out);
  static void DumpPair(const ACPair& p, fmt::MemoryWriter* out);

  static constexpr const char* pair_dump_re = "\\d{2}:\\d{2}:\\d{2}:[0-9a-f]+:[0-9a-f]+";

  static ACPair LoadPair(const std::string& pair_dump);

  void NewMinimum(const ACClass&, const ACPair&);
  void DumpVertexHarvest(const ACPair& v, unsigned int harvest_limit, unsigned int complete_count);

  void DumpHarvestEdge(const ACPair& from, const ACPair& to, bool from_is_flipped);
  void DumpHarvestEdges(const ACPair& from, const std::vector<ACPair>& to, bool from_is_flipped);
  void DumpAutomorphEdge(const ACPair& from, const ACPair& to, bool inverse);

  template<typename Container>
  void DumpAutomorphEdges(const ACPair& from, const Container& to, bool inverse);
  void DumpPairClass(const ACPair& p, const ACClass& c);

  enum class PairQueueState : size_t {
    Pushed = (1 << 0),
    AutoNormalized = (1 << 1),
    Popped = (1 << 2),
    Processed = (1 << 3),
  };

  void DumpPairQueueState(const ACPair& pair, PairQueueState state);

  struct WriteTask {
    std::ostream* out_ = nullptr;
    fmt::MemoryWriter data_;

    WriteTask() = default;

    WriteTask(const WriteTask&)=delete;
    WriteTask& operator=(const WriteTask&)=delete;

    WriteTask& operator=(WriteTask&& other) {
      out_ = other.out_;
      data_ = std::move(other.data_);
      return *this;
    }

    WriteTask(WriteTask&& other)
      : out_(other.out_)
      , data_(std::move(other.data_))
    { }

    WriteTask(std::ostream* out, fmt::MemoryWriter data)
      : out_(out)
      , data_(std::move(data))
    { }
  };

  crag::multithreading::SharedQueue<WriteTask> write_tasks_;

  std::thread writer_;
 private:
  template<typename F>
  void Write(std::ostream& to, F&& f) {
    fmt::MemoryWriter data;
    f(data);
    write_tasks_.Push(WriteTask{&to, std::move(data)});
  }
};

template <typename Container>
void ACStateDump::DumpAutomorphEdges(const ACPair& from, const Container& to, bool inverse)  {
  Write(ac_graph_edges_, [&](fmt::MemoryWriter& data) {
    DumpPair(from, &data);
    for (auto&& p : to) {
      if (from == p) {
        continue;
      }
      assert(inverse ? from < p : from > p);
      data.write(" ");
      DumpPair(p, &data);
      data.write(" a{:d}", inverse);
    }
    data.write("\n");
  });
}



#endif //ACC_ACC_CLASS_LOGGER_H
