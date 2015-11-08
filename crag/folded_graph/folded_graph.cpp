//
// Created by dpantele on 11/7/15.
//

#include <vector>
#include "folded_graph.h"

namespace crag {

FoldedGraph::Edge FoldedGraph::Vertex::edge(crag::FoldedGraph::Label l) const {
  assert(!this->epsilon_);
  assert(!edges_[l.AsInt()].terminus_ || !edges_[l.AsInt()].terminus_->epsilon_);
  return edges_[l.AsInt()];
}

void FoldedGraph::Vertex::Combine(FoldedGraph::Vertex* other, Weight shift) {
  std::vector<Vertex*> merged; //!< Recently merged vertices

  merged.push_back(Merge(this, other, shift));

  while (!merged.empty()) {
    auto child_vertex = merged.back();
    merged.pop_back();

    assert(child_vertex->epsilon_);

    auto edge_to_root = FollowEdge(child_vertex->epsilon_);
    auto child_shift = edge_to_root.weight_;
    auto root_vertex = edge_to_root.terminus_;

    auto root_edge_iter = root_vertex->edges_.begin();
    Label label(0);

    for (Edge child_edge : child_vertex->edges_) {
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

          root_vertex->AddEdge(label, child_edge.terminus_, child_edge.weight_ - child_shift);
        } else {
          //here we will have to merge two terminates
          child_edge = FollowEdge(child_edge);
          Edge root_edge = FollowEdge(*root_edge_iter);

          //assume there was a path Child->ChildTerminus of weight child_edge.weight
          //now there is a path Child->Root->RootTerminus->ChildTerminus
          //the total weight of that path is child_shift + root_edge.weight_ + termini_shift, should be equal to child_edge

          auto termini_shift = child_edge.weight_ - child_shift - root_edge.weight_;

          if (child_edge.terminus_ == root_edge.terminus_) {
            //there is no other way to make termini_shift == 0
            //other than change modulus_
          } else {
            //termini shift in the formula above is for root_terminus->child_terminus
            merged.push_back(Merge(root_edge.terminus_, child_edge.terminus_, termini_shift));
          }
        }
      }
      ++label;
      ++root_edge_iter;
    }
  }
}

/**
 * Disjoint subset merge. @param v1_shift is applied in a way that the weight of positively-labeled edges from
 * v1 is increased by v1_shift.
 *
 * A non-root vertex chosen is returned then.
 */
FoldedGraph::Vertex* crag::Merge(FoldedGraph::Vertex* v1, FoldedGraph::Vertex* v2, FoldedGraph::Weight v1_shift) {
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

FoldedGraph::Edge crag::FollowEdge(const FoldedGraph::Edge& e) {
  FoldedGraph::Edge result(e);

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
  assert(!this->epsilon_);
  assert(!terminus->epsilon_);

  edges_[l.AsInt()].terminus_ = terminus;
  edges_[l.AsInt()].weight_ = w;

  terminus->edges_[l.Inverse().AsInt()].terminus_ = this;
  terminus->edges_[l.Inverse().AsInt()].weight_ = -w;
}

void FoldedGraph::Vertex::RemoveEdge(FoldedGraph::Label l) {
  Edge& edge = edges_[l.AsInt()];
  assert(edge);
  auto terminus = edge.terminus_;
  Edge& inverse = terminus->edges_[l.Inverse().AsInt()];
  assert(inverse);
  assert(inverse.terminus_ == this);

  edge = Edge{};
  inverse = Edge{};
}

}
