//
// Created by dpantele on 12/6/15.
//

#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>

#include <compressed_word/compressed_word.h>
#include <compressed_word/endomorphism.h>
#include <crag/compressed_word/enumerate_words.h>
#include "canonical_word_mapping.h"
#include "normal_form.h"

using namespace crag;

std::vector<Endomorphism> GenAllEndomorphisms(CWord::size_type max_image_length) {
  std::vector<Endomorphism> result;
  for(CWord::size_type total_length = 2; total_length <= 2*max_image_length; ++total_length) {
    for(CWord::size_type x_image_length = 1; x_image_length + 1 < total_length; ++x_image_length) {
      for (auto&& x_image : EnumerateWords(x_image_length)) {
        auto y_image_length = static_cast<CWord::size_type>(total_length - x_image_length);
        for (auto&& y_image : EnumerateWords(y_image_length)) {
          result.emplace_back(Endomorphism(x_image, y_image));
        }
      }
    }
  }
  return result;
}

int main() {
  constexpr const CWord::size_type min_length = 1;
  constexpr const CWord::size_type max_length = 15;

  auto start = std::chrono::steady_clock::now();

  CanonicalMapping mapping(min_length, max_length + 1);

  std::cout << "Resolved at " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;

  const auto all_endomorphisms = GenAllEndomorphisms(5);
  std::cout << all_endomorphisms.size() << std::endl;

  struct CanonicalType {
    CWord root_;
    size_t class_size_;

    CWord type_ = root_;
  };

  std::vector<CanonicalType> canonical_types_;

  std::ofstream result("roots.txt");
  for (auto&& word : mapping.mapping()) {
    if (word.root_ != nullptr) {
      continue;
    }
    canonical_types_.emplace_back(CanonicalType{word.canonical_word_, word.set_size_});
  }

  std::sort(canonical_types_.begin(), canonical_types_.end(), [](
      const CanonicalType& a, const CanonicalType& b) {
        return a.root_ < b.root_;
      });


  //first remove roots
  for (auto&& canonical : canonical_types_) {
    auto root = canonical.root_;
    auto power_root = TakeRoot(root);

    if (root != power_root) {
      canonical.type_ = power_root;
    }
  }

  //now list all images of y^(-1) x^n y x^(-m)
  for (CWord::size_type n_plus_m = 2; n_plus_m <= max_length - 2; ++n_plus_m) {
    for (CWord::size_type n = 1; n < n_plus_m; ++n) {
      auto m = static_cast<CWord::size_type>(n_plus_m - n);
      assert(m > 0);

      //construct y^(-1) x^n y x^(-m)
      auto the_word = CWord("Y") + CWord(n, XYLetter('x')) + CWord("y") + CWord(m, XYLetter('X'));
      std::cout << "Processing ";
      auto images_start = std::chrono::steady_clock::now();
      PrintWord(the_word, &std::cout);
      std::cout.flush();
      assert(the_word.size() <= max_length);

      for (auto&& end : all_endomorphisms) {
        try {
          auto image = end.Apply(the_word);
          if (image.size() > max_length || image.size() < 1) {
            continue;
          }

          auto canonical_image = mapping.GetCanonical(image);

          if (canonical_image.size() == 1) {
            continue;
          }

          if (canonical_image.size() < the_word.size()) {
            continue;
          }

          auto image_type = std::lower_bound(canonical_types_.begin(), canonical_types_.end(), canonical_image,
            [](auto& type, auto& word) { return type.root_ < word; });

          assert(image_type != canonical_types_.end() && image_type->root_ == canonical_image);

          if (image_type->type_ == image_type->root_) {
            image_type->type_ = the_word;
          }
        } catch (const std::length_error&) { /*do nothing*/ }
      }
      std::cout << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - images_start).count() << std::endl;

    }
  }

  for (auto&& canonical : canonical_types_) {
    PrintWord(canonical.root_, &result);
    result << ' ' << canonical.class_size_;

    if (canonical.root_ != canonical.type_) {
      result << " (of type ";
      PrintWord(canonical.type_, &result);
      result << ")";
    }

    result << "\n";
  }

  std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;
}