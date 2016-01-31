//
// Created by dpantele on 12/6/15.
//

#include <chrono>
#include <iterator>
#include <fstream>

#include <compressed_word/compressed_word.h>
#include <compressed_word/tuple_normal_form.h>

using namespace crag;

CWord normal_form(const CWord& w) {
  auto result = w;
  for (auto&& u : ShortestAutomorphicImages(CWordTuple<1>({w}))) {
    if (u[0] < result) {
      result = u[0];
    }
    auto U = LeastCyclicPermutation(u[0].Inverse());
    if (U < result) {
      result = U;
    }
  }
  return result;
}

int main() {
  auto start = std::chrono::steady_clock::now();
  std::ifstream in("ak3_words.txt");
  std::ofstream out("ak3_words_normal.txt");

  std::istream_iterator<std::string> eof;
  for (auto iter = std::istream_iterator<std::string>(in); iter != eof; ++iter) {
    PrintWord(normal_form(CWord(*iter)), &out);
    out << "\n";
  }

  std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;
}