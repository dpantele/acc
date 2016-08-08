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

//! Thread-safe version of an index which adds multiple items at once
class ACIndex
{
  struct Storage;
 public:
  ACIndex(const Config& c);
  ~ACIndex();

  class DataReadHandle;
  DataReadHandle GetData() const {
    return DataReadHandle(std::static_pointer_cast<const Storage>(std::atomic_load_explicit(&current_version_, std::memory_order_acquire)));
  }

  class AddBatch;
  AddBatch NewBatch() {
    return AddBatch(&pairs_to_add_);
  }

 private:
  struct Storage {
    crag::multithreading::DefaultSemaphoreType* copies_count_;
    std::vector<std::pair<ACPair, ACClass*>> index_;

    static bool KeyLess(const std::pair<ACPair, ACClass*>& lhs, const std::pair<ACPair, ACClass*>& rhs) {
      return lhs.first < rhs.first;
    }

    static bool KeyEq(const std::pair<ACPair, ACClass*>& lhs, const std::pair<ACPair, ACClass*>& rhs) {
      return lhs.first == rhs.first;
    }

    static bool KeyLessVal(const std::pair<ACPair, ACClass*>& lhs, const ACPair& rhs) {
      return lhs.first < rhs;
    }

    Storage(crag::multithreading::DefaultSemaphoreType* copies_count)
      : copies_count_(copies_count)
    { }

    ~Storage() {
      copies_count_->signal();
    }
  };

  using IndexValues = typename std::remove_reference<decltype(*std::declval<Storage>().index_.begin())>::type;
  using BatchStorage = std::deque<IndexValues>;
  using BatchesQueue = crag::multithreading::SharedQueue<BatchStorage>;
 public:
  class AddBatch {
   public:
    void Execute() {
      if (!to_add_.empty()) {
        if (!sorted_and_unique_) {
          std::stable_sort(to_add_.begin(), to_add_.end(), Storage::KeyLess);
          to_add_.erase(std::unique(to_add_.begin(), to_add_.end(), Storage::KeyEq));
        }
        commit_to_->Push(std::move(to_add_));
      }
      to_add_.clear();
    }

    ~AddBatch() {
      Execute();
    }

    AddBatch(BatchesQueue* commit_to)
        : commit_to_(commit_to)
    { }

    void Push(ACPair p, ACClass* c) {
      if (!to_add_.empty() && to_add_.back().first >= p) {
        sorted_and_unique_ = false;
      }

      to_add_.emplace_back(std::move(p), c);
    }
   private:
    bool sorted_and_unique_ = true;
    std::deque<IndexValues> to_add_;
    BatchesQueue* commit_to_;
  };

  class DataReadHandle {
   public:
    auto begin() const {
      return data_->index_.begin();
    }

    auto end() const {
      return data_->index_.end();
    }

    auto find(const ACPair& p) const {
      auto pos = std::lower_bound(begin(), end(), p, Storage::KeyLessVal);
      if (pos == end() || pos->first != p) {
        return end();
      }
      return pos;
    }

    size_t count(const ACPair& p) const {
      return find(p) == end() ? 0u : 1u;
    }

    auto at(const ACPair& p) {
      auto pos = find(p);
      if (pos == end()) {
        throw std::out_of_range("Pair is not in index");
      }
      return pos->second;
    }

    size_t size() const {
      return data_->index_.size();
    }

    DataReadHandle(std::shared_ptr<const Storage> data)
        : data_(std::move(data))
    { }

   private:
    std::shared_ptr<const Storage> data_;
  };

 private:
  std::shared_ptr<Storage> current_version_; // shared between all threads
  BatchesQueue pairs_to_add_;
  crag::multithreading::DefaultSemaphoreType versions_count_{2}; // don't allow more than two versions exist to limit memory usage

  std::thread index_updater_;
};


#endif //ACC_ACINDEX_H
