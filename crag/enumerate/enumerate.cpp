#include <compressed_word/compressed_word.h>

#include <iostream>
#include <fstream>
#include <set>
#include <chrono>
#include <algorithm>

using namespace crag;

CWord NumberToWord(size_t length, unsigned int* num) {
  CWord result;
  result.PushBack(*num % 4);
  *num /= 4;

  while (result.size() < length) {
    assert(result.GetBack().Inverse() != ((*num % 3) ^ result.GetBack().AsInt() ^ 2));
    result.PushBack(((*num % 3) ^ result.GetBack().AsInt() ^ 2));
    *num /= 3;
  }
  return result;
}

CWord NumberToWord(size_t length, unsigned int num) {
  return NumberToWord(length, &num);
}

unsigned int CountWords(size_t length) {
  auto result = 4u;
  while (length > 1) {
    --length;
    result *= 3u;
  }
  return result;
}

int main() {
  std::chrono::steady_clock::duration last_duration{};
  auto program_start = std::chrono::steady_clock::now();
  double last_coef = 1.;
  std::vector<CWord> all_words;
  all_words.reserve(CountWords(17));
  for(auto length = 1u; length <= 15; ++length) {
    std::cout << "Length " << length << ": "
        << std::chrono::duration_cast<std::chrono::duration<double>>(last_duration).count() / last_coef << ": "
        << std::flush;
    auto start = std::chrono::steady_clock::now();
    for (auto i = 0u; ; ++i) {
      auto iter = i;
      auto result = NumberToWord(length, &iter);
      if (iter != 0) {
        break;
      }
      if (result.GetBack().Inverse() == result.GetFront()) {
        continue;
        //Length 16: 2663428
      }
      all_words.push_back(result);
    }

    std::cout << all_words.size() << " ";
    auto sort_start = std::chrono::steady_clock::now();
    std::sort(all_words.begin(), all_words.end());
    std::cout << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - sort_start).count() << " ";
    std::ofstream out("all_words.txt", std::ios_base::app);

    for(auto&& word : all_words) {
      PrintWord(word, &out);
      out.put('\n');
    }
    out.close();
    all_words.clear();
    auto current_duration = std::chrono::steady_clock::now() - start;
    last_coef = double(last_duration.count()) / (current_duration.count());
    std::cout << last_coef << " ";
    std::cout << std::chrono::duration_cast<std::chrono::duration<double>>(current_duration).count() << std::endl;
    last_duration = current_duration;
  }
  std::cout << "Total duration: "
      << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - program_start).count() << std::endl;
}

