/**
* \file 
* @brief Short word of 2-alphabet stored in a single 64-bit field
*/

#ifndef CRAG_COMPRESSED_WORD_H_
#define CRAG_COMPRESSED_WORD_H_

#include <assert.h>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <random>

#include "xy_letter.h"

namespace crag {

//! Stores word of length up to 32
/**
* The size of alphabet is also fixed and equal to 2. Stores every word as a 64-bit field + short length.
*
* Word is always reduced, but not cyclically reduced.
*/
class CWord {
public:
  typedef unsigned short size_type; //!< STL container spec

  static const unsigned short kAlphabetSize = 2; //!< Size of alphabet, not designed to be changed
  static const size_type kMaxLength = 32;        //!< Maximum length, some runtime checks are performed in debug

  typedef XYLetter Letter; //!< Letters are passed as simple integers
  typedef Letter value_type; //!< STL container req

  CWord()
    : size_(0)
    , letters_(0)
  { }

  //! Construct from an initializer list of 0-3 integers
  CWord(std::initializer_list<Letter> letters);

  //! Construct a power of a letter
  CWord(size_type count, Letter letter);

  //! Construct from an X-Y string
  explicit CWord(const std::string& letters) 
    : CWord(letters.c_str())
  { }

  //! Construct from an X-Y C-string
  explicit CWord(const char* letters);

  bool Empty() const {
    return size_ == 0;
  }

  void Clear() {
    size_ = 0;
    letters_ = 0;
  }

  size_type size() const {
    return size_;
  }

  //! Append a letter
  inline void PushBack(Letter letter);

  //! Append a word. As always, result is reduced
  inline void PushBack(CWord w);

  //! Prepend a letter
  inline void PushFront(Letter letter);

  //! Prepend a word. As always, result is reduced
  inline void PushFront(CWord w);

  //! Remove the last letter
  inline void PopBack();

  //! Remove the last @p count letters
  inline void PopBack(size_type count);

  //! Remove the first letter
  inline void PopFront();

  //! Remove the first @p count letters
  inline void PopFront(size_type count);

  //! Flip the word so that the first letter swaps with the last one, etc
  inline void Flip();

  //! In-place inverse
  inline void Invert();

  //! Get inverted words
  inline CWord Inverse() const;

  //! Get the first letter
  Letter GetFront() const {
    assert(!Empty());
    return Letter(static_cast<unsigned short>(letters_ >> (kLetterShift * (size_ - 1))));
  }

  //! Get the last letter
  Letter GetBack() const {
    assert(!Empty());
    return Letter(static_cast<unsigned short>(letters_ & kLetterMask));
  }

  //! Lexicographic order
  bool operator < (const CWord& other) const {
    return size_ == other.size_ ?  letters_ < other.letters_ : size_ < other.size_;
  }

  bool operator == (const CWord& other) const {
    return letters_ == other.letters_ && size_ == other.size();
  }

  bool operator != (const CWord& other) const {
    return !(*this == other);
  }

  //! Transform xyXY into yXYx (for @p shift = 1)
  inline void CyclicLeftShift(size_type shift = 1);

  //! Transform xyXY into YxyX (for @p shift = 1)
  void CyclicRightShift(size_type shift = 1) {
    CyclicLeftShift(size_ - shift);
  }

  //! Cyclic reduction of the word
  void CyclicReduce() {
    while(!Empty() && (GetFront() == GetBack().Inverse())) {
      PopFront();
      PopBack();
    }
  }

private:
  size_type size_;   //!< The length of the word
  uint64_t letters_; //!< Main bit-compressed storage, every 2 bits is one letters

  static const uint64_t kLetterMask = 3;  //!< Zeros extra bits besides the last two
  static const uint64_t kLetterShift = 2; //!< Lenght of shift which switches a single letter
  static const uint64_t kFullMask = ~uint64_t{0}; //!< 64 true bits

  //! Clears the bits which are not used but coudl be trashed during some bitwise shift
  void ZeroUnused() {
    if (size_) {
      letters_ &= (kFullMask >> (sizeof(kFullMask) * 8 - kLetterShift * size_));
    } else {
      letters_ = 0;
    }
    assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
  }

};

void CWord::CyclicLeftShift(size_type shift) {
  if (Empty()) {
    return;
  }
  shift %= size_;
  auto left_part = (letters_ >> (kLetterShift * (size_ - shift)));
  letters_ <<= kLetterShift * shift;
  letters_ |= left_part;
  ZeroUnused();
}

void CWord::Invert() {
  static const uint64_t kInvertMask = 0x5555555555555555ull;
  Flip();
  letters_ ^= kInvertMask;
  ZeroUnused();
}

void CWord::Flip() {
  // swap consecutive pairs
  letters_ = ((letters_ >> 2) & 0x3333333333333333ull) | ((letters_ & 0x3333333333333333ull) << 2);
  // swap nibbles ...
  letters_ = ((letters_ >> 4) & 0x0F0F0F0F0F0F0F0Full) | ((letters_ & 0x0F0F0F0F0F0F0F0Full) << 4);
  // swap bytes
  letters_ = ((letters_ >> 8) & 0x00FF00FF00FF00FFull) | ((letters_ & 0x00FF00FF00FF00FFull) << 8);
  // swap 2-byte long pairs
  letters_ = ( letters_ >> 16 & 0x0000FFFF0000FFFFull) | ((letters_ & 0x0000FFFF0000FFFFull) << 16);
  // swap 4-byte long pairs
  letters_ = ( letters_ >> 32                        ) | ((letters_                        ) << 32);
  // shift significant part back to the right
  letters_ >>= (sizeof(letters_) * 8 - size_ * kLetterShift);
}

void CWord::PopFront(size_type count) {
  assert(size() >= count);
  size_ -= count;
  ZeroUnused();
}

void inline CWord::PopFront() {
  assert(!Empty());
  --size_;
  ZeroUnused();
}

void CWord::PushBack(Letter letter) {
  if(Empty() || letter.Inverse() != GetBack()) {
    if (size_ == kMaxLength) {
      throw std::length_error("Length of CWord is limited by 32");
    }
    letters_ <<= kLetterShift;
    letters_ |= letter.AsInt();
    ++size_;
  } else {
    PopBack();
  }
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}

void CWord::PushBack(CWord w) {
  while (!w.Empty()) {
    PushBack(w.GetFront());
    w.PopFront();
  }
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}

void CWord::PopBack(size_type count) {
  assert(size() >= count);
  size_ -= count;
  letters_ >>= (kLetterShift * count);
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}

void CWord::PopBack() {
  assert(!Empty());
  --size_;
  letters_ >>= kLetterShift;
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}

void CWord::PushFront(Letter letter) {
  if(Empty() || letter.Inverse() != GetFront()) {
    if (size_ == kMaxLength) {
      throw std::length_error("Length of CWord is limited by 32");
    }
    letters_ |= (static_cast<uint64_t>(letter.AsInt()) << (kLetterShift * size_));
    ++size_;
  } else {
    PopFront();
  }
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}

void CWord::PushFront(CWord w) {
  while (!w.Empty()) {
    PushFront(w.GetBack());
    w.PopBack();
  }
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}


CWord CWord::Inverse() const {
  CWord copy(*this);
  copy.Invert();
  return copy;
}


inline void PrintWord(const CWord& w1, std::ostream* out) {
  CWord w = w1;
  while (!w.Empty()) {
    *out << w.GetFront();
    w.PopFront();
  }
}

//gtest debugging print
inline void PrintTo(const CWord& w, ::std::ostream* out) {
  *out << w.size() << ": ";
  PrintWord(w, out);
}

inline std::ostream& operator<<(std::ostream& out, const CWord& w) {
  PrintTo(w, &out);
  return out;
}

class RandomWord {
public:
  RandomWord(size_t min_size, size_t max_size)
    : random_letter_(0, 2 * CWord::kAlphabetSize - 1)
    , random_length_(min_size, max_size)
  { }

  template<class RandomEngine>
  CWord operator()(RandomEngine& engine) {
    CWord w;
    size_t length = random_length_(engine);
    while(w.size() < length) {
      w.PushBack(random_letter_(engine));
    }
    return w;
  }

private:
  std::uniform_int_distribution<> random_letter_;
  std::uniform_int_distribution<size_t> random_length_;


};

}

#endif //CRAG_COMPREESED_WORDS_H_