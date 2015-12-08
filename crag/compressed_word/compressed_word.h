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

  static constexpr const unsigned short kAlphabetSize = 2; //!< Size of alphabet, not designed to be changed
  static constexpr const size_type kMaxLength = 32;        //!< Maximum length, some runtime checks are performed in debug

  typedef XYLetter Letter; //!< Letters are passed as simple integers
  typedef Letter value_type; //!< STL container req

  constexpr CWord()
    : size_(0)
    , letters_(0)
  { }

  //! Construct from an initializer list of 0-3 integers
  CWord(std::initializer_list<Letter> letters);

  //! Construct a power of a letter
  constexpr CWord(size_type count, Letter letter);

  //! Construct from an X-Y string
  explicit CWord(const std::string& letters) 
    : CWord(letters.c_str())
  { }

  //! Construct from an X-Y C-string
  constexpr explicit CWord(const char* letters);

  constexpr bool Empty() const {
    return size_ == 0;
  }

  constexpr void Clear() {
    size_ = 0;
    letters_ = 0;
  }

  constexpr size_type size() const {
    return size_;
  }

  //! Append a letter
  constexpr inline void PushBack(Letter letter);

  //! Append a word. As always, result is reduced
  constexpr inline void PushBack(CWord w);

  //! Prepend a letter
  constexpr inline void PushFront(Letter letter);

  //! Prepend a word. As always, result is reduced
  constexpr inline void PushFront(CWord w);

  //! Remove the last letter
  constexpr inline void PopBack();

  //! Remove the last @p count letters
  constexpr inline void PopBack(size_type count);

  //! Remove the first letter
  constexpr inline void PopFront();

  //! Remove the first @p count letters
  constexpr inline void PopFront(size_type count);

  //! Flip the word so that the first letter swaps with the last one, etc
  constexpr inline void Flip();

  //! In-place inverse
  constexpr inline void Invert();

  //! Get inverted words
  constexpr inline CWord Inverse() const;

  //! Get the first letter
  constexpr Letter GetFront() const {
    assert(!Empty());
    return Letter(static_cast<unsigned short>(letters_ >> (kLetterShift * (size_ - 1))));
  }

  //! Get the last letter
  constexpr Letter GetBack() const {
    assert(!Empty());
    return Letter(static_cast<unsigned short>(letters_ & kLetterMask));
  }

  //! Lexicographic order
  constexpr bool operator < (const CWord& other) const {
    return size_ == other.size_ ?  letters_ < other.letters_ : size_ < other.size_;
  }

  constexpr bool operator == (const CWord& other) const {
    return letters_ == other.letters_ && size_ == other.size();
  }

  constexpr bool operator != (const CWord& other) const {
    return !(*this == other);
  }

  //! Transform xyXY into yXYx (for @p shift = 1)
  constexpr inline void CyclicLeftShift(size_type shift = 1);

  //! Transform xyXY into YxyX (for @p shift = 1)
  constexpr void CyclicRightShift(size_type shift = 1) {
    CyclicLeftShift(size_ - shift);
  }

  //! Cyclic reduction of the word
  constexpr void CyclicReduce() {
    while(!Empty() && (GetFront() == GetBack().Inverse())) {
      PopFront();
      PopBack();
    }
  }

  //! Proceeds to the next word in the defined order
  inline constexpr CWord& ToNextWord();

private:
  size_type size_;   //!< The length of the word
  uint64_t letters_; //!< Main bit-compressed storage, every 2 bits is one letters

  static const uint64_t kLetterMask = 3;  //!< Zeros extra bits besides the last two
  static const uint64_t kLetterShift = 2; //!< Lenght of shift which switches a single letter
  static const uint64_t kFullMask = ~uint64_t{0}; //!< 64 true bits

  constexpr uint64_t current_mask() {
    if (size_) {
      return (kFullMask >> (sizeof(kFullMask) * 8 - kLetterShift * size_));
    } else {
      return 0;
    }
  }

  //! Clears the bits which are not used but could be trashed during some bitwise shift
  constexpr void ZeroUnused() {
    letters_ &= current_mask();
    assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
  }

};

constexpr CWord::CWord(size_type count, Letter letter)
    : CWord() {
  while (count > 0) {
    --count;
    PushBack(letter);
  }
  assert((letters_ >> (kLetterShift * size_)) == 0);
}

constexpr CWord::CWord(const char* letters)
    : size_(0)
      , letters_(0)
{
  for (; *letters != 0; ++letters) {
    PushBack(Letter(*letters));
  }
  assert((letters_ >> (kLetterShift * size_)) == 0);
}

constexpr void CWord::CyclicLeftShift(size_type shift) {
  if (Empty()) {
    return;
  }
  shift %= size_;
  auto left_part = (letters_ >> (kLetterShift * (size_ - shift)));
  letters_ <<= kLetterShift * shift;
  letters_ |= left_part;
  ZeroUnused();
}

constexpr void CWord::Invert() {
  constexpr const uint64_t kInvertMask = 0x5555555555555555ull;
  Flip();
  letters_ ^= kInvertMask;
  ZeroUnused();
}

constexpr void CWord::Flip() {
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

constexpr void CWord::PopFront(size_type count) {
  assert(size() >= count);
  size_ -= count;
  ZeroUnused();
}

constexpr void inline CWord::PopFront() {
  assert(!Empty());
  --size_;
  ZeroUnused();
}

constexpr void CWord::PushBack(Letter letter) {
  if(Empty() || letter.Inverse() != GetBack()) {
    size_ == kMaxLength
        ? throw std::length_error("Length of CWord is limited by 32")
        : letters_ <<= kLetterShift;

    letters_ |= letter.AsInt();
    ++size_;
  } else {
    PopBack();
  }
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}

constexpr void CWord::PushBack(CWord w) {
  while (!w.Empty()) {
    PushBack(w.GetFront());
    w.PopFront();
  }
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}

constexpr void CWord::PopBack(size_type count) {
  assert(size() >= count);
  size_ -= count;
  letters_ >>= (kLetterShift * count);
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}

constexpr void CWord::PopBack() {
  assert(!Empty());
  --size_;
  letters_ >>= kLetterShift;
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}

constexpr void CWord::PushFront(Letter letter) {
  if(Empty() || letter.Inverse() != GetFront()) {
    size_ == kMaxLength
      ? throw std::length_error("Length of CWord is limited by 32")
      : letters_ |= (static_cast<uint64_t>(letter.AsInt()) << (kLetterShift * size_));

    ++size_;
  } else {
    PopFront();
  }
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}

constexpr void CWord::PushFront(CWord w) {
  while (!w.Empty()) {
    PushFront(w.GetBack());
    w.PopBack();
  }
  assert(size_ == kMaxLength || (letters_ >> (kLetterShift * size_)) == 0);
}


constexpr CWord CWord::Inverse() const {
  CWord copy(*this);
  copy.Invert();
  return copy;
}

constexpr CWord& CWord::ToNextWord() {
  assert(~letters_ != 0 && "CWord is limited by length 32");
  auto new_letters = letters_ + 1;
  if ((new_letters & current_mask()) == 0) {
    //increase length and reset letters
    assert(size_ != kMaxLength && "This should never happen since all Ys is a good word");
    ++size_;
    letters_ = 0; //all xs also has no cancellations
    return *this;
  }

  if (size_ == 1) {
    letters_ = new_letters;
    return *this;
  }

  auto checked_count = 1u; // Last letter is always fine
  auto to_check = new_letters;
  auto last_letter = Letter(to_check & CWord::kLetterMask);
  while (checked_count < size_) {
    to_check >>= CWord::kLetterShift;
    auto current_letter = Letter(to_check & CWord::kLetterMask);
    if (current_letter.Inverse() == last_letter) {
      to_check = ++new_letters;
      last_letter = Letter(to_check & CWord::kLetterMask);
      checked_count = 1u;
    } else {
      last_letter = current_letter;
      ++checked_count;
    }
  }
  letters_ = new_letters;
  return *this;
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