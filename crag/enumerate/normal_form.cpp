//
// Created by dpantele on 12/8/15.
//

#include <compressed_word/endomorphism.h>

#include "normal_form.h"

namespace crag {

CWord LeastCyclicShift(CWord w) {
  CWord result = w;
  for (auto i = 0u; i < w.size(); ++i) {
    w.CyclicLeftShift();
    if (w < result) {
      result = w;
    }
  }

  return result;
}
CWord CyclicNormalForm(CWord w) {
  auto u = LeastCyclicShift(w);
  auto v = LeastCyclicShift(w.Inverse());
  return u < v ? u : v;
}

CWord AutomorphicReduction(CWord w) {
  constexpr const Endomorphism autos[] = {
      {"yx", "y"}
      , {"Yx", "y"}
      , {"xy", "y"}
      , {"xY", "y"}
      , {"yxY", "y"}
      , {"Yxy", "y"}
      , {"x", "yx"}
      , {"x", "yX"}
      , {"x", "xy"}
      , {"x", "Xy"}
      , {"x", "Xyx"}
      , {"x", "xyX"}
      , {"x", "y"}
      , {"x", "Y"}
      , {"X", "y"}
      , {"X", "Y"}
      , {"y", "x"}
      , {"y", "X"}
      , {"Y", "x"}
      , {"Y", "X"}
  };

  for (auto&& a : autos) {
    try {
      auto image = CyclicNormalForm(a.Apply(w));
      if (image < w) {
        return image;
      }
    }
    catch (const std::length_error&) {
    }
  }

  return w;
}

CWord TakeRoot(CWord w) {
  for (CWord::size_type period = 1; period <= w.size()/2; ++period) {
    if (w.size() % period != 0) {
      continue;
    }
    auto w_copy = w;
    w_copy.CyclicLeftShift(period);
    if (w_copy == w) {
      w_copy.PopBack(w.size() - period);
      return w_copy;
    }
  }
  return w;
}

} //crag