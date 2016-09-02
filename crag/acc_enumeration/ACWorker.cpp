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

  std::atomic<size_t> processed_count{0u};
  std::atomic<std::chrono::system_clock::time_point> last_report{std::chrono::system_clock::now()};
  std::atomic<std::chrono::system_clock::time_point> last_full_report{std::chrono::system_clock::now()};
};

static Endomorphism ToIdentityImageMapping(const ACClass::AutKind k) {
  switch (k) {
    case ACClass::AutKind::Ident:
      return Endomorphism("x", "y");
    case ACClass::AutKind::x_xy:
      return Endomorphism("xY", "y");
    case ACClass::AutKind::x_y:
      return Endomorphism("y", "x");
    case ACClass::AutKind::y_Y:
      return Endomorphism("x", "Y");
  }
  assert(false);
}

static Endomorphism ToIdentityImageMapping(const ACClass* c) {
  return ToIdentityImageMapping(c->init_kind());
  assert(false);
}

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
      auto index = state_->data.ac_index->GetData();
      auto pair_class_pos = index.find(p);
      if (pair_class_pos == index.end()) {
        // it is possible that some index modifications are not committed yet
        std::this_thread::yield();
        continue;
      }

      auto classes = state_->data.ac_index->GetCurrentACClasses();
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
    std::vector<std::pair<ACClasses::ClassId, ACClasses::ClassId>> classes_to_merge;
    std::vector<std::pair<ACPair, ACClasses::ClassId>> pairs_to_add;
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

    std::vector<std::pair<Endomorphism, ACClasses::ClassId>> automorphic_classes;
    if (!use_automorphisms) {
      // find all automorphic_classes
      const auto& classes = *step_info.classes;
      auto original_class = classes.at(step_info.class_id);
      auto identity_image_id = classes.IdentityImageFor(step_info.class_id);
      auto identity_image_class = classes.at(identity_image_id);
      auto to_identity_mapping = ToIdentityImageMapping(original_class);

      auto pushClass = [&](const Endomorphism& e, ACClass::AutKind type) {
        if (identity_image_class->AllowsAutomorphism(type)) {
          // the identity class is always considered primary
          // so if original_class is already merged with identity_image
          // then we add the new pairs to identity_image
          // so happens with any other automorphic images
          return;
        }

        automorphic_classes.emplace_back(to_identity_mapping.ComposeWith(e),
                                         classes.at(identity_image_id + static_cast<size_t>(type))->id_);
      };

      thread_local auto ident = Endomorphism(CWord("x"), CWord("y"));
      thread_local auto x_xy = Endomorphism(CWord("xy"), CWord("y"));
      thread_local auto x_y = Endomorphism(CWord("y"), CWord("x"));
      thread_local auto y_Y = Endomorphism(CWord("x"), CWord("Y"));

      pushClass(ident, ACClass::AutKind::Ident);
      pushClass(x_xy, ACClass::AutKind::x_xy);
      pushClass(x_y, ACClass::AutKind::x_y);
      pushClass(y_Y, ACClass::AutKind::y_Y);
    } else {
      automorphic_classes.emplace_back(Endomorphism("x", "y"), step_info.class_id);
    }

    std::vector<std::pair<CWordTuple<2>, ACClasses::ClassId>> new_tuples;
    new_tuples.reserve(harvested_words.size() * automorphic_classes.size());

    //state_dump may be used without locks
    auto& state_dump = *state_->data.dump;

    //they were not cyclically normalized, do it now
    //also if uv_class allows Automorphisms, perform the first phase
    for (auto& word : harvested_words) {
      if (word == u) {
        continue;
      }

      for (auto&& aut_class : automorphic_classes) {
        try {
          auto image = Apply(aut_class.first, CWordTuple<2>{v, word});

          stats->ConjNormalizeClick();
          image = ConjugationInverseFlipNormalForm(image);
          stats->ConjNormalizeClick();

          if (image[1].size() > harvest_limit) {
            continue;
          }

          if (image[0].size() < 4
              || image[0].size() + image[1].size() < 13) {
            step_data->got_trivial_class = true;
            return;
          }

          assert(image[0].size() <= harvest_limit && "First word must always be shorter than second");

          new_tuples.emplace_back(image, aut_class.second);
        } catch(const std::length_error&) {
          continue;
        }
      }
    }

    // looks like dumping graph is not possible anyway
    // so I am going to deprecate this API
    // state_dump.DumpHarvestEdges(uv_pair, new_tuples, is_flipped);

    if (use_automorphisms) {
      for (auto& new_tuple : new_tuples) {
        stats->WhiteheadReduceClick();
        auto normalized_tuple = WhitheadMinLengthTuple(new_tuple.first);
        stats->WhiteheadReduceClick();
        stats->ConjNormalizeClick();
        normalized_tuple = ConjugationInverseFlipNormalForm(normalized_tuple);
        stats->ConjNormalizeClick();

        if (normalized_tuple.length() < 13 || normalized_tuple[0].size() < 4) {
          step_data->got_trivial_class = true;
          return;
        }

        if (normalized_tuple != new_tuple.first) {
          state_dump.DumpAutomorphEdge(new_tuple.first, normalized_tuple, false);
          new_tuple.first = normalized_tuple;
        }
      }
    }

    std::sort(new_tuples.begin(), new_tuples.end());
    auto tuples_end = std::unique(new_tuples.begin(), new_tuples.end());

    auto new_tuple = new_tuples.begin();
    auto new_tuples_keep = new_tuple;

    stats->SetUniquePairs(static_cast<size_t>(tuples_end - new_tuple));

    while (new_tuple != tuples_end) {
      auto exists = step_info.index.find(new_tuple->first);
      if (exists != step_info.index.end()) {
        //we merge two ac classes
        if (exists->second != new_tuple->second) {
          step_data->classes_to_merge.emplace_back(exists->second, new_tuple->second);
        }
      } else {
        //we move this pair to the ones which will be kept
        *new_tuples_keep = *new_tuple;
        ++new_tuples_keep;
      }
      ++new_tuple;
    }

    new_tuples.erase(new_tuples_keep, new_tuples.end());

    if (use_automorphisms) {
      for (auto&& tuple : new_tuples) {
        stats->WhiteheadExtendClick();
        std::set<CWordTuple<2>> minimal_orbit = {tuple.first};
        CompleteWithShortestAutoImages(&minimal_orbit);
        stats->WhiteheadExtendClick();

        auto min_tuple = tuple.first;

        for (auto image : minimal_orbit) {
          stats->ConjNormalizeClick();
          image = ConjugationInverseFlipNormalForm(image);
          stats->ConjNormalizeClick();

          if (image[0].size() < 4
              || image.length() < 13) {
            step_data->got_trivial_class = true;
            return;
          }

          //all pairs in the min orbit are added to index so that later we could check fast a non-auto-normalized pair
          step_data->pairs_to_add.emplace_back(image, tuple.second);

          if (image < min_tuple) {
            min_tuple = image;
          }
        }

        state_dump.DumpAutomorphEdges(min_tuple, minimal_orbit, true);
        step_data->pairs_to_process.push_back(min_tuple);
      }
      stats->SetAutOrbitSize(step_data->pairs_to_process.empty() ? 0 : step_data->pairs_to_add.size() / step_data->pairs_to_process.size());
      stats->SetAddedPairs(step_data->pairs_to_process.size());
    } else {
      step_data->pairs_to_add.insert(step_data->pairs_to_add.end(), new_tuples.begin(), new_tuples.end());
      stats->SetAddedPairs(step_data->pairs_to_add.size());
    }

  };

  void ProcessedStats(const ACPair& p) {
    auto time = std::chrono::system_clock::now();
    auto last_report = state_->last_report.load(std::memory_order_relaxed);
    if (time - last_report > std::chrono::seconds(5)
        && state_->last_report.compare_exchange_strong(last_report, time, std::memory_order_relaxed)) {

      bool full_report = false;

      if (time - state_->last_full_report.load(std::memory_order_relaxed) > std::chrono::minutes(1)) {
        state_->last_full_report.store(time, std::memory_order_relaxed);
        full_report = true;
      }

      fmt::MemoryWriter new_stats;

      new_stats.write("{}/{}/{}\n",
                      state_->processed_count,
                      state_->data.queue->GetTasksCount(),
                      state_->data.ac_index->GetData().size());

      std::map<crag::CWord::size_type, size_t> lengths_counts;
      auto distinct_count = 0u;
      auto ac_classes = state_->data.ac_index->GetCurrentACClasses();
      for (auto&& c : *ac_classes) {
        if (c.IsPrimary()) {
          ++distinct_count;
          ++lengths_counts[c.minimal_[0].size() + c.minimal_[1].size()];
        }
      }

      new_stats.write("Distinct classes: {}\n", distinct_count);

      if (distinct_count < 100) {
        for (auto&& c : *ac_classes) {
          if (c.IsPrimary()) {
            c.DescribeForLog(&new_stats);
            new_stats.write("\n");
          }
        }
      } else {
        for (auto&& length : lengths_counts) {
          new_stats.write("Length {}: {}\n", length.first, length.second);
          if (full_report && length.second < 50) {
            for (auto&& c : *ac_classes) {
              if (c.IsPrimary() && c.minimal_[0].size() + c.minimal_[1].size() == length.first) {
                c.DescribeForLog(&new_stats);
                new_stats.write("\n");
              }
            }
          }
        }
      }

      std::clog << new_stats.c_str();
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
      return ProcessedStats(pair);
    }
    if (Length(pair) < 13 || pair[0].size() < 4) {
      // pair is certainly trivial
      pair_info.index_writer.Merge(pair_info.class_id, state_->trivial_class);
      return ProcessedStats(pair);
    }
    if (pair_info.use_automorphisms && !was_aut_normalized
        && AutMinIsInIndex(pair, pair_info.index)) {
      return ProcessedStats(pair);
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
      if (step_data.got_trivial_class) {
        auto identity_class = ACClasses::IdentityImageFor(pair_info.class_id);
        pair_info.index_writer.Merge(identity_class, state_->trivial_class);
        pair_info.index_writer.Merge(identity_class + 1, state_->trivial_class);
        pair_info.index_writer.Merge(identity_class + 2, state_->trivial_class);
        pair_info.index_writer.Merge(identity_class + 3, state_->trivial_class);

        return ProcessedStats(pair);
      }

      auto& ac_index = pair_info.index_writer;

      for (auto&& other_class : step_data.classes_to_merge) {
        ac_index.Merge(other_class.first, other_class.second);
      }

      for (auto&& to_add : step_data.pairs_to_add) {
        ac_index.Push(to_add.first, to_add.second);
      }

      ProcessedStats(pair);
    }

    //schedule obtained words
    {
      auto& to_process = state_->data.queue;
      if (step_data.pairs_to_process.empty()) {
        // happens when we work with non-automorphic case
        for (auto&& p : step_data.pairs_to_add) {
          assert(!pair_info.use_automorphisms);
          to_process->Push(p.first, false);
        }
      } else {
        assert(pair_info.use_automorphisms);
        for (auto&& pair_to_process : step_data.pairs_to_process) {
          to_process->Push(pair_to_process, true);
        }
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

