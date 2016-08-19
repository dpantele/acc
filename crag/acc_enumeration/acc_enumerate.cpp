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

#include <sys/time.h>
#include <sys/resource.h>

#include "acc_class.h"
#include "acc_classes.h"
#include "ACIndex.h"
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

template<typename Rep, typename Unit>
std::string ToHumanString(std::chrono::duration<Rep, Unit> time) {
  using namespace std::chrono;

  typedef duration<int, std::ratio<3600 * 24>> days;
  typedef duration<int, std::ratio<3600 * 24 * 7>> weeks;

  if (time >= weeks(1)) {
    std::string result;
    auto weeks_count = duration_cast<weeks>(time);
    time -= weeks_count;
    auto days_count = duration_cast<days>(time);
    time -= days_count;
    auto hours_count = duration_cast<hours>(time);

    result += fmt::format("{}W", weeks_count.count());
    if (days_count.count() != 0) {
      result += fmt::format(" {}D", days_count.count());
    }
    if (hours_count.count() != 0) {
      result += fmt::format(" {}H", hours_count.count());
    }
    return result;
  }
  if (time >= days(1)) {
    auto days_count = duration_cast<days>(time);
    time -= days_count;
    auto hours_count = std::round(duration<double, hours::period>(time).count() * 10) / 10;
    if (hours_count < 0.3) {
      return fmt::format("{}D", days_count.count());
    } else {
      return fmt::format("{}D {:.1}H", days_count.count(), hours_count);
    }
  }
  auto hours_count = duration_cast<hours>(time);
  time -= hours_count;
  auto minutes_count = duration_cast<minutes>(time);
  time -= minutes_count;
  auto seconds_count = duration_cast<seconds>(time);

  if (hours_count.count() != 0) {
    return fmt::format("{}h:{}m:{}s", hours_count.count(), minutes_count.count(), seconds_count.count());
  }
  if (minutes_count.count() != 0) {
    return fmt::format("{}m:{}s", minutes_count.count(), seconds_count.count());
  }

  time -= seconds_count;
  auto milliseconds_count = duration_cast<milliseconds>(time);
  if (seconds_count.count() != 0) {
    return fmt::format("{}.{}s", seconds_count.count(), milliseconds_count.count());
  }

  if (milliseconds_count.count() >= 10) {
    return fmt::format("{}ms", milliseconds_count.count());
  }

  auto nanoseconds_count = duration_cast<nanoseconds>(time);
  return fmt::format("{}us", nanoseconds_count.count());
};

auto TimevalToDuration(timeval t) {
  return std::chrono::seconds(t.tv_sec) + std::chrono::microseconds(t.tv_usec);
}

void EnumerateAC(path config_path) {
  auto start_time = std::chrono::system_clock::now();
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

  std::shared_ptr<ACClasses> initial_classes_version = std::make_shared<ACClasses>(config, &state_dump);


  ACPair trivial_pair{CWord("x"), CWord("y")};

  // these are trivial classes which should be excluded from harvest
  initial_classes_version->AddClass(trivial_pair);

  for (auto i = 1u; i <= 3u; ++i) {
    initial_classes_version->Merge(0, i);
  }

  initial_classes_version->AddClasses(inputs);
  initial_classes_version->RestoreMerges();
  initial_classes_version->RestoreMinimums();

  ACIndex ac_index(config, initial_classes_version);

  //maybe restore the index
  if (fs::exists(config.pairs_classes_in())) {
    std::string next_line;
    auto input = config.ifstream(config.pairs_classes_in());

    auto expected_index_size = 0u;
    auto initial_batch = ac_index.NewBatch();

    while(std::getline(input, next_line)) {
      auto split = next_line.find(' ');
      if (split == std::string::npos) {
        continue;
      }

      auto pair = ACStateDump::LoadPair(next_line.substr(0, split));
      auto ac_class = initial_classes_version->at(std::stoul(next_line.substr(split + 1)));

      initial_batch.Push(pair, ac_class->id_);
      ++expected_index_size;
      initial_classes_version->AddPair(ac_class->id_, pair);
    }

    initial_batch.Execute();
    while (ac_index.GetData().size() < expected_index_size) {
      std::this_thread::yield();
    }
  } else {
    initial_classes_version->InitACIndex(&ac_index);
  }
  initial_classes_version.reset();

  ACPairProcessQueue to_process(config, &state_dump);

  if (fs::exists(config.pairs_queue_in())) {
    // we just restore the queue
    auto queue_input = config.ifstream(config.pairs_queue_in());
    std::string next_line;
    std::regex pairs_queue_in_re(fmt::format("^({}) (\\d+)$", ACStateDump::pair_dump_re));
    std::smatch pair_parsed;
    while (std::getline(queue_input, next_line)) {
      if (!std::regex_match(next_line, pair_parsed, pairs_queue_in_re)) {
        continue;
      }

      ACPair elem = ACStateDump::LoadPair(pair_parsed[1]);
      to_process.Push(elem,
          (std::stoul(pair_parsed[2])
            & static_cast<size_t>(ACStateDump::PairQueueState::AutoNormalized)) != 0);
    }
  } else {
    auto ac_classes = ac_index.GetCurrentACClasses();
    for (auto&& c : *ac_classes) {
      to_process.Push(ac_classes->minimal_in(c.id_), false);
    }
  }

  std::map<crag::CWord::size_type, size_t> lengths_counts;
  auto total_count = 0u;
  auto distinct_count = 0u;

  {
    auto ac_classes = ac_index.GetCurrentACClasses();
    for (auto &&c : *ac_classes) {
      ++total_count;
      if (c.IsPrimary()) {
        ++distinct_count;
        const auto &minimal = ac_classes->minimal_in(c.id_);
        ++lengths_counts[minimal[0].size() + minimal[1].size()];
      }
    }
  }

  fmt::print(std::clog, "Loaded {} classes, {} are distinct now\n", total_count, distinct_count);

  if (distinct_count < 100) {
    auto ac_classes = ac_index.GetCurrentACClasses();
    for (auto&& c : *ac_classes) {
      if (c.IsPrimary() || total_count < 100) {
        c.DescribeForLog(&std::clog);
        std::clog << "\n";
      }
    }
  } else {
    for (auto&& length : lengths_counts) {
      fmt::print(std::clog, "Length {}: {}\n", length.first, length.second);
    }
  }


  ACTasksData data{
      config      , // const Config& config;
      t           , // const Terminator& terminator;
      &to_process , // ACPairProcessQueue* queue;
      &state_dump , // ACStateDump* dump;
      &ac_index     // ACIndex* ac_index;
  };

  Process(data);
  to_process.Terminate();
  std::clog << "Enumeration stopped, wait till dump is synchronized" << std::endl;

  //getting some stats

  struct rusage usage;

  if (getrusage(RUSAGE_SELF, &usage) == -1) {
    throw fmt::SystemError(errno, "Can't query resorse usage");
  }

  auto final_stats = config.ofstream(config.run_stats(), std::ios::app);
  fmt::print(final_stats, "Spent {}\n", ToHumanString(std::chrono::system_clock::now() - start_time));
  fmt::print(final_stats, "{} CPU time in user mode\n", ToHumanString(TimevalToDuration(usage.ru_utime)));
  fmt::print(final_stats, "{} CPU time in kernel mode\n", ToHumanString(TimevalToDuration(usage.ru_stime)));
  fmt::print(final_stats, "Used {} max RAM\n", ToHumanReadableByteCount(static_cast<size_t>(usage.ru_maxrss) * 1024));
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