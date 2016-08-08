//
// Created by dpantele on 6/19/16.
//

#include "ACIndex.h"


ACIndex::ACIndex(const Config& c)
 : current_version_(std::make_shared<Storage>(&versions_count_))
 , pairs_to_add_(c.workers_count_ * 2)
{
  auto initial_semaphore = versions_count_.tryWait();
  assert(initial_semaphore);

  index_updater_ = std::thread([this] {
    while (true) {
      std::deque<BatchStorage> new_batches(1);
      if (!pairs_to_add_.Pop(new_batches.front())) {
        // queue is closed
        break;
      }

      versions_count_.wait();

      do {
        new_batches.emplace_back();
      } while (pairs_to_add_.TryPop(new_batches.back()));

      assert(new_batches.back().empty());
      new_batches.pop_back();

      auto batches_size = std::accumulate(new_batches.begin(), new_batches.end(), 0u,
        [](size_t sum, const BatchStorage& next) { return sum + next.size(); } );

      if (batches_size == 0) {
        continue;
      }

      // first we merge-sort new batches
      std::vector<IndexValues> new_elements;
      new_elements.reserve(batches_size);
      for (auto&& batch : new_batches) {
        auto merged_end = new_elements.end();
        new_elements.insert(merged_end, batch.begin(), batch.end());
        std::inplace_merge(new_elements.begin(), merged_end, new_elements.end(), Storage::KeyLess);
      }

      new_batches.clear();

      auto new_version = std::make_shared<Storage>(&versions_count_);

      auto& new_index = new_version->index_;
      new_index.reserve(current_version_->index_.size() + batches_size);
      std::merge(current_version_->index_.begin(), current_version_->index_.end(),
          new_elements.begin(), new_elements.end(), std::back_inserter(new_index), Storage::KeyLess);

      auto last_unique = new_index.begin();

      assert(!new_index.empty());

      for(auto current = std::next(new_index.begin()); current != new_index.end(); ++current) {
        if (current->first == last_unique->first) {
          // 'remove' current and merge classes
          last_unique->second->Merge(current->second);
        } else {
          ++last_unique;
          *last_unique = *current;
        }
      }

      ++last_unique;
      new_index.erase(last_unique);

      std::atomic_store_explicit(&current_version_, std::move(new_version), std::memory_order_release);
    }
  });
}

ACIndex::~ACIndex() {
  pairs_to_add_.Close();
  index_updater_.join();
}

