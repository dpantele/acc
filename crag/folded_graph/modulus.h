//
// Created by dpantele on 11/8/15.
//

#ifndef ACC_MODULUS_H
#define ACC_MODULUS_H

#include <stdint.h>

namespace crag {

class Modulus {
 public:
  using Weight = int64_t;

  bool AreEqual(Weight w1, Weight w2) const {
    return Reduce(w1 - w2) == 0;
  }

  void EnsureEqual(Weight w1, Weight w2);

  bool infinite() const {
    return modulus_ == 0;
  }

  Weight modulus() const {
    return modulus_;
  }

  Weight Reduce(Weight w) const {
    return modulus_ == 0 ? w : (((w % modulus_) + modulus_) % modulus_);
  }
 private:
  Weight modulus_ = 0;
};

}


#endif //ACC_MODULUS_H
