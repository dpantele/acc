//
// Created by dpantele on 1/14/16.
//

#include <gtest/gtest.h>

#include "compressed_word.h"
#include "tuple_normal_form.h"

#include <iterator>
#include <chrono>
#include <random>

namespace crag {
namespace {

template <class T, std::size_t N>
constexpr std::size_t size(const T (&array)[N]) noexcept
{
  return N;
}

TEST(TupleNormalForm, Example1) {
  EXPECT_EQ(CWord("x"), MinimalElementInAutomorphicOrbit(CWord("x")));
}

TEST(TupleNormalForm, Example2) {
  EXPECT_EQ(CWord("x"), MinimalElementInAutomorphicOrbit(CWord("xy")));
}

TEST(TupleNormalForm, Example3) {
  EXPECT_EQ(CWord("xxxxx"), MinimalElementInAutomorphicOrbit(CWord("xyxyxyxyxy")));
}

TEST(TupleNormalForm, Example4) {
  EXPECT_EQ(CWord("xyXY"), MinimalElementInAutomorphicOrbit(CWord("XYxy")));
}

TEST(TupleNormalForm, Example5) {
  EXPECT_EQ(CWord("xxyxxyxyyXYY"), MinimalElementInAutomorphicOrbit(CWord("xyyXyXXYXYxy")));
}


TEST(TupleNormalForm, StressRandom) {
  using namespace std::chrono_literals;
  using Clock = std::chrono::steady_clock;

  auto pickRandomNielsenAuto = [&](auto& generator) {
    constexpr const Endomorphism aut_generators[] = {
        {"y", "x"},
        {"X", "y"},
        {"xy", "y"},
    };
    static std::uniform_int_distribution<size_t> index(0, size(aut_generators) - 1);
    return aut_generators[index(generator)];
  };

  RandomWord random_word(5, 15);

  auto start = Clock::now();

  auto count = 0;
  while (Clock::now() - start < 10s) {
    ++count;
    std::mt19937_64 generator(count);
    auto example_start = Clock::now();
    CWord word = random_word(generator);
    auto initial_normal = MinimalElementInAutomorphicOrbit(word);
    std::vector<CWord> generated_words = {word};
    auto getRandomGeneratedWord = [&] {
      auto index = std::uniform_int_distribution<size_t>(0, generated_words.size() - 1)(generator);
      return generated_words[index];
    };

    while (Clock::now() - example_start < 1s) {
      try {
        auto word = getRandomGeneratedWord();
        auto image = pickRandomNielsenAuto(generator).Apply(word);
        if (image.size() + 2 >= CWord::kMaxLength) {
          continue;
        }

        auto current_pos = std::upper_bound(generated_words.begin(), generated_words.end(), image);
        if (current_pos == generated_words.begin() || *std::prev(current_pos) != image) {
          generated_words.insert(current_pos, image);
          assert(std::is_sorted(generated_words.begin(), generated_words.end()));

          ASSERT_EQ(initial_normal, MinimalElementInAutomorphicOrbit(image)) << "For image " << image;
        }
      } catch (std::length_error&) { }
    }
    EXPECT_GT(generated_words.size(), 500u);
  }
}

}
}