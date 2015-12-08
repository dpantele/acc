//
// Created by dpantele on 12/8/15.
//

#ifndef ACC_ENDOMORPHISM_H
#define ACC_ENDOMORPHISM_H

namespace crag {

class Endomorphism
{
 public:
  constexpr crag::CWord Apply(crag::CWord w) const {
    crag::CWord result;
    while (!w.Empty()) {
      result.PushBack(mapping_[w.GetFront().AsInt()]);
      w.PopFront();
    }
    result.CyclicReduce();
    return result;
  }

  constexpr Endomorphism(const char* x_image, const char* y_image)
      : mapping_{crag::CWord(x_image), crag::CWord(x_image), crag::CWord(y_image), crag::CWord(y_image)}
  {
    for (auto i = 0; i < CWord::kAlphabetSize; ++i) {
      mapping_[2*i + 1].Invert();
    }
  }

 private:
  CWord mapping_[2 * CWord::kAlphabetSize];
};

}


#endif //ACC_ENDOMORPHISM_H
