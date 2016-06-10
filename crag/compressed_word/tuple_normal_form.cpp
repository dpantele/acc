//
// Created by dpantele on 1/14/16.
//

#include "tuple_normal_form.h"

namespace crag {


CWord LeastCyclicPermutation(const CWord& w) {
  auto shifted = w;
  shifted.CyclicLeftShift();
  auto min = w;
  while (shifted != w) {
    if (shifted < min) {
      min = shifted;
    }
    shifted.CyclicLeftShift();
  }
  return min;
}
CWord ConjugationInverseNormalForm(const CWord& w) {
  assert(w.Empty() || w.GetBack().Inverse() != w.GetFront());
  auto min_w = LeastCyclicPermutation(w);
  auto min_i = LeastCyclicPermutation(w.Inverse());
  if (min_w < min_i) {
    return min_w;
  } else {
    return min_i;
  }
}

}