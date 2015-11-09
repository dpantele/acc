//
// Created by dpantele on 11/8/15.
//

#include "folded_graph.h"

#ifndef ACC_HARVEST_H
#define ACC_HARVEST_H

namespace crag {

//! Primary version of Harvest. Collect all paths from origin to terminus of length no more that k and of weight @param weight
void Harvest(
    const FoldedGraph& graph
    , FoldedGraph::Word::size_type k
    , FoldedGraph::Weight weight
    , const FoldedGraph::Vertex& origin
    , const FoldedGraph::Vertex& terminus
    , std::vector<FoldedGraph::Word>* result
);

//! Wrapper for primary harvest to return sorted vector of words
std::vector<FoldedGraph::Word> Harvest(
    const FoldedGraph& graph, FoldedGraph::Word::size_type k, const FoldedGraph::Vertex& origin
    , const FoldedGraph::Vertex& terminus, FoldedGraph::Weight w);

//! Some harvest from past, which also specifies the first edge
void Harvest(
    const FoldedGraph& graph,
    FoldedGraph::Word::size_type k,
    FoldedGraph::Weight weight,
    const FoldedGraph::Vertex& origin,
    const FoldedGraph::Vertex& terminus,
    FoldedGraph::Word::Letter first_letter,
    std::vector<FoldedGraph::Word> *result
);

//! The main harvest, which consumes @p graph and produces the list of all cycles of wight @p weight up to length @p k
std::vector<FoldedGraph::Word> Harvest(FoldedGraph::Word::size_type k, FoldedGraph::Weight weight, FoldedGraph* graph);

}


#endif //ACC_HARVEST_H
