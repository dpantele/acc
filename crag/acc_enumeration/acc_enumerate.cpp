//
// Created by dpantele on 5/4/16.
//

#include <bitset>
#include <map>

#include <ostream>
#include <regex>
#include <string>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <folded_graph/complete.h>
#include <folded_graph/folded_graph.h>
#include <folded_graph/harvest.h>
#include <compressed_word/tuple_normal_form.h>
#include <fstream>

#include "acc_class.h"
#include "acc_classes.h"
#include "Terminator.h"

using namespace crag;

//! The input file should cotain a pair of {x,y} word on each line
std::vector<ACPair> LoadInput(const Config& c) {
  std::ifstream in(c.input().native());
  if (in.fail()) {
    throw fmt::SystemError(errno, "Can't read {}", c.input());
  }

  std::vector<ACPair> result;

  std::regex pair_re("^\\W*([xXyY]+)\\W+([xXyY]+).*");
  std::smatch pair;
  std::string next_line;
  while (std::getline(in, next_line)) {
    if (!std::regex_match(next_line, pair, pair_re)) {
      fmt::print(std::cerr, "Can't parse input line {}", next_line);
      continue;
    }
    result.push_back(ACPair{CWord(pair[1]), CWord(pair[2])});
  }

  return result;
}

class ACPairProcessQueue {
 public:
  ACPairProcessQueue(const Config&, ACStateDump* state_dump)
      : state_dump_(state_dump)
  { }

  std::pair<ACPair, bool> Pop() {
    auto next_pair = *to_process.begin();
    to_process.erase(to_process.begin());
    state_dump_->DumpPairQueueState(next_pair.first, ACStateDump::PairQueueState::Popped);
    return next_pair;
  }

  void Push(ACPair pair, bool is_aut_normalized) {
    to_process.emplace(pair, is_aut_normalized);
    state_dump_->DumpPairQueueState(pair, is_aut_normalized ? ACStateDump::PairQueueState::AutoNormalized : ACStateDump::PairQueueState::Pushed);
  }

  bool IsEmpty() const {
    return to_process.empty();
  }

  size_t GetSize() const {
    return to_process.size();
  }

 private:
  std::map<ACPair, bool> to_process;
  ACStateDump* state_dump_;
};

void EnumerateAC(path config_path) {
  Terminator t;

  Config config;
  config.LoadFromJson(config_path);

  //first, read all input pairs
  auto inputs = LoadInput(config);

  //for each pair we also generate 4 other pairs which are images under the fixed canonical nielsen automorphisms
  //namely, phi_1 = x->xy, phi_2 = x<->y and phi_3 y->Y. So, we have classes UV_0, UV_1, UV_2, UV_3.
  //As soon as UV_0 and UV_k have an intersection, it means that phi_k produces AC-equivalent pairs and
  //UV_0 and UV_k may be merged. If it happens for every k, then Aut-normalization may be used after that.
  //Hence we will forget about UK_k, and every pair in UV_0 should be aut-normalized.

  //So, what kind of interface 'classes' should provide? Initially we have a separate class
  //for every pair. We also have another class for every image of every initial pair under each of
  //the canonical autos. Now, as soon as (u, v) gives (u', v'), (u', v') is
  //assigned the same class as (u, v) has. If that happens that (u', v') already belongs to
  //some class, two classes are merged.

  //It is reasonable to provide the following information for every class: initial pair for it,
  //the automorphism which was applied, and also the minimal pair in the class.
  //We should also keep a (k+1)-bit bitfield which would allow to track which automorphisms
  //classes are AC-equal to the current one, applying 'or' on each merge.

  ACStateDump state_dump(config);

  ACClasses ac_classes(config, &state_dump);
  ac_classes.AddClasses(inputs);
  ac_classes.RestoreMerges();
  ac_classes.RestoreMinimums();

  auto ac_index = ac_classes.GetInitialACIndex();

  //maybe restore the index
  if (fs::exists(config.pairs_classes_in())) {
    std::string next_line;
    auto input = config.ifstream(config.pairs_classes_in());

    while(std::getline(input, next_line)) {
      auto split = next_line.find(' ');
      if (split == std::string::npos) {
        continue;
      }

      auto pair = ACStateDump::LoadPair(next_line.substr(0, split));
      auto ac_class = ac_classes.at(std::stoul(next_line.substr(split + 1)));

      ac_index.emplace(pair, ac_class);
      ac_class->AddPair(pair);
    }
  }

  ACPairProcessQueue to_process(config, &state_dump);

  if (fs::exists(config.pairs_queue_in())) {
    // we just restore the queue
    // format: word word
    auto queue_input = config.ifstream(config.pairs_queue_in());
    std::string next_line;
    std::regex pairs_queue_in_re(fmt::format("^({}) (\\d+)$", ACStateDump::pair_dump_re));
    std::smatch pair_parsed;
    while (std::getline(queue_input, next_line)) {
      if (!std::regex_match(next_line, pair_parsed, pairs_queue_in_re)) {
        continue;
      }

      ACPair elem = ACStateDump::LoadPair(pair_parsed[1]);
      to_process.Push(elem, (std::stoul(pair_parsed[2])
          & static_cast<size_t>(ACStateDump::PairQueueState::AutoNormalized)) != 0);
    }
  } else {
    for (auto&& c : ac_classes) {
      to_process.Push(c.minimal(), false);
    }
  }

  for (auto&& c : ac_classes) {
    c.DescribeForLog(&std::clog);
    std::clog << "\n";
  }


  auto MaxHarvestLength = [](const ACClass& c) {
    return static_cast<CWord::size_type>(c.minimal()[0].size() + c.minimal()[1].size());
  };

  //the main worker function - take a pair and apply an AC-move to it
  auto process = [&](crag::CWordTuple<2> pair, bool was_aut_normalized) {
    //each pair has a class to which it belongs
    auto pair_class = ac_index.at(pair);
    if(pair_class->AllowsAutMoves() && !was_aut_normalized) {
      //we have proved that AC moves may be used, but this exact pair was not normalized yet

      //first we find some pair of minimal length
      auto reduced_pair = WhitheadMinLengthTuple(pair);

      //we also need to find the actual minimal pair
      if (Length(reduced_pair) < Length(pair)) {
        reduced_pair = ConjugationInverseFlipNormalForm(reduced_pair);
      }

      if (reduced_pair != pair && ac_index.count(pair) != 0) {
        state_dump.DumpAutomorphEdge(pair, reduced_pair);
        return;
      }

      //if pair still was not processed, we just continue from here
    }

    //and we perform ACM-moves for (u, v) and for (v, u)

    auto ACMMove = [&](const CWordTuple<2>& uv_pair, bool is_flipped, CWord::size_type harvest_limit,  unsigned short complete_count) {
      assert(ConjugationInverseFlipNormalForm(uv_pair) == uv_pair);

      const CWord& u = is_flipped ? uv_pair[1] : uv_pair[0];
      const CWord& v = is_flipped ? uv_pair[0] : uv_pair[1];

      auto uv_class_pos = ac_index.find(uv_pair);
      assert(uv_class_pos != ac_index.end());
      ACClass* uv_class = uv_class_pos->second;

      //to make a single ACM-move, first we need to build a FoldedGraph
      FoldedGraph g;

      //start from a cycle u of weight 1
      g.PushCycle(u, 1);

      //complete this with v
      while (complete_count-- > 0) {
        CompleteWith(v, &g);
      }

      //harvest all cycles of weight \pm
      auto harvested_words = Harvest(harvest_limit, 1, &g);
      std::vector<CWordTuple<2>> new_tuples;
      new_tuples.reserve(harvested_words.size());

      //they were not cyclically normalized, do it now
      //also if uv_class allows Automorphisms, perform the first phase
      for (auto& word : harvested_words) {
        new_tuples.emplace_back(CWordTuple<2>{v, word});
        new_tuples.back() = ConjugationInverseFlipNormalForm(new_tuples.back());
        state_dump.DumpHarvestEdge(uv_pair, new_tuples.back(), is_flipped);

        if (uv_class->AllowsAutMoves()) {
          auto harvested_tuple = new_tuples.back();
          new_tuples.back() = WhitheadMinLengthTuple(new_tuples.back());
          new_tuples.back() = ConjugationInverseFlipNormalForm(new_tuples.back());
          state_dump.DumpAutomorphEdge(harvested_tuple, new_tuples.back());
        }
      }

      std::sort(new_tuples.begin(), new_tuples.end());
      auto tuples_end = std::unique(new_tuples.begin(), new_tuples.end());

      auto new_tuple = new_tuples.begin();
      auto new_tuples_keep = new_tuple;

      while (new_tuple != tuples_end) {
        auto exists = ac_index.find(*new_tuple);
        if (exists != ac_index.end()) {
          //we merge two ac classes
          uv_class->Merge(exists->second);
        } else {
          //we move this pair to the ones which will be kept
          *new_tuples_keep = *new_tuple;
          ++new_tuples_keep;
        }
        ++new_tuple;
      }

      new_tuples.erase(new_tuples_keep, new_tuples.end());

      for (auto&& tuple : new_tuples) {
        if (uv_class->AllowsAutMoves()) {
          std::set<CWordTuple<2>> minimal_orbit = {tuple};
          CompleteWithShortestAutoImages(&minimal_orbit);

          for (auto image : minimal_orbit) {
            image = ConjugationInverseFlipNormalForm(image);
            uv_class->AddPair(image);
            ac_index.emplace(image, uv_class);
            if (image < tuple) {
              tuple = image;
            }
          }

          for (const auto& image : minimal_orbit) {
            state_dump.DumpAutomorphEdge(image, tuple);
          }
          to_process.Push(tuple, true);
        } else {
          uv_class->AddPair(tuple);
          ac_index.emplace(tuple, uv_class);
          to_process.Push(tuple, false);
        }

      }
    };

    auto harvest_limit = MaxHarvestLength(*pair_class);
    unsigned short complete_count = 2;

    ACMMove(pair, false, harvest_limit, complete_count);
    ACMMove(pair, true, harvest_limit, complete_count);

    // pair was finally processed and should not be retried
    state_dump.DumpVertexHarvest(pair, harvest_limit, complete_count);
  };

  auto processed_count = 0u;

  while (!to_process.IsEmpty() && !t.ShouldTerminate()) {
    auto current_pair = to_process.Pop();
    process(current_pair.first, current_pair.second);
    state_dump.DumpPairQueueState(current_pair.first, ACStateDump::PairQueueState::Harvested);

    if (++processed_count % 1000 == 0) {
      std::clog << processed_count << "/" << to_process.GetSize() << "/" << ac_index.size() <<"\n";
      for (auto&& c : ac_classes) {
        if (c.IsPrimary()) {
          c.DescribeForLog(&std::clog);
          std::clog << "\n";
        }
      }
    }
  }
  std::clog << "Enumeration stopped" << std::endl;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " [run_config.json]";
  }

  EnumerateAC(argv[1]);

  //then exec the cleanup script
  std::clog << "Running dumps cleanup" << std::endl;

  auto cleanup_path = fs::path(argv[0]).parent_path() / path("crag.acc_enumeration.dump_cleanup");
  execl(cleanup_path.c_str(), cleanup_path.c_str(), argv[1], (char *) NULL);

  // execl never returns
  throw fmt::SystemError(errno, "Can\'t exec {}", cleanup_path);
}