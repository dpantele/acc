//
// Created by dpantele on 12/6/15.
//

#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>

#include <boost/variant.hpp>

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

CWord::size_type CommonPrefixLength(CWord u, CWord v) {
  CWord::size_type result = 0;
  while (u.GetFront() == v.GetFront()) {
    ++result;
    u.PopFront();
    v.PopFront();
  }
  return result;
}

CWord::size_type LongestPieceFixedStart(const CWord& w) {
  CWord::size_type result = 1;
  auto shifted = w;
  shifted.CyclicLeftShift();
  while (shifted != w) {
    result = std::max(result, CommonPrefixLength(w, shifted));
    if (result * 6 >= w.size()) {
      return result;
    }
    shifted.CyclicLeftShift();
  }

  auto inverse = w.Inverse();
  if (LeastCyclicShift(inverse) == LeastCyclicShift(w)) {
    return result;
  }
  result = std::max(result, CommonPrefixLength(w, inverse));
  inverse.CyclicLeftShift();

  while (inverse != w.Inverse()) {
    result = std::max(result, CommonPrefixLength(w, inverse));
    if (result * 6 >= w.size()) {
      return result;
    }
    inverse.CyclicLeftShift();
  }

  return result;
}

CWord::size_type LongestPiece(const CWord& w) {
  auto shifted = w;
  CWord::size_type result = 1;
  do {
    result = std::max(result, LongestPieceFixedStart(shifted));
    if (result * 6 >= w.size()) {
      return result;
    }
    shifted.CyclicLeftShift();
  } while (shifted != w);
  return result;
}

int main() {
  constexpr const CWord::size_type min_length = 1;
  constexpr const CWord::size_type max_length = 16;

  auto start = std::chrono::steady_clock::now();

  CanonicalMapping mapping(min_length, max_length + 1);

  std::cout << "Resolved at " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;

  const auto all_endomorphisms = GenAllEndomorphisms(5);
  std::cout << all_endomorphisms.size() << std::endl;

  struct TrivialType { };

  struct UnknownType { };

  struct PowerType {
    CWord root_;
  };

  struct BSType {
    int n;
    int m;
  };

  struct C16Type {  };

  class PrintTypeVisitor
      : public boost::static_visitor<>
  {
   public:
    std::ostream& out_;

    PrintTypeVisitor(std::ostream& out)
        : out_(out)
    { }

    void operator()(UnknownType&) const
    { }

    void operator()(TrivialType&) const {
      out_ << "  trivial";
    }

    void operator()(PowerType& t) const
    {
      out_ << "  power of ";
      PrintWord(t.root_, &out_);
    }
    void operator()(BSType& t) const
    {
      out_ << "  of type BS(" << t.n << ", " << t.m << ")";
    }
    void operator()(C16Type&) const
    {
      out_ << "  of type C'(1/6)";
    }
  };

  struct CanonicalType {
    CWord root_;
    size_t class_size_;

    boost::variant<UnknownType, TrivialType, PowerType, BSType, C16Type> type_ = UnknownType{};
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

  //Mark x as trivial
  assert(canonical_types_[0].root_ == CWord("x"));
  canonical_types_[0].type_ = TrivialType{};

  //first remove roots
  for (auto&& canonical : canonical_types_) {
    auto root = canonical.root_;
    auto power_root = TakeRoot(root);

    if (root != power_root) {
      canonical.type_ = PowerType{power_root};
    }
  }

  //check C16
  for (auto&& canonical : canonical_types_) {
    if (!boost::get<UnknownType>(&canonical.type_)) {
      continue;
    }
    if (LongestPiece(canonical.root_) * 6 < canonical.root_.size()) {
      canonical.type_ = C16Type{};
    }
  }

  //now list all images of y^(-1) x^n y x^(-m)
  std::deque<std::pair<CWord, BSType>> images_;

  auto setBSType = [&](const CWord& bs_word, BSType bs_type) {
    auto canonical_image = mapping.GetCanonical(bs_word);
    auto image_type = std::lower_bound(canonical_types_.begin(), canonical_types_.end(), canonical_image,
        [](auto& type, auto& word) { return type.root_ < word; });

    assert(image_type != canonical_types_.end() && image_type->root_ == canonical_image);

    if (boost::get<UnknownType>(&image_type->type_)) {
      image_type->type_ = bs_type;
      images_.emplace_back(canonical_image, bs_type);
      return true;
    } else {
      return false;
    }
  };

  for (CWord::size_type n_plus_m = 2; n_plus_m <= max_length - 2; ++n_plus_m) {
    for (CWord::size_type n = 1; n < n_plus_m; ++n) {
      auto m = static_cast<CWord::size_type>(n_plus_m - n);
      assert(m > 0);

      setBSType(
          CWord("Y") + CWord(n, XYLetter('x')) + CWord("y") + CWord(m, XYLetter('X')),
          BSType{n, m}
      );

      setBSType(
          CWord("Y") + CWord(n, XYLetter('x')) + CWord("y") + CWord(m, XYLetter('x')),
          BSType{n, -static_cast<int>(m)}
      );
    }
  }

  auto processed_count = 0u;

  while (!images_.empty()) {
    auto the_word = images_.front().first;
    auto the_type = images_.front().second;
    images_.pop_front();

    ++processed_count;
    std::cout << processed_count << "/" << processed_count + images_.size() << ": ";
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

        setBSType(image, the_type);
      } catch (const std::length_error&) { /*do nothing*/ }
    }
    std::cout << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - images_start).count() << std::endl;
  }

  for (auto&& canonical : canonical_types_) {
    PrintWord(canonical.root_, &result);
    result << ' ' << canonical.class_size_;

    boost::apply_visitor(PrintTypeVisitor(result), canonical.type_);
    result << "\n";
  }

  std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start).count() << std::endl;
}