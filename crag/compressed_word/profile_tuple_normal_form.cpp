//
// Created by dpantele on 1/25/16.
//

#include "endomorphism.h"
#include "tuple_normal_form.h"
#include <chrono>

template <class T, std::size_t N>
constexpr std::size_t size(const T (& array)[N]) noexcept {
  return N;
}

using namespace crag;

int main() {
  auto pickRandomNielsenAuto = [&](auto& generator) {
    constexpr const Endomorphism aut_generators[] = {
        {"y", "x"}, {"X", "y"}, {"xy", "y"},};
    static std::uniform_int_distribution<size_t> index(0, size(aut_generators) - 1);
    return aut_generators[index(generator)];
  };

  RandomWord random_word(5, 15);

  auto fail_count = 0;
  using Clock = std::chrono::steady_clock;

  constexpr size_t kRunCount = 50;

  auto run = [&] {
    std::mt19937_64 generator;
    for (auto example_count = 0u; example_count < 10u; ++example_count) {
      CWord initial_word = random_word(generator);
      auto initial_normal = MinimalElementInAutomorphicOrbit(initial_word);
      std::vector<CWord> generated_words = {initial_word};
      auto getRandomGeneratedWord = [&] {
        auto index = std::uniform_int_distribution<size_t>(0, generated_words.size() - 1)(generator);
        return generated_words[index];
      };

      while (generated_words.size() < 500) {
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

            if (MinimalElementInAutomorphicOrbit(image) != initial_normal) {
              ++fail_count;
            }
          }
        }
        catch (std::length_error&) {
        }
      }
    }
  };


  std::array<Clock::duration, kRunCount> iteration_time;
  auto current_cell = iteration_time.begin();
  auto saveResult = [&](Clock::duration last_result) {
    if (current_cell == iteration_time.end()) {
      current_cell = iteration_time.begin();
    }
    *current_cell = last_result;
    ++current_cell;
  };

  auto run_count = 0u;

  for (auto i = 0u; i < 3*kRunCount; ++i) {
    auto start = Clock::now();
    run();
    ++run_count;
    saveResult(Clock::now() - start);
  }

  auto min_max_time = std::minmax_element(iteration_time.begin(), iteration_time.end());

  std::cout << std::chrono::duration_cast<std::chrono::duration<double>>(*min_max_time.first).count() << "s .. "
      << std::chrono::duration_cast<std::chrono::duration<double>>(*min_max_time.second).count() << "s\n";
  std::cout << fail_count << std::endl;
  return 0;
}