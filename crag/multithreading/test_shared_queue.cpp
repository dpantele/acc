//
// Created by dpantele on 6/3/16.
//

#include "SharedQueue.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <future>
#include <vector>

namespace {

using namespace crag::multithreading;

void SimpleSPSCQueueTest(size_t queue_size, unsigned push_count) {
  std::vector<size_t> pushed_elems;
  SharedQueue<size_t> q(queue_size);
  auto handle = std::async(std::launch::async, [&] {
    size_t e;
    while (q.Pop(e)) {
      pushed_elems.push_back(e);
    }
  });

  for (auto i = 0u; i < push_count; ++i) {
    q.Push(i);
  }

  q.Close();

  handle.get();

  if (push_count != pushed_elems.size()) {
    throw std::runtime_error("Not all elements popped");
  }
  for (auto i = 0u; i < push_count; ++i) {
    if (i != pushed_elems[i]) {
      throw std::runtime_error("Improper element");
    }
  }
}

void SimpleMPMCQueueTest(size_t queue_size, unsigned push_count, float producers_fraction, float consumers_fraction) {
  SharedQueue<size_t> q(queue_size);

  auto producers_count = static_cast<size_t>(std::thread::hardware_concurrency() * producers_fraction);
  if (producers_count == 0) {
    producers_count = 1;
  }
  auto consumers_count = static_cast<size_t>(std::thread::hardware_concurrency() * consumers_fraction);
  if (consumers_count == 0) {
    consumers_count = 1;
  }

  auto Producer = [&] (size_t block) {
    auto start = std::chrono::high_resolution_clock::now();
    for (auto i = block; i < push_count; i += producers_count) {
      q.Push(i);
    }
    return std::chrono::high_resolution_clock::now() - start;
  };

  auto Consumer = [&] (std::vector<size_t>* pushed_elems) {
    size_t e;
    auto start = std::chrono::high_resolution_clock::now();
    while (q.Pop(e)) {
      pushed_elems->push_back(e);
    }
    return std::chrono::high_resolution_clock::now() - start;
  };

  std::vector<std::future<std::chrono::high_resolution_clock::duration>> producers;
  for (auto i = 0u; i < producers_count; ++i) {
    producers.push_back(std::async(std::launch::async, Producer, i));
  }

  std::vector<std::pair<std::future<std::chrono::high_resolution_clock::duration>, std::vector<size_t>>> consumers;
  consumers.reserve(consumers_count);
  for (auto i = 0u; i < consumers_count; ++i) {
    consumers.emplace_back();
    consumers.back().first = std::async(std::launch::async, Consumer, &consumers.back().second);
  }

  bool producer_timeout = false;

  for (auto&& p : producers) {
    if (p.get() > std::chrono::milliseconds(100)) {
      producer_timeout = true;
    }
  }

  q.Close();

  bool consumer_timeout = false;

  std::vector<size_t> pushed_elems;
  for (auto&& c : consumers) {
    if (c.first.get() > std::chrono::milliseconds(100)) {
      consumer_timeout = true;
    }
    pushed_elems.insert(pushed_elems.end(), c.second.begin(), c.second.end());
  }

  if (producer_timeout) {
    throw std::runtime_error("Producer timeout");
  }

  if (consumer_timeout) {
    throw std::runtime_error("Producer timeout");
  }


  std::sort(pushed_elems.begin(), pushed_elems.end());

  if (push_count != pushed_elems.size()) {
    throw std::runtime_error("Not all elements popped");
  }
  for (auto i = 0u; i < push_count; ++i) {
    if (i != pushed_elems[i]) {
      throw std::runtime_error("Improper element");
    }
  }
}


}

template<typename F, typename ... Args>
bool Try(const char* name, F&& f, Args&&... args) {
  try {
    std::cout << name << "... " << std::flush;

    auto binded = std::bind(f, std::forward<Args>(args)...);

    auto run = [&binded] {
      auto start = std::chrono::high_resolution_clock::now();
      binded();
      auto stop = std::chrono::high_resolution_clock::now();
      return stop - start;
    };

    auto best_result = run();
    auto start = std::chrono::high_resolution_clock::now();
    auto i = 0u;
    for(; std::chrono::high_resolution_clock::now() - start < std::chrono::seconds(2); ++i) {
      auto current_result = run();
      if (current_result < best_result) {
        best_result = current_result;
      }
    }

    std::cout << "Ok " << i << " times, min=" << std::chrono::duration_cast<std::chrono::microseconds>(best_result).count() << "us" << std::endl;
  } catch(std::runtime_error& e) {
    std::cout << "Fail: " << e.what() << std::endl;
    return false;
  }
  return true;
}

int main() {
  bool success = true;
  success &= Try("SimpleSPSCQueueTest(  64,     50)", SimpleSPSCQueueTest,      64,     50);
  success &= Try("SimpleSPSCQueueTest(  64,    100)", SimpleSPSCQueueTest,      64,    100);
  success &= Try("SimpleSPSCQueueTest(  64,  10000)", SimpleSPSCQueueTest,      64,  10000);
  success &= Try("SimpleSPSCQueueTest(8192, 100000)", SimpleSPSCQueueTest, 1 << 13, 100000);
  success &= Try("SimpleMPMCQueueTest(  64,     50, 0.5, 0.5)", SimpleMPMCQueueTest,      64,     50, 0.5, 0.5);
  success &= Try("SimpleMPMCQueueTest(  64,    100, 0.5, 0.5)", SimpleMPMCQueueTest,      64,    100, 0.5, 0.5);
  success &= Try("SimpleMPMCQueueTest(  64,  10000, 0.5, 0.5)", SimpleMPMCQueueTest,      64,  10000, 0.5, 0.5);
  success &= Try("SimpleMPMCQueueTest(   2,  10000, 0.5, 0.5)", SimpleMPMCQueueTest,       2,  10000, 0.5, 0.5);
  success &= Try("SimpleMPMCQueueTest(8192, 100000, 0.5, 0,5)", SimpleMPMCQueueTest, 1 << 13, 100000, 0.5, 0.5);
  success &= Try("SimpleMPMCQueueTest(  64,     50,   1,   1)", SimpleMPMCQueueTest,      64,     50,   1,   1);
  success &= Try("SimpleMPMCQueueTest(  64,    100,   1,   1)", SimpleMPMCQueueTest,      64,    100,   1,   1);
  success &= Try("SimpleMPMCQueueTest(  64,  10000,   1,   1)", SimpleMPMCQueueTest,      64,  10000,   1,   1);
  success &= Try("SimpleMPMCQueueTest(   2,  10000,   1,   1)", SimpleMPMCQueueTest,       2,  10000,   1,   1);
  success &= Try("SimpleMPMCQueueTest(8192, 100000,   1,   1)", SimpleMPMCQueueTest, 1 << 13, 100000,   1,   1);
  success &= Try("SimpleMPMCQueueTest(  64,     50,   3,   3)", SimpleMPMCQueueTest,      64,     50,   3,   3);
  success &= Try("SimpleMPMCQueueTest(  64,    100,   3,   3)", SimpleMPMCQueueTest,      64,    100,   3,   3);
  success &= Try("SimpleMPMCQueueTest(  64,  10000,   3,   3)", SimpleMPMCQueueTest,      64,  10000,   3,   3);
  success &= Try("SimpleMPMCQueueTest(   2,  10000,   3,   3)", SimpleMPMCQueueTest,       2,  10000,   3,   3);
  success &= Try("SimpleMPMCQueueTest(8192, 100000,   3,   3)", SimpleMPMCQueueTest, 1 << 13, 100000,   3,   3);
  success &= Try("SimpleMPMCQueueTest(  64,     50, 0.5,   3)", SimpleMPMCQueueTest,      64,     50, 0.5,   3);
  success &= Try("SimpleMPMCQueueTest(  64,    100, 0.5,   3)", SimpleMPMCQueueTest,      64,    100, 0.5,   3);
  success &= Try("SimpleMPMCQueueTest(  64,  10000, 0.5,   3)", SimpleMPMCQueueTest,      64,  10000, 0.5,   3);
  success &= Try("SimpleMPMCQueueTest(   2,  10000, 0.5,   3)", SimpleMPMCQueueTest,       2,  10000, 0.5,   3);
  success &= Try("SimpleMPMCQueueTest(8192, 100000, 0.5,   3)", SimpleMPMCQueueTest, 1 << 13, 100000, 0.5,   3);
  success &= Try("SimpleMPMCQueueTest(  64,     50,   3, 0.5)", SimpleMPMCQueueTest,      64,     50,   3, 0.5);
  success &= Try("SimpleMPMCQueueTest(  64,    100,   3, 0.5)", SimpleMPMCQueueTest,      64,    100,   3, 0.5);
  success &= Try("SimpleMPMCQueueTest(  64,  10000,   3, 0.5)", SimpleMPMCQueueTest,      64,  10000,   3, 0.5);
  success &= Try("SimpleMPMCQueueTest(   2,  10000,   3, 0.5)", SimpleMPMCQueueTest,       2,  10000,   3, 0.5);
  success &= Try("SimpleMPMCQueueTest(8192, 100000,   3, 0.5)", SimpleMPMCQueueTest, 1 << 13, 100000,   3, 0.5);

  if (!success) {
    return 1;
  }
}