//
// Created by dpantele on 6/8/16.
//

#ifndef ACC_STOPWATCH_H
#define ACC_STOPWATCH_H

#include <chrono>

class ClickTimer {
 public:
  using Clock = typename std::conditional<
      std::chrono::high_resolution_clock::is_steady,
            std::chrono::high_resolution_clock,
            std::chrono::steady_clock>::type;
  using Units = Clock::period;
  using Duration = Clock::duration;

  //! Click and return time since the last click
  Duration Click() {
    auto now = Clock::now();
    auto since_last = now - last_click_;
    last_click_ = now;
    ++clicks_;
    if (clicks_ % 2 == 0) {
      total_running_ += since_last;
    }
    return since_last;
  }

  Duration Total() const {
    return total_running_;
  }

  std::chrono::duration<double, Units>
  Average() const {
    return clicks_ == 0 ? std::chrono::duration<double, Units>{} : std::chrono::duration<double, Units>(total_running_) / clicks_;
  };
 private:
  Clock::time_point last_click_{};
  unsigned clicks_{};
  Duration total_running_{};
};

#endif //ACC_STOPWATCH_H
