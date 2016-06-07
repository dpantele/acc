//
// Created by dpantele on 6/2/16.
//

#ifndef ACC_SHAREDQUEUE_H
#define ACC_SHAREDQUEUE_H

#include <atomic>
#include <cassert>
#include <type_traits>
#include <thread>
#include <mutex>
#include <iostream>

#include "sem.h"

namespace crag { namespace multithreading {

namespace internal {

// copied from http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
template<typename T>
class mpmc_bounded_queue
{
 public:
  mpmc_bounded_queue(size_t buffer_size)
      : buffer_(new cell_t [buffer_size])
      , buffer_mask_(buffer_size - 1)
  {
    assert((buffer_size >= 2) &&
        ((buffer_size & (buffer_size - 1)) == 0));
    for (size_t i = 0; i != buffer_size; i += 1)
      buffer_[i].sequence_.store(i, std::memory_order_relaxed);
    enqueue_pos_.store(0, std::memory_order_relaxed);
    dequeue_pos_.store(0, std::memory_order_relaxed);
  }

  mpmc_bounded_queue(mpmc_bounded_queue const&) = delete;
  void operator = (mpmc_bounded_queue const&) = delete;

  mpmc_bounded_queue(mpmc_bounded_queue&& other)
      : buffer_(other.buffer_)
      , buffer_mask_(other.buffer_mask_)
      , enqueue_pos_(other.enqueue_pos_.load(std::memory_order_relaxed))
      , dequeue_pos_(other.dequeue_pos_.load(std::memory_order_relaxed))
  {
    other.buffer_ = nullptr;
  }

  mpmc_bounded_queue& operator=(mpmc_bounded_queue&& other) = delete;

  ~mpmc_bounded_queue()
  {
    delete [] buffer_;
  }

  bool enqueue(T&& data)
  {
    cell_t* cell;
    size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
    for (;;)
    {
      cell = &buffer_[pos & buffer_mask_];
      size_t seq =
          cell->sequence_.load(std::memory_order_acquire);
      intptr_t dif = (intptr_t)seq - (intptr_t)pos;
      if (dif == 0)
      {
        if (enqueue_pos_.compare_exchange_weak
            (pos, pos + 1, std::memory_order_relaxed))
          break;
      }
      else if (dif < 0)
        return false;
      else
        pos = enqueue_pos_.load(std::memory_order_relaxed);
    }
    cell->data_ = std::move(data);
    cell->sequence_.store(pos + 1, std::memory_order_release);
    return true;
  }

  bool dequeue(T& data)
  {
    cell_t* cell;
    size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
    for (;;)
    {
      cell = &buffer_[pos & buffer_mask_];
      size_t seq =
          cell->sequence_.load(std::memory_order_acquire);
      intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
      if (dif == 0)
      {
        if (dequeue_pos_.compare_exchange_weak
            (pos, pos + 1, std::memory_order_relaxed))
          break;
      }
      else if (dif < 0)
        return false;
      else
        pos = dequeue_pos_.load(std::memory_order_relaxed);
    }
    data = std::move(cell->data_);
    cell->sequence_.store
        (pos + buffer_mask_ + 1, std::memory_order_release);
    return true;
  }

  size_t approximate_size() const {
    auto size = enqueue_pos_.load(std::memory_order_relaxed) - dequeue_pos_.load(std::memory_order_relaxed);
    // note that 1u - ~0u is 2, so overflows are handled well

    //we can't write size < 0 since size is unsigned
    //but actually size can't ever be too big

    if (size > ~0u / 2) {
      //size < 0
      //someone dequeued an element in between two loads
      //which means that size is nearly 0
      return 0;
    }

    // must go after previous if
    if (size > buffer_mask_ + 1) {
      return buffer_mask_ + 1;
    }

    return size;
  }

  size_t max_size() const {
    return buffer_mask_ + 1;
  }

  size_t enqueue_pos() const {
    return enqueue_pos_.load(std::memory_order_relaxed);
  }

  size_t dequeue_pos() const {
    return dequeue_pos_.load(std::memory_order_relaxed);
  }
 private:
  struct cell_t
  {
    std::atomic<size_t>   sequence_;
    T                     data_;
  };

  static size_t const     cacheline_size = 64;
  typedef char            cacheline_pad_t [cacheline_size];

  cacheline_pad_t         pad0_;
  cell_t*                 buffer_;
  size_t const            buffer_mask_;
  cacheline_pad_t         pad1_;
  std::atomic<size_t>     enqueue_pos_;
  cacheline_pad_t         pad2_;
  std::atomic<size_t>     dequeue_pos_;
  cacheline_pad_t         pad3_;
};
}


//! Blocking fixed-size MPMC queue
template<typename T>
class SharedQueue
{
 public:
  SharedQueue(size_t size)
    : size_(size)
    , queue_(size)
    , space_available_(static_cast<int>(size))
    , elements_available_(0)
  { }

  SharedQueue(SharedQueue&& other)
    : size_(other.size_)
    , queue_(std::move(other.queue_))
    , push_state_(other.push_state_.load(std::memory_order_relaxed))
    , space_available_(other.space_available_.approximateCount())
    , elements_available_(other.elements_available_.approximateCount())
  {
    assert(space_available_.approximateCount() >= 0);
    assert(elements_available_.approximateCount() >= 0);
  }

  //! Blocks if no space is available
  void Push(T val) {
    if (TryPush(val)) {
      return;
    }

    if ((push_state_.fetch_add(kPushCountIncrement, std::memory_order_relaxed) & kClosedMask) != 0u) {
      throw std::runtime_error("Push after close");
    }

    space_available_.wait();

    DoPush(val);

    auto push_state = push_state_.fetch_sub(kPushCountIncrement, std::memory_order_relaxed);

    if ((push_state & (kPushCountMask | kClosedMask)) == 1u) {
      elements_available_.signalAndWakeAll(1);
    }
  }

  bool TryPush(T& val) {
    if (!space_available_.tryWait()) {
      return false;
    }

    DoPush(val);

    return true;
  }

  //! Returns false if queue is closed
  bool Pop(T& val) {
    if (!elements_available_.tryWait()) {
      auto push_state = push_state_.load(std::memory_order_relaxed);
      if ((push_state & kClosedMask) != 1u) {
        elements_available_.wait();
      }
    }
    for(auto i = 0u;; ++i) {
      // dequeue returns false if the first element is ready
      // that means that queue is empty (but then elements_available_ should not have fired
      // except when queue is closed) or that second element became ready before the first
      // so we will just wait until the first element becomes ready
      if (queue_.dequeue(val)) {
        DecElemCount();
        space_available_.signal();
        return true;
      } else if ((push_state_.load(std::memory_order_relaxed) & (kClosedMask | kPushCountMask)) == 1u) {
        // if all elements were pushed already, then we may return right now
        // try to awake someone else
        // since queue is empty and hence a nothing-to-wait now
        elements_available_.signal(1);
        return false;
      }
      if (i > 10000u) {
        std::this_thread::yield();
      }
    }
  }

  size_t ApproximateCount() const {
    auto count = elements_available_.approximateCount();
    if (count > 0) {
      return static_cast<size_t>(count);
    }
    return 0u;
  }

  //! Signal that there will be no more extra pushes
  /**
   * Push after Close is undefined behaviour
   */
  void Close() {
    auto push_state = push_state_.fetch_or(1u, std::memory_order_relaxed);

    if ((push_state & (kClosedMask | kPushCountMask)) == 0u) {
      elements_available_.signalAndWakeAll(1);
    }
  }

  size_t MaxCount() const {
    return max_count_.load(std::memory_order_relaxed);
  }

  size_t MaxCountIsSizeTimes() const {
    return count_times_is_max_.load(std::memory_order_relaxed);
  }

 private:
  size_t size_;
  internal::mpmc_bounded_queue<T> queue_;

  static constexpr uint64_t kClosedMask = 1u;
  static constexpr uint64_t kPushCountIncrement = (1ull << 1);
  static constexpr uint64_t kPushCountMask = (~0u << 1);

  // the lower bit is an indicator if Close() was called
  // and the rest is the number of Pushes active
  std::atomic<uint64_t> push_state_{0};

  std::atomic_size_t max_count_{0};
  std::atomic_size_t count_{0};
  std::atomic_size_t count_times_is_max_{0};

//  void UpdateMax(size_t new_count) {
//    size_t last_max = max_count_.load(std::memory_order_relaxed);
//    while (last_max < new_count && max_count_.compare_exchange_weak(last_max, new_count, std::memory_order_relaxed));
//    if (new_count >= size_) {
//      count_times_is_max_.fetch_add(1, std::memory_order_relaxed);
//    }
//  }
//
//  void IncElemCount() {
//    auto val = count_.fetch_add(1, std::memory_order_relaxed);
//    UpdateMax(val + 1);
//  }
//  void DecElemCount() {
//    count_.fetch_sub(1, std::memory_order_relaxed);
//  }

  void IncElemCount() {
  }
  void DecElemCount() {
  }

  DefaultSemaphoreType space_available_;
  DefaultSemaphoreType elements_available_;

  void DoPush(T& val) {
    for(auto i = 0u;; ++i) {
      // enqueue returns false if current position to insert is not ready
      // that means that queue is full (but then space_available_ should not have fired)
      // or that someone have popped an element not from the end
      // the last element is being popped in some thread which was paused
      // so we will just wait until that thread wakes up
      if (queue_.enqueue(std::move(val))) {
        IncElemCount();
        elements_available_.signal();
        break;
      } else {
        // just wait
      }
      if (i > 10000u) {
        std::this_thread::yield();
      }
    }
  }
};

}} //crag::multithreading

#endif //ACC_SHAREDQUEUE_H
