//
// Created by dpantele on 5/23/16.
//

#ifndef ACC_CONFIG_H
#define ACC_CONFIG_H

#include <algorithm>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>

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

  Config()
      : base_dir_(boost::filesystem::current_path())
  { }

  std::string ConfigAsString() const {
    json dump;
    // base_dir is not dumped, because in general it should be derived from the config path
    dump["dump_dir"] = dump_dir_.generic_string();
    dump["dump_memory_limit"] = ToHumanReadableByteCount(memory_limit_);
    dump["dump_queue_limit"] = std::to_string(dump_queue_limit_);
    dump["input"] = input_.generic_string();
    return dump.dump(4);
  }

  void DumpConfig() const;

  void LoadFromJson(const json& config) {
    ConfigFromJson(config, "base_dir", &base_dir_);
    ConfigFromJson(config, "input", &input_);
    ConfigFromJson(config, "dump_dir", &dump_dir_);

    std::string temp;
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
  }

  void LoadFromJson(path json_path);

 private:
  static void ConfigFromJson(const json& config, const char* name, path* var) {
    auto val = config.find(name);
    if (val != config.end()) {
      *var = val->get<std::string>();
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
