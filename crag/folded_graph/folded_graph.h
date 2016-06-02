//
// Created by dpantele on 11/7/15.
//

#ifndef ACC_FOLDED_GRAPH_H
#define ACC_FOLDED_GRAPH_H

#include <array>
#include <cstddef>
#include <deque>
#include <stdint.h>

#include <compressed_word/compressed_word.h>
#include "modulus.h"
#include <boost/optional.hpp>

namespace crag {

class FoldedGraph
{
 public:
  using Word = CWord;
  using Label = Word::Letter;
  using Weight = int64_t;

  class Vertex;

  struct EdgeData
  {
//   public:
//    friend class FoldedGraph::Vertex;

    explicit operator bool() const {
      return terminus_ != nullptr;
    }

//    const Vertex& terminus() const {
//      assert(terminus_);
//      return *terminus_;
//    }
//
//    Weight weight() const {
//      return weight_;
//    }
//
//   private:
    Vertex* terminus_ = nullptr;
    Weight weight_ = 0;
  };

  //! Template to build const & non-const iterators
  /**
   * @todo use boost.iterator
   */
  template <typename VertexT, typename EdgeIter>
  class EdgesIteratorT
  {
   public:

    struct EdgeDataAccess
    {
      friend class EdgesIteratorT;

      EdgeDataAccess(EdgeIter edge, Label label)
          : edge_(edge), label_(label) {
      }

      Label label() const {
        return label_;
      }

      VertexT& terminus() const {
        assert(edge_->terminus_);
        return *edge_->terminus_;
      }

      Weight weight() const {
        return edge_->weight_;
      }

      explicit operator bool() const {
        return static_cast<bool>(*edge_);
      }

      bool operator==(const EdgeDataAccess& other) const {
        return edge_ == other.edge_ && label_ == other.label_;
      }

      bool operator!=(const EdgeDataAccess& other) const {
        return !(*this == other);
      }


     private:
      EdgeIter edge_;
      Label label_;
    };

    typedef std::forward_iterator_tag iterator_category;
    typedef EdgeDataAccess value_type;
    typedef EdgeDataAccess* pointer;
    typedef EdgeDataAccess& reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    EdgesIteratorT(Label l, EdgeIter e)
        : data_(e, l) {
      ProceedToNonNull();
    }

    EdgesIteratorT& operator++() {
      ++data_.edge_;
      ++data_.label_;

      ProceedToNonNull();
      return *this;
    }

    EdgesIteratorT operator++(int) {
      auto copy = *this;
      this->operator++();
      return copy;
    }

    const EdgeDataAccess operator*() const {
      return data_;
    }

    const EdgeDataAccess* operator->() const {
      return &data_;
    }

    bool operator==(const EdgesIteratorT& other) const {
      return data_.edge_ == other.data_.edge_;
    }

    bool operator!=(const EdgesIteratorT& other) const {
      return !(*this == other);
    }

   private:
    void ProceedToNonNull() {
      while (data_.label_ != Label(2 * Word::kAlphabetSize) && !*data_.edge_) {
        ++data_.edge_;
        ++data_.label_;
      }
    }

    EdgeDataAccess data_;
  };

  class Vertex
  {
   public:
    Vertex() = default;

    Vertex(const Vertex&) = delete;

    Vertex(Vertex&&) = delete;

    Vertex& operator=(const Vertex&) = delete;

    Vertex& operator=(Vertex&&) = delete;

    //below is semi-public interface
    //in general, there should be no need to use this directly

    //NOTE: if called with root() vertex, the root may be merged
    //so this should be used really carefully
    //most likely the best way is via PushCycle procedure
    void Combine(FoldedGraph::Vertex* other, Weight this_shift, Modulus* modulus);

    void AddEdge(FoldedGraph::Label l, Vertex* terminus, FoldedGraph::Weight w);

    void RemoveEdge(FoldedGraph::Label l);

    bool operator==(const Vertex& other) const {
      return this == &other;
    }

    bool operator!=(const Vertex& other) const {
      return this != &other;
    }

    using iterator = EdgesIteratorT<Vertex, std::array<EdgeData, Word::kAlphabetSize>::iterator>;
    using const_iterator = EdgesIteratorT<const Vertex, std::array<EdgeData, Word::kAlphabetSize>::const_iterator>;

    iterator begin() {
      return iterator(0, edges_.begin());
    }
    const_iterator begin() const {
      return const_iterator(0, edges_.begin());
    }

    iterator end() {
      return iterator(2 * Word::kAlphabetSize, edges_.end());
    }
    const_iterator end() const {
      return const_iterator(2 * Word::kAlphabetSize, edges_.end());
    }

    iterator::EdgeDataAccess edge(Label l);

    const_iterator::EdgeDataAccess edge(Label l) const;


    //! Rarely used procedure to check if a vertex is merged into another one
    bool IsMerged() const {
      return static_cast<bool>(epsilon_);
    }

    //! If IsMerged(), then the canonical representative is returned. Otherwise return *this
    const Vertex& Parent() const {
      return epsilon_ ? *FollowEdge(epsilon_).terminus_ : *this;
    }

    Vertex& Parent() {
      return epsilon_ ? *FollowEdge(epsilon_).terminus_ : *this;
    }
   private:
    std::array<EdgeData, 2 * Word::kAlphabetSize> edges_;


    //fields below are used only in Combine() process
    //and in general may be allocated externally
    //in Combine()
    //but it is not clear what will be more expensive here - memory is allocated for
    //every vertex anyway

    //! If @e epsilon_ is not null, it is followed any time this vertex is accessed
    /** You could think of that as an implementation of disjoint set partition **/
    EdgeData epsilon_{};
    size_t equivalent_vertices_count_ = 1;

    static Vertex* Merge(Vertex* v1, Vertex* v2, Weight v1_shift);

    static EdgeData FollowEdge(const EdgeData& e);

#ifndef NDEBUG
    //set to true when vertex was processed in Combine
    //if merged_ is true, the vertex should never have any edges in and out
    bool merged_ = false;
#endif
    friend class FoldedGraphInternalChecks;
  };

  FoldedGraph()
      : vertices_(1)
        , root_(&vertices_.front()) {
  }

  Vertex& root() {
    return const_cast<Vertex&>(static_cast<const FoldedGraph*>(this)->root());
  }

  const Vertex& root() const {
    return root_->Parent();
  }

  template <typename BaseIter>
  class VertexIterT
  {
   public:
    typedef std::forward_iterator_tag iterator_category;
    typedef typename BaseIter::value_type value_type;
    typedef typename BaseIter::pointer pointer;
    typedef typename BaseIter::reference reference;
    typedef typename BaseIter::difference_type difference_type;

    VertexIterT(BaseIter iter, BaseIter end)
        : base_iter_(iter)
          , current_end_(end) {
      AdvanceToNotNull();
    }

    VertexIterT& operator++() {
      ++base_iter_;
      AdvanceToNotNull();

      return *this;
    }

    auto& operator*() const {
      return base_iter_.operator*();
    }

    auto operator->() const {
      return base_iter_.operator->();
    }

    bool operator==(const VertexIterT& other) const {
      return base_iter_ == other.base_iter_;
    }

    bool operator!=(const VertexIterT& other) const {
      return base_iter_ != other.base_iter_;
    }

   private:
    BaseIter base_iter_;

    // The current end of vertices vertor is stored in the iterator to
    // allow automatic advancing over merged vertices
    // Since in general we don't want to extend the vertices vector
    // while someone iterates over it, it should be ok
    BaseIter current_end_;

    void AdvanceToNotNull() {
      while (base_iter_ != current_end_ && base_iter_->IsMerged()) {
        ++base_iter_;
      }
    }
  };

  using Edge = Vertex::iterator::EdgeDataAccess;
  using ConstEdge = Vertex::const_iterator::EdgeDataAccess;

  using iterator = VertexIterT<std::deque<Vertex>::iterator>;
  using const_iterator = VertexIterT<std::deque<Vertex>::const_iterator>;

  iterator begin() {
    return iterator(vertices_.begin(), vertices_.end());
  }
  iterator end() {
    return iterator(vertices_.end(), vertices_.end());
  }
  const_iterator begin() const {
    return const_iterator(vertices_.begin(), vertices_.end());
  }
  const_iterator end() const {
    return const_iterator(vertices_.end(), vertices_.end());
  }

  size_t size() const {
    return vertices_.size();
  }

  Vertex& operator[](size_t i) {
    return vertices_[i];
  }

  const Vertex& operator[](size_t i) const {
    return vertices_[i];
  }

  Vertex& CreateVertex() {
    vertices_.emplace_back();
    return vertices_.back();
  }

  void Combine(Vertex* v1, Vertex* v2, Weight v1_shift);

  const Modulus& modulus() const {
    return modulus_;
  }
 private:
  template <typename Vertex>
  struct PathTemplate
  {
   private:
    Vertex* origin_;
    Vertex* terminus_;
    Weight weight_;
    Word unread_word_part_;
   public:
    Vertex& origin() const {
      return *origin_;
    }
    Vertex& terminus() const {
      return *terminus_;
    }

    Weight weight() const {
      return weight_;
    }
    const Word& unread_word_part() const {
      return unread_word_part_;
    }

    PathTemplate(Vertex& origin, Vertex& terminus, Weight weight, Word unread_word_part)
        : origin_(&origin), terminus_(&terminus), weight_(weight), unread_word_part_(std::move(unread_word_part)) {
    }
  };

 public:
  typedef PathTemplate<Vertex> Path;
  typedef PathTemplate<const Vertex> ConstPath;

  Path ReadWord(const Word& w, Vertex& origin, Word::size_type length_limit) const;

  Path ReadWord(const Word& w, Vertex& origin) const {
    return ReadWord(w, origin, w.size());
  }

  ConstPath ReadWord(const Word& w, const Vertex& origin, Word::size_type length_limit) const;

  ConstPath ReadWord(const Word& w, const Vertex& origin) const {
    return ReadWord(w, origin, w.size());
  }

  Path PushWord(const Word& w, Vertex* origin);

  Path PushWord(const Word& w) {
    return PushWord(w, &root());
  }

  void EnsurePath(const Word& w, Vertex* origin, Vertex* terminus, Weight weight);

  void PushCycle(const Word& w, Vertex* origin, Weight weight = 0);

  void PushCycle(const Word& w, Weight weight = 0) {
    PushCycle(w, &root(), weight);
  }

  boost::optional<Word> FindShortestPath(const Vertex& from, const Vertex& to) const;
  boost::optional<Word> FindShortestCycle(const Vertex& base) const;

  bool HasPath(const Vertex& from, const Vertex& to) const;
  bool HasPath(const Word& label, const Vertex& from, const Vertex& to) const;
  bool HasPath(const Word& label, Weight weight, const Vertex& from, const Vertex& to) const;
  bool HasCycle(const Vertex& base) const;
  bool HasCycle(const Word& label, const Vertex& base) const;
  bool HasCycle(const Word& label, Weight weight, const Vertex& base) const;


 private:
  std::deque<Vertex> vertices_;
  Vertex* root_; //!< We need explicit root since vertices_.front() may be merged

  Modulus modulus_;

  friend class FoldedGraphInternalChecks;
};

extern template
struct FoldedGraph::PathTemplate<FoldedGraph::Vertex>;
extern template
struct FoldedGraph::PathTemplate<const FoldedGraph::Vertex>;
extern template
class FoldedGraph::VertexIterT<std::deque<FoldedGraph::Vertex>::iterator>;
extern template
class FoldedGraph::VertexIterT<std::deque<FoldedGraph::Vertex>::const_iterator>;
extern template
class FoldedGraph::EdgesIteratorT<
    FoldedGraph::Vertex, std::array<
        FoldedGraph::EdgeData
        , FoldedGraph::Word::kAlphabetSize>::iterator>;
extern template
class FoldedGraph::EdgesIteratorT<
    const FoldedGraph::Vertex, std::array<
        FoldedGraph::EdgeData
        , FoldedGraph::Word::kAlphabetSize>::const_iterator>;

} //namespace crag

#endif //ACC_FOLDED_GRAPH_H
