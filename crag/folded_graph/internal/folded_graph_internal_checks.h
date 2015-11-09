//
// Created by dpantele on 11/9/15.
//

#ifndef ACC_FOLDED_GRAPH_INTERNAL_CHECKS_H
#define ACC_FOLDED_GRAPH_INTERNAL_CHECKS_H

#include "../folded_graph.h"

#include <gtest/gtest.h>

namespace crag {

class FoldedGraphInternalChecks
{
 public:
  using Label = FoldedGraph::Label;

  static ::testing::AssertionResult CheckEdge(const FoldedGraph::Vertex& from, FoldedGraph::Label l, const FoldedGraph::EdgeData& e) {
    using ::testing::AssertionFailure;
    using ::testing::AssertionSuccess;

    if (!e) {
      return AssertionSuccess();
    }

    if (!e.terminus_->edges_[l.Inverse().AsInt()]) {
      return AssertionFailure() << "The inverse edge is not presented for egde " << l;
    }

    if (e.terminus_->edges_[l.Inverse().AsInt()].terminus_ != &from) {
      return AssertionFailure() << "The inverse edge of " << l << " does not contain point back";
    }

    if (-e.weight_ != e.terminus_->edges_[l.Inverse().AsInt()].weight_) {
      return AssertionFailure() << "The inverse edge of " << l << " has weight "
          << e.terminus_->edges_[l.Inverse().AsInt()].weight_ << ", should be " << -e.weight_;
    }

    return AssertionSuccess();
  }

  static ::testing::AssertionResult CheckVertex(const FoldedGraph::Vertex& vertex) {
    using ::testing::AssertionFailure;
    using ::testing::AssertionSuccess;

    if (vertex.epsilon_) {
#ifndef NDEBUG
      if (!vertex.merged_) {
        return AssertionFailure() << "Vertex was merged but was not processed";
      }
#endif
      Label l(0);
      for (auto&& edge : vertex.edges_) {
        if (edge) {
          return AssertionFailure() << "Edge labelled " << l << " is presented on merged vertex";
        }
        ++l;
      }

      if (vertex.begin() != vertex.end()) {
        return AssertionFailure() << "being is not end for a merged vertex";
      }
    } else {
      Label l(0);
      for (auto&& edge : vertex.edges_) {
        auto edge_result = CheckEdge(vertex, l, edge);
        if (!edge_result) {
          return edge_result;
        }
        ++l;
      }
    }

    return AssertionSuccess();
  }


  static ::testing::AssertionResult Check(const FoldedGraph& g) {
    using ::testing::AssertionFailure;
    using ::testing::AssertionSuccess;

    if (g.vertices_.empty()) {
      return AssertionFailure() << "Graph can not be empty";
    }
    if (g.root_->IsMerged()) {
      return AssertionFailure() << "Root of graph should never be merged into another vertex";
    }
    auto id = 0u;
    for (auto&& vertex : g.vertices_) {
      auto vertex_result = CheckVertex(vertex);
      if (!vertex_result) {
        vertex_result << "\n for vertex " << id;

        return vertex_result;
      }
      ++id;
    }

    return AssertionSuccess();
  }
};

}


#endif //ACC_FOLDED_GRAPH_INTERNAL_CHECKS_H
