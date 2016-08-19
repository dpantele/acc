//
// Created by dpantele on 6/7/16.
//

#ifndef ACC_ACWORKER_H
#define ACC_ACWORKER_H

#include <crag/compressed_word/compressed_word.h>

#include "ACPairProcessQueue.h"
#include "acc_classes.h"
#include "config.h"
#include "state_dump.h"
#include "Terminator.h"

static constexpr crag::CWord::size_type kMaxTotalPairLength = 26u;

inline crag::CWord::size_type MaxHarvestLength(const ACClasses& classes, const ACClasses::ClassId id) {
  const auto& minimal = classes.minimal_in(id);
  auto base_size = minimal[0].size() + minimal[1].size();
  if (base_size > 20) {
    return base_size;
  }
  if (base_size < 15) {
    return 16;
  }

  return static_cast<crag::CWord::size_type>(base_size + 2);
};

struct ACTasksData {
  const Config& config;
  const Terminator& terminator;
  ACPairProcessQueue* queue;
  ACStateDump* dump;
  ACIndex* ac_index;
};

void Process(const ACTasksData&);


#endif //ACC_ACWORKER_H
