//
// Created by dpantele on 6/7/16.
//

#include "ACWorker.h"

#include "acc_class.h"

#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <crag/compressed_word/tuple_normal_form.h>
#include <crag/folded_graph/complete.h>
#include <crag/folded_graph/folded_graph.h>
#include <crag/folded_graph/harvest.h>

using namespace crag;

struct WorkersSharedState {
  ACTasksData data;

  //data.ac_index (and uv_classes we get from it), index_size and processed count may be changed only under reader lock
  //data.ac_index, uv_classes may be viewed only after getting a reading lock

  size_t index_size = 0;
  size_t processed_count = 0;

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

  struct PairInfo {
    ACClass* p_class;
    bool use_automorphisms;
    CWord::size_type harvest_limit;
    unsigned short complete_count;
  };

  PairInfo GetPairInfo(const ACPair& p) {
    auto lock = ReadingLock();
    auto pair_class = state_->data.ac_index->at(p);
    return PairInfo{pair_class, pair_class->AllowsAutMoves(), MaxHarvestLength(*pair_class), 2};
  }

  bool AutMinIsInIndex(const ACPair& pair) {
    //first we find some pair of minimal length
    auto reduced_pair = WhitheadMinLengthTuple(pair);

    //we also need to find the actual minimal pair
    if (Length(reduced_pair) < Length(pair)) {
      reduced_pair = ConjugationInverseFlipNormalForm(reduced_pair);
    }

    if (reduced_pair == pair) {
      return false;
    }
    auto lock = ReadingLock();
    if (state_->data.ac_index->count(reduced_pair) != 0) {
      //reduced pair was or will be harvested
      lock.unlock();
      state_->data.dump->DumpAutomorphEdge(pair, reduced_pair, false);
      return true;
    }
    return false;
  }

  void ACMMove(const ACPair& uv_pair, bool is_flipped, const PairInfo& pair_info,
      std::vector<ACClass*>* classes_to_merge,
      std::vector<ACPair>* pairs_to_add, std::vector<ACPair>* pairs_to_process
  ) {
    assert(ConjugationInverseFlipNormalForm(uv_pair) == uv_pair);

    const CWord& u = is_flipped ? uv_pair[1] : uv_pair[0];
    const CWord& v = is_flipped ? uv_pair[0] : uv_pair[1];

    // we either use automorphisms or don't, we don't start using them after some pair was processed
    bool use_automorphisms = pair_info.use_automorphisms;

    //to make a single ACM-move, first we need to build a FoldedGraph
    FoldedGraph g;

    //start from a cycle u of weight 1
    g.PushCycle(u, 1);

    //complete this with v
    auto complete_count = pair_info.complete_count;
    while (complete_count-- > 0) {
      CompleteWith(v, &g);
    }

    //harvest all cycles of weight \pm
    auto harvested_words = Harvest(pair_info.harvest_limit, 1, &g);
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
      new_tuples.back() = ConjugationInverseFlipNormalForm(new_tuples.back());
      state_dump.DumpHarvestEdge(uv_pair, new_tuples.back(), is_flipped);
    }

    if (use_automorphisms) {
      for (auto& new_tuple : new_tuples) {
        auto normalized_tuple = WhitheadMinLengthTuple(new_tuple);
        normalized_tuple = ConjugationInverseFlipNormalForm(normalized_tuple);
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

    //! Actual merging requires write lock, we combine all writes under a single lock
    {
      auto read_lock = ReadingLock();
      auto& ac_index = *state_->data.ac_index;
      while (new_tuple != tuples_end) {
        auto exists = ac_index.find(*new_tuple);
        if (exists != ac_index.end()) {
          //we merge two ac classes
          classes_to_merge->emplace_back(exists->second);
        } else {
          //we move this pair to the ones which will be kept
          *new_tuples_keep = *new_tuple;
          ++new_tuples_keep;
        }
        ++new_tuple;
      }
    }

    new_tuples.erase(new_tuples_keep, new_tuples.end());

    for (auto&& tuple : new_tuples) {
      if (use_automorphisms) {
        std::set<CWordTuple<2>> minimal_orbit = {tuple};
        CompleteWithShortestAutoImages(&minimal_orbit);

        auto min_tuple = tuple;

        for (auto image : minimal_orbit) {
          image = ConjugationInverseFlipNormalForm(image);

          //all pairs in the min orbit are added to index so that later we could check fast a non-auto-normalized pair
          pairs_to_add->push_back(image);

          if (image < min_tuple) {
            min_tuple = image;
          }
        }

        for (const auto& image : minimal_orbit) {
          if (min_tuple != image) {
            state_dump.DumpAutomorphEdge(min_tuple, image, true);
          }
        }

        pairs_to_process->push_back(tuple);
      } else {
        pairs_to_process->push_back(tuple);
      }
    }
  };

  void ProcessedStats(const ACPair& p, boost::unique_lock<boost::shared_mutex> final_lock) {
    if (++state_->processed_count % 1000 == 0) {
      std::clog << state_->processed_count << "/" << state_->data.queue->GetSize()
          << "/" << state_->data.ac_index->size() << "\n";
      for (auto&& c : state_->data.ac_classes) {
        if (c.IsPrimary()) {
          c.DescribeForLog(&std::clog);
          std::clog << "\n";
        }
      }
    }
  }

  void Process(ACPair pair, bool was_aut_normalized) {
    auto pair_info = GetPairInfo(pair);
    if (pair_info.use_automorphisms && !was_aut_normalized
        && AutMinIsInIndex(pair)) {
      return ProcessedStats(pair, WritingLock());
    }

    std::vector<ACClass*> classes_to_merge;
    std::vector<ACPair> pairs_to_add, pairs_to_process;

    // Harvest the pair and its flip
    ACMMove(pair, false, pair_info, &classes_to_merge, &pairs_to_add, &pairs_to_process);
    ACMMove(pair, true, pair_info, &classes_to_merge, &pairs_to_add, &pairs_to_process);

    //and finally we write the results
    {
      auto final_lock = WritingLock();

      for (auto&& other_class : classes_to_merge) {
        pair_info.p_class->Merge(other_class);
      }

      auto& ac_index = *state_->data.ac_index;
      for (auto&& to_add : (pair_info.use_automorphisms ? pairs_to_add : pairs_to_process)) {
        pair_info.p_class->AddPair(to_add);
        ac_index.emplace(to_add, pair_info.p_class);
        ++state_->index_size;
      }

      ProcessedStats(pair, std::move(final_lock));
    }

    //schedule obtained words
    {
      auto& to_process = state_->data.queue;
      for (auto&& pair_to_process : pairs_to_process) {
        to_process->Push(pair_to_process, pair_info.use_automorphisms);
      }
    }
  }
};

void Process(const ACTasksData& data) {
  WorkersSharedState state{data};

  std::deque<ACWorker> workers;
  while(workers.size() < data.config.workers_count_) {
    workers.emplace_back(&state);
  }

  while(!workers.empty()) {
    workers.pop_back();
  }
}

