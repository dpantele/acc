//
// Created by dpantele on 5/25/16.
//

#ifndef ACC_ACC_CLASS_LOGGER_H
#define ACC_ACC_CLASS_LOGGER_H

#include "acc_class.h"
#include "boost_filtering_stream.h"

struct ACStateDump {
  BoostFilteringOStream classes_merges_out_;
  BoostFilteringOStream classes_minimums_out_;
  BoostFilteringOStream ac_graph_vertex_harvest_;
  BoostFilteringOStream ac_graph_edges_;

  uint64_t queue_index_ = 0u;
  BoostFilteringOStream pairs_queue_;

  BoostFilteringOStream ac_pair_class_;

  ACStateDump(const Config& c);
  ACStateDump(ACStateDump&&)=default;
  ACStateDump& operator=(ACStateDump&&)=default;

  void Merge(const ACClass&, const ACClass&);

  static void DumpPair(const ACPair& p, std::ostream* out);
  static void DumpPair(const ACPair& p, fmt::MemoryWriter* out);

  static constexpr const char* pair_dump_re = "\\d{2}:\\d{2}:\\d{2}:[0-9a-f]+:[0-9a-f]+";

  static ACPair LoadPair(const std::string& pair_dump);

  void NewMinimum(const ACClass&, const ACPair&);
  void DumpVertexHarvest(const ACPair& v, unsigned int harvest_limit, unsigned int complete_count);
  void DumpHarvestEdge(const ACPair& from, const ACPair& to, bool from_is_flipped);
  void DumpAutomorphEdge(const ACPair& from, const ACPair& to);
  void DumpPairClass(const ACPair& p, const ACClass& c);

  enum class PairQueueState : size_t {
    Pushed = (1 << 0),
    AutoNormalized = (1 << 1),
    Popped = (1 << 2),
    Harvested = (1 << 3),
  };

  void DumpPairQueueState(const ACPair& pair, PairQueueState state);
};

#endif //ACC_ACC_CLASS_LOGGER_H
