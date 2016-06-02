//
// Created by dpantele on 1/14/16.
//

#ifndef ACC_TUPLE_NORMAL_FORM_H
#define ACC_TUPLE_NORMAL_FORM_H

#include <array>
#include <algorithm>
#include <deque>
#include <set>

#include <boost/optional.hpp>

#include "compressed_word.h"
#include "endomorphism.h"

namespace crag {

template<size_t N>
class CWordTuple {
 public:
  constexpr CWordTuple()=default;
  constexpr CWordTuple(std::initializer_list<CWord> init) {
    std::copy(init.begin(), init.end(), words_.begin());
  }
  constexpr CWordTuple(const CWordTuple& other) {
    std::copy(other.words_.begin(), other.words_.end(), words_.begin());
  }
  constexpr CWordTuple& operator=(const CWordTuple& other) {
    std::copy(other.words_.begin(), other.words_.end(), words_.begin());
    return *this;
  }

  CWordTuple(CWordTuple&& other) {
    std::copy(other.words_.begin(), other.words_.end(), words_.begin());
  }

  CWordTuple& operator=(CWordTuple&& other) {
    std::copy(other.words_.begin(), other.words_.end(), words_.begin());
    return *this;
  }

  constexpr void Reverse() {
    auto first = begin();
    auto last = end();
    while ((first!=last)&&(first!=--last)) {
      CWord c = std::move(*first);
      *first = std::move(*last);
      *last = std::move(c);
      ++first;
    }
  }

  constexpr CWordTuple GetReversed() const {
    auto copy = *this;
    copy.Reverse();
    return copy;
  }

  constexpr CWord& operator[](size_t i) {
    return words_[i];
  }

  constexpr const CWord& operator[](size_t i) const {
    return words_[i];
  }

  constexpr auto begin() {
    return std::begin(words_);
  }
  constexpr auto begin() const {
    return std::begin(words_);
  }
  constexpr auto end() {
    return std::end(words_);
  }
  constexpr auto end() const {
    return std::end(words_);
  }
  constexpr auto size() const {
    return N;
  }

  constexpr auto length() const {
    auto result = 0u;
    for (auto&& elem : *this) {
      result += elem.size();
    }
    return result;
  }

  constexpr bool operator<(const CWordTuple& other) const {
    auto this_length = this->length();
    auto other_length = other.length();
    if (this_length != other_length) {
      return this_length < other_length;
    }

    auto mismatch = std::mismatch(begin(), end(), other.begin(), [](const auto& a, const auto& b) { return a.size() == b.size(); });
    if (mismatch.first != end()) {
      return mismatch.first->size() < mismatch.second->size();
    }

    mismatch = std::mismatch(begin(), end(), other.begin());
    if (mismatch.first == end()) {
      return false;
    }
    return *mismatch.first < *mismatch.second;
  }

  constexpr bool operator==(const CWordTuple& other) const {
    return std::mismatch(begin(), end(), other.begin()).first == end();
  }
  constexpr bool operator!=(const CWordTuple& other) const {
    return !(*this == other);
  }
 private:
  std::array<CWord, N> words_;
};
template<size_t N>
std::ostream& operator<<(std::ostream& out, const CWordTuple<N>& t) {
  if (t.size() == 1) {
    return out << t[0];
  }
  out << "(";
  bool is_first = true;
  for (auto&& w : t) {
    if (!is_first) {
      out << ",";
    } else {
      is_first = false;
    }
    out << w;
  }
  return out << ")";
}

//! Return the minimal cyclic permutation of @p w in CWord's order
CWord LeastCyclicPermutation(const CWord& w);

template<size_t N>
CWordTuple<N> LeastCyclicPermutation(CWordTuple<N> words) {
  std::transform(words.begin(), words.end(), words.begin(), [](const CWord& w) { return LeastCyclicPermutation(w); });
  return words;
}

//! Return the minimal cyclic permutation of @p w or its inverse
CWord ConjugationInverseNormalForm(const CWord& w);

template<size_t N>
CWordTuple<N> ConjugationInverseNormalForm(CWordTuple<N> words) {
  std::transform(words.begin(), words.end(), words.begin(), [](const CWord& w) { return ConjugationInverseNormalForm(w); });
  return words;
}

template<size_t N>
size_t Length(const CWordTuple<N>& words) {
  return std::accumulate(words.begin(), words.end(), size_t{0}, [](size_t s, const CWord& w) { return s + w.size(); });
}

template<size_t N>
CWordTuple<N> Apply(const Endomorphism& e, CWordTuple<N> words) {
  std::transform(words.begin(), words.end(), words.begin(), [&e](const CWord& w) { return e.Apply(w); });
  return words;
}

//! Find some whitehead endomorphism which reduce the length of tuple if any
template<size_t N>
boost::optional<std::pair<CWordTuple<N>, Endomorphism>> WhiteheadReduce(const CWordTuple<N>& words) {
  //these are all whitehead automorphisms which may reduce the length of the words tuple
  //I don't include left-multiplications because they are cyclically equivalent to right-multiplications
  static constexpr const Endomorphism kShiftConjAutos[] = {
      {"xy", "y"},
      {"xY", "y"},
      {"x", "yx"},
      {"x", "yX"},
      {"yxY", "y"},
      {"Yxy", "y"},
      {"x", "xyX"},
      {"x", "Xyx"},
  };

  auto initial_length = Length(words);
  auto image = words;

  auto result = std::find_if(std::begin(kShiftConjAutos), std::end(kShiftConjAutos)
    , [&](const Endomorphism& e) {
        try {
          image = Apply(e, words);
          if (Length(image) < initial_length) {
            return true;
          }
        } catch(const std::length_error&) { }
        return false;
      });

  if (result != std::end(kShiftConjAutos)) {
    return std::pair<CWordTuple<N>, Endomorphism>(image, *result);
  }

  return boost::none;
};

template<size_t N>
std::set<CWordTuple<N>> ShortestAutomorphicImages(CWordTuple<N> words) {
  bool length_is_decreasing = true;
  while (length_is_decreasing) {
    auto reduced = WhiteheadReduce(words);
    if (reduced) {
      length_is_decreasing = true;
      words = reduced->first;
    } else {
      length_is_decreasing = false;
    }
  }

  auto minimal_length = Length(words);

  words = LeastCyclicPermutation(words);

  std::set<CWordTuple<N>> minimal_orbit = {words};
  std::deque<const CWordTuple<N>*> to_check = {&words};

  auto addElement = [&](const CWordTuple<N>& new_element) {
    auto result = minimal_orbit.insert(new_element);
    if (result.second) {
      to_check.emplace_back(&*result.first);
    }
  };

  static constexpr const Endomorphism kWhiteheadAutomorphisms [] = {
      //permutations
      {"x", "Y"},
      {"X", "y"},
      {"X", "Y"},
      {"y", "x"},

      //shifts and conjugations
      {"xy", "y"},
      {"xY", "y"},
      {"x", "yx"},
      {"x", "yX"},
      {"yxY", "y"},
      {"Yxy", "y"},
      {"x", "xyX"},
      {"x", "Xyx"},
  };

  while (!to_check.empty()) {
    for (auto&& e : kWhiteheadAutomorphisms) {
      try {
        auto image = Apply(e, *to_check.front());
        assert(Length(image) >= minimal_length);
        if (Length(image) == minimal_length) {
          addElement(LeastCyclicPermutation(image));
        }
      }
      catch(std::length_error&) { }
    }
    to_check.pop_front();
  }

  return minimal_orbit;
}

template<size_t N>
inline CWordTuple<N> MinimalElementInAutomorphicOrbit(const CWordTuple<N>& w) {
  return *ShortestAutomorphicImages(w).begin();
}

inline CWord MinimalElementInAutomorphicOrbit(CWord w) {
  return MinimalElementInAutomorphicOrbit(CWordTuple<1>{w})[0];
}

}

#endif //ACC_TUPLE_NORMAL_FORM_H
