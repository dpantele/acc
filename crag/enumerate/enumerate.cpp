#include <iostream>
#include <fstream>
#include <set>
#include <chrono>
#include <algorithm>

#include <compressed_word/compressed_word.h>
#include <compressed_word/enumerate_words.h>

using namespace crag;

unsigned int CountWords(size_t length) {
  auto result = 4u;
  while (length > 1) {
    --length;
    result *= 3u;
  }
  return result;
}

int main() {
  std::ofstream out("all_words.txt");
  for(CWord::size_type length = 1; length <= 5; ++length) {
    std::cout << "Length " << length << ": "
        << std::flush;
//    auto start = std::chrono::steady_clock::now();
    size_t count = 0;
    for (auto&& current_word : EnumerateWords::CyclicReduced(length)) {
      PrintWord(current_word, &out);
      out << "\n";
      ++count;
    }

    std::cout << count << std::endl;
  }
}

