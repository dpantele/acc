//
// Created by dpantele on 12/6/15.
//

#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>

#include <compressed_word/compressed_word.h>
#include "canonical_word_mapping.h"

using namespace crag;

int main() {
  constexpr const CWord::size_type min_length = 1;
  constexpr const CWord::size_type max_length = 15;

  auto start = std::chrono::steady_clock::now();

  CanonicalMapping mapping(min_length, max_length + 1);

  std::cout << "Resolved at " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;

  std::ofstream result("roots.txt");
  for (auto&& word : mapping.mapping()) {
    if (word.root_ != nullptr) {
      continue;
    }
    PrintWord(word.canonical_word_, &result);
    result << " " << word.set_size_ << std::endl;
  }
  std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;
}