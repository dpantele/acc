//
// Created by dpantele on 5/23/16.
//

#ifndef ACC_ACC_CLASSES_H
#define ACC_ACC_CLASSES_H

#include <map>

#include "acc_class.h"
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

  std::map<ACPair, ACClass*>
  GetInitialACIndex() {
    std::map<ACPair, ACClass*> index;
    for (auto&& c : classes_) {
      index.emplace(c.minimal(), &c);
    }
    return index;
  };

  std::deque<ACClass>::iterator
  begin() {
    return classes_.begin();
  }

  std::deque<ACClass>::iterator
  end() {
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
