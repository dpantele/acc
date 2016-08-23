//
// Created by dpantele on 5/25/16.
//

#include "acc_class.h"
#include "state_dump.h"

#include <fmt/ostream.h>

namespace io = boost::iostreams;

static void InitOstream(const Config& c, std::initializer_list<std::pair<path, BoostFilteringOStream*>> to_init) {
  for (auto&& stream : to_init) {
    if (!c.should_clear_dumps() && fs::exists(stream.first)) {
      throw std::runtime_error(".dump files were not post-processed or deleted after a run");
    }
  }

  for (auto&& stream : to_init) {
    *stream.second = c.ofstream(stream.first, std::ios_base::out | std::ios_base::trunc);
    if (stream.second->fail()) {
      throw fmt::SystemError(errno, "Can\'t open {}", stream.first);
    }
  }
}

ACStateDump::ACStateDump(const Config& c)
  : write_tasks_(c.dump_queue_limit_)
{
  fs::create_directories(c.dump_dir());

  InitOstream(c, {
      {c.classes_merges_dump(), &classes_merges_out_},
      {c.classes_minimums_dump(), &classes_minimums_out_},
      {c.ac_graph_vertices_dump(), &ac_graph_vertex_harvest_},
      {c.ac_graph_edges_dump(), &ac_graph_edges_},
      {c.pairs_queue_dump(), &pairs_queue_},
      {c.pairs_classes_dump(), &ac_pair_class_},
  });

  writer_ = std::thread([this] {
    ACStateDump::WriteTask task;
    while (write_tasks_.Pop(task)) {
      task.out_->write(task.data_.c_str(), task.data_.size());
    }
//    std::clog << "Max queue size: " << write_tasks_.MaxCount()
//        << "\nCount times blocked: " << write_tasks_.MaxCountIsSizeTimes() << "\n";
  });
}

ACStateDump::~ACStateDump() {
  write_tasks_.Close();
  writer_.join();
}

void ACStateDump::Merge(const ACClass& a, const ACClass& b) {
  Write(classes_merges_out_, [&](fmt::MemoryWriter& data) {
    data.write("{} {}\n", a.id_, b.id_);
  });
}

void ACStateDump::NewMinimum(const ACClass& c, const ACPair& p) {
  Write(classes_minimums_out_, [&](fmt::MemoryWriter& data) {
    data.write("{} {} {}\n", c.id_, ToString(p[0]), ToString(p[1]));
  });
}

void ACStateDump::DumpVertexHarvest(const ACPair& v, unsigned int harvest_limit, unsigned int complete_count) {
  Write(ac_graph_vertex_harvest_, [&](fmt::MemoryWriter& data) {
    DumpPair(v, &data);
    data.write(" {:2} {}\n", harvest_limit, complete_count);
  });
}

static constexpr unsigned LengthToWidth(unsigned short length) {
  //length is actually width in base-4 system
  //each base-16 digit is 2 base-4 digit
  //hence the width is
  return length == 0 ? 1u : (length + 1u) / 2u;
}

void ACStateDump::DumpPair(const ACPair& p, std::ostream* out) {
  auto first_dump = p[0].GetDump();
  auto second_dump = p[1].GetDump();

  fmt::print(*out,
      "{0:02}:{1:02}:{2:02}:{3:0{4}x}:{5:0{6}x}",
      p.length(),
      first_dump.length, second_dump.length,
      first_dump.letters, LengthToWidth(first_dump.length),
      second_dump.letters, LengthToWidth(second_dump.length));
}

void ACStateDump::DumpPair(const ACPair& p, fmt::MemoryWriter* out) {
  auto first_dump = p[0].GetDump();
  auto second_dump = p[1].GetDump();

  out->write(
      "{0:02}:{1:02}:{2:02}:{3:0{4}x}:{5:0{6}x}",
      p.length(),
      first_dump.length, second_dump.length,
      first_dump.letters, LengthToWidth(first_dump.length),
      second_dump.letters, LengthToWidth(second_dump.length));
}

void ACStateDump::DumpHarvestEdge(const ACPair& from, const ACPair& to, bool from_is_flipped) {
  Write(ac_graph_edges_, [&](fmt::MemoryWriter& data) {
    DumpPair(from, &data);
    data.write(" ");
    DumpPair(to, &data);
    data.write(" h{:d}\n", from_is_flipped);
  });
}

void ACStateDump::DumpHarvestEdges(const ACPair& from, const std::vector<ACPair>& to, bool from_is_flipped) {
  Write(ac_graph_edges_, [&](fmt::MemoryWriter& data) {
    DumpPair(from, &data);
    for (auto&& p : to) {
      if (from == p) {
        continue;
      }
      data.write(" ");
      DumpPair(p, &data);
      data.write(" h{:d}", from_is_flipped);
    }
    data.write("\n");
  });
}

void ACStateDump::DumpAutomorphEdge(const ACPair& from, const ACPair& to, bool inverse) {
  assert(inverse ? from < to : from > to);
  Write(ac_graph_edges_, [&](fmt::MemoryWriter& data) {
    DumpPair(from, &data);
    data.write(" ");
    DumpPair(to, &data);
    data.write(" a{:d}\n", inverse);
  });
}

ACPair ACStateDump::LoadPair(const std::string& pair_dump) {
  const char* parse_pos = pair_dump.c_str();

  //first 3 chars is the length of tuple
  parse_pos += 3;

  char* length_end;
  auto first_length = strtoull(parse_pos, &length_end, 10);
  assert(first_length == static_cast<unsigned short>(first_length));
  assert(*length_end == ':');
  parse_pos = length_end + 1;

  auto second_length = strtoull(parse_pos, &length_end, 10);
  assert(second_length == static_cast<unsigned short>(second_length));
  assert(*length_end == ':');
  parse_pos = length_end + 1;

  auto first_word = strtoull(parse_pos, &length_end, 16);
  assert(*length_end == ':');
  parse_pos = length_end + 1;

  auto second_word = strtoull(parse_pos, &length_end, 16);

  return crag::CWordTuple<2>{
      crag::CWord(crag::CWord::Dump{static_cast<unsigned short>(first_length), first_word}),
      crag::CWord(crag::CWord::Dump{static_cast<unsigned short>(second_length), second_word})};
}

void ACStateDump::DumpPairQueueState(const ACPair& pair, PairQueueState state) {
  Write(pairs_queue_, [&](fmt::MemoryWriter& data) {
    DumpPair(pair, &data);
    data.write(" {}\n", static_cast<int>(state));
  });
}

void ACStateDump::DumpPairClass(const ACPair& p, const ACClass& c) {
  Write(ac_pair_class_, [&](fmt::MemoryWriter& data) {
    DumpPair(p, &data);
    data.write(" {}\n", c.id_);
  });
}

constexpr const char* ACStateDump::pair_dump_re;














