/**
* @file
* @brief Definition of DisjointSubset
*/
#pragma once
#ifndef _CRAG_DISJOINTSUBSET_H_
#define _CRAG_DISJOINTSUBSET_H_

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <utility>

//! Element of the disjoint-set forest
template <typename Label>
class DisjointSubset {
 public:

  //! Construct a single-element subset
  DisjointSubset(Label l)
    : size_(1)
    , parent_(this)
    , label_(std::move(l))
  { }

  template<typename ... LabelArgs>
  explicit DisjointSubset(LabelArgs&&... label_construct_args)
    : size_(1)
    , parent_(this)
    , label_(std::forward<LabelArgs>(label_construct_args)...)
  { }

  void Reset(Label&& l) {
    if (parent_) {
      throw std::logic_error("You may not reset non-empty subsets");
    }

    parent_ = this;
    size_ = 1;
    label_ = std::move(l);
  }
  //! Construct an empty subset
  template<
    bool IsLabelDefaultConstructible = std::is_default_constructible<Label>::value
    , typename = typename std::enable_if<IsLabelDefaultConstructible>::type
  >
  DisjointSubset(std::nullptr_t)
    : size_()
    , parent_()
    , label_()
  { }

  DisjointSubset(const DisjointSubset&) = delete;
  DisjointSubset(DisjointSubset&&) = delete;

  //! Get the label of the root of the subset
  const Label& root() const {
    return FindRoot()->label_;
  }

  bool operator==(const DisjointSubset& other) const {
    return FindRoot() == other.FindRoot();
  }

  bool operator!=(const DisjointSubset& other) const {
    return FindRoot() != other.FindRoot();
  }

  //! Get the label of the root of the subset
  Label& root() {
    return FindRoot()->label_;
  }

  //! Get the label of this element
  //! Note that it is not updated on the subset merging
  Label& element() {
    return label_;
  }

  //! Get the label of this element
  //! Note that it is not updated on the subset merging
  const Label& element() const {
    return label_;
  }


  //! Get the size of the subset
  size_t size() const {
    return FindRoot()->size_;
  }

  //! Check if this node is the root of the tree
  bool IsRoot() const {
    return parent_ == nullptr || parent_ == this;
  }

  //! Check if subset is invalid
  explicit operator bool() const {
    return parent_ != nullptr;
  }

  //! Merge two subsets
  /**
   * @retval 1 If lhs is a new root
   * @retval 0 If lhs and rhs were representing the same set
   * @retval -1 If rhs is a new root
   */
  template<typename T>
  friend inline int Merge(DisjointSubset<T>* lhs, DisjointSubset<T>* rhs);

  //! Merge this subset with another one, return the label of the new root and the discarded label
  std::pair<Label&, Label&> MergeWith(DisjointSubset<Label>* other) {
    if (!this->IsRoot() || !other->IsRoot()) {
      return FindRoot()->MergeWith(other->FindRoot());
    }

    auto res = Merge(this, other);
    if (res >= 0) {
      return std::pair<Label&, Label&>(this->label_, other->label_);
    } else {
      return std::pair<Label&, Label&>(other->label_, this->label_);
    }
  }


 private:
  //! Halving variant of the disjoint-set find operation
  DisjointSubset* FindRoot() {
    return const_cast<DisjointSubset*>(static_cast<const DisjointSubset*>(this)->FindRoot());
  }

  //! Halving variant of the disjoint-set find operation
  const DisjointSubset* FindRoot() const {
    if (!parent_) {
      return this;
    }
    auto current = this;

    auto parent = current->parent_;
    auto grand_parent = parent->parent_;

    while(parent != grand_parent) {
      current->parent_ = grand_parent;
      current = grand_parent;

      parent = current->parent_;
      grand_parent = parent->parent_;
    }

    return parent;
  }

  size_t size_;
  mutable DisjointSubset* parent_;
  Label label_;
};

template<typename Label>
inline int Merge(DisjointSubset<Label>* lhs, DisjointSubset<Label>* rhs) {
  if (!lhs->IsRoot() || !rhs->IsRoot()) {
    return Merge(lhs->FindRoot(), rhs->FindRoot());
  }

  if ((!*lhs && !*rhs) || *lhs == *rhs) {
    return 0;
  }

  if (lhs->size_ < rhs->size_) {
    return -Merge(rhs, lhs);
  }

  assert(lhs->parent_ == lhs);

  lhs->size_ += rhs->size_;
  rhs->parent_ = lhs;
  return 1;
}


#endif //_CRAG_DISJOINTSUBSET_H_
