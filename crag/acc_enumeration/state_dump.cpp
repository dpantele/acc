//
// Created by dpantele on 5/25/16.
//

#include "acc_class.h"
#include "state_dump.h"

#include <fmt/ostream.h>

namespace io = boost::iostreams;

void InitOstream(const Config& c, std::initializer_list<std::pair<path, BoostFilteringOStream*>> to_init) {
  for (auto&& stream : to_init) {
    if (fs::exists(stream.first)) {
      throw std::runtime_error(".dump files were not post-processed or deleted after a run");
    }
  }

  for (auto&& stream : to_init) {
    *stream.second = c.ofstream(stream.first);
    if (stream.second->fail()) {
      throw fmt::SystemError(errno, "Can\'t open {}", stream.first);
    }
  }
}

ACStateDump::ACStateDump(const Config& c) {
  fs::create_directories(c.dump_dir());

  InitOstream(c, {
      {c.classes_merges_dump(), &classes_merges_out_},
      {c.classes_minimums_dump(), &classes_minimums_out_},
      {c.ac_graph_vertices_dump(), &ac_graph_vertex_harvest_},
      {c.ac_graph_edges_dump(), &ac_graph_edges_},
      {c.pairs_queue_dump(), &pairs_queue_},
      {c.pairs_classes_dump(), &ac_pair_class_},
  });
}

void ACStateDump::Merge(const ACClass& a, const ACClass& b) {
  fmt::print(classes_merges_out_, "{} {}\n", a.id_, b.id_);
}

void ACStateDump::NewMinimum(const ACClass& c, const ACPair& p) {
  fmt::print(classes_minimums_out_, "{} {} {}\n", c.id_, ToString(p[0]), ToString(p[1]));
}

void ACStateDump::DumpVertexHarvest(const ACPair& v, unsigned int harvest_limit, unsigned int complete_count) {
  DumpPair(v, &this->ac_graph_vertex_harvest_);
  fmt::print(ac_graph_vertex_harvest_, " {:2} {}\n", harvest_limit, complete_count);
}

void ACStateDump::DumpPair(const ACPair& p, std::ostream* out) {
  auto first_dump = p[0].GetDump();
  auto second_dump = p[1].GetDump();

  fmt::print(*out, "{:02}:{:02}:{:02}:{:x}:{:x}", p.length(), first_dump.length, second_dump.length, first_dump.letters, second_dump.letters);
}



void ACStateDump::DumpPair(const ACPair& p, fmt::MemoryWriter* out) {
  auto first_dump = p[0].GetDump();
  auto second_dump = p[1].GetDump();

  out->write("{:02}:{:02}:{:02}:{:x}:{:x}", p.length(), first_dump.length, second_dump.length, first_dump.letters, second_dump.letters);
}

void ACStateDump::DumpHarvestEdge(const ACPair& from, const ACPair& to, bool from_is_flipped) {
  if (from != last_origin_) {
    last_origin_ = from;
    ac_graph_edges_.put('\n');
    DumpPair(from, &this->ac_graph_edges_);
  }
  fmt::print(ac_graph_edges_, " ");
  DumpPair(to, &this->ac_graph_edges_);
  fmt::print(ac_graph_edges_, " h{:d}", from_is_flipped);
}

void ACStateDump::DumpAutomorphEdge(const ACPair& from, const ACPair& to, bool inverse) {
  assert(inverse ? from < to : from > to);
  if (from != last_origin_) {
    last_origin_ = from;
    ac_graph_edges_.put('\n');
    DumpPair(from, &this->ac_graph_edges_);
  }
  fmt::print(ac_graph_edges_, " ");
  DumpPair(to, &this->ac_graph_edges_);
  fmt::print(ac_graph_edges_, " a{:d}", inverse);
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
  DumpPair(pair, &pairs_queue_);
  fmt::print(pairs_queue_, " {} {:x}\n", static_cast<int>(state), ++queue_index_);
}

void ACStateDump::DumpPairClass(const ACPair& p, const ACClass& c) {
  DumpPair(p, &ac_pair_class_);
  fmt::print(ac_pair_class_, " {}\n", c.id_);
}

constexpr const char* ACStateDump::pair_dump_re;














