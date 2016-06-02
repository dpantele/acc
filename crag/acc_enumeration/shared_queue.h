//
// Created by dpantele on 5/4/16.
//

#ifndef ACC_SHARED_QUEUE_H
#define ACC_SHARED_QUEUE_H

namespace crag {

template <typename T>
class SharedQueue
{
  std::queue <T> queue_;
  mutable std::mutex m_;
  std::condition_variable data_cond_;

  SharedQueue& operator=(const SharedQueue&) = delete;
  SharedQueue(const SharedQueue& other) = delete;

 public:
  SharedQueue()
  { }

  void push(T item) {
    std::lock_guard <std::mutex> lock(m_);
    queue_.push(std::move(item));
    data_cond_.notify_one();
  }

  /// \return immediately, with true if successful retrieval
  bool try_and_pop(T& popped_item) {
    std::lock_guard <std::mutex> lock(m_);
    if (queue_.empty()) {
      return false;
    }
    popped_item = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  /// Try to retrieve, if no items, wait till an item is available and try again
  bool wait_and_pop(T& popped_item, std::chrono::milliseconds timeout) {
    std::unique_lock <std::mutex> lock(m_); // note: unique_lock is needed for std::condition_variable::wait
    if (data_cond_.wait_for(lock, timeout, [this]() { return !queue_.empty(); })) {
      popped_item = std::move(queue_.front());
      queue_.pop();
      return true;
    }
    return false;
  }

  bool empty() const {
    std::lock_guard <std::mutex> lock(m_);
    return queue_.empty();
  }

  size_t size() const {
    std::lock_guard <std::mutex> lock(m_);
    return queue_.size();
  }

  void clear() {
    std::lock_guard <std::mutex> lock(m_);
    while (!queue_.empty()) {
      queue_.pop();
    }
  }

};

}

#endif //ACC_SHARED_QUEUE_H
