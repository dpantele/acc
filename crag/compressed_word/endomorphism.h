//
// Created by dpantele on 12/8/15.
//

#ifndef ACC_ENDOMORPHISM_H
#define ACC_ENDOMORPHISM_H

#include "compressed_word.h"

namespace crag {

class Endomorphism
{
 public:
  constexpr CWord Apply(CWord w) const {
    CWord result;
    while (!w.Empty()) {
      result.PushBack(mapping_[w.GetFront().AsInt()]);
      w.PopFront();
    }
    result.CyclicReduce();
    return result;
  }

  constexpr Endomorphism(CWord x_image, CWord y_image)
      : mapping_{x_image, x_image, y_image, y_image}
  {
    for (auto i = 0; i < CWord::kAlphabetSize; ++i) {
      mapping_[2*i + 1].Invert();
    }
  }

  constexpr Endomorphism(const char* x_image, const char* y_image)
    : mapping_{CWord(x_image), CWord(x_image), CWord(y_image), CWord(y_image)}
  {
    for (auto i = 0; i < CWord::kAlphabetSize; ++i) {
      mapping_[2*i + 1].Invert();
    }
  }

  //! Applies @p a to every image
  constexpr Endomorphism& RightMultiplyBy(const Endomorphism& a) {
    for(auto& w : mapping_) {
      w = a.Apply(w);
    }
    return *this;
  }

  friend std::ostream& operator<<(std::ostream& out, const Endomorphism& e) {
    out << "(";
    PrintWord(e.mapping_[XYLetter('x').AsInt()], &out);
    out << ",";
    PrintWord(e.mapping_[XYLetter('y').AsInt()], &out);
    out << ")";
    return out;
  }

 private:
  CWord mapping_[2 * CWord::kAlphabetSize];
};

inline Endomorphism operator*(const Endomorphism& a, const Endomorphism& b) {
  Endomorphism result = a;
  result.RightMultiplyBy(b);
  return result;
}

}


#endif //ACC_ENDOMORPHISM_H
