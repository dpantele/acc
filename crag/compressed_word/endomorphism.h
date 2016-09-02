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
    if (IsIdent()) {
      return w;
    }
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

  constexpr bool IsIdent() const {
    return mapping_[0] == CWord(1, XYLetter('x')) && mapping_[2] == CWord(1, XYLetter('y'));
  }

  //! Returns this ∘ psi
  Endomorphism ComposeWith(Endomorphism psi) const {
    return Endomorphism(psi.Apply(mapping_[0]), psi.Apply(mapping_[2]));
  }

 private:
  CWord mapping_[2 * CWord::kAlphabetSize];
};

}


#endif //ACC_ENDOMORPHISM_H
