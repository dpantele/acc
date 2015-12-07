//
// Created by dpantele on 12/6/15.
//

#include <algorithm>
#include <chrono>
#include <fstream>

#include <compressed_word/compressed_word.h>

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
  auto start = std::chrono::steady_clock::now();
  std::ifstream words_map("mapped_words.txt");

  std::vector<CanonicalWord> all_words;

  std::string next_word_string;

  while (words_map >> next_word_string) {
    all_words.emplace_back(CWord(next_word_string));
    words_map >> next_word_string; //skip second word now
  }

  words_map.close();

  std::cout << "Read in " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;

  words_map.open("mapped_words.txt");

  for (auto word = all_words.begin(); word != all_words.end(); ++word) {
    words_map >> next_word_string;
    if (CWord(next_word_string) != word->word_) {
      std::cout << word - all_words.begin() << ": " << next_word_string << " " << word->word_;
      throw std::runtime_error("Different word read");
    }
    words_map >> next_word_string;
    auto root_word = CWord(next_word_string);
    if (root_word != word->word_) {
      auto root = std::lower_bound(all_words.begin(), all_words.end(), root_word,
        [](const CanonicalWord& lhs, const CWord& rhs) { return lhs.word_ < rhs; });
      if(root == all_words.end() || root->word_ != root_word) {
        throw std::runtime_error("Root not found");
      }

      word->Merge(&*root);
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