//
// Created by dpantele on 12/6/15.
//

#include <chrono>
#include <fstream>

#include <compressed_word/compressed_word.h>
#include <compressed_word/enumerate_words.h>
#include "normal_form.h"

using namespace crag;

int main() {
  constexpr const CWord::size_type min_length = 1;
  constexpr const CWord::size_type max_length = 5;

  auto start = std::chrono::steady_clock::now();
  std::ofstream result("mapped_words.txt");

  for (auto&& word : EnumerateWords::CyclicReduced(min_length, max_length)) {
    PrintWord(word, &result);
    result << " ";
    PrintWord(AutomorphicReduction(word), &result);
    result << "\n";
  }

  std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;
}