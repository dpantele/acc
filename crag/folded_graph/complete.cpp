//
// Created by dpantele on 11/12/15.
//

#include "complete.h"

namespace crag {

void CompleteWith(FoldedGraph g, FoldedGraph::Word r, size_t max_vertex_id) {
  for (size_t shift = 0; shift < r.size(); ++shift, r.CyclicLeftShift()) {
    for (auto vertex = 0; vertex < max_vertex_id; ++vertex) {
      if (g[vertex].IsMerged()) {
        continue;
      }

      g.PushCycle(r, &g[vertex], 0);
    }
  }
}

}
