//
// Created by dpantele on 12/6/15.
//

#include <chrono>
#include <fstream>

#include <compressed_word/compressed_word.h>

using namespace crag;

class Automorhpism {
 public:
  CWord Apply(CWord w) const {
    CWord result;
    while (!w.Empty()) {
      result.PushBack(mapping_[w.GetFront().AsInt()]);
      w.PopFront();
    }
    result.CyclicReduce();
    return result;
  }

  Automorhpism(const char* x_image, const char* y_image)
    : mapping_{CWord(x_image), CWord(x_image), CWord(y_image), CWord(y_image)}
  {
    mapping_[1].Invert();
    mapping_[3].Invert();
  }

  typedef std::array<CWord, 2 * CWord::kAlphabetSize> Mapping;
  Mapping mapping_;
 private:

};

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
  static const std::vector<Automorhpism> autos = {
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

  //now try to find powers
//  for (auto period = 1u; period <= w.size() / 2; ++period) {
//    auto shifted = w;
//    shifted.CyclicLeftShift(period);
//    if (shifted == w) {
//      while(w.size() > period) {
//        w.PopBack();
//      }
//      return w;
//    }
//  }

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