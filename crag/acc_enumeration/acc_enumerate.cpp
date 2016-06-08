//
// Created by dpantele on 5/4/16.
//

#include <bitset>
#include <cassert>
#include <fstream>
#include <map>
#include <ostream>
#include <regex>
#include <string>

#include <boost/variant.hpp>

#include <crag/folded_graph/complete.h>
#include <crag/folded_graph/folded_graph.h>
#include <crag/folded_graph/harvest.h>
#include <crag/compressed_word/tuple_normal_form.h>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "acc_class.h"
#include "acc_classes.h"
#include "ACPairProcessQueue.h"
#include "Terminator.h"
#include "ACWorker.h"

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

  ACPair trivial_pair{CWord("x"), CWord("y")};

  // these are trivial classes which should be excluded from harvest since
  ac_classes.AddClass(trivial_pair);

  ACClass* trivial_class = ac_classes.at(0);
  for (auto i = 1u; i <= 3u; ++i) {
    trivial_class->Merge(ac_classes.at(i));
  }

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

  ACTasksData data{
      config      , // const Config& config;
      t           , // const Terminator& terminator;
      &to_process , // ACPairProcessQueue* queue;
      &state_dump , // ACStateDump* dump;
      &ac_index   , // ACIndex* ac_index;
      ac_classes    // const ACClasses& ac_classes;
  };

  Process(data);
  to_process.Terminate();
  std::clog << "Enumeration stopped, wait till dump is synchronized" << std::endl;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " [run_config.json]";
  }

  EnumerateAC(argv[1]);

  std::clog << "Dump is syncronized, it is safe to interrupt process now" << std::endl;

  //then exec the cleanup script
  std::clog << "Running dumps cleanup" << std::endl;

  auto cleanup_path = fs::path(argv[0]).parent_path() / path("crag.acc_enumeration.dump_cleanup");
  execl(cleanup_path.c_str(), cleanup_path.c_str(), argv[1], (char *) NULL);

  // execl never returns
  throw fmt::SystemError(errno, "Can\'t exec {}", cleanup_path);
}