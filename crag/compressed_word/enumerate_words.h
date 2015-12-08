//
// Created by dpantele on 12/8/15.
//

#ifndef ACC_ENUMERATEWORDS_H
#define ACC_ENUMERATEWORDS_H

#include "compressed_word.h"

namespace crag {

//! Enumerates all reduced words of defined size.
class EnumerateWords
{
  struct Iter {
    CWord word_;
    bool cyclic_reduced_;

    constexpr Iter& operator++() {
      word_.ToNextWord();
      if (cyclic_reduced_) {
        while (word_.GetFront().Inverse() == word_.GetBack()) {
          word_.ToNextWord();
        }
      }
      return *this;
    }

    constexpr const CWord& operator*() const {
      return word_;
    }

    constexpr CWord& operator*() {
      return word_;
    }

    constexpr bool operator==(const Iter& other) const {
      return word_ == other.word_;
    }

    constexpr bool operator!=(const Iter& other) const {
      return word_ != other.word_;
    }
  };

  CWord::size_type min_length_;
  CWord::size_type end_length_ = static_cast<CWord::size_type>(min_length_ + 1);
  bool cyclic_reduced_ = false;

  constexpr EnumerateWords(CWord::size_type min_length, CWord::size_type end_length, bool cyclic_reduced)
      : min_length_(min_length), end_length_(end_length), cyclic_reduced_(cyclic_reduced) {
    assert(min_length < end_length);
  }


 public:
  constexpr EnumerateWords(CWord::size_type length)
      : min_length_(length) {
  }

  constexpr EnumerateWords(CWord::size_type min_length, CWord::size_type end_length)
      : min_length_(min_length), end_length_(end_length) {
    assert(min_length < end_length);
  }

  constexpr Iter begin() const {
    return Iter{CWord(min_length_, XYLetter('x')), cyclic_reduced_};
  }

  constexpr Iter end() const {
    return Iter{CWord(end_length_, XYLetter('x')), cyclic_reduced_};
  }

  static constexpr EnumerateWords CyclicReduced(CWord::size_type length) {
    return EnumerateWords(length, static_cast<CWord::size_type>(length + 1), true);
  }
  static constexpr EnumerateWords CyclicReduced(CWord::size_type min_length, CWord::size_type end_length) {
    return EnumerateWords(min_length, end_length, true);
  }
};

} //crag


#endif //ACC_ENUMERATEWORDS_H
