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

inline crag::CWord::size_type MaxHarvestLength(const ACClass& c) {
  auto base_size = c.minimal()[0].size() + c.minimal()[1].size();
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
  const ACClasses& ac_classes;
};

void Process(const ACTasksData&);


#endif //ACC_ACWORKER_H
