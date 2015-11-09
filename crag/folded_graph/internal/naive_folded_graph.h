#pragma once
#ifndef CRAG_NAIVE_FOLDED_GRAPH2_H_
#define CRAG_NAIVE_FOLDED_GRAPH2_H_

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

//! Naive and slow implementatoin of FoldedGraph2
class NaiveFoldedGraph2 {
 public:
  typedef CWord Word;

  //! Reference constant to be used as a size of the alphabet
  static const size_t kAlphabetSize = Word::kAlphabetSize;

  typedef Word::Letter Label; //!< Labels of edges

  typedef int64_t Weight;      //!< Weight of edges

  //types for the public interface
  struct Vertex;
  struct Edge;

  using VertexData = std::multimap<Label, std::unique_ptr<Edge>>;
  using EdgeIterator = VertexData::iterator;

  struct Edge {
    Vertex& origin_;
    Vertex& endpoint_;
    Weight w_;
    Label l_;

    EdgeIterator edge_position_;
    EdgeIterator inverse_position_;

    Edge& inverse() {
      return *inverse_position_->second;
    }

    Edge(Vertex& o, Vertex& e, Weight w, const Label& l) : origin_(o), endpoint_(e), w_(w), l_(l) { }

    void Init(EdgeIterator edge_position, EdgeIterator inverse_position) {
      assert(inverse_position->second->endpoint_ == origin_);
      assert(inverse_position->second->origin_ == endpoint_);
      assert(inverse_position->second->w_ == -w_);
      assert(inverse_position->second->l_ == l_.Inverse());

      edge_position_ = edge_position;
      inverse_position_ = inverse_position;
      inverse_position->second->edge_position_ = inverse_position;
      inverse_position->second->inverse_position_ = edge_position;
    }

    bool Fold(Edge* e, Weight* modulus);
  };


  struct Vertex {
    std::shared_ptr<VertexData> edges_;
    size_t id_ = 0;

    Vertex* next_ = nullptr;
    Vertex* first_;

    Vertex(const Vertex&) = delete;
    Vertex(Vertex&&) = delete;
    Vertex()
      : edges_(std::make_shared<VertexData>())
      , first_(this)
    { }

    void ConnectTo(Vertex* e, const Label& l, Weight w) {
      auto edge = edges_->emplace(l, std::unique_ptr<Edge>(new Edge(*this, *e, w, l)));
      auto inverse = e->edges_->emplace(l.Inverse(), std::unique_ptr<Edge>(new Edge(*e, *this, -w, l.Inverse())));

      edge->second->Init(edge, inverse);
    }

    void ShiftWeight(Weight d) {
      for (auto&& edge : *edges_) {
        edge.second->w_ += d;
        edge.second->inverse().w_ -= d;
      }
    }

    void ReplaceWith(Vertex* v) {
      assert(*this != *v);
      for (auto&& edge : *edges_) {
        auto moved_edge = v->edges_->insert(std::move(edge));
        moved_edge->second->Init(moved_edge, moved_edge->second->inverse_position_);
      }

      Vertex* eq = first_;
      Vertex* last = first_;
      while (eq != nullptr) {
        eq->edges_ = v->edges_;
        last = eq;
        eq = eq->next_;
      }

      last->next_ = v->first_;
      eq = v->first_;
      while (eq != nullptr) {
        eq->first_ = first_;
        eq = eq->next_;
      }
    }

    const Vertex& ReadWord(Word* to_read, Weight* read_weight) const {
      if (to_read->Empty()) {
        return *this;
      }

      auto edges = edges_->equal_range(to_read->GetFront());
      if (edges.first == edges.second) {
        return *this;
      }
      if (std::next(edges.first) != edges.second) {
        throw std::runtime_error("Can't traverse non-folded graphs");
      }
      const Edge& edge = *edges.first->second;
      *read_weight += edge.w_;
      to_read->PopFront();
      return edge.endpoint_.ReadWord(to_read, read_weight);
    }

    bool operator==(const Vertex& rhs) const {
      return edges_ == rhs.edges_;
    }

    bool operator!=(const Vertex& rhs) const {
      return edges_ != rhs.edges_;
    }

    void Erase(Edge* e) {
      assert(e->origin_ == *this);
      //first, remove inverse
      assert(e->inverse_position_->second->origin_ == e->endpoint_);

      e->endpoint_.edges_->erase(e->inverse_position_);
      //then remove e
      edges_->erase(e->edge_position_);
    }
  };

  std::deque<Vertex> vertices_;
  Weight modulus_ = 0;

  NaiveFoldedGraph2()
    : vertices_(1)
  {
    root().id_ = 1;
  }

  Vertex& root() {
    return vertices_[0];
  }

  const Vertex& root() const {
    return vertices_[0];
  }

  Vertex& PushVertex() {
    vertices_.emplace_back();
    vertices_.back().id_ = vertices_.size();
    return vertices_.back();
  }

  void PushCycle(Word word, Vertex* v, Weight w = 0) {
    auto current = v;
    while (word.size() > 1) {
      auto& next = PushVertex();
      current->ConnectTo(&next, word.GetFront(), 0);
      current = &next;
      word.PopFront();
    }

    if (word.size() > 0) {
      current->ConnectTo(v, word.GetFront(), w);
    }
  }

  void Fold() {
    bool no_more_folds = false;

//    std::cout << "Before fold: \n";
//    PrintTo(&std::cout);

    while (!no_more_folds) {
      no_more_folds = true;

      for (auto&& vertex : vertices_) {
        auto e1 = vertex.edges_->begin();

        if (e1 == vertex.edges_->end()) {
          continue;
        }

        auto e2 = std::next(e1);

        while (e2 != vertex.edges_->end()) {
          if (e1->second->Fold(e2->second.get(), &modulus_)) {
            no_more_folds = false;

//            std::cout << "Folded in " << vertex.id_ << ":\n";
//            PrintTo(&std::cout);
            break;
          }

          e1 = e2;
          e2 = std::next(e1);
        }

        if (!no_more_folds) {
          break;
        }
      }
    }

  }

  void PrintTo(std::ostream* out) {
    std::map<VertexData*, size_t> ids_;

    for (auto&& v : vertices_) {
      *out << "V" << v.id_ << ": ";
      auto id_seen = ids_.emplace(v.edges_.get(), v.id_);
      if (!id_seen.second) {
        *out << "seen as " << id_seen.first->second << "; ";
      }

      for (auto&& e : *v.edges_) {
        if (!e.second) {
          *out << "nil(" << e.first << "), ";
        } else {
          *out << e.second->origin_.id_ << "-" << e.second->l_ << e.second->w_ << ">" << e.second->endpoint_.id_ <<
                                                                                         ", ";
        }
      }
      *out << "\n";
    }

    *out << std::flush;
  }

};

inline bool NaiveFoldedGraph2::Edge::Fold(Edge* e, NaiveFoldedGraph2::Weight* modulus) {
  assert(origin_ == e->origin_);

  if (l_ != e->l_) {
    return false;
  }

  auto weight_diff = WeightMod(w_ - e->w_, *modulus);

  if (endpoint_ != e->endpoint_) {
    endpoint_.ShiftWeight(weight_diff);
    e->endpoint_.ReplaceWith(&endpoint_);
  } else {
    *modulus = GCD(*modulus, weight_diff);
    if (*modulus < 0) {
      *modulus *= -1;
    }
  }

  assert(WeightMod(w_ - e->w_, *modulus) == 0);

  //ok, now remove inverse of e and e
  origin_.Erase(e);

  return true;
}

} //namespace naive

} //namespace crag

#endif //CRAG_NAIVE_FOLDED_GRAPH2_H_
