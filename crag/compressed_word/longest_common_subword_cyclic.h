//
// Created by dpantele on 11/7/15.
//

#ifndef ACC_LONGEST_COMMON_SUBWORD_CYCLIC_H
#define ACC_LONGEST_COMMON_SUBWORD_CYCLIC_H

#include <tuple>

#include "compressed_word.h"

namespace crag {

//! Find the position of the longest a such that uu = u_1 a u_2, vv = v_1 a v_2.
std::tuple<unsigned short, unsigned short, unsigned short> LongestCommonSubwordCyclic(CWord u, CWord v);

} //namesapce crag

#endif //ACC_LONGEST_COMMON_SUBWORD_CYCLIC_H
