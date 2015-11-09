//
// Created by dpantele on 11/8/15.
//

#include <algorithm>
#include <deque>

#include "harvest.h"

namespace crag {

using Word = FoldedGraph::Word;
using Weight = FoldedGraph::Weight;
using Vertex = FoldedGraph::Vertex;

//! Calls functor on each equivalent range of common elements from first and second
template <typename RandomIterator, typename Compare, typename F>
void MergeRanges(
    RandomIterator first_begin
    , RandomIterator first_end
    , RandomIterator second_begin
    , RandomIterator second_end
    , Compare comp
    , F f
) {
  while (first_begin != first_end && second_begin != second_end) {
    if (comp(*first_begin, *second_begin)) {
      first_begin = std::lower_bound(first_begin, first_end, *second_begin, comp);
    } else if (comp(*second_begin, *first_begin)) {
      second_begin = std::lower_bound(second_begin, second_end, *first_begin, comp);
    } else {
      auto first_equal = std::equal_range(first_begin, first_end, *first_begin, comp);
      auto second_equal = std::equal_range(second_begin, second_end, *second_begin, comp);

      f(first_equal.first, first_equal.second, second_equal.first, second_equal.second);

      first_begin = first_equal.second;
      second_begin = second_equal.second;
    }
  }
}


void Harvest(
    const FoldedGraph& graph
    , Word::size_type k
    , Weight weight
    , const Vertex& origin
    , const Vertex& terminus
    , std::vector<Word>* result
) {

  struct Path
  {
    const Vertex* terminus_;
    Weight weight_;
    Word word_;

    const Vertex& terminus() const {
      return *terminus_;
    }

    bool Advance(const FoldedGraph::ConstEdge& edge) {
      if (!edge) {
        return false;
      }

      terminus_ = &edge.terminus();
      weight_ += edge.weight();
      word_.PushBack(edge.label());
      return true;
    }
  };

  auto HarvestPaths = [](size_t max_length, const Vertex& v0, auto path_action) {
    std::deque<Path> active_paths;
    active_paths.push_back(Path{&v0, 0, Word()});

    while (!active_paths.empty()) {
      Path current_path = std::move(active_paths.front());
      active_paths.pop_front();
      path_action(current_path);

      if (current_path.word_.size() == max_length) {
        continue;
      }

      for (const FoldedGraph::ConstEdge& edge : *current_path.terminus_) {
        if (!current_path.word_.Empty() && edge.label().Inverse() == current_path.word_.GetBack()) {
          continue;
        }

        active_paths.push_back(current_path);
        active_paths.back().Advance(edge);
      }
    }
  };

  //these should be always of length ceil(k/2.) and start from origin_v
  std::vector<Path> prefixes;

  //these should be of length not more than floor(k/2.) and start from terminus_v
  std::vector<Path> suffixes;

  assert(static_cast<size_t>(ceil(k / 2.)) == ceil(k / 2.));
  assert(static_cast<size_t>(floor(k / 2.)) == floor(k / 2.));
  if (origin != terminus) {
    HarvestPaths(static_cast<size_t>(ceil(k / 2.)), origin, [&](const Path& prefix) {
      //if the path hits terminus, add that to the result
      if (prefix.terminus() == terminus && graph.modulus().AreEqual(prefix.weight_, weight)) {
        result->push_back(prefix.word_);
      }
      if (prefix.word_.size() == ceil(k / 2.)) {
        prefixes.push_back(prefix);
      }
    });

    HarvestPaths(static_cast<size_t>(floor(k / 2.)), terminus, [&](const Path& suffix) {
      if (!suffix.word_.Empty()) {
        //suffix may not be empty - all such paths are added while harvesting prefixes
        suffixes.push_back(suffix);
      }
    });
  } else {
    //try to do the same this in a single HarvestPaths
    HarvestPaths(static_cast<size_t>(ceil(k / 2.)), origin, [&](const Path& path) {
      //if the path hits terminus, add that to the result
      if (path.terminus() == terminus
          && graph.modulus().AreEqual(path.weight_, weight)
          && (path.word_.Empty() || path.word_.GetFront() != path.word_.GetBack().Inverse())
          ) {
        result->push_back(path.word_);
      }
      if (path.word_.size() == ceil(k / 2.)) {
        prefixes.push_back(path);
      }
      if (path.word_.size() <= floor(k / 2.) && !path.word_.Empty()) {
        suffixes.push_back(path);
      }
    });
  }

  //we will merge prefixes and suffixes so that they have the same terminus
  //and also we will look for the path where prefix.w_ + suffix_.w_ is equal
  //to @param w
  //so we sort by a) terminus b) weight and c) words
  // (to have them sorted afterwards)
  std::sort(prefixes.begin(), prefixes.end(), [](const Path& p1, const Path& p2) {
    if (p1.terminus_ != p2.terminus_) {
      return p1.terminus_ < p2.terminus_;
    }
    if (p1.weight_ != p2.weight_) {
      return p1.weight_ < p2.weight_;
    }
    return p1.word_ < p2.word_;
  });
  std::sort(suffixes.begin(), suffixes.end(), [](const Path& p1, const Path& p2) {
    if (p1.terminus_ != p2.terminus_) {
      return p1.terminus_ < p2.terminus_;
    }
    if (p1.weight_ != p2.weight_) {
      return p1.weight_ < p2.weight_;
    }
    return p1.word_ < p2.word_;
  });


  //routine which combines prefixes and suffixes of the appropriate weights
  auto ConcatenatePrefixesSuffixes =
      [weight, &graph]
          (auto prefixes_begin, auto prefixes_end, auto suffixes_begin, auto suffixes_end, auto PushPath) {
        while (prefixes_begin != prefixes_end && suffixes_begin != suffixes_end) {
          auto current_prefix_weight = prefixes_begin->weight_;
          //suffixes will be appended inversed, so their weight must be inversed now
          auto needed_suffix_weight = graph.modulus().Reduce(-(weight - current_prefix_weight));
          auto suffix_weight_range = std::equal_range(
              suffixes_begin
              , suffixes_end
              , Path{nullptr, needed_suffix_weight, Word{}}
              , [](const Path& p1, const Path& p2) { return p1.weight_ < p2.weight_; }
          );
          if (suffix_weight_range.first == suffix_weight_range.second) {
            prefixes_begin = std::upper_bound(
                prefixes_begin
                , prefixes_end
                , *prefixes_begin
                , [](const Path& p1, const Path& p2) { return p1.weight_ < p2.weight_; }
            );
          } else {
            auto prefix_weight_range = std::equal_range(
                prefixes_begin
                , prefixes_end
                , *prefixes_begin
                , [](const Path& p1, const Path& p2) { return p1.weight_ < p2.weight_; }
            );

            for (auto prefix = prefix_weight_range.first; prefix != prefix_weight_range.second; ++prefix) {
              for (auto suffix = suffix_weight_range.first; suffix != suffix_weight_range.second; ++suffix) {
                if (suffix->word_.Empty()
                    || prefix->word_.Empty()
                    || suffix->word_.GetBack() != prefix->word_.GetBack()
                    ) {
                  Word new_word = suffix->word_;
                  new_word.Invert();
                  new_word.PushFront(prefix->word_);
                  PushPath(std::move(new_word));
                }
              }
            }
            if (graph.modulus().infinite()) {
              // in this case weights of suffixes are sorted as well, and we may not consider
              // any suffixes if weight less that the weight of suffix_weight_range.second
              suffixes_begin = suffix_weight_range.second;
            }
            prefixes_begin = prefix_weight_range.second;
          }
        }
      };

  if (origin != terminus) {
    MergeRanges(
        prefixes.begin()
        , prefixes.end()
        , suffixes.begin()
        , suffixes.end()
        , [](const Path& p1, const Path& p2) {
          return p1.terminus_ < p2.terminus_;
        }
        , [&](auto prefixes_begin, auto prefixes_end, auto suffixes_begin, auto suffixes_end) {
          ConcatenatePrefixesSuffixes(
              prefixes_begin
              , prefixes_end
              , suffixes_begin
              , suffixes_end
              , [&](Word new_word) {
                result->push_back(new_word);
              }
          );
        }
    );
  } else {
    //for cycles case, we don't allow cyclic reductions
    MergeRanges(
        prefixes.begin(), prefixes.end(), suffixes.begin(), suffixes.end(), [](const Path& p1, const Path& p2) {
      return p1.terminus_ < p2.terminus_;
    }, [&](auto prefixes_begin, auto prefixes_end, auto suffixes_begin, auto suffixes_end) {
      ConcatenatePrefixesSuffixes(
          prefixes_begin, prefixes_end, suffixes_begin, suffixes_end, [&](Word new_word) {
        if (new_word.GetFront() != new_word.GetBack().Inverse()) {
          result->push_back(std::move(new_word));
        }
      }
      );
    }
    );
  }
}

template<typename Iter>
bool IsSortedAndUnique(Iter current, Iter end) {
  if (current == end) {
    return true;
  }
  auto next = std::next(current);
  while (next != end) {
    if (!(*current < *next)) {
      return false;
    }
    ++next;
    ++current;
  }
  return true;
}

std::vector<Word> Harvest(
    const FoldedGraph& graph, Word::size_type k, const Vertex& origin, const Vertex& terminus, Weight w) {
  std::vector<Word> result;

  Harvest(graph, k, w, origin, terminus, &result);

  std::sort(result.begin(), result.end());
  assert(IsSortedAndUnique(result.begin(), result.end()));

  return result;
}


void Harvest(
    const FoldedGraph& graph,
    Word::size_type k,
    Weight weight,
    const Vertex& origin,
    const Vertex& terminus,
    Word::Letter first_letter,
    std::vector<Word> *result
) {

  auto first_edge = origin.edge(first_letter);

  if (!first_edge) {
    return;
  }

  if (k == 0) {
    return;
  }

  std::vector<Word> this_result;
  Harvest(graph,
      static_cast<CWord::size_type>(k - 1),
      graph.modulus().Reduce(weight - first_edge.weight()),
      first_edge.terminus(),
      terminus,
      &this_result);

  std::sort(this_result.begin(), this_result.end());

  assert(IsSortedAndUnique(this_result.begin(), this_result.end()));

  for (auto&& word : this_result) {
    if (word.GetBack() != first_letter.Inverse()) {
      word.PushFront(first_letter);
      result->push_back(word);
    }
  }
}


std::vector<FoldedGraph::Word> Harvest(FoldedGraph::Word::size_type k, FoldedGraph::Weight weight, FoldedGraph* graph) {
  std::vector<Word> result;

  if (graph->modulus().AreEqual(weight, 0)) {
    result.push_back(Word{ });
  }

  for(FoldedGraph::Vertex& vertex : *graph) {
    //optimization: don't harvest cycles which start from
    //non-zero-weight edges for non-zero w

    if (!graph->modulus().AreEqual(weight, 0)) {
      bool has_non_trivial = false;
      for(auto&& edge : vertex) {
        if (!graph->modulus().AreEqual(edge.weight(), 0)) {
          has_non_trivial = true;
          break;
        }
      }
      if (!has_non_trivial) {
        continue;
      }
    }
    Harvest(*graph, k, weight, vertex, vertex, &result);

    //just get rid of all edges at this vertex
    for(auto&& edge : vertex) {
      vertex.RemoveEdge(edge.label());
    }
  }

  std::sort(result.begin(), result.end());
  auto unique_end = std::unique(result.begin(), result.end());
  result.erase(unique_end, result.end());

  return result;
}

}