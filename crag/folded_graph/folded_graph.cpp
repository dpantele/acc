//
// Created by dpantele on 11/7/15.
//
#include <set>
#include <vector>
#include <map>

#include "folded_graph.h"
#include "modulus.h"


namespace crag {

FoldedGraph::Vertex::const_iterator::EdgeDataAccess FoldedGraph::Vertex::edge(crag::FoldedGraph::Label l) const {
  assert(!this->epsilon_);
  assert(!edges_[l.AsInt()].terminus_ || !edges_[l.AsInt()].terminus_->epsilon_);
  return {edges_.begin() + l.AsInt(), l};
}

FoldedGraph::Vertex::iterator::EdgeDataAccess FoldedGraph::Vertex::edge(crag::FoldedGraph::Label l) {
  assert(!this->epsilon_);
  assert(!edges_[l.AsInt()].terminus_ || !edges_[l.AsInt()].terminus_->epsilon_);
  return {edges_.begin() + l.AsInt(), l};
}


void FoldedGraph::Vertex::Combine(FoldedGraph::Vertex* other, Weight this_shift, Modulus* modulus) {
  std::vector<Vertex*> merged; //!< Recently merged vertices

  this_shift = modulus->Reduce(this_shift);
  merged.push_back(Merge(this, other, this_shift));

  while (!merged.empty()) {
    auto child_vertex = merged.back();
    merged.pop_back();

    assert(child_vertex->epsilon_);
    assert(!child_vertex->merged_);

    auto edge_to_root = FollowEdge(child_vertex->epsilon_);
    auto child_shift = edge_to_root.weight_;
    auto root_vertex = edge_to_root.terminus_;

    auto root_edge_iter = root_vertex->edges_.begin();
    Label label(0);

    for (EdgeData child_edge : child_vertex->edges_) {
      if (child_edge) {
        //remove edge from this vertex
        //it will be presented in the root anyways
        child_vertex->RemoveEdge(label);

        if (!*root_edge_iter) {
          //if there is no edge labeled in the same way, just clone the edge
          //but we have to shift weight
          //if we think of automatic following the epsilon edge, we already got child_shift
          //while travelling from child to root, so only child_edge.weight_ - child_shift
          //is left

          root_vertex->AddEdge(label, child_edge.terminus_, modulus->Reduce(child_edge.weight_ - child_shift));
        } else {
          //here we will have to merge two terminates
          child_edge = FollowEdge(child_edge);
          EdgeData root_edge = FollowEdge(*root_edge_iter);

          //assume there was a path Child->ChildTerminus of weight child_edge.weight
          //now there is a path Child->Root->RootTerminus->ChildTerminus
          //the total weight of that path is child_shift + root_edge.weight_ + termini_shift, should be equal to child_edge

          auto termini_shift = modulus->Reduce(child_edge.weight_ - child_shift - root_edge.weight_);

          if (child_edge.terminus_ == root_edge.terminus_) {
            //there is no other way to make termini_shift == 0
            //other than change modulus_
            modulus->EnsureEqual(termini_shift, 0);
          } else {
            //termini shift in the formula above is for root_terminus->child_terminus
            merged.push_back(Merge(root_edge.terminus_, child_edge.terminus_, termini_shift));
          }
        }
      }
      ++label;
      ++root_edge_iter;
    }
#ifndef NDEBUG
    child_vertex->merged_ = true;
#endif
  }
}

/**
 * Disjoint subset merge. @param v1_shift is applied in a way that the weight of positively-labeled edges from
 * v1 is increased by v1_shift.
 *
 * A non-root vertex chosen is returned then.
 */
FoldedGraph::Vertex* FoldedGraph::Vertex::Merge(
    FoldedGraph::Vertex* v1
    , FoldedGraph::Vertex* v2
    , FoldedGraph::Weight v1_shift) {
  //Merge should be called only on the root of trees
  assert(!v1->epsilon_);
  assert(!v2->epsilon_);

  //And looks like they always must be distinct
  assert(v1 != v2);

  if (v1->equivalent_vertices_count_ < v2->equivalent_vertices_count_) {
    //in this case we make v1 point to v2
    v1->epsilon_.terminus_ = v2;
    v1->epsilon_.weight_ = v1_shift;
    v2->equivalent_vertices_count_ += v1->equivalent_vertices_count_;
    return v1;
  } else {
    //otherwise v2 points to v1
    v2->epsilon_.terminus_ = v1;
    v2->epsilon_.weight_ = -v1_shift;
    v1->equivalent_vertices_count_ += v2->equivalent_vertices_count_;
    return v2;
  }
}

FoldedGraph::EdgeData FoldedGraph::Vertex::FollowEdge(const FoldedGraph::EdgeData& e) {
  FoldedGraph::EdgeData result(e);

  if (!result.terminus_->epsilon_) {
    return result;
  }

  while (true) {
    auto current = result.terminus_;
    auto parent = current->epsilon_.terminus_;

    if (!parent) {
      return result;
    }

    auto grand_parent = parent->epsilon_.terminus_;

    if (!grand_parent) {
      result.terminus_ = parent;
      result.weight_ += current->epsilon_.weight_;
      return result;
    }

    current->epsilon_.terminus_ = grand_parent;
    current->epsilon_.weight_ += parent->epsilon_.weight_;

    result.terminus_ = grand_parent;
    result.weight_ += current->epsilon_.weight_;
  }
}

void FoldedGraph::Vertex::AddEdge(FoldedGraph::Label l, Vertex* terminus, FoldedGraph::Weight w) {
  assert(!edges_[l.AsInt()]);
  assert(!terminus->edges_[l.Inverse().AsInt()]);
  assert(!this->merged_);
  assert(!terminus->merged_);

  edges_[l.AsInt()].terminus_ = terminus;
  edges_[l.AsInt()].weight_ = w;

  terminus->edges_[l.Inverse().AsInt()].terminus_ = this;
  terminus->edges_[l.Inverse().AsInt()].weight_ = -w;
}

void FoldedGraph::Vertex::RemoveEdge(FoldedGraph::Label l) {
  EdgeData& edge = edges_[l.AsInt()];
  assert(edge);
  auto terminus = edge.terminus_;
  EdgeData& inverse = terminus->edges_[l.Inverse().AsInt()];
  assert(inverse);
  assert(inverse.terminus_ == this);

  edge = EdgeData{};
  inverse = EdgeData{};
}

void FoldedGraph::Combine(FoldedGraph::Vertex* v1, FoldedGraph::Vertex* v2, FoldedGraph::Weight v1_shift) {
  v1->Combine(v2, v1_shift, &modulus_);
  if (root_->IsMerged()) {
    root_ = &(root_->Parent());
  }
}

template <typename Path>
Path ReadWord(
    const FoldedGraph::Word& w, decltype(std::declval<Path>().origin()) origin, CWord::size_type length_limit
    , const Modulus& modulus) {
  auto current_vertex = &origin;
  auto to_read = w;
  FoldedGraph::Weight current_weight = 0;

  while(length_limit > 0) {
    --length_limit;
    auto next_edge = current_vertex->edge(to_read.GetFront());
    if (!next_edge) {
      break;
    }

    to_read.PopFront();

    current_vertex = &next_edge.terminus();
    current_weight += next_edge.weight();
  }

  return Path(origin, *current_vertex, modulus.Reduce(current_weight), std::move(to_read));
}

FoldedGraph::Path FoldedGraph::ReadWord(
    const FoldedGraph::Word& w, FoldedGraph::Vertex& origin, CWord::size_type length_limit) const {
  return crag::ReadWord<Path>(w, origin, length_limit, modulus_);
}

FoldedGraph::ConstPath FoldedGraph::ReadWord(
    const FoldedGraph::Word& w, const FoldedGraph::Vertex& origin, CWord::size_type length_limit) const {
  return crag::ReadWord<ConstPath>(w, origin, length_limit, modulus_);
}


FoldedGraph::Path FoldedGraph::PushWord(const FoldedGraph::Word& w, Vertex* origin) {
  auto current_path = ReadWord(w, *origin);

  auto current_terminus = &current_path.terminus();
  auto to_push = current_path.unread_word_part();

  while (!to_push.Empty()) {
    auto next_vertex = &CreateVertex();
    current_terminus->AddEdge(to_push.GetFront(), next_vertex, 0);
    current_terminus = next_vertex;
    to_push.PopFront();
  }

  return Path(current_path.origin(), *current_terminus, current_path.weight(), Word());
}


void FoldedGraph::PushCycle(const FoldedGraph::Word& w, Vertex* origin, FoldedGraph::Weight weight) {
  return EnsurePath(w, origin, origin, weight);
}


//! Make origin and terminus equal vertices, with weight on the epsilon-edge
void EnsureEpsilon(
    FoldedGraph::Vertex* origin, FoldedGraph::Vertex* terminus, FoldedGraph::Weight weight, Modulus* modulus) {
  if(origin != terminus) {
    //add an epsilon-edge from origin to terminus
    origin->Combine(terminus, weight, modulus);
  } else {
    //or just adjust the modulus
    modulus->EnsureEqual(weight, 0);
  }
};

//! Make sure that @p origin and @p terminus are connected with an edge of weight @p weight nad label @p label
void EnsureEdge(
    FoldedGraph::Label label, FoldedGraph::Vertex* origin, FoldedGraph::Vertex* terminus, FoldedGraph::Weight weight
    , Modulus* modulus) {
  {
    auto edge = origin->edge(label);

    if (edge) {
      return EnsureEpsilon(&edge.terminus(), terminus, weight - edge.weight(), modulus);
    }
  }

  {
    auto inverse_edge = terminus->edge(label.Inverse());
    if (inverse_edge) {
      return EnsureEpsilon(origin, &inverse_edge.terminus(), weight + inverse_edge.weight(), modulus);
    }
  }

  return origin->AddEdge(label, terminus, modulus->Reduce(weight));
}

void FoldedGraph::EnsurePath(
    const FoldedGraph::Word& w, FoldedGraph::Vertex* origin, FoldedGraph::Vertex* terminus
    , FoldedGraph::Weight weight) {
  //the algorithm is the following:
  //First, we make sure that we can read first half of w (but one letter 'middle') starting at origin
  //and the second half terminating at terminus. After that we make sure that the edge in between these
  //halves is of required weight

  if (w.Empty()) {
    EnsureEpsilon(origin, terminus, weight, &modulus_);
  } else {
    auto from_origin = w;
    from_origin.PopBack(static_cast<Word::size_type>(from_origin.size() / 2));

    auto from_terminus = w;
    from_terminus.PopFront(from_origin.size());
    from_terminus.Invert();

    auto middle = from_origin.GetBack();
    from_origin.PopBack();

    auto origin_path = PushWord(from_origin, origin);
    auto terminus_path = PushWord(from_terminus, terminus);

    EnsureEdge(middle, &origin_path.terminus(), &terminus_path.terminus(),
        weight - (origin_path.weight() - terminus_path.weight()), &modulus_);
  }
  if (root_->IsMerged()) {
    root_ = &root_->Parent();
  }
}

boost::optional<FoldedGraph::Word> FindNontrivialPath(const FoldedGraph::Vertex& from, const FoldedGraph::Vertex& to) {
  using Word = FoldedGraph::Word;
  using Vertex = FoldedGraph::Vertex;
  struct GraphPath {
    Word w;
    const Vertex* from;
    const Vertex* to;

    GraphPath Advance(const FoldedGraph::ConstEdge& edge) const {
      assert(to->edge(edge.label()) == edge);
      auto result = GraphPath{w, from, &edge.terminus()};
      result.w.PushBack(edge.label());
      return result;
    }
  };

  std::map<const Vertex*, GraphPath> visited;
  std::deque<GraphPath> next;

  next.push_back(GraphPath{Word(), &from, &from});

  if (from != to) {
    next.push_back(GraphPath{Word(), &to, &to});
  }

  while(!next.empty()) {
    auto& current = next.front();

    auto first_visit = visited.emplace(current.to, current);
    if (!first_visit.second) {
      auto& existing = first_visit.first->second;
      if (from == to || existing.from != current.from) {
        //found path
        assert(existing.w.Empty()
               || current.w.Empty()
               || existing.w.GetBack() != current.w.GetBack());
        if (*current.from == from) {
          auto result = current.w;
          existing.w.Invert();
          result.PushBack(existing.w);
          return result;
        } else {
          assert(*current.from == to);
          auto result = existing.w;
          current.w.Invert();
          result.PushBack(current.w);
          return result;
        }
      } else {
        next.pop_front();
        continue;
      }
    }

    for(auto&& edge : *current.to) {
      if (!current.w.Empty() && edge.label().Inverse() == current.w.GetBack()) {
        continue;
      }
      auto advanced = current.Advance(edge);
      next.push_back(advanced);
    }

    next.pop_front();
  }
  return {};
}

boost::optional<FoldedGraph::Word> FoldedGraph::FindShortestPath(
    const FoldedGraph::Vertex& from, const FoldedGraph::Vertex& to) const {
  if (from == to) {
    return Word();
  }

  return FindNontrivialPath(from, to);
}

boost::optional<FoldedGraph::Word> FoldedGraph::FindShortestCycle(const FoldedGraph::Vertex& base) const {
  auto result = FindNontrivialPath(base, base);
  if (result && result->Inverse() < result) {
    return result->Inverse();
  } else {
    return result;
  }
}

bool FoldedGraph::HasPath(const FoldedGraph::Vertex& from, const FoldedGraph::Vertex& to) const {
  return static_cast<bool>(FindShortestPath(from, to));
}

bool FoldedGraph::HasPath(
    const FoldedGraph::Word& label, const FoldedGraph::Vertex& from, const FoldedGraph::Vertex& to) const {
  auto existing_path = ReadWord(label, from);
  if (!existing_path.unread_word_part().Empty()) {
    return false;
  }
  return existing_path.terminus() == to;
}

bool FoldedGraph::HasPath(
    const FoldedGraph::Word& label, FoldedGraph::Weight weight, const FoldedGraph::Vertex& from
    , const FoldedGraph::Vertex& to) const {
  auto existing_path = ReadWord(label, from);
  if (!existing_path.unread_word_part().Empty()) {
    return false;
  }
  return existing_path.terminus() == to && existing_path.weight() == weight;
}

bool FoldedGraph::HasCycle(const FoldedGraph::Vertex& base) const {
  return static_cast<bool>(FindShortestCycle(base));
}

bool FoldedGraph::HasCycle(const FoldedGraph::Word& label, const FoldedGraph::Vertex& base) const {
  return HasPath(label, base, base);
}

bool FoldedGraph::HasCycle(
    const FoldedGraph::Word& label, FoldedGraph::Weight weight, const FoldedGraph::Vertex& base) const {
  return HasPath(label, weight, base, base);
}


template
struct FoldedGraph::PathTemplate<FoldedGraph::Vertex>;
template
struct FoldedGraph::PathTemplate<const FoldedGraph::Vertex>;
template
class FoldedGraph::VertexIterT<std::deque<FoldedGraph::Vertex>::iterator>;
template
class FoldedGraph::VertexIterT<std::deque<FoldedGraph::Vertex>::const_iterator>;
template
class FoldedGraph::EdgesIteratorT<
    FoldedGraph::Vertex, std::array<
        FoldedGraph::EdgeData
        , FoldedGraph::Word::kAlphabetSize>::iterator>;
template
class FoldedGraph::EdgesIteratorT<
    const FoldedGraph::Vertex, std::array<
        FoldedGraph::EdgeData
        , FoldedGraph::Word::kAlphabetSize>::const_iterator>;


}
