//
// Created by dpantele on 6/19/16.
//

#include "ACIndex.h"

ACIndex::ACIndex(const Config& c, std::shared_ptr<ACClasses> initial_classes, ACPairProcessQueue* pairs_queue)
 : current_version_(std::make_shared<Storage>(&versions_count_))
 , current_classes_version_(std::move(initial_classes))
 , pairs_to_add_(c.workers_count_ * 2)
 , pairs_queue_(pairs_queue)
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
      std::vector<std::tuple<ACPair, bool, ACClasses::ClassId>> new_elements;
      new_elements.reserve(batches_size);
      for (auto&& batch : new_batches) {
        new_elements.insert(new_elements.end(), batch.begin(), batch.end());
      }
      new_batches.clear();

      std::sort(new_elements.begin(), new_elements.end(),
                [](const auto& a, const auto& b) { return std::get<ACPair>(a) < std::get<ACPair>(b); });
      auto new_version = std::make_shared<Storage>(&versions_count_);

      auto& new_index = new_version->index_;
      new_index.reserve(current_version_->index_.size() + batches_size);

      auto& current_index = current_version_->index_;
      auto current_new = new_elements.begin();

      auto current_exists = current_index.begin();
      while (current_exists != current_index.end()) {
        if (current_new == new_elements.end()) {
          std::copy(current_exists, current_index.end(), std::back_inserter(new_index));
          break;
        }
        if (std::get<ACPair>(*current_new) < std::get<ACPair>(*current_exists)) {
          // should insert current_new into the index
          // but we also need to check if it is different from the previous element
          if (!current_index.empty()
              && std::get<ACPair>(*current_new) == std::get<ACPair>(current_index.back())) {
            // we got a duplicate
            new_classes->Merge(std::get<ACClasses::ClassId>(*current_new),
                               std::get<ACClasses::ClassId>(current_index.back()));
          } else {
            // new unique pair
            new_index.emplace_back(std::get<ACPair>(*current_new), std::get<ACClasses::ClassId>(*current_new));
            new_classes->AddPair(std::get<ACClasses::ClassId>(*current_new), std::get<ACPair>(*current_new));
            pairs_queue_->Push(std::get<ACPair>(*current_new), std::get<bool>(*current_new));
          }
        } else {
          // should insert the old element into the index
          // it is always unique
          new_index.emplace_back(std::get<ACPair>(*current_exists), std::get<ACClasses::ClassId>(*current_exists));
        }
      }

      std::atomic_store_explicit(&current_version_, std::move(new_version), std::memory_order_release);
      StoreNewClasses();
    }
  });
}

ACIndex::~ACIndex() {
  pairs_to_add_.Close();
  index_updater_.join();
}

