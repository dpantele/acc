//
// Created by dpantele on 11/8/15.
//

#include <stdlib.h>
#include "modulus.h"

namespace crag {

using Weight = Modulus::Weight;

Weight Gcd(Weight a, Weight b) {
  while (b != 0) {
    auto c = a % b;
    a = b;
    b = c;
  }

  return a;
}

void Modulus::EnsureEqual(Weight w1, Weight w2) {
  modulus_ = Gcd(abs(Reduce(w1 - w2)), modulus_);
}


}