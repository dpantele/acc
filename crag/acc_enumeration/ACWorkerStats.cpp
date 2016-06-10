//
// Created by dpantele on 5/25/16.
//


#include "acc_class.h"
#include "ACWorkerStats.h"

#include <fmt/ostream.h>

namespace io = boost::iostreams;

constexpr char MoveStats::timers_order[];
constexpr char MoveStats::stats_order[];

static void InitOstream(const Config& c, std::initializer_list<std::pair<path, BoostFilteringOStream*>> to_init) {
  for (auto&& stream : to_init) {
    *stream.second = c.ofstream(stream.first);
    if (stream.second->fail()) {
      throw fmt::SystemError(errno, "Can\'t open {}", stream.first);
    }
  }
}

ACWorkerStats::ACWorkerStats(const Config& c)
  // this one does not need to be large since it will get just 2 events per job
  : write_tasks_(c.workers_count_ * 4)
{
  fs::create_directories(c.stats_dir());

  InitOstream(c, {
      {c.ac_move_stats(), &move_stats_out_},
  });

  writer_ = std::thread([this] {
    WriteTask task;
    while (write_tasks_.Pop(task)) {
      if (task.out_) {
        task.out_->write(task.data_.c_str(), task.data_.size());
      }
      if (task.tee_to_stdout_) {
        std::cout.write(task.data_.c_str(), task.data_.size());
        std::cout.flush();
      }
    }
  });
}

ACWorkerStats::~ACWorkerStats() {
  write_tasks_.Close();
  writer_.join();
}















