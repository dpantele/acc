//
// Created by dpantele on 6/7/16.
//

#ifndef ACC_ACPAIRPROCESSQUEUE_H
#define ACC_ACPAIRPROCESSQUEUE_H

#include <map>
#include <thread>

#include <boost/variant.hpp>
#include <crag/multithreading/SharedQueue.h>

#include "acc_class.h"
#include "state_dump.h"


class ACPairProcessQueue {
 public:
  using Value = std::pair<ACPair, bool>;

  ACPairProcessQueue(const Config& c, ACStateDump* state_dump)
      : output_queue_(c.workers_count_ * 2)
        , input_queue_(8192 /*magic size, TODO: replace with auto-resizing queue */)
        , state_dump_(state_dump)
  {
    queue_processor_ = std::thread([this] {
      InputMessage next;

      while (input_queue_.Pop(next)) {
        if (next.which() == 1) {
          switch (boost::get<Message>(next)) {
            case Message::Popped: {
              while (!to_process.empty() && output_queue_.TryPush(Value(*to_process.begin()))) {
                to_process.erase(to_process.begin());
              }
              break;
            }
            case Message::Terminated: {
              to_process.clear();
              {
                InputMessage m;
                while (input_queue_.Pop(m));
              }
              {
                Value v;
                output_queue_.Close();
                while (output_queue_.Pop(v));
              }
            }
          }
        } else {
          Value next_as_value = boost::get<Value>(next);
          auto was_inserted = to_process.insert(next_as_value);
          if (!was_inserted.second) {
            TaskDone();
            was_inserted.first->second |= next_as_value.second;
          }
          while (!to_process.empty() && output_queue_.TryPush(Value(*to_process.begin()))) {
            to_process.erase(to_process.begin());
          }
        }
      }

      output_queue_.Close();
    });
  }

  ~ACPairProcessQueue() {
    Close();
    queue_processor_.join();
  }

  //! Pops the next task, returns false if queue is closed
  bool Pop(Value& next) {
    auto result = output_queue_.Pop(next);
    if (result) {
      input_queue_.Push(Message::Popped);
      state_dump_->DumpPairQueueState(next.first, ACStateDump::PairQueueState::Popped);
    }
    return result;
  }

  void Push(ACPair pair, bool is_aut_normalized) {
    state_dump_->DumpPairQueueState(pair, is_aut_normalized ? ACStateDump::PairQueueState::AutoNormalized : ACStateDump::PairQueueState::Pushed);
    tasks_to_do_.fetch_add(1, std::memory_order_relaxed);
    input_queue_.Push(Value(pair, is_aut_normalized));
  }

  size_t GetTasksCount() const {
    return tasks_to_do_.load(std::memory_order_relaxed);
  }

  void Close() {
    input_queue_.Close();
  }

  void Terminate() {
    InputMessage m;
    while (input_queue_.TryPop(m));
    input_queue_.Push(Message::Terminated);
    input_queue_.Close();
  }

  void TaskDone() {
    auto result = tasks_to_do_.fetch_sub(1, std::memory_order_relaxed);
    if (result <= 0) {
      throw std::runtime_error("TaskDone() is called without an active task");
    }
    if (result == 1) {
      if (output_queue_.ApproximateCount() != 0) {
        throw std::runtime_error("TaskDone() was not called after some pair was processed");
      }

      // Closing queue, but actually we could be calling some callbacks
      Close();
    }
  }

 private:
  crag::multithreading::SharedQueue<Value> output_queue_;

  enum class Message {
    Popped,
    Terminated,
  };

  using InputMessage = boost::variant<Value, Message>;

  crag::multithreading::SharedQueue<InputMessage> input_queue_; // TODO: replace with auto-resizing mpsc queue
  std::map<ACPair, bool> to_process;
  ACStateDump* state_dump_;
  std::thread queue_processor_;

  // Block so that the queue could be joinable
  std::atomic_int_fast64_t tasks_to_do_{0};

};

#endif //ACC_ACPAIRPROCESSQUEUE_H
