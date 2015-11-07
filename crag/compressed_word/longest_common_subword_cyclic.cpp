//
// Created by dpantele on 11/7/15.
//

#include "longest_common_subword_cyclic.h"

namespace crag {

std::tuple<unsigned short, unsigned short, unsigned short> LongestCommonSubwordCyclic(CWord u, CWord v) {
  unsigned short max_common_prefix = 0;
  unsigned short u_max_begin = u.size();
  unsigned short v_max_begin = v.size();

  for (unsigned short current_u_begin = 0; current_u_begin < u.size(); ++current_u_begin) {
    for (unsigned short current_v_begin = 0; current_v_begin < v.size(); ++current_v_begin) {
      auto u_copy = u;
      auto v_copy = v;
      unsigned short current_common_prefix_length = 0;
      while (u_copy.GetFront() == v_copy.GetFront()) {
        ++current_common_prefix_length;
        u_copy.PopFront();
        if (u_copy.Empty()) {
          break;
        }
        v_copy.PopFront();
        if (v_copy.Empty()) {
          break;
        }
      }
      if (current_common_prefix_length > max_common_prefix) {
        u_max_begin = current_u_begin;
        v_max_begin = current_v_begin;
        max_common_prefix = current_common_prefix_length;
      }
      v.CyclicLeftShift();
    }
    u.CyclicLeftShift();
  }

  return std::make_tuple(u_max_begin, v_max_begin, max_common_prefix);
}

} //namespace crag