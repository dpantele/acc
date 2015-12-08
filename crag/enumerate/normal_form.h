//
// Created by dpantele on 12/8/15.
//

#ifndef ACC_NORMAL_FORM_H
#define ACC_NORMAL_FORM_H

#include <compressed_word/compressed_word.h>

namespace crag {

//! Find the smallest word among all cyclic permutations
CWord LeastCyclicShift(CWord w);

//! Find the smallest word among all cyclic permutations of @p w and its inverse
CWord CyclicNormalForm(CWord w);

//! Use Whitehead algorithm to find a smaller automorphic image
/**
 * Not that CyclicNormalForm is applied to every image
 */
CWord AutomorphicReduction(CWord w);

//! Finds maximal @p n such that there is \f$u^n = w\f$. Returns @p u
CWord TakeRoot(CWord w);

}

#endif //ACC_NORMAL_FORM_H
