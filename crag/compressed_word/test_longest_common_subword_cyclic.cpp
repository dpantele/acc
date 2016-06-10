//
// Created by dpantele on 11/7/15.
//

#include <chrono>

#include <gtest/gtest.h>

#include "longest_common_subword_cyclic.h"

namespace crag {
namespace {

namespace naive_find_subword {

std::tuple<CWord::size_type, CWord::size_type, CWord::size_type> LongestCommonSubwordCyclic(CWord u, CWord v) {
  //this is actually not naive, but turns out that in general this is slower than naive approach
  std::vector<CWord::size_type> previous_longest_suffix(v.size() * 2 + 1);
  std::vector<CWord::size_type> current_longest_suffix(v.size() * 2 + 1);

  auto cur_u = u;
  auto cur_v = v;
  CWord::size_type max_common_length = 0;
  CWord::size_type u_common_begin = u.size();
  CWord::size_type v_common_begin = v.size();
  for(CWord::size_type i = 1u; i <= u.size() * 2; ++i) {
    std::swap(previous_longest_suffix, current_longest_suffix);
    for(CWord::size_type j = 1u; j <= v.size() * 2; ++j) {
      if (cur_u.GetFront() == cur_v.GetFront()) {
        current_longest_suffix[j] = previous_longest_suffix[j - 1] + 1;
        if (current_longest_suffix[j] > u.size()) {
          current_longest_suffix[j] = u.size();
        }
        if (current_longest_suffix[j] > v.size()) {
          current_longest_suffix[j] = v.size();
        }
        if (current_longest_suffix[j] >= max_common_length) {
          CWord::size_type cur_u_begin = (i - current_longest_suffix[j]) % u.size();
          CWord::size_type cur_v_begin = (j - current_longest_suffix[j]) % v.size();
          if (current_longest_suffix[j] > max_common_length || cur_u_begin < u_common_begin || (cur_u_begin == u_common_begin && cur_v_begin < v_common_begin)) {
            max_common_length = current_longest_suffix[j];
            u_common_begin = cur_u_begin;
            v_common_begin = cur_v_begin;
          }
        }
      } else {
        current_longest_suffix[j] = 0;
      }
      cur_v.PopFront();
      if (cur_v.Empty()) {
        cur_v = v;
      }
    }
    cur_u.PopFront();
    if (cur_u.Empty()) {
      cur_u = u;
    }
  }

  assert(cur_u == u && cur_v == v);
  if (max_common_length > u.size()) {
    max_common_length = u.size();
  }
  if (max_common_length > v.size()) {
    max_common_length = v.size();
  }

  return std::make_tuple(u_common_begin, v_common_begin, max_common_length);
}

}

TEST(LongestCommonSubwordCyclic, Example1) {
  CWord u("xy");
  CWord v("xY");
  EXPECT_EQ(std::make_tuple(0, 0, 1), LongestCommonSubwordCyclic(u, v));
  EXPECT_EQ(std::make_tuple(0, 0, 1), naive_find_subword::LongestCommonSubwordCyclic(u, v));
}

TEST(LongestCommonSubwordCyclic, Example2) {
  CWord u("xx");
  CWord v("xY");
  EXPECT_EQ(std::make_tuple(0, 0, 1), LongestCommonSubwordCyclic(u, v));
  EXPECT_EQ(std::make_tuple(0, 0, 1), naive_find_subword::LongestCommonSubwordCyclic(u, v));
}

TEST(LongestCommonSubwordCyclic, Example3) {
  CWord u("xYx");
  CWord v("xx");
  EXPECT_EQ(std::make_tuple(2, 0, 2), LongestCommonSubwordCyclic(u, v));
  EXPECT_EQ(std::make_tuple(2, 0, 2), naive_find_subword::LongestCommonSubwordCyclic(u, v));
}

TEST(LongestCommonSubwordCyclic, Example4) {
  CWord u("x");
  CWord v("xx");
  EXPECT_EQ(std::make_tuple(0, 0, 1), LongestCommonSubwordCyclic(u, v));
  EXPECT_EQ(std::make_tuple(0, 0, 1), naive_find_subword::LongestCommonSubwordCyclic(u, v));
}

TEST(LongestCommonSubwordCyclic, Example5) {
  CWord u("YXyyXyx");
  CWord v("yX");
  EXPECT_EQ(std::make_tuple(1, 1, 2), LongestCommonSubwordCyclic(u, v));
  EXPECT_EQ(std::make_tuple(1, 1, 2), naive_find_subword::LongestCommonSubwordCyclic(u, v));
}


TEST(LongestCommonSubwordCyclic, StressTest) {
  static const auto kDuration = std::chrono::seconds(10);
  std::mt19937_64 engine(17);
  RandomWord rw(30, 30);

  auto begin = std::chrono::steady_clock::now();

  std::chrono::high_resolution_clock::duration naive_op_duration{}, op_duration{};
  std::chrono::high_resolution_clock::time_point proc_begin;

  auto repeat = 0ull;
  while (std::chrono::steady_clock::now() - begin < kDuration) {
    ++repeat;
    auto u = rw(engine);
    auto v = rw(engine);

    proc_begin = std::chrono::high_resolution_clock::now();
    auto result = LongestCommonSubwordCyclic(u, v);
    op_duration += (std::chrono::high_resolution_clock::now() - proc_begin);

    proc_begin = std::chrono::high_resolution_clock::now();
    auto naive_result = naive_find_subword::LongestCommonSubwordCyclic(u, v);
    naive_op_duration += (std::chrono::high_resolution_clock::now() - proc_begin);
    ASSERT_EQ(naive_result, result) << u << ' ' << v;
  }
  std::cout << std::string(13, ' ') << repeat << " repeats" << std::endl;
  std::cout << std::string(13, ' ')
      << std::chrono::duration_cast<std::chrono::milliseconds>(op_duration).count()
      << " vs "
      << std::chrono::duration_cast<std::chrono::milliseconds>(naive_op_duration).count()
      << std::endl;
  ASSERT_GT(repeat, 10000u);
}

}
}