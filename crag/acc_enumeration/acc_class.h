//
// Created by dpantele on 5/23/16.
//

#ifndef ACC_ACC_CLASS_H
#define ACC_ACC_CLASS_H

#include <bitset>
#include <fstream>
#include <map>
#include <ostream>

#include <crag/folded_graph/complete.h>
#include <crag/compressed_word/tuple_normal_form.h>
#include <crag/disjoint_subsets/disjoint_subsets.h>

#include "acc_class.h"
#include "config.h"

typedef crag::CWordTuple<2> ACPair;

struct ACStateDump;

struct ACClass {
  //it will have a fixed location in memory so that the pointers are stable
  ACClass(const ACClass&) = delete;
  ACClass(ACClass&&) = delete;

  ACPair initial_;

  enum class AutKind : int {
    Ident = 0, //!< The original pair
    x_xy  = 1, //!< x->xy
    x_y   = 2, //!< x<->y
    y_Y   = 3, //!< y->Y
  };

  AutKind init_kind_;

  std::bitset<4> aut_types_;
  ACPair minimal_;

  DisjointSubset<ACClass*> canonical_;

  size_t id_;
  ACStateDump* logger_;

  size_t pairs_count_ = 0u;

  //! Simple way to print the state of the class, single-line
  void DescribeForLog(std::ostream* out);

  //! Get the 'canonical' representative for the class
  const crag::CWordTuple<2>& minimal() const {
    return canonical_.root()->minimal_;
  }

  //! Check if automorphisms may be used for this class
  /**
   * This become true as soon as it gets merged with all 3 automorphic 'relatives',
   * i.e. there is are 3 pairs of words in this class such that \phi(u, v) ~(ACM) (u, v)
   */
  bool AllowsAutMoves() const {
    return canonical_.root()->aut_types_.all();
  }

  bool IsPrimary() const {
    return canonical_.IsRoot();
  }

  ACClass(size_t id, ACPair pair, AutKind kind, ACStateDump* logger);

  void AddPair(ACPair pair);
  void Merge(ACClass* other);

  static ACStateDump CreateLogger(const Config& c);
};


#endif //ACC_ACC_CLASS_H
