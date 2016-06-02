//
// Created by dpantele on 5/30/16.
//

#include <algorithm>
#include <future>
#include <iostream>
#include <vector>


#include "boost_filtering_stream.h"
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/device/file.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include "external_sort.h"

struct A {
  A(A&&) {
    std::clog << "moved" << std::endl;
  }
  A()=default;
  A(const A&)=delete;
};

struct B {
  B(B&&) {
    std::clog << "Bmoved" << std::endl;
  }
  B()=default;
  B(const B&) {
    std::clog << "Bcopied" << std::endl;
  }
};


//! Helper to ensure that no more than N tasks are running
class LimitedAsyncExecutor {
 public:
  LimitedAsyncExecutor(size_t limit)
      : kLimit_(limit)
  {
    std::atomic_init(&running_count_, 0);
  }

  template< class Function, class... Args>
  std::future<typename std::result_of<Function(Args...)>::type>
  async(Function&& f, Args&&... args) {
    while (running_count_.fetch_add(1) >= kLimit_) {
      if (running_count_.fetch_sub(1) > kLimit_) {
        // this lock can't be used to synchronise access to running_count_
        // since we want to allow a no-lock path
        std::unique_lock<std::mutex> lk(thread_available_lock_);
        thread_available_cv_.wait(lk);
      }
    }

    return std::async(std::launch::async, &InvokeAndReport<Function, Args...>, this, std::forward<Function>(f), std::forward<Args>(args)...);
  }

 private:
  template< class Function, class... Args>
  static typename std::result_of<Function(Args...)>::type
  InvokeAndReport(LimitedAsyncExecutor* e, Function&& f, Args&&... args) {
    auto result = f(std::forward<Args>(args)...);
    e->running_count_.fetch_sub(1);
    e->thread_available_cv_.notify_one();
    return result;
  }

  const size_t kLimit_;
  std::atomic<int> running_count_;
  std::condition_variable thread_available_cv_;
  std::mutex thread_available_lock_;

};

void ExternalSort(const fs::path& input_path, const fs::path& output_path, const fs::path& temp_dir, const size_t kMemoryLimit) {
  bool compress = input_path.extension() == ".gz";

  auto input_size = fs::file_size(input_path);

  if (input_size > kMemoryLimit * kMemoryLimit) {
    // 2-stage external sort is not possible, so we just throw
    std::runtime_error(fmt::format("Input is too big ({} bytes), it is not possible to do a two-stage external sort "
                                       "with {} bytes RAM limit", input_size, kMemoryLimit));
  }

  fs::ifstream input_file(input_path, compress ? (std::ios::in | std::ios::binary) : std::ios::in);
  BoostFilteringIStream input;
  if (compress) {
    input.push(boost::iostreams::gzip_decompressor());
  }
  input.push(input_file);

  //we split the file to chunks of size from 1Mb up to kMemoryLimit
  auto threads_count = std::thread::hardware_concurrency();
  if (threads_count == 0) {
    threads_count = 1;
  }

  auto chunk_size = kMemoryLimit / threads_count;

  size_t kMinChunkSize = (1u << 20);
  if (kMinChunkSize * threads_count >= input_size) {
    // in this case we use kMinChunkSize for a chunk size
    chunk_size = kMinChunkSize;
  }

  std::atomic<bool> write_failed;
  std::atomic_init(&write_failed, false);
  std::remove_reference<decltype(errno)>::type write_error{};
  std::vector<std::future<fs::path>> chunks_sorts_results;
  LimitedAsyncExecutor executor(threads_count);

  while (!write_failed.load()) {
    std::string next_line;
    std::vector<std::string> chunk_lines;
    size_t current_chunk_size = 0u;
    while (current_chunk_size < chunk_size && std::getline(input, next_line)) {
      current_chunk_size += next_line.size() + 1;
      chunk_lines.push_back(std::move(next_line));
    }

    if (current_chunk_size == 0) {
      break;
    }

    //and then spawn an async task
    chunks_sorts_results.push_back(executor.async(
        [compress, &temp_dir, &write_failed, &write_error] (std::vector<std::string> lines) {
      std::sort(lines.begin(), lines.end());

      auto chunk_path = temp_dir / "sort";
      chunk_path += fs::unique_path();
      fs::ofstream chunk_file(chunk_path, compress ? (std::ios::out | std::ios::binary) : std::ios::out);

      BoostFilteringOStream out_;
      if (compress) {
        out_.push(boost::iostreams::gzip_compressor());
      }

      out_.push(chunk_file);
      for(auto && line : lines) {
        out_.write(line.c_str(), line.size());
        out_.put('\n');
        if (out_.fail()) {
          auto error = errno;
          if (write_failed.exchange(true) == false) {
            write_error = error;
          }
          return chunk_path;
        }
      }
      return chunk_path;
    }, std::move(chunk_lines)));
    if (!chunk_lines.empty()) {
      throw std::runtime_error("Was not moved");
    }
  }

  //then we resolve all futures
  std::vector<fs::path> chunks_paths;
  chunks_paths.reserve(chunks_sorts_results.size());
  for (auto&& fut : chunks_sorts_results) {
    chunks_paths.push_back(fut.get());
  }

  struct RemoveTempChunks {
    std::vector<fs::path>& paths_;

    RemoveTempChunks(std::vector<fs::path>& paths)
        : paths_(paths)
    { }

    ~RemoveTempChunks() {
      for (auto&& path : paths_) {
        boost::system::error_code e;
        fs::remove(path, e); //errors are ignored
      }
    }
  } remove_temp_chunks(chunks_paths);

  if (write_failed.load()) {
    throw fmt::SystemError(write_error, "Can't write to a temporary chunk file");
  }

  fs::ofstream output_file(output_path, compress ? (std::ios::out | std::ios::binary) : std::ios::out);
  BoostFilteringOStream output;

  if (compress) {
    output.push(boost::iostreams::gzip_compressor(9));
  }

  output.push(output_file);

  //now we open all files and do the merge
  std::deque<fs::ifstream> chunks_files;
  std::vector<BoostFilteringIStream> chunks;
  chunks.reserve(chunks_paths.size());
  for (auto&& chunk_path : chunks_paths) {
    chunks_files.emplace_back(chunk_path, compress ? (std::ios::in | std::ios::binary) : std::ios::in);
    chunks.emplace_back();
    if (compress) {
      chunks.back().push(boost::iostreams::gzip_decompressor());
    }
    chunks.back().push(chunks_files.back());
  }

  std::vector<std::pair<std::string, size_t>> top_lines;
  top_lines.reserve(chunks_paths.size());
  for (auto i = 0u; i < chunks.size(); ++i) {
    top_lines.emplace_back(std::string(), i);
    std::getline(chunks[i], top_lines.back().first);
    if (chunks[i].fail()) {
      top_lines.pop_back();
    }
  }
  std::make_heap(top_lines.begin(), top_lines.end());
  while (!top_lines.empty()) {
    std::pop_heap(top_lines.begin(), top_lines.end());
    output.write(top_lines.back().first.c_str(), top_lines.back().first.size());
    output.put('\n');
    if (output.fail()) {
      throw fmt::SystemError(errno, "Can't record the result of sort to {}", output_path);
    }

    std::getline(chunks[top_lines.back().second], top_lines.back().first);
    if (chunks[top_lines.back().second].fail()) {
      chunks[top_lines.back().second].reset();
      top_lines.pop_back();
    } else {
      std::push_heap(top_lines.begin(), top_lines.end());
    }
  }
}

