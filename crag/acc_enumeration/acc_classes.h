//
// Created by dpantele on 5/23/16.
//

#ifndef ACC_ACC_CLASSES_H
#define ACC_ACC_CLASSES_H

#include <map>

#include "acc_class.h"
#include "ACIndex.h"
#include "state_dump.h"
#include "config.h"

class ACClasses {
 public:
  ACClasses(const Config& c, ACStateDump* logger)
      : config_(c)
      , logger_(logger)
  { }

  void AddClass(const ACPair& a);

  template<typename C>
  void AddClasses(const C& container) {
    for (auto&& pair : container) {
      AddClass(pair);
    }
  }

  //! Restores the sequence of merges
  void RestoreMerges();
  void RestoreMinimums();

  void InitACIndex(ACIndex* index) {
    std::map<ACPair, ACClass*> pairs_classes;
    for (auto&& c : classes_) {
      auto was_inserted = pairs_classes.emplace(c.minimal(), &c);
      if (!was_inserted.second) {
        c.Merge(was_inserted.first->second);
      }
    }

    auto initial_batch = index->NewBatch();

    for (auto&& p : pairs_classes) {
      initial_batch.Push(p.first, p.second);
    }

    initial_batch.Execute();

    // make sure data is committed
    while (index->GetData().size() < pairs_classes.size()) {
      std::this_thread::yield();
    }
  };

  std::deque<ACClass>::iterator
  begin() {
    return classes_.begin();
  }

  std::deque<ACClass>::const_iterator
  begin() const {
    return classes_.begin();
  }

  std::deque<ACClass>::iterator
  end() {
    return classes_.end();
  }

  std::deque<ACClass>::const_iterator
  end() const {
    return classes_.end();
  }

  ACClass* at(size_t id) {
    return classes_.at(id).canonical_.root();
  }

 private:
  std::deque<ACClass> classes_;
  const Config& config_;
  ACStateDump* logger_;
};

#endif //ACC_ACC_CLASSES_H
