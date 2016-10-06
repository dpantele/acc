//
// Created by dpantele on 6/19/16.
//

#ifndef ACC_ACINDEX_H
#define ACC_ACINDEX_H

#include <memory>

#include <crag/multithreading/sem.h>
#include <crag/multithreading/SharedQueue.h>

#include "acc_class.h"
#include "config.h"
#include "acc_classes.h"
#include "ACPairProcessQueue.h"

//! Thread-safe version of an index which adds multiple items at once
class ACIndex {
  struct Storage;
 public:
  ACIndex(const Config &c, std::shared_ptr<ACClasses> initial_classes, ACPairProcessQueue* pairs_queue);
  ~ACIndex();

  // just to be safe - we don't need the index to be copyable
  ACIndex(const ACIndex&)=delete;
  ACIndex& operator=(const ACIndex&)=delete;

  class DataReadHandle;
  DataReadHandle GetData() const {
    return DataReadHandle(std::shared_ptr<const Storage>(std::atomic_load_explicit(&current_version_,
                                                                                   std::memory_order_acquire)));
  }

  std::shared_ptr<const ACClasses> GetCurrentACClasses() {
    return std::atomic_load_explicit(&current_classes_version_, std::memory_order_acquire);
  }

  class AddBatch;
  AddBatch NewBatch() {
    return AddBatch(&pairs_to_add_);
  }

 private:
  struct Storage {
    crag::multithreading::DefaultSemaphoreType *copies_count_;

    using ItemType = std::pair<ACPair, ACClasses::ClassId>;
    std::vector<ItemType> index_;

    static bool KeyLess(const ItemType &lhs, const ItemType &rhs) {
      return lhs.first < rhs.first;
    }

    static bool KeyEq(const ItemType &lhs, const ItemType &rhs) {
      return lhs.first == rhs.first;
    }

    static bool KeyLessVal(const ItemType &lhs, const ACPair &rhs) {
      return lhs.first < rhs;
    }

    Storage(crag::multithreading::DefaultSemaphoreType *copies_count)
        : copies_count_(copies_count) {}

    ~Storage() {
      copies_count_->signal();
    }
  };

  using IndexValues = typename std::remove_reference<decltype(*std::declval<Storage>().index_.begin())>::type;
  using ToMerge = std::pair<ACClasses::ClassId, ACClasses::ClassId>;

  using BatchStorage = std::pair<std::deque<std::tuple<ACPair, bool, ACClasses::ClassId>>, std::vector<ToMerge>>;
  using BatchesQueue = crag::multithreading::SharedQueue<BatchStorage>;
 public:
  class AddBatch {
   public:
    void Execute() {
      if (!to_add_.empty() || !to_merge_.empty()) {
        if (!sorted_and_unique_) {
          std::stable_sort(to_add_.begin(), to_add_.end(),
                           [](const auto& a, const auto& b) { return std::get<ACPair>(a) < std::get<ACPair>(b); });
          to_add_.erase(std::unique(to_add_.begin(), to_add_.end(),
                                    [](const auto& a, const auto& b) {
                                      return std::get<ACPair>(a) == std::get<ACPair>(b);
                                    }), to_add_.end());
        }
        commit_to_->Push(BatchStorage(std::piecewise_construct, std::make_tuple(std::move(to_add_)),
                                      std::make_tuple(std::move(to_merge_))));
      }
      to_add_.clear();
      to_merge_.clear();
    }

    ~AddBatch() {
      Execute();
    }

    AddBatch(BatchesQueue *commit_to)
        : commit_to_(commit_to) {}

    void Push(ACPair p, bool is_aut_normalized, ACClasses::ClassId c) {
      if (!to_add_.empty() && std::get<ACPair>(to_add_.back()) >= p) {
        sorted_and_unique_ = false;
      }

      to_add_.emplace_back(std::move(p), is_aut_normalized, c);
    }

    void Merge(ACClasses::ClassId first, ACClasses::ClassId second) {
      if (first != second) {
        to_merge_.emplace_back(first, second);
      }
    }
   private:
    bool sorted_and_unique_ = true;
    std::deque<std::tuple<ACPair, bool, ACClasses::ClassId>> to_add_;
    std::vector<std::pair<ACClasses::ClassId, ACClasses::ClassId>> to_merge_;
    BatchesQueue *commit_to_;
  };

  class DataReadHandle {
   public:
    auto begin() const {
      return data_->index_.begin();
    }

    auto end() const {
      return data_->index_.end();
    }

    auto find(const ACPair &p) const {
      auto pos = std::lower_bound(begin(), end(), p, Storage::KeyLessVal);
      if (pos == end() || pos->first != p) {
        return end();
      }
      return pos;
    }

    size_t count(const ACPair &p) const {
      return find(p) == end() ? 0u : 1u;
    }

    auto at(const ACPair &p) {
      auto pos = find(p);
      if (pos == end()) {
        throw std::out_of_range("Pair is not in index");
      }
      return pos->second;
    }

    size_t size() const {
      return data_->index_.size();
    }

    void release() {
      data_.reset();
    }

    DataReadHandle(std::shared_ptr<const Storage> data)
        : data_(std::move(data)) {}

   private:
    std::shared_ptr<const Storage> data_;
  };

 private:
  std::shared_ptr<Storage> current_version_; // shared between all threads
  std::shared_ptr<const ACClasses> current_classes_version_; // shared between all threads as well

  BatchesQueue pairs_to_add_;
  crag::multithreading::DefaultSemaphoreType
      versions_count_{2}; // don't allow more than two versions exist to limit memory usage

  std::thread index_updater_;

  ACPairProcessQueue* pairs_queue_; // only used by index updater
};

#endif //ACC_ACINDEX_H
