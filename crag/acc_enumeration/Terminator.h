//
// Created by dpantele on 5/28/16.
//

#ifndef ACC_TERMINATOR_H
#define ACC_TERMINATOR_H

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

//! Some handle over SIGTERM/SIGINT
/**
 * Spawns a separate thread which sets up a signal handler and a self-pipe
 * Must be called in the first thread in the beginning
 */
class Terminator
{
 public:
  //thread-safe, always false then true after SIGTERM
  bool ShouldTerminate() const {
    return should_terminate_.load();
  }

  void AddCallback(std::function<void()> callback);

  //! Sets the terminating state and executes all callbacks
  void ScheduleTermination();

  //! Since it must be unique for the whole program,
  //! constructor will throw if caled twice
  Terminator();

  //! Non-movable, non-copyable
  Terminator(const Terminator&)=delete;

  //! Ensures that the sigwait is triggered and joins the signal thread
  ~Terminator();

 private:
  std::thread signal_handler_;
  std::vector<std::function<void()>> callbacks_;
  std::atomic_bool should_terminate_ = ATOMIC_FLAG_INIT;
};


#endif //ACC_TERMINATOR_H
