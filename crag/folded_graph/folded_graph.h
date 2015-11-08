//
// Created by dpantele on 11/7/15.
//

#ifndef ACC_FOLDED_GRAPH_H
#define ACC_FOLDED_GRAPH_H

#include <stdint.h>
#include <array>
#include <compressed_word/compressed_word.h>

namespace crag {

class FoldedGraph
{
 public:
  using Word = CWord;
  using Label = Word::Letter;
  using Weight = int64_t;

  class Vertex;
  struct Edge
  {
    Vertex* terminus_ = nullptr;
    Weight weight_ = 0;

    explicit operator bool() const {
      return terminus_ != nullptr;
    }
  };

  class Vertex
  {
   public:
    Edge edge(Label l) const;

    void Combine(FoldedGraph::Vertex* other, Weight shift);

    void AddEdge(FoldedGraph::Label l, Vertex* terminus, FoldedGraph::Weight w);

    void RemoveEdge(FoldedGraph::Label l);

   private:
    std::array<Edge, Word::kAlphabetSize> edges_; //!< Mutable since it is modified like in any disjoint partition


    //fields below are used only in Combine() process
    //and in general may be allocated externally
    //in Combine()
    //but it is not clear what will be more expensive here - memory is allocated for
    //every vertex anyway

    //! If @e epsilon_ is not null, it is followed any time this vertex is accessed
    /** You could think of that as an implementation of disjoint set partition **/
    Edge epsilon_{};
    size_t equivalent_vertices_count_ = 1;

    friend Vertex* Merge(Vertex* v1, Vertex* v2, Weight v1_shift);

    friend Edge FollowEdge(const Edge& e);

  };


 private:
};

}

#endif //ACC_FOLDED_GRAPH_H
