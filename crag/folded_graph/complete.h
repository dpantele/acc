//
// Created by dpantele on 11/12/15.
//

#ifndef ACC_COMPLETE_H
#define ACC_COMPLETE_H

#include "folded_graph.h"

namespace crag {

void CompleteWith(FoldedGraph g, FoldedGraph::Word r, size_t max_vertex_id);

//! For every vertex s and every cyclic permutation r' of the word r use pushCycle(r',s).
void CompleteWith(FoldedGraph g, FoldedGraph::Word r) {
  return CompleteWith(g, std::move(r), g.size());
}




}

#endif //ACC_COMPLETE_H
