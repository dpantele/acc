//
// Created by dpantele on 12/6/15.
//

#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>

#include <compressed_word/compressed_word.h>
#include <crag/compressed_word/enumerate_words.h>
#include "normal_form.h"

using namespace crag;

struct CanonicalWord {
  CWord word_;
  CWord canonical_word_;
  CanonicalWord* root_ = nullptr;
  size_t set_size_ = 1;

  CanonicalWord* root() {
    if (root_ == nullptr) {
      return this;
    }
    root_ = root_->root();
    return root_;
  }

  void Merge(CanonicalWord* other) {
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

  CanonicalWord(CWord word)
    : word_(word)
    , canonical_word_(word)
  { }
};

int main() {
  constexpr const CWord::size_type min_length = 1;
  constexpr const CWord::size_type max_length = 15;

  auto start = std::chrono::steady_clock::now();

  std::deque<CanonicalWord> all_words;

  for (auto&& word : EnumerateWords::CyclicReduced(min_length, max_length + 1)) {
    all_words.emplace_back(word);
    auto root_word = AutomorphicReduction(word);
    if (root_word != word) {
      auto root = std::lower_bound(all_words.begin(), all_words.end(), root_word,
        [](const CanonicalWord& lhs, const CWord& rhs) { return lhs.word_ < rhs; });

      if(root == all_words.end() || root->word_ != root_word) {
        throw std::runtime_error("Root not found");
      }

      all_words.back().Merge(&*root);
    }
  }

  std::cout << "Resolved at " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;

  std::ofstream result("roots.txt");
  for (auto&& word : all_words) {
    if (word.root_ != nullptr) {
      continue;
    }
    PrintWord(word.canonical_word_, &result);
    result << " " << word.set_size_ << std::endl;
  }
  std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;
}