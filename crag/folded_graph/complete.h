//
// Created by dpantele on 11/12/15.
//

#ifndef ACC_COMPLETE_H
#define ACC_COMPLETE_H

#include "folded_graph.h"

namespace crag {

void CompleteWith(FoldedGraph::Word r, size_t max_vertex_id, FoldedGraph* g);

//! For every vertex s and every cyclic permutation r' of the word r use pushCycle(r',s).
inline void CompleteWith(FoldedGraph::Word r, FoldedGraph* g) {
  return CompleteWith(std::move(r), g->size(), g);
}




}

#endif //ACC_COMPLETE_H
