//
// Created by dpantele on 5/23/16.
//

#ifndef ACC_CONFIG_H
#define ACC_CONFIG_H

#include <algorithm>
#include <iomanip>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <thread>

#include "boost_filtering_stream.h"
#include "convert_byte_count.h"

using boost::filesystem::path;
using json = nlohmann::json;
namespace fs = boost::filesystem;
namespace io = boost::iostreams;


/*
 * The following structure for data will be used:
 *  - prefix
 *    - acc.conf.json
 *    - initial_presentations.txt
 *    - dumps
 *      - classes_merges.txt
 *      - classes_minimums.txt(.gz)?
 *      - ac_graph.txt.gz
 *    - stats
 *      - time.txt
 *      - workers.txt
 *      - queue.txt
 *
 */
struct Config {

  //! The base path for all data files, current_path by default
  path base_dir_ = ".";

  //! Path to the config file, with which this config is synchronised
  path config_path_ = "./acc.conf.json";

  //! Location of various files keeping the state of computation
  path dump_dir_ = "./dumps";

  //! Statistics anout the program execution
  path stats_dir_ = "./stats";

  //! Input file with a list of pairs to process
  path input_ = "./initial_presentations.txt";

  path input() const {
    if (input_.is_absolute()) {
      return input_;
    }
    return base_dir_ / input_;
  }

  path dump_dir() const {
    if (dump_dir_.is_absolute()) {
      return dump_dir_;
    }
    return base_dir_ / dump_dir_;
  }

  path classes_merges_in() const {
    return dump_dir() / path("classes_merges.in.txt");
  }

  path classes_merges_dump() const {
    return dump_dir() / path("classes_merges.dump.txt");
  }

  path classes_minimums_in() const {
    return dump_dir() / path("classes_minimums.in.txt");
  }

  path classes_minimums_dump() const {
    return dump_dir() / path("classes_minimums.dump.txt.gz");
  }

  path ac_graph_vertices_encoding() const {
    return dump_dir() / path("ac_graph_vertices_encoding.txt.gz");
  }

  path ac_graph_vertices_in() const {
    return dump_dir() / path("ac_graph_vertices.in.txt.gz");
  }

  path ac_graph_vertices_dump() const {
    return dump_dir() / path("ac_graph_vertices.dump.txt.gz");
  }

  path ac_graph_edges_in() const {
    return dump_dir() / path("ac_graph_edges.in.txt.gz");
  }

  path ac_graph_edges_dump() const {
    return dump_dir() / path("ac_graph_edges.dump.txt.gz");
  }

  path pairs_queue_in() const {
    return dump_dir() / path("pairs_queue.in.txt.gz");
  }

  path pairs_queue_dump() const {
    return dump_dir() / path("pairs_queue.dump.txt.gz");
  }

  path pairs_classes_in() const {
    return dump_dir() / path("pairs_classes.in.txt.gz");
  }

  path pairs_classes_dump() const {
    return dump_dir() / path("pairs_classes.dump.txt.gz");
  }

  path stats_dir() const {
    if (stats_dir_.is_absolute()) {
      return stats_dir_;
    }
    return base_dir_ / stats_dir_;
  }

  bool should_clear_dumps_ = false;
  bool should_clear_dumps() const {
    return should_clear_dumps_;
  }

  enum class StatsToStdout {
    kNo,
    kShort,
    kFull
  } stats_to_stout_ = StatsToStdout::kNo;

  char stats_startime[20];

  path ac_move_stats() const {
    return stats_dir() / path(fmt::format("{}.ac_move_stats.txt.gz", stats_startime));
  }

  path run_stats() const {
    return stats_dir() / path(fmt::format("{}.run.txt", stats_startime));
  }

  // simple wrappers which create the appropriate chain
  BoostFilteringIStream ifstream(const path& p, std::ios_base::openmode mode) const;
  BoostFilteringIStream ifstream(const path& p) const {
    return this->ifstream(p, std::ios_base::in);
  }
  BoostFilteringOStream ofstream(const path& p, std::ios_base::openmode mode) const;
  BoostFilteringOStream ofstream(const path& p) const {
    return this->ofstream(p, std::ios_base::out);
  }

  size_t memory_limit_ = 0u;

  size_t dump_queue_limit_ = (1u << 13);

  size_t workers_count_ = std::thread::hardware_concurrency();

  static constexpr float kFractionNotUsed = -1.0f;
  float workers_count_fraction_ = kFractionNotUsed;

  Config()
      : base_dir_(boost::filesystem::current_path())
  {
    std::time_t now = std::time(nullptr);
    std::strftime(stats_startime, sizeof(stats_startime), "%Y-%m-%d.%H-%M-%S", std::localtime(&now));
  }

  std::string ConfigAsString() const {
    json dump;
    // base_dir is not dumped, because in general it should be derived from the config path
    dump["dump_dir"] = dump_dir_.generic_string();
    dump["dump_memory_limit"] = ToHumanReadableByteCount(memory_limit_);
    dump["dump_queue_limit"] = std::to_string(dump_queue_limit_);
    dump["input"] = input_.generic_string();
    dump["stats_dir"] = stats_dir_.generic_string();
    dump["should_clear_dumps"] = should_clear_dumps_;

    switch(stats_to_stout_) {
      case StatsToStdout::kNo:
        dump["stats_to_stdout"] = "no";
        break;
      case StatsToStdout::kShort:
        dump["stats_to_stdout"] = "short";
        break;
      case StatsToStdout::kFull:
        dump["stats_to_stdout"] = "full";
        break;
    }

    if (workers_count_fraction_ == kFractionNotUsed) {
      dump["workers_count"] = std::to_string(workers_count_);
    } else {
      dump["workers_count"] = fmt::format("{:.3}", workers_count_fraction_);
    }
    return dump.dump(4);
  }

  void DumpConfig() const;

  void LoadFromJson(const json& config) {
    ConfigFromJson(config, "base_dir", &base_dir_);
    ConfigFromJson(config, "input", &input_);
    ConfigFromJson(config, "dump_dir", &dump_dir_);
    ConfigFromJson(config, "stats_dir", &stats_dir_);
    ConfigFromJson(config, "should_clear_dumps", &should_clear_dumps_);

    std::string temp;
    ConfigFromJson(config, "stats_to_stdout", &temp);
    if (!temp.empty()) {
      if (temp == "short") {
        stats_to_stout_ = StatsToStdout::kShort;
      }
      if (temp == "full") {
        stats_to_stout_ = StatsToStdout::kFull;
      }
      temp.clear();
    }

    ConfigFromJson(config, "dump_memory_limit", &temp);
    if (!temp.empty()) {
      memory_limit_ = FromHumanReadableByteCount(temp);
      temp.clear();
    }

    ConfigFromJson(config, "dump_queue_limit", &temp);
    if (!temp.empty()) {
      dump_queue_limit_ = std::stoul(temp);
      if ((dump_queue_limit_ & (dump_queue_limit_ - 1)) != 0u) {
        throw std::runtime_error("dump_queue_limit must be a power of 2");
      }
      temp.clear();
    }

    ConfigFromJson(config, "workers_count", &temp);
    if (!temp.empty()) {
      if (temp.find('.') != std::string::npos) {
        workers_count_fraction_ = std::stof(temp);
        if (workers_count_fraction_ > 1.5 || workers_count_fraction_ <= 0) {
          throw std::runtime_error(fmt::format("It is likely a mistake to put {} as a workers count hardware concurrency fraction", workers_count_fraction_));
        }
        workers_count_ = static_cast<size_t>(std::lround(workers_count_fraction_ * std::thread::hardware_concurrency()));
      } else {
        workers_count_fraction_ = kFractionNotUsed;
        workers_count_ = std::stoul(temp);
      }
      temp.clear();
    }
  }

  void LoadFromJson(path json_path);

 private:
  static void ConfigFromJson(const json& config, const char* name, path* var) {
    auto val = config.find(name);
    if (val != config.end()) {
      *var = val->get<std::string>();
    }
  }

  static void ConfigFromJson(const json& config, const char* name, bool* var) {
    auto val = config.find(name);
    if (val != config.end()) {
      *var = val->get<bool>();
    }
  }

  static void ConfigFromJson(const json& config, const char* name, std::string* var) {
    auto val = config.find(name);
    if (val != config.end()) {
      *var = val->get<std::string>();
    }
  }

};

#endif //ACC_CONFIG_H
