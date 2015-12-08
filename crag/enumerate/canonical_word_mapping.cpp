//
// Created by dpantele on 12/8/15.
//

#include <algorithm>

#include <compressed_word/enumerate_words.h>

#include "canonical_word_mapping.h"
#include "normal_form.h"

namespace crag {

CWord MakeLess(CWord w) {
  auto inverse = w.Inverse();

  if (inverse < w) {
    return inverse;
  }

  auto cyclic_shift = w;

  for (auto i = 0; i < w.size(); ++i) {
    cyclic_shift.CyclicLeftShift();
    if (cyclic_shift < w) {
      return cyclic_shift;
    }
  }

  return AutomorphicReduction(w);
}

void CanonicalMapping::CanonicalWord::Merge(CanonicalWord* other) {
  auto this_root = root();
  auto other_root = other->root();
  assert(this_root->root_ == nullptr);
  assert(other_root->root_ == nullptr);
  if (this_root != other_root) {
    if (this_root->set_size_ < other_root->set_size_) {
      //other_root becomes the new root
      this_root->root_ = other_root;
      other_root->set_size_ += this_root->set_size_;
      if (this_root->canonical_word_ < other_root->canonical_word_) {
        other_root->canonical_word_ = this_root->canonical_word_;
      }
    } else {
      other_root->root_ = this_root;
      this_root->set_size_ += other_root->set_size_;
      if (other_root->canonical_word_ < this_root->canonical_word_) {
        this_root->canonical_word_ = other_root->canonical_word_;
      }
    }
  }
}

CanonicalMapping::CanonicalMapping(CWord::size_type min_length, CWord::size_type end_length) {
  for (auto&& word : EnumerateWords::CyclicReduced(min_length, end_length)) {
    mapping_.emplace_back(word);
    auto reduced_word = MakeLess(word);
    if (reduced_word != word) {
      auto root = std::lower_bound(mapping_.begin(), mapping_.end(), reduced_word,
          [](const CanonicalWord& lhs, const CWord& rhs) { return lhs.word_ < rhs; });

      if(root == mapping_.end() || root->word_ != reduced_word) {
        throw std::runtime_error("Image not found");
      }

      mapping_.back().Merge(&*root);
    }
  }
}

const CWord& CanonicalMapping::GetCanonical(const CWord& word) const {
  auto pos = std::lower_bound(mapping_.begin(), mapping_.end(), word,
      [](const CanonicalWord& lhs, const CWord& rhs) { return lhs.word_ < rhs; });

  if (pos == mapping_.end() || pos->word_ != word) {
    throw std::out_of_range("Can't find the word");
  }
  return pos->root()->canonical_word_;
}

} //crag

