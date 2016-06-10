//
// Created by dpantele on 5/23/16.
// This program should remove unnecessary duplicates from the dumps
//

#include <iostream>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <regex>

#include "boost_filtering_stream.h"
#include "config.h"
#include "state_dump.h"

#include "external_sort.h"

void ProcessMergesDump(const Config& c) {
  std::clog << "classes_merges.dump.txt" << std::endl;
  // since all merges will be recorded again when classes are restored,
  // we don't need to append anything

  if (!fs::exists(c.classes_merges_dump())) {
    std::clog << c.classes_merges_dump() << " does not exits, skipping" << std::endl;
    return;
  }

  fs::rename(c.classes_merges_dump(), c.classes_merges_in());
}

void ProcessNewWordsDump(const Config& c) {
  std::clog << "classes_minimums.dump.txt" << std::endl;
  if (!fs::exists(c.classes_minimums_dump())) {
    std::clog << c.classes_minimums_dump() << " does not exits, skipping" << std::endl;
    return;
  }
  //here for each class id we need to keep only the last minimal words
  std::map<unsigned long, std::string> min_word;

  BoostFilteringIStream dump = c.ifstream(c.classes_minimums_dump());
  if (dump.fail()) {
    throw fmt::SystemError(errno, "cannot open file '{}' for reading", c.classes_minimums_dump().native());
  }

  auto ParseInput = [&] (std::istream& in) {
    std::string next_line;
    std::regex min_dump_format("^(\\d+) (\\w+ \\w+)");
    std::smatch m;
    while (std::getline(in, next_line)) {
      if (!std::regex_match(next_line, m, min_dump_format)) {
        continue;
      }
      min_word[std::stoul(m[1])] = m[2];
    }
  };

  // we don't need to read current input, it was stored in the dump when we restored ACClasses
  ParseInput(dump);

  dump.reset();
  fs::remove(c.classes_minimums_dump());

  std::ofstream out(c.classes_minimums_in().native(), std::ios_base::trunc);

  if (out.fail()) {
    throw fmt::SystemError(errno, "cannot open file '{}' for writing", c.classes_minimums_in().native());
  }

  for (auto&& elem : min_word) {
    fmt::print(out, "{} {}\n", elem.first, elem.second);
  }
}

void ProcessQueue(const Config& c) {
  std::clog << "pairs_queue.dump.txt.gz" << std::endl;
  if (!fs::exists(c.pairs_queue_dump())) {
    std::clog << c.pairs_queue_dump() << " does not exits, skipping" << std::endl;
    return;
  }


  // queue should be sorted and then compressed so that 2 (pop) owerwrites 1 (push aut-norm) and 0 (push aut)

  auto queue_sorted_path = c.dump_dir() / fs::unique_path("pairs_queue.%%%%-%%%%-%%%%-%%%%.txt.gz");

  ExternalSort(c.pairs_queue_dump(), queue_sorted_path, c.dump_dir(), c.memory_limit_);

  // for now it must be really simple: each pair should have only one push/pop
  // so we find if a pair has a pop operation
  // we also don't try to preserve order since current queue
  // will reorder it anyway
  // in general, we better preserve the order, but it requires ExternalSort with a special
  // keys comparison function
  auto sorted = c.ifstream(queue_sorted_path);
  auto output = c.ofstream(c.pairs_queue_in());

  std::string next_line;
  std::string queue_sump_line_regex_str = std::string("^(") + ACStateDump::pair_dump_re + ") (\\d)$";
  std::regex queue_sump_line(queue_sump_line_regex_str);
  std::smatch parsed;
  ACPair last_pair;
  size_t pair_state = static_cast<size_t>(ACStateDump::PairQueueState::Processed); //we don't write the empty pair back

  while (std::getline(sorted, next_line)) {
    if (!std::regex_match(next_line, parsed, queue_sump_line)) {
      continue;
    }

    ACPair current = ACStateDump::LoadPair(parsed[1]);
    if (last_pair != current) {
      if (!(pair_state & static_cast<size_t>(ACStateDump::PairQueueState::Processed))) {
        ACStateDump::DumpPair(last_pair, &output);
        fmt::print(output, " {}\n", pair_state);
      }
      pair_state = 0;
      last_pair = current;
    }
    pair_state |= std::stoul(parsed[2]);
  }
  if (!(pair_state & static_cast<size_t>(ACStateDump::PairQueueState::Processed))) {
    ACStateDump::DumpPair(last_pair, &output);
    fmt::print(output, " {}\n", pair_state);
  }
  sorted.reset();
  fs::remove(queue_sorted_path);
  fs::remove(c.pairs_queue_dump());
}

path ConcatDumps(const Config& c, path first, path second) {
  auto output_path = c.dump_dir() / boost::filesystem::unique_path(first.stem().native() + "%%%%-%%%%-%%%%-%%%%" + first.extension().c_str());
  if (!fs::exists(first)) {
    if (!fs::exists(second)) {
      auto touch = c.ofstream(output_path);
      return output_path;
    }
    fs::copy_file(second, output_path);
    return output_path;
  }

  fs::copy_file(first, output_path);

  if (!fs::exists(second)) {
    return output_path;
  }

  auto previous = c.ifstream(second);
  auto append_to = c.ofstream(output_path, std::ios::app);

  char buffer[4 << 10];
  while(!previous.fail()) {
    previous.read(buffer, sizeof(buffer));
    append_to.write(buffer, previous.gcount());
  }
  return output_path;
}

void ProcessVertices(const Config& c) {
  std::clog << c.ac_graph_vertices_dump().filename() << std::endl;
  if (!fs::exists(c.ac_graph_vertices_dump())) {
    std::clog << c.ac_graph_vertices_dump() << " does not exits, skipping" << std::endl;
    return;
  }


  // for vertices, we just want to concatenate this file with what we had before, sort it and
  // remove dominating harvests
  path temp_vertices = ConcatDumps(c, c.ac_graph_vertices_in(), c.ac_graph_vertices_dump());

  //then we sort the file
  ExternalSort(temp_vertices, temp_vertices, c.dump_dir(), c.memory_limit_);

  // and now we need to group by word
  {
    ACPair vertex;
    std::set<std::pair<size_t, size_t>> harvests;

    auto input = c.ifstream(temp_vertices);
    auto output = c.ofstream(c.ac_graph_vertices_in());
    auto vertices = c.ofstream(c.ac_graph_vertices_encoding());

    auto DumpVertexHarvest = [&] {
      if (harvests.empty()) {
        return;
      }
      //we need to remove 'dominated' pairs
      auto checked_up_to = std::make_pair(0u, 0u);
      while (1) {
        auto next_pair = harvests.upper_bound(checked_up_to);
        if (next_pair == harvests.end()) {
          break;
        }
        auto compare_with = next_pair;
        ++compare_with;
        bool is_dominated = false;
        while (compare_with != harvests.end()) {
          if (compare_with->first >= next_pair->first && compare_with->second >= next_pair->second) {
            harvests.erase(next_pair);
            is_dominated = true;
            break;
          }
          ++compare_with;
        }
        if (!is_dominated) {
          checked_up_to = *next_pair;
        }
      }

      // and now we write down what remained
      ACStateDump::DumpPair(vertex, &vertices);
      fmt::print(vertices, " {} {}\n", ToString(vertex[0]), ToString(vertex[1]));

      for (auto&& harvest : harvests) {
        ACStateDump::DumpPair(vertex, &output);
        fmt::print(output, " {} {}\n", harvest.first, harvest.second);
      }
    };

    std::string next_line;
    std::regex vertices_format(fmt::format("^({}) (\\d+) (\\d+)$", ACStateDump::pair_dump_re));
    std::smatch parsed;

    while (std::getline(input, next_line)) {
      if (!std::regex_match(next_line, parsed, vertices_format)) {
        continue;
      }

      auto current = ACStateDump::LoadPair(parsed[1]);
      if (current != vertex) {
        DumpVertexHarvest();
        vertex = current;
        harvests.clear();
      }

      harvests.emplace(std::stoul(parsed[2]), std::stoul(parsed[3]));
    }
    DumpVertexHarvest();
  }
  fs::remove(temp_vertices);
  fs::remove(c.ac_graph_vertices_dump());
}

void ProcessEdges(const Config& c) {
  std::clog << c.ac_graph_edges_dump().filename() << std::endl;
  if (!fs::exists(c.ac_graph_edges_dump())) {
    std::clog << c.ac_graph_edges_dump() << " does not exits, skipping" << std::endl;
    return;
  }

  //group edges by the origin
  path temp_edges = ConcatDumps(c, c.ac_graph_edges_in(), c.ac_graph_edges_dump());
  ExternalSort(temp_edges, temp_edges, c.dump_dir(), c.memory_limit_);

  auto input = c.ifstream(temp_edges);
  auto output = c.ofstream(c.ac_graph_edges_in());
  std::string origin;
  std::vector<std::string> termini;
  std::string current_line;

  std::regex terminus_regex(fmt::format(" {} [ha][01]", ACStateDump::pair_dump_re));

  std::sregex_iterator current_terminus;
  auto termini_end = std::sregex_iterator();

  while (std::getline(input, current_line)) {
    auto origin_end = current_line.find(' ');
    if (origin_end == std::string::npos || origin_end == 0) {
      continue;
    }

    fmt::StringRef current_origin(current_line.c_str(), origin_end);
    if (current_origin != origin) {
      if (!termini.empty()) {
        output.write(origin.c_str(), origin.size());

        std::sort(termini.begin(), termini.end());
        termini.erase(std::unique(termini.begin(), termini.end()), termini.end());

        for(auto&& t : termini) {
          output.write(t.c_str(), t.size());
        }

        output.put('\n');
        termini.clear();
      }
      origin = current_origin.to_string();
    }

    current_terminus = std::sregex_iterator(current_line.begin() + origin_end, current_line.end(), terminus_regex);
    while (current_terminus != termini_end) {
      termini.emplace_back(current_terminus->str());
      ++current_terminus;
    }
  }
  input.reset();
  fs::remove(temp_edges);
  fs::remove(c.ac_graph_edges_dump());
}

void ProcessPairsClasses(const Config& c) {
  std::clog << c.pairs_classes_dump().filename() << std::endl;
  if (!fs::exists(c.pairs_classes_dump())) {
    std::clog << c.pairs_classes_dump() << " does not exits, skipping" << std::endl;
    return;
  }

  //we keep only the first time when a pair appears here
  ExternalSort(c.pairs_classes_dump(), c.pairs_classes_dump(), c.dump_dir(), c.memory_limit_);

  auto input = c.ifstream(c.pairs_classes_dump());
  auto output = c.ofstream(c.pairs_classes_in());

  std::string previous_pair;
  std::string current_line;
  while (std::getline(input, current_line)) {
    auto current_pair_end = current_line.find(' ');
    if (current_pair_end == std::string::npos) {
      continue;
    }

    auto current_pair = current_line.substr(0, current_pair_end);

    if (previous_pair == current_pair) {
      continue;
    }

    output.write(current_line.c_str(), current_line.size());
    output.put('\n');

    previous_pair = std::move(current_pair);
  }
  input.reset();
  fs::remove(c.pairs_classes_dump());
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " [run_config.json]";
    return -1;
  }

  Config c;
  c.LoadFromJson(path(argv[1]));

  ProcessMergesDump(c);
  ProcessNewWordsDump(c);
  ProcessVertices(c);
  ProcessEdges(c);
  ProcessQueue(c);
  ProcessPairsClasses(c);
}
