#include "compressed_word.h"

#include <algorithm>

namespace crag {

CWord::CWord(std::initializer_list<Letter> letters)
    : size_(0)
    , letters_(0)
{
  for (auto letter : letters) {
    if (!Empty() && GetBack() == letter.Inverse()) {
      PopBack();
    } else {
      if (size_ == kMaxLength) {
        throw std::length_error("Length of CWord is limited by 32");
      }
      letters_ <<= kLetterShift;
      letters_ |= letter.AsInt();
      ++size_;
    }
  }
  assert((letters_ >> (kLetterShift * size_)) == 0);
}

CWord::CWord(size_type count, Letter letter) : size_(0)
  , letters_(0) {
  while (count > 0) {
    --count;
    PushBack(letter);
  }
  assert((letters_ >> (kLetterShift * size_)) == 0);
}

CWord::CWord(const char* letters)
  : size_(0)
  , letters_(0)
{
  for (; *letters != 0; ++letters) {
    PushBack(Letter(*letters));
  }
  assert((letters_ >> (kLetterShift * size_)) == 0);
}


} //namespace crag
