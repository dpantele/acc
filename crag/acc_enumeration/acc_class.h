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
struct ACClasses;

struct ACClass {
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

  /**
   * Initially ``this``, but points to the current canonical representative among all merged classed
   */
  size_t merged_with_;

  size_t id_; //!< Index amond ACClasses
  ACStateDump* logger_;

  size_t pairs_count_ = 0u;

  //! Simple way to print the state of the class, single-line
  void DescribeForLog(std::ostream* out) const;

  bool IsPrimary() const {
    return merged_with_ == id_;
  }

  ACClass(size_t id, ACPair pair, AutKind kind, ACStateDump* logger);

  // class is trivially copyable & movable
};


#endif //ACC_ACC_CLASS_H
