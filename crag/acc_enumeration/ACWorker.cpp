//
// Created by dpantele on 6/7/16.
//

#include "ACWorker.h"

#include <chrono>
#include <regex>

#include "acc_class.h"
#include "ACIndex.h"
#include "ACWorkerStats.h"

#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <crag/compressed_word/tuple_normal_form.h>
#include <crag/folded_graph/complete.h>
#include <crag/folded_graph/folded_graph.h>
#include <crag/folded_graph/harvest.h>

using namespace crag;

struct WorkersSharedState {
  ACTasksData data;
  ACClasses::ClassId trivial_class;
  ACWorkerStats* worker_stats;

  //data.ac_index (and uv_classes we get from it), index_size and processed count may be changed only under reader lock
  //data.ac_index, uv_classes may be viewed only after getting a reading lock

  size_t index_size = 0;
  size_t processed_count = 0;

  std::chrono::system_clock::time_point last_report = std::chrono::system_clock::now();
  boost::shared_mutex state_mutex_;
};

struct ACWorker {
 public:
  ACWorker(WorkersSharedState* state)
      : state_(state)
  {
    worker_thread_ = std::thread([this] {
      std::pair<ACPair, bool> next_task;
      while(!state_->data.terminator.ShouldTerminate()
        && state_->data.queue->Pop(next_task)) {
        Process(next_task.first, next_task.second);
        state_->data.dump->DumpPairQueueState(next_task.first, ACStateDump::PairQueueState::Processed);
        state_->data.queue->TaskDone();
      }
    });
  }

  ~ACWorker() {
    worker_thread_.join();
  }

 private:
  WorkersSharedState* state_;
  std::thread worker_thread_;

  boost::shared_lock<boost::shared_mutex> ReadingLock() {
    return boost::shared_lock<boost::shared_mutex>(state_->state_mutex_);
  }

  boost::unique_lock<boost::shared_mutex> WritingLock() {
    return boost::unique_lock<boost::shared_mutex>(state_->state_mutex_);
  }

  struct ACStepInfo {
    ACClasses::ClassId class_id;
    bool use_automorphisms;
    bool is_trivial;
    CWord::size_type harvest_limit;
    unsigned short complete_count;
    std::shared_ptr<const ACClasses> classes;
    ACIndex::DataReadHandle index;
    ACIndex::AddBatch index_writer;

    ~ACStepInfo() {
      // this has to be in specific order since Execute may not proceed
      // if queue is too long and it will be long if index folds a reference to the current
      // version
      index.release();
      classes.reset();
      index_writer.Execute();
    }
  };

  ACStepInfo GetPairInfo(const ACPair& p) {
    while (true) {
      auto lock = ReadingLock();
      auto index = state_->data.ac_index->GetData();
      auto classes = state_->data.ac_index->GetCurrentACClasses();
      auto pair_class_pos = index.find(p);
      if (pair_class_pos == index.end()) {
        // it is possible that some index modifications are not committed yet
        std::this_thread::yield();
        continue;
      }
      auto pair_class_id = pair_class_pos->second;
      auto batch = state_->data.ac_index->NewBatch();
      return ACStepInfo{
          pair_class_id,
          classes->AllowsAutMoves(pair_class_id),
          classes->AreMerged(pair_class_id, state_->trivial_class),
          MaxHarvestLength(*classes, pair_class_id),
          2,
          std::move(classes),
          std::move(index),
          std::move(batch)
      };
    }
  }

  bool AutMinIsInIndex(const ACPair& pair, const ACIndex::DataReadHandle& index) {
    //first we find some pair of minimal length
    auto reduced_pair = WhitheadMinLengthTuple(pair);

    //we also need to find the actual minimal pair
    if (Length(reduced_pair) < Length(pair)) {
      reduced_pair = ConjugationInverseFlipNormalForm(reduced_pair);
    }

    if (reduced_pair == pair) {
      return false;
    }

    if (index.count(reduced_pair) != 0) {
      //reduced pair was or will be harvested
      state_->data.dump->DumpAutomorphEdge(pair, reduced_pair, false);
      return true;
    }
    return false;
  }

  struct ProcessStepData {
    std::vector<ACClasses::ClassId> classes_to_merge;
    std::vector<ACPair> pairs_to_add;
    std::vector<ACPair> pairs_to_process;

    bool got_trivial_class = false;
  };

  void ACMMove(const ACPair& uv_pair, bool is_flipped, const ACStepInfo& step_info,
      ProcessStepData* step_data, MoveStats* stats
  ) {
    assert(ConjugationInverseFlipNormalForm(uv_pair) == uv_pair);

    const CWord& u = is_flipped ? uv_pair[1] : uv_pair[0];
    const CWord& v = is_flipped ? uv_pair[0] : uv_pair[1];

    // we either use automorphisms or don't, we don't start using them after some pair was processed
    bool use_automorphisms = step_info.use_automorphisms;

    //to make a single ACM-move, first we need to build a FoldedGraph
    stats->GraphConstructClick();
    FoldedGraph g;

    //start from a cycle u of weight 1
    g.PushCycle(u, 1);

    //complete this with v
    auto complete_count = step_info.complete_count;
    while (complete_count-- > 0) {
      CompleteWith(v, &g);
    }
    stats->GraphConstructClick();

    stats->SetGraphSize(g.size());
    stats->SetGraphModulus(static_cast<size_t>(std::abs(g.modulus().modulus())));

    FoldedGraph::Weight max_weight = 0;
    for (auto&& vx : g) {
      for (auto&& e : vx) {
        max_weight = std::max(max_weight, std::abs(e.weight()));
      }
    }

    stats->SetGraphMaxWeight(static_cast<size_t>(max_weight));

    //harvest all cycles of weight \pm 1
    auto harvest_limit = step_info.harvest_limit;
    if (harvest_limit + v.size() > kMaxTotalPairLength) {
      harvest_limit = kMaxTotalPairLength - v.size();
    }
    stats->HarvestClick();
    auto harvested_words = Harvest(harvest_limit, 1, &g);
    stats->HarvestClick();
    stats->SetHarvestedPairs(harvested_words.size());

    if (harvested_words.front().size() < 4 || harvested_words.front().size() + v.size() < 13) {
      step_data->got_trivial_class = true;
      return;
    }

    std::vector<CWordTuple<2>> new_tuples;
    new_tuples.reserve(harvested_words.size());

    //state_dump may be used without locks
    auto& state_dump = *state_->data.dump;

    //they were not cyclically normalized, do it now
    //also if uv_class allows Automorphisms, perform the first phase
    for (auto& word : harvested_words) {
      if (word == u) {
        continue;
      }
      new_tuples.emplace_back(CWordTuple<2>{v, word});
      stats->ConjNormalizeClick();
      new_tuples.back() = ConjugationInverseFlipNormalForm(new_tuples.back());
      stats->ConjNormalizeClick();
    }

    state_dump.DumpHarvestEdges(uv_pair, new_tuples, is_flipped);

    if (use_automorphisms) {
      for (auto& new_tuple : new_tuples) {
        stats->WhiteheadReduceClick();
        auto normalized_tuple = WhitheadMinLengthTuple(new_tuple);
        stats->WhiteheadReduceClick();
        stats->ConjNormalizeClick();
        normalized_tuple = ConjugationInverseFlipNormalForm(normalized_tuple);
        stats->ConjNormalizeClick();

        if (normalized_tuple.length() < 13 || normalized_tuple[0].size() < 4) {
          step_data->got_trivial_class = true;
          return;
        }

        if (normalized_tuple != new_tuple) {
          state_dump.DumpAutomorphEdge(new_tuple, normalized_tuple, false);
          new_tuple = normalized_tuple;
        }
      }
    }

    std::sort(new_tuples.begin(), new_tuples.end());
    auto tuples_end = std::unique(new_tuples.begin(), new_tuples.end());

    auto new_tuple = new_tuples.begin();
    auto new_tuples_keep = new_tuple;

    stats->SetUniquePairs(static_cast<size_t>(tuples_end - new_tuple));

    while (new_tuple != tuples_end) {
      auto exists = step_info.index.find(*new_tuple);
      if (exists != step_info.index.end()) {
        //we merge two ac classes
        step_data->classes_to_merge.emplace_back(exists->second);
      } else {
        //we move this pair to the ones which will be kept
        *new_tuples_keep = *new_tuple;
        ++new_tuples_keep;
      }
      ++new_tuple;
    }

    new_tuples.erase(new_tuples_keep, new_tuples.end());

    for (auto&& tuple : new_tuples) {
      if (use_automorphisms) {
        stats->WhiteheadExtendClick();
        std::set<CWordTuple<2>> minimal_orbit = {tuple};
        CompleteWithShortestAutoImages(&minimal_orbit);
        stats->WhiteheadExtendClick();

        auto min_tuple = tuple;

        for (auto image : minimal_orbit) {
          stats->ConjNormalizeClick();
          image = ConjugationInverseFlipNormalForm(image);
          stats->ConjNormalizeClick();

          if (image[0].size() < 4) {
            step_data->got_trivial_class = true;
            return;
          }

          //all pairs in the min orbit are added to index so that later we could check fast a non-auto-normalized pair
          step_data->pairs_to_add.push_back(image);

          if (image < min_tuple) {
            min_tuple = image;
          }
        }

        state_dump.DumpAutomorphEdges(min_tuple, minimal_orbit, true);
        step_data->pairs_to_process.push_back(min_tuple);
      } else {
        step_data->pairs_to_process.push_back(tuple);
      }
    }

    stats->SetAutOrbitSize(step_data->pairs_to_process.empty() ? 0 : step_data->pairs_to_add.size() / step_data->pairs_to_process.size());
    stats->SetAddedPairs(step_data->pairs_to_process.size());
  };

  void ProcessedStats(const ACPair& p, boost::unique_lock<boost::shared_mutex> final_lock) {
    ++state_->processed_count;
    auto time = std::chrono::system_clock::now();
    if (time - state_->last_report > std::chrono::seconds(5)) {
      state_->last_report = std::chrono::system_clock::now();

      std::clog << state_->processed_count << "/" << state_->data.queue->GetTasksCount()
          << "/" << state_->data.ac_index->GetData().size() << "\n";
      std::map<crag::CWord::size_type, size_t> lengths_counts;
      auto distinct_count = 0u;
      auto ac_classes = state_->data.ac_index->GetCurrentACClasses();
      for (auto&& c : *ac_classes) {
        if (c.IsPrimary()) {
          ++distinct_count;
          ++lengths_counts[c.minimal_[0].size() + c.minimal_[1].size()];
        }
      }

      fmt::print(std::clog, "Distinct classes: {}\n", distinct_count);
      if (distinct_count < 100) {
        for (auto&& c : *ac_classes) {
          if (c.IsPrimary()) {
            c.DescribeForLog(&std::clog);
            std::clog << "\n";
          }
        }
      } else {
        for (auto&& length : lengths_counts) {
          fmt::print(std::clog, "Length {}: {}\n", length.first, length.second);
        }
      }
    }
  }

  void ReportACMoveStats(const ACPair& p, bool swapped, const ACStepInfo& info, const ProcessStepData& step_data, const MoveStats& stats) {
    ClickTimer::Duration all_time{};

    auto GetMs = [](auto t) {
      return std::chrono::duration_cast<std::chrono::microseconds>(t).count();
    };

    auto GetPct = [&](ClickTimer::Duration d) {
      return (d * 100 / stats.total_time.Total());
    };

    state_->worker_stats->Write(&state_->worker_stats->move_stats_out_,
        state_->data.config.stats_to_stout_ == Config::StatsToStdout::kFull,  [&] (fmt::MemoryWriter& out) {
      out.write("{}", state_->data.queue->GetTasksCount());


      out.write(", {:.4f}", std::chrono::duration<double>(stats.total_time.Total()).count());
      for(auto& f : stats.timers) {
        all_time += f.Total();
        out.write(", {}@{}", GetMs(f.Total()), GetMs(f.Average()));
      }

      out.write(", {}", GetMs(stats.total_time.Total() - all_time));

      for(auto& f : stats.timers) {
        out.write(", {}", GetPct(f.Total()));
      }

      for (auto& c : stats.num_stats) {
        out.write(", {}", c);
      }

      out.write(", {:d}, {:d}, {:d}, {:d}, {:d}, ", swapped, info.use_automorphisms, info.is_trivial, info.harvest_limit,
          info.complete_count);
      ACStateDump::DumpPair(p, &out);
      out.write("\n");
    });

    if (Config::StatsToStdout::kShort == state_->data.config.stats_to_stout_) {
      state_->worker_stats->Write(nullptr,
                                  true, [&](fmt::MemoryWriter &out) {
            out.write("{:7d}, {:7.4f}, {:2}, {:2},{:5},{:5}, {:7}, {:7}\n",
                      state_->data.queue->GetTasksCount(),
                      std::chrono::duration<double>(stats.total_time.Total()).count(),
                      p[0].size(), p[1].size(),
                      info.use_automorphisms,
                      step_data.got_trivial_class,
                      stats.GetHarvestedPairs(),
                      stats.GetAddedPairs()
            );
          });
    }
  }

  void Process(ACPair pair, bool was_aut_normalized) {
    auto pair_info = GetPairInfo(pair);

    if (pair_info.is_trivial) {
      return ProcessedStats(pair, WritingLock());
    }
    if (Length(pair) < 13 || pair[0].size() < 4) {
      // pair is certainly trivial
      auto writer = WritingLock();

      pair_info.index_writer.Merge(pair_info.class_id, state_->trivial_class);
      return ProcessedStats(pair, std::move(writer));
    }
    if (pair_info.use_automorphisms && !was_aut_normalized
        && AutMinIsInIndex(pair, pair_info.index)) {
      return ProcessedStats(pair, WritingLock());
    }


    // Harvest the pair and its flip
    ProcessStepData step_data;

    MoveStats first_move;
    first_move.TotalClick();
    ACMMove(pair, false, pair_info, &step_data, &first_move);
    first_move.TotalClick();
    ReportACMoveStats(pair, false, pair_info, step_data, first_move);

    if (!step_data.got_trivial_class) {
      MoveStats second_move;
      second_move.TotalClick();
      ACMMove(pair, true, pair_info, &step_data, &second_move);
      second_move.TotalClick();
      ReportACMoveStats(pair, true, pair_info, step_data, second_move);
    }

    state_->data.dump->DumpVertexHarvest(pair, pair_info.harvest_limit, pair_info.complete_count);

    //and finally we write the results
    {
      auto final_lock = WritingLock();

      if (step_data.got_trivial_class) {
        pair_info.index_writer.Merge(pair_info.class_id, state_->trivial_class);
        return ProcessedStats(pair, std::move(final_lock));
      }

      auto& ac_index = pair_info.index_writer;

      for (auto&& other_class : step_data.classes_to_merge) {
        ac_index.Merge(pair_info.class_id, other_class);
      }

      for (auto&& to_add : (pair_info.use_automorphisms ? step_data.pairs_to_add : step_data.pairs_to_process)) {
        ac_index.Push(to_add, pair_info.class_id);
        ++state_->index_size;
      }

      ProcessedStats(pair, std::move(final_lock));
    }

    //schedule obtained words
    {
      auto& to_process = state_->data.queue;
      for (auto&& pair_to_process : step_data.pairs_to_process) {
        to_process->Push(pair_to_process, pair_info.use_automorphisms);
      }
    }
  }
};

void Process(const ACTasksData& data) {
  ACWorkerStats worker_stats(data.config);

  worker_stats.Write(&worker_stats.move_stats_out_,
      data.config.stats_to_stout_ == Config::StatsToStdout::kFull, [](fmt::MemoryWriter& out) {
    std::regex header("([^,]+)(,?)");
    std::string timers_names_pct = std::regex_replace(MoveStats::timers_order, header, "$1_pct$2");

    out.write("tasks_left, move_total_time, {}, rest_time, {}, {}, is_swapped, use_automorphisms, is_trivial, "
                  "harvest_limit, complete_count, pair\n", MoveStats::timers_order, timers_names_pct,
        MoveStats::stats_order);
  });

  WorkersSharedState state{data, data.ac_index->GetData().at(ACPair{CWord("x"), CWord("y")}), &worker_stats};

  std::deque<ACWorker> workers;
  while(workers.size() < data.config.workers_count_) {
    workers.emplace_back(&state);
  }

  while(!workers.empty()) {
    workers.pop_back();
  }

  auto final_stats = data.config.ofstream(data.config.run_stats(), std::ios::app);
  fmt::print(final_stats, "Processed {} pairs\n", state.processed_count);
}

