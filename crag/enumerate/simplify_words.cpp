//
// Created by dpantele on 12/6/15.
//

#include <chrono>
#include <fstream>

#include <compressed_word/compressed_word.h>
#include <compressed_word/endomorphism.h>

using namespace crag;

CWord LeastCyclicShift(CWord w) {
  CWord result = w;
  for (auto i = 0u; i < w.size(); ++i) {
    w.CyclicLeftShift();
    if (w < result) {
      result = w;
    }
  }

  return result;
}

CWord NormalForm(CWord w) {
  auto u = LeastCyclicShift(w);
  auto v = LeastCyclicShift(w.Inverse());
  return u < v ? u : v;
}

CWord MakeLess(CWord w) {
//  auto inverse = w.Inverse();
//
//  if (inverse < w) {
//    return inverse;
//  }
//
//  auto cyclic_shift = w;
//
//  for (auto i = 0; i < w.size(); ++i) {
//    cyclic_shift.CyclicLeftShift();
//    if (cyclic_shift < w) {
//      return cyclic_shift;
//    }
//  }
//
  constexpr const Endomorphism autos[] = {
      {"yx", "y"},
      {"Yx", "y"},
      { "xy", "y" },
      { "xY", "y" },
      { "yxY", "y" },
      { "Yxy", "y" },
      { "x", "yx" },
      { "x", "yX" },
      { "x", "xy" },
      { "x", "Xy" },
      { "x", "Xyx" },
      { "x", "xyX" },
      {"x", "y"},
      {"x", "Y"},
      {"X", "y"},
      {"X", "Y"},
      {"y", "x"},
      {"y", "X"},
      {"Y", "x"},
      {"Y", "X"},
  };

  for (auto&& a : autos) {
    try {
      auto image = NormalForm(a.Apply(w));
      if (image < w) {
        return image;
      }
    } catch(const std::length_error&)
    { }
  }

  return w;
}

int main() {
  auto start = std::chrono::steady_clock::now();
  std::ifstream words("all_words.txt");
  std::ofstream result("mapped_words.txt");

  std::string next_word_string;
  while (words >> next_word_string) {
    auto next_word = CWord(next_word_string);
    PrintWord(next_word, &result);
    result << " ";
    PrintWord(MakeLess(next_word), &result);
    result << std::endl;
  }
  std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;
}