//
// Created by dpantele on 6/19/16.
//

#include "ACIndex.h"

ACIndex::ACIndex(const Config& c, std::shared_ptr<ACClasses> initial_classes)
 : current_version_(std::make_shared<Storage>(&versions_count_))
 , current_classes_version_(std::move(initial_classes))
 , pairs_to_add_(c.workers_count_ * 2)
{
  auto initial_semaphore = versions_count_.tryWait();
  assert(initial_semaphore);

  index_updater_ = std::thread([this] {
    while (true) {
      std::deque<BatchStorage::first_type> new_batches;

      BatchStorage next_batch;
      if (!pairs_to_add_.Pop(next_batch)) {
        // queue is closed
        break;
      }

      std::shared_ptr<ACClasses> new_classes;
      bool something_merged = false;

      /* block of functions working with the new version of ACClasses */

      auto MergeMany = [&new_classes, &something_merged, this](const BatchStorage::second_type& ids_to_merge) {
        if (ids_to_merge.empty()) {
          return;
        }
        if (!new_classes) {
          new_classes = this->GetCurrentACClasses()->Clone();
        }
        for (auto&& ids : ids_to_merge) {
          new_classes->Merge(ids.first, ids.second);
        }
        something_merged |= !ids_to_merge.empty();
      };

      auto StoreNewClasses = [&new_classes, &something_merged, this]() {
        if (new_classes && something_merged) {
          new_classes->Normalize();
        }
        if (new_classes) {
          std::atomic_store_explicit(&current_classes_version_, std::shared_ptr<const ACClasses>(std::move(new_classes)), std::memory_order_release);
        }
      };

      /* this is it for acc classes */

      versions_count_.wait();

      do {
        MergeMany(next_batch.second);
        new_batches.push_back(std::move(next_batch.first));
      } while (pairs_to_add_.TryPop(next_batch));

      auto batches_size = std::accumulate(new_batches.begin(), new_batches.end(), 0u,
        [](size_t sum, const BatchStorage::first_type& next) { return sum + next.size(); } );

      if (batches_size == 0) {
        StoreNewClasses();

        // we had a new version reserved, but actually did not
        // create one
        // hence we need to release one version
        versions_count_.signal();
        continue;
      }

      // we may be updating the minimums in some classes, so create a new version
      if (!new_classes) {
        new_classes = GetCurrentACClasses()->Clone();
      }

      // first we sort new batches
      std::vector<IndexValues> new_elements;
      new_elements.reserve(batches_size);
      for (auto&& batch : new_batches) {
        new_elements.insert(new_elements.end(), batch.begin(), batch.end());
      }
      new_batches.clear();

      std::sort(new_elements.begin(), new_elements.end(), Storage::KeyLess);
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
          new_classes->Merge(last_unique->second, current->second);
        } else {
          // add current to the class
          new_classes->AddPair(current->second, current->first);
          ++last_unique;
          *last_unique = *current;
        }
      }

      new_index.erase(std::next(last_unique), new_index.end());

      std::atomic_store_explicit(&current_version_, std::move(new_version), std::memory_order_release);
      StoreNewClasses();
    }
  });
}

ACIndex::~ACIndex() {
  pairs_to_add_.Close();
  index_updater_.join();
}

