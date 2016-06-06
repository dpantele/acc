//
// Created by dpantele on 6/3/16.
//

#include "SharedQueue.h"

#include <chrono>
#include <iostream>
#include <future>
#include <vector>

namespace {

using namespace crag::multithreading;

void SimpleSPSCQueueTest(size_t queue_size, int push_count) {
  std::vector<int> pushed_elems;
  SharedQueue<int> q(queue_size);
  auto handle = std::async(std::launch::async, [&] {
    int e;
    while (q.Pop(e)) {
      pushed_elems.push_back(e);
    }
  });

  for (auto i = 0; i < push_count; ++i) {
    q.Push(i);
  }

  q.Close();

  handle.get();

  if (push_count != pushed_elems.size()) {
    throw std::runtime_error("Not all elements popped");
  }
  for (auto i = 0; i < push_count; ++i) {
    if (i != pushed_elems[i]) {
      throw std::runtime_error("Improper element");
    }
  }
}

void SimpleMPMCQueueTest(size_t queue_size, int push_count, float producers_fraction, float consumers_fraction) {
  SharedQueue<int> q(queue_size);

  auto producers_count = static_cast<size_t>(std::thread::hardware_concurrency() * producers_fraction);
  if (producers_count == 0) {
    producers_count = 1;
  }
  auto consumers_count = static_cast<size_t>(std::thread::hardware_concurrency() * consumers_fraction);
  if (consumers_count == 0) {
    consumers_count = 1;
  }

  auto Producer = [&] (int block) {
    for (auto i = block; i < push_count; i += producers_count) {
      q.Push(i);
    }
  };

  auto Consumer = [&] (std::vector<int>* pushed_elems) {
    int e;
    while (q.Pop(e)) {
      pushed_elems->push_back(e);
    }
  };

  std::vector<std::future<void>> producers;
  for (int i = 0; i < producers_count; ++i) {
    producers.push_back(std::async(std::launch::async, Producer, i));
  }

  std::vector<std::pair<std::future<void>, std::vector<int>>> consumers;
  consumers.reserve(consumers_count);
  for (int i = 0; i < consumers_count; ++i) {
    consumers.emplace_back();
    consumers.back().first = std::async(std::launch::async, Consumer, &consumers.back().second);
  }

  for (auto&& p : producers) {
    if (p.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
      throw std::runtime_error("Producer timeout");
    }
  }

  q.Close();

  std::vector<int> pushed_elems;
  for (auto&& c : consumers) {
    if (c.first.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
      throw std::runtime_error("Consumer timeout");
    }
    pushed_elems.insert(pushed_elems.end(), c.second.begin(), c.second.end());
  }

  std::sort(pushed_elems.begin(), pushed_elems.end());

  if (push_count != pushed_elems.size()) {
    throw std::runtime_error("Not all elements popped");
  }
  for (auto i = 0; i < push_count; ++i) {
    if (i != pushed_elems[i]) {
      throw std::runtime_error("Improper element");
    }
  }
}


}

template<typename F, typename ... Args>
void Try(const char* name, F&& f, Args&&... args) {
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
  }
}

int main() {
  Try("SimpleSPSCQueueTest(  64,     50)", SimpleSPSCQueueTest,      64,     50);
  Try("SimpleSPSCQueueTest(  64,    100)", SimpleSPSCQueueTest,      64,    100);
  Try("SimpleSPSCQueueTest(  64,  10000)", SimpleSPSCQueueTest,      64,  10000);
  Try("SimpleSPSCQueueTest(8192, 100000)", SimpleSPSCQueueTest, 1 << 13, 100000);
  Try("SimpleMPMCQueueTest(  64,     50, 0.5, 0.5)", SimpleMPMCQueueTest,      64,     50, 0.5, 0.5);
  Try("SimpleMPMCQueueTest(  64,    100, 0.5, 0.5)", SimpleMPMCQueueTest,      64,    100, 0.5, 0.5);
  Try("SimpleMPMCQueueTest(  64,  10000, 0.5, 0.5)", SimpleMPMCQueueTest,      64,  10000, 0.5, 0.5);
  Try("SimpleMPMCQueueTest(   2,  10000, 0.5, 0.5)", SimpleMPMCQueueTest,       2,  10000, 0.5, 0.5);
  Try("SimpleMPMCQueueTest(8192, 100000, 0.5, 0,5)", SimpleMPMCQueueTest, 1 << 13, 100000, 0.5, 0.5);
  Try("SimpleMPMCQueueTest(  64,     50,   1,   1)", SimpleMPMCQueueTest,      64,     50,   1,   1);
  Try("SimpleMPMCQueueTest(  64,    100,   1,   1)", SimpleMPMCQueueTest,      64,    100,   1,   1);
  Try("SimpleMPMCQueueTest(  64,  10000,   1,   1)", SimpleMPMCQueueTest,      64,  10000,   1,   1);
  Try("SimpleMPMCQueueTest(   2,  10000,   1,   1)", SimpleMPMCQueueTest,       2,  10000,   1,   1);
  Try("SimpleMPMCQueueTest(8192, 100000,   1,   1)", SimpleMPMCQueueTest, 1 << 13, 100000,   1,   1);
  Try("SimpleMPMCQueueTest(  64,     50,   3,   3)", SimpleMPMCQueueTest,      64,     50,   3,   3);
  Try("SimpleMPMCQueueTest(  64,    100,   3,   3)", SimpleMPMCQueueTest,      64,    100,   3,   3);
  Try("SimpleMPMCQueueTest(  64,  10000,   3,   3)", SimpleMPMCQueueTest,      64,  10000,   3,   3);
  Try("SimpleMPMCQueueTest(   2,  10000,   3,   3)", SimpleMPMCQueueTest,       2,  10000,   3,   3);
  Try("SimpleMPMCQueueTest(8192, 100000,   3,   3)", SimpleMPMCQueueTest, 1 << 13, 100000,   3,   3);
  Try("SimpleMPMCQueueTest(  64,     50, 0.5,   3)", SimpleMPMCQueueTest,      64,     50, 0.5,   3);
  Try("SimpleMPMCQueueTest(  64,    100, 0.5,   3)", SimpleMPMCQueueTest,      64,    100, 0.5,   3);
  Try("SimpleMPMCQueueTest(  64,  10000, 0.5,   3)", SimpleMPMCQueueTest,      64,  10000, 0.5,   3);
  Try("SimpleMPMCQueueTest(   2,  10000, 0.5,   3)", SimpleMPMCQueueTest,       2,  10000, 0.5,   3);
  Try("SimpleMPMCQueueTest(8192, 100000, 0.5,   3)", SimpleMPMCQueueTest, 1 << 13, 100000, 0.5,   3);
  Try("SimpleMPMCQueueTest(  64,     50,   3, 0.5)", SimpleMPMCQueueTest,      64,     50,   3, 0.5);
  Try("SimpleMPMCQueueTest(  64,    100,   3, 0.5)", SimpleMPMCQueueTest,      64,    100,   3, 0.5);
  Try("SimpleMPMCQueueTest(  64,  10000,   3, 0.5)", SimpleMPMCQueueTest,      64,  10000,   3, 0.5);
  Try("SimpleMPMCQueueTest(   2,  10000,   3, 0.5)", SimpleMPMCQueueTest,       2,  10000,   3, 0.5);
  Try("SimpleMPMCQueueTest(8192, 100000,   3, 0.5)", SimpleMPMCQueueTest, 1 << 13, 100000,   3, 0.5);
}