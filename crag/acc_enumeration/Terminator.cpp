//
// Created by dpantele on 5/28/16.
//

#include "Terminator.h"
#include <atomic>

#include <signal.h>
#include <unistd.h>
#include <fmt/format.h>
#include <iostream>


// linux-specific version
std::thread SpawnThread(Terminator* terminator) {

  /* Block SIGINT (for release builds) and SIGTERM; other threads created by main()
     will inherit a copy of the signal mask. */

  sigset_t set;
  sigemptyset(&set);
#ifdef NDEBUG
  sigaddset(&set, SIGINT);
#endif
  sigaddset(&set, SIGTERM);
  int s = pthread_sigmask(SIG_BLOCK, &set, NULL);
  if (s != 0) {
    throw fmt::SystemError(s, "Can't block SIGINT/SIGTERM");
  }

  return std::thread([terminator] {
    sigset_t listen_to;

    sigemptyset(&listen_to);
#ifdef NDEBUG
    sigaddset(&listen_to, SIGINT);
#endif
    sigaddset(&listen_to, SIGTERM);

    sigset_t block_all;
    sigfillset(&block_all);
    int all_err = pthread_sigmask(SIG_BLOCK, &block_all, NULL);
    if (all_err != 0) {
      throw fmt::SystemError(all_err, "Can't block all signals");
    }

    int sig;
    auto wait_err = sigwait(&listen_to, &sig);
    if (wait_err != 0) {
      throw fmt::SystemError(wait_err, "Sigwait failed");
    }
    assert(sig == SIGINT || sig == SIGTERM);
    terminator->ScheduleTermination();
  });

}

Terminator::Terminator() {
  static std::atomic_flag created = ATOMIC_FLAG_INIT;
  if (created.test_and_set()) {
    throw std::runtime_error("Terminator may be crated exactly once.");
  }

  std::atomic_init(&this->should_terminate_, false);
  signal_handler_ = SpawnThread(this);
}
void Terminator::ScheduleTermination() {
  if (should_terminate_.exchange(true) == false) {
    sigset_t unblock_all;
    sigfillset(&unblock_all);
    int all_err = pthread_sigmask(SIG_UNBLOCK, &unblock_all, NULL);
    if (all_err != 0) {
      throw fmt::SystemError(all_err, "Can't unblock all signals");
    }

    std::clog << "Terminating (second signal will terminate processes immediately)" << std::endl;
    for (auto&& callback : this->callbacks_) {
      callback();
    }
  }
}
void Terminator::AddCallback(std::function<void()> callback) {
  callbacks_.push_back(std::move(callback));
}

Terminator::~Terminator() {
  if (should_terminate_.exchange(true) == false) {
    //send the SIGTERM to the thread
    pthread_kill(this->signal_handler_.native_handle(), SIGTERM);
  }

  //unblock SIGTERM & SIGINT for the main thread
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);
  int s = pthread_sigmask(SIG_UNBLOCK, &set, NULL);
  if (s != 0) {
    throw fmt::SystemError(s, "Can't unblock SIGINT/SIGTERM");
  }

  this->signal_handler_.join();
}







