#pragma once
#ifndef CRAG_NAIVE_FOLDED_GRAPH2_H_
#define CRAG_NAIVE_FOLDED_GRAPH2_H_

#include <limits>

#include <array>
#include <cassert>
#include <cstdint>
#include <deque>
#include <iostream>
#include <set>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include <compressed_word/compressed_word.h>

namespace crag {

inline namespace naive {

namespace {

int64_t GCD(int64_t a, int64_t b) {
  return a == 0 ? b : GCD(b % a, a);
}

int64_t WeightMod(int64_t a, int64_t b) {
  return b == 0 ? a : (((a % b) + b) % b);
}

}

//! Naive and slow implementation of FoldedGraph2
class NaiveFoldedGraph2 {
 public:
  typedef CWord Word;

  //! Reference constant to be used as a size of the alphabet
  static const size_t kAlphabetSize = Word::kAlphabetSize;

  typedef Word::Letter Label; //!< Labels of edges
  typedef int64_t Weight;      //!< Weight of edges

  using Vertex = uint64_t;

  struct Edge {
    std::tuple<Vertex, Label, Vertex> data_;
    template<typename ... Args>
    Edge(Args&&... args)
        : data_(std::forward<Args>(args)...)
    { }

    bool operator<(const Edge& other) const {
      return data_ < other.data_;
    }

    Edge Inverse() const {
      return Edge(std::get<2>(data_), std::get<1>(data_).Inverse(), std::get<0>(data_));
    }
    Vertex from() const {
      return std::get<0>(data_);
    }
    Label label() const {
      return std::get<1>(data_);
    }
    Vertex to() const {
      return std::get<2>(data_);
    }

    static Edge min_edge(Vertex v) {
      return Edge(v, 0, std::numeric_limits<Vertex>::min());
    }
    static Edge max_edge(Vertex v) {
      return Edge(v, kAlphabetSize * 2 - 1, std::numeric_limits<Vertex>::max());
    }
    static Edge min_edge(Vertex v, Label l) {
      return Edge(v, l, std::numeric_limits<Vertex>::min());
    }
    static Edge max_edge(Vertex v, Label l) {
      return Edge(v, l, std::numeric_limits<Vertex>::max());
    }
  };

  std::multimap<Edge, Weight> edges_;


  static Vertex GetOrigin(const std::pair<Edge, Weight>& e) {
    return e.first.from();
  }
  static Label GetLabel(const std::pair<Edge, Weight>& e) {
    return e.first.label();
  }
  static Vertex GetTerminus(const std::pair<Edge, Weight>& e) {
    return e.first.to();
  }
  static Vertex GetWeight(const std::pair<Edge, Weight>& e) {
    return e.second;
  }

  Vertex last_vertex_ = 0;
  Weight modulus_ = 0;

  Vertex root() const {
    return 0;
  }

  Vertex ReadWord(Vertex from, Word* to_read, Weight* read_weight) const {
    if (to_read->Empty()) {
      return from;
    }
    auto next = to_read->GetFront();
    to_read->PopFront();

    auto edge = edges_.lower_bound(Edge::min_edge(from, next));
    *read_weight += GetWeight(*edge);
    return ReadWord(GetTerminus(*edge), to_read, read_weight);
  }

  Vertex AddEdge(Vertex from, Label l, Vertex to, Weight w) {
    edges_.emplace(Edge(from, l, to), w);
    edges_.emplace(Edge(to, l.Inverse(), from), -w);
    return to;
  }

  Vertex AddEdge(Vertex from, Label l, Weight w) {
    return AddEdge(from, l, ++last_vertex_, w);
  }


  Vertex PushWord(Vertex from, Word* to_push, Weight push_weight) {
    if (to_push->Empty()) {
      return from;
    }

    auto next = to_push->GetFront();
    to_push->PopFront();

    return PushWord(AddEdge(from, next, push_weight), to_push, 0);
  }

  Vertex CombineVertices(Vertex parent, Vertex child) {
    auto current_edge = edges_.begin();
    while (current_edge != edges_.end()) {
      if (GetOrigin(*current_edge) == child || GetTerminus(*current_edge) == child) {
        auto from = (GetOrigin(*current_edge) == child ? parent : GetOrigin(*current_edge));
        auto to = (GetTerminus(*current_edge) == child ? parent : GetTerminus(*current_edge));

        edges_.emplace(Edge(from, GetLabel(*current_edge), to), GetWeight(*current_edge));
        current_edge = edges_.erase(current_edge);
      } else {
        ++current_edge;
      }
    }

    return parent;
  }

  bool WeightsEqual(Weight w1, Weight w2) const {
    return WeightMod(w1- w2, modulus_) == 0;
  }

  void EnsureEqual(Weight w1, Weight w2) {
    if (!WeightsEqual(w1, w2)) {
      modulus_ = std::abs(GCD(modulus_, WeightMod(w1- w2, modulus_)));
    }
  }

  Vertex PushCycle(Vertex root, Word to_push, Weight weight) {
    return PushCycle(root, &to_push, weight);
  }

  Vertex PushCycle(Vertex root, Word* to_push, Weight weight) {
    if (to_push->Empty()) {
      EnsureEqual(weight, 0);
      return root;
    }

    auto v = PushWord(root, to_push, weight);
    return CombineVertices(root, v);
  }

  void ShiftWeight(Vertex v, Weight shift) {
    auto edge = std::make_pair(
        edges_.lower_bound(Edge::min_edge(v)),
        edges_.upper_bound(Edge::max_edge(v))
    );

    while (edge.first != edge.second) {
      auto current_range = edges_.equal_range(edge.first->first);
      while (current_range.first != current_range.second) {
        current_range.first->second += shift;
        ++current_range.first;
      }
      auto inverse_range = edges_.equal_range(edge.first->first.Inverse());
      while (inverse_range.first != inverse_range.second) {
        inverse_range.first->second -= shift;
        ++inverse_range.first;
      }
      edge.first = current_range.second;
    }
  }

  bool FoldOne() {
    if (edges_.size() < 1) {
      return false;
    }

    auto current = edges_.begin();
    auto next = std::next(current);

    while (next != edges_.end()) {
      if (GetOrigin(*current) == GetOrigin(*next) && GetLabel(*current) == GetLabel(*next)) {
        if (GetTerminus(*current) == GetTerminus(*next)) {
          //next line does not change modulus if weights are equal
          EnsureEqual(GetWeight(*current), GetWeight(*next));
          next = edges_.erase(next);
        } else {
          //w2 = w1 - (w1 - w2)
          ShiftWeight(GetTerminus(*current), WeightMod(GetWeight(*current) - GetWeight(*next), modulus_));
          assert(WeightsEqual(GetWeight(*current), GetWeight(*next)));

          CombineVertices(GetTerminus(*current), GetTerminus(*next));
          return true;
        }
      } else {
        ++current;
        ++next;
      }
    }

    return false;
  }

  void Fold() {
    while(FoldOne()) { }
  }




  void Harvest(size_t k, Vertex v1, Vertex v2, Weight weight, std::vector<Word>* result) const {
    std::deque<std::tuple<Vertex, Word, Weight>> current_path = {std::make_tuple(v1, Word{ }, 0)};

    while (!current_path.empty()) {
      Vertex v;
      Word w;
      Weight c;
      std::tie(v, w, c) = current_path.front();
      current_path.pop_front();
      if (v == v2 && (WeightsEqual(c, weight))) {
        if (v1 != v2 || (w.Empty() || w.GetFront() != w.GetBack().Inverse())) {
          result->push_back(w);
        }
      }

      if (w.size() >= k) {
        continue;
      }

      auto v_edges = std::make_pair(
          edges_.lower_bound(Edge::min_edge(v)),
          edges_.upper_bound(Edge::max_edge(v))
      );

      while (v_edges.first != v_edges.second) {
        auto& edge = *v_edges.first;
        if (!w.Empty() && w.GetBack() == GetLabel(edge).Inverse()) {
          ++v_edges.first;
          continue;
        }
        Word next_word = w;
        next_word.PushBack(GetLabel(edge));
        current_path.emplace_back(GetTerminus(edge), next_word, c + GetWeight(edge));

        ++v_edges.first;
      }
    }
  }

  std::vector<Word> Harvest(size_t k, Vertex v1, Vertex v2, Weight weight = 0) const {
    std::vector<Word> result;
    Harvest(k, v1, v2, weight, &result);
    std::sort(result.begin(), result.end());
    auto unique_end = std::unique(result.begin(), result.end());
    result.erase(unique_end, result.end());
    return result;
  }

  std::vector<Word> Harvest(size_t k, Weight w) const {
    std::vector<Word> result;
    for(auto v = 0u; v <= last_vertex_; ++v) {
      Harvest(k, v, v, w, &result);
    }
    std::sort(result.begin(), result.end());
    auto unique_end = std::unique(result.begin(), result.end());
    result.erase(unique_end, result.end());
    return result;
  }
};

//inline bool NaiveFoldedGraph2::Edge::Fold(Edge* e, NaiveFoldedGraph2::Weight* modulus) {
//  assert(origin_ == e->origin_);
//
//  if (l_ != e->l_) {
//    return false;
//  }
//
//  auto weight_diff = WeightMod(w_ - e->w_, *modulus);
//
//  if (endpoint_ != e->endpoint_) {
//    endpoint_.ShiftWeight(weight_diff);
//    e->endpoint_.ReplaceWith(&endpoint_);
//  } else {
//    *modulus = GCD(*modulus, weight_diff);
//    if (*modulus < 0) {
//      *modulus *= -1;
//    }
//  }
//
//  assert(WeightMod(w_ - e->w_, *modulus) == 0);
//
//  //ok, now remove inverse of e and e
//  origin_.Erase(e);
//
//  return true;
//}
//
} //namespace naive

} //namespace crag

#endif //CRAG_NAIVE_FOLDED_GRAPH2_H_
