//
// Created by dpantele on 11/8/15.
//

#include <gtest/gtest.h>
#include <chrono>

#include "harvest.h"
#include "internal/cycles_examples.h"
#include "internal/naive_folded_graph.h"

namespace crag { namespace {

using Word = FoldedGraph::Word;
using Label = FoldedGraph::Label;
using Weight = FoldedGraph::Weight;

TEST(NaiveGraphFolding, Harvest1) {
  naive::NaiveFoldedGraph2 g;
  g.PushCycle(g.root(), Word("xxyx"), 0);
  g.PushCycle(g.root(), Word("xyyX"), 0);
  g.Fold();

  auto v = g.edges_.lower_bound(naive::NaiveFoldedGraph2::Edge::min_edge(g.root(), Label('X')))->first.to();

  auto words = g.Harvest(10, g.root(), v);

  std::vector<Word> correct = {
      { 1 },
      { 0, 0, 2 },
      { 0, 2, 2, 0, 2 },
      { 0, 2, 2, 1, 1 },
      { 0, 3, 3, 0, 2 },
      { 0, 3, 3, 1, 1 },
      { 1, 3, 1, 1, 1 },
      { 0, 0, 2, 0, 0, 0, 2 },
      { 0, 2, 2, 2, 2, 0, 2 },
      { 0, 2, 2, 2, 2, 1, 1 },
      { 0, 3, 3, 3, 3, 0, 2 },
      { 0, 3, 3, 3, 3, 1, 1 },
      { 1, 3, 1, 2, 2, 0, 2 },
      { 1, 3, 1, 2, 2, 1, 1 },
      { 1, 3, 1, 3, 3, 0, 2 },
      { 1, 3, 1, 3, 3, 1, 1 },
      { 0, 0, 2, 0, 0, 2, 2, 0, 2 },
      { 0, 0, 2, 0, 0, 2, 2, 1, 1 },
      { 0, 0, 2, 0, 0, 3, 3, 0, 2 },
      { 0, 0, 2, 0, 0, 3, 3, 1, 1 },
      { 0, 2, 2, 0, 2, 0, 0, 0, 2 },
      { 0, 2, 2, 1, 1, 3, 1, 1, 1 },
      { 0, 2, 2, 2, 2, 2, 2, 0, 2 },
      { 0, 2, 2, 2, 2, 2, 2, 1, 1 },
      { 0, 3, 3, 0, 2, 0, 0, 0, 2 },
      { 0, 3, 3, 1, 1, 3, 1, 1, 1 },
      { 0, 3, 3, 3, 3, 3, 3, 0, 2 },
      { 0, 3, 3, 3, 3, 3, 3, 1, 1 },
      { 1, 3, 1, 1, 1, 3, 1, 1, 1 },
      { 1, 3, 1, 2, 2, 2, 2, 0, 2 },
      { 1, 3, 1, 2, 2, 2, 2, 1, 1 },
      { 1, 3, 1, 3, 3, 3, 3, 0, 2 },
      { 1, 3, 1, 3, 3, 3, 3, 1, 1 }
  };

  EXPECT_EQ(correct, words);
}

TEST(NaiveGraphFolding, HarvestWeight4) {
  Word first("xxY");
  Word second("y");

  naive::NaiveFoldedGraph2 g;
  g.PushCycle(g.root(), first, 1);
  g.PushCycle(g.root(), second, 0);
  g.Fold();

  EXPECT_EQ(0, g.modulus_);

  Weight first_weight = 0;
  EXPECT_EQ(g.root(), g.ReadWord(g.root(), &first, &first_weight));
  EXPECT_EQ(1, first_weight);
  EXPECT_EQ(Word(), first);

  Weight second_weight = 0;
  EXPECT_EQ(g.root(), g.ReadWord(g.root(), &second, &first_weight));
  EXPECT_EQ(0, second_weight);
  EXPECT_EQ(Word(), second);

  EXPECT_EQ(
      std::vector<Word>(
      {
        Word("xx"),
        Word("xxy"),
        Word("xxY"),
        Word("yxx"),
        Word("Yxx"),
        Word("xxyy"),
        Word("xxYY"),
        Word("yxxy"),
        Word("yyxx"),
        Word("YxxY"),
        Word("YYxx"),
      })
      , g.Harvest(4, g.root(), g.root(), 1)
  );

}

TEST(FoldedGraphHarvest, Example1) {
  FoldedGraph g;
  g.PushCycle(Word("xxyx"));
  g.PushCycle(Word("xyyX"));

  auto words = Harvest(g, 10, g.root(), g.root().edge(Label('X')).terminus(), 0);

  std::vector<Word> correct = {
      { 1 },
      { 0, 0, 2 },
      { 0, 2, 2, 0, 2 },
      { 0, 2, 2, 1, 1 },
      { 0, 3, 3, 0, 2 },
      { 0, 3, 3, 1, 1 },
      { 1, 3, 1, 1, 1 },
      { 0, 0, 2, 0, 0, 0, 2 },
      { 0, 2, 2, 2, 2, 0, 2 },
      { 0, 2, 2, 2, 2, 1, 1 },
      { 0, 3, 3, 3, 3, 0, 2 },
      { 0, 3, 3, 3, 3, 1, 1 },
      { 1, 3, 1, 2, 2, 0, 2 },
      { 1, 3, 1, 2, 2, 1, 1 },
      { 1, 3, 1, 3, 3, 0, 2 },
      { 1, 3, 1, 3, 3, 1, 1 },
      { 0, 0, 2, 0, 0, 2, 2, 0, 2 },
      { 0, 0, 2, 0, 0, 2, 2, 1, 1 },
      { 0, 0, 2, 0, 0, 3, 3, 0, 2 },
      { 0, 0, 2, 0, 0, 3, 3, 1, 1 },
      { 0, 2, 2, 0, 2, 0, 0, 0, 2 },
      { 0, 2, 2, 1, 1, 3, 1, 1, 1 },
      { 0, 2, 2, 2, 2, 2, 2, 0, 2 },
      { 0, 2, 2, 2, 2, 2, 2, 1, 1 },
      { 0, 3, 3, 0, 2, 0, 0, 0, 2 },
      { 0, 3, 3, 1, 1, 3, 1, 1, 1 },
      { 0, 3, 3, 3, 3, 3, 3, 0, 2 },
      { 0, 3, 3, 3, 3, 3, 3, 1, 1 },
      { 1, 3, 1, 1, 1, 3, 1, 1, 1 },
      { 1, 3, 1, 2, 2, 2, 2, 0, 2 },
      { 1, 3, 1, 2, 2, 2, 2, 1, 1 },
      { 1, 3, 1, 3, 3, 3, 3, 0, 2 },
      { 1, 3, 1, 3, 3, 3, 3, 1, 1 }
  };

  EXPECT_EQ(correct, words);
}

class FoldedGraphHarvestExamples : public ::testing::TestWithParam<PushReadCyclesParam> { };

FoldedGraph GetFolded(const std::vector<Cycle>& cycles) {
  FoldedGraph g;
  for (auto&& cycle : cycles) {
    g.PushCycle(cycle.word(), &g.root(), cycle.weight());
  }

  return g;
}

naive::NaiveFoldedGraph2 GetNaiveFolded(const std::vector<Cycle>& cycles) {
  naive::NaiveFoldedGraph2 g;
  for(auto&& cycle : cycles) {
    Word w(cycle.word());
    g.PushCycle(g.root(), &w, cycle.weight());
  }

  g.Fold();

  return g;
}

TEST_P(FoldedGraphHarvestExamples, CompareRootHarvest) {
  auto g_folded = GetFolded(GetParam().first);
  auto g_naive = GetNaiveFolded(GetParam().first);

  auto max_length = 0u;
  for (auto& cycle : GetParam().first) {
    max_length = std::max(max_length, static_cast<decltype(max_length)>(cycle.word().size()));
  }

  auto harvest_length = max_length + 2;

  if (harvest_length > 16) {
    harvest_length = 16;
  }

  auto folded_harvest = Harvest(g_folded, max_length, g_folded.root(), g_folded.root(), 1);
  auto naive_harvest = g_naive.Harvest(max_length, g_naive.root(), g_naive.root(), 1);

  EXPECT_EQ(naive_harvest, folded_harvest);
}

INSTANTIATE_TEST_CASE_P(FoldedGraphHarvest, FoldedGraphHarvestExamples, ::testing::ValuesIn(push_read_cycles_params));

TEST(FoldedGraphHarvest, StressRootHarvestCompareWithNaive) {
  static const auto kDuration = std::chrono::seconds(10);
  static const unsigned int kWords = 2;
  std::mt19937_64 engine(17);
  RandomWord rw(2, 4);
  std::discrete_distribution<Weight> random_weight({0.6, 0.4, 0.1});

  auto begin = std::chrono::steady_clock::now();

  std::chrono::high_resolution_clock::duration harvest_folded_duration{}, harvest_naive_duration{};
  std::chrono::high_resolution_clock::time_point proc_begin;
  auto repeat = 0ull;
  while (std::chrono::steady_clock::now() - begin < kDuration) {
    ++repeat;
#ifdef DEBUG_PRINT
    std::cout << "== " << i << " ==========================" << std::endl;
#endif
    std::vector<std::pair<Word, Weight>> words;
    FoldedGraph g;
    naive::NaiveFoldedGraph2 g_naive;
    auto max_length = 0u;
    for (auto j = 0u; j < kWords; ++j) {
      words.emplace_back(rw(engine), random_weight(engine));
#ifdef DEBUG_PRINT
      std::cout << ::testing::PrintToString(words.back()) << "\n";
#endif
      proc_begin = std::chrono::high_resolution_clock::now();
      g.PushCycle(words.back().first, &g.root(), words.back().second);
      harvest_folded_duration += (std::chrono::high_resolution_clock::now() - proc_begin);

      proc_begin = std::chrono::high_resolution_clock::now();
      g_naive.PushCycle(g_naive.root(), words.back().first, words.back().second);
      harvest_naive_duration += (std::chrono::high_resolution_clock::now() - proc_begin);
      if (words.back().first.size() > max_length) {
        max_length = words.back().first.size();
      }
    }

    proc_begin = std::chrono::high_resolution_clock::now();
    g_naive.Fold();
    harvest_naive_duration += (std::chrono::high_resolution_clock::now() - proc_begin);

    proc_begin = std::chrono::high_resolution_clock::now();
    auto harvest_folded = Harvest(g, max_length + 2, g.root(), g.root(), 1);
    harvest_folded_duration += (std::chrono::high_resolution_clock::now() - proc_begin);

    proc_begin = std::chrono::high_resolution_clock::now();
    auto harvest_naive = g_naive.Harvest(max_length + 2, g_naive.root(), g_naive.root(), 1);
    harvest_naive_duration += (std::chrono::high_resolution_clock::now() - proc_begin);

    ASSERT_EQ(harvest_naive, harvest_folded) << "Pushed " << ::testing::PrintToString(words);
  }
  std::cout << std::string(13, ' ') << repeat << " repeats" << std::endl;
  std::cout << std::string(13, ' ')
      << std::chrono::duration_cast<std::chrono::milliseconds>(harvest_folded_duration).count()
      << " vs "
      << std::chrono::duration_cast<std::chrono::milliseconds>(harvest_naive_duration).count()
      << std::endl;
  ASSERT_GT(repeat, 10000);
}

Word MinCycle(Word word) {
  assert(word.Empty() || word.GetFront() != word.GetBack().Inverse());
  auto initial = word;
  auto candidate = word;
  word.CyclicLeftShift();
  while (word != initial) {
    if (word < candidate) {
      candidate = word;
    }
    word.CyclicLeftShift();
  }
  return candidate;
}

std::vector<Word> MinCycle(std::vector<Word> words) {
  for (auto& word : words) {
    auto word_min = MinCycle(word);
    auto inverse_min = MinCycle(word.Inverse());
    word = word_min < inverse_min ? word_min : inverse_min;
  }
  std::sort(words.begin(), words.end());
  auto end = std::unique(words.begin(), words.end());
  words.erase(end, words.end());
  return words;
}

TEST(FoldedGraphHarvest, StressFullHarvestCompareWithNaive) {
  static const auto kDuration = std::chrono::seconds(10);
  static const auto kMinRepeat = 1000u;
  static const unsigned int kWords = 4;
  std::mt19937_64 engine(17);
  RandomWord rw(2, 6);
  std::discrete_distribution<Weight> random_weight({0.6, 0.4, 0.1});

  auto begin = std::chrono::steady_clock::now();

  std::chrono::high_resolution_clock::duration harvest_folded_duration{}, harvest_naive_duration{};
  std::chrono::high_resolution_clock::time_point proc_begin;
  auto repeat = 0ull;
  auto non_trivial = 0ull;
  while (std::chrono::steady_clock::now() - begin < kDuration || repeat < kMinRepeat) {
    ++repeat;
#ifdef DEBUG_PRINT
    std::cout << "== " << i << " ==========================" << std::endl;
#endif
    std::vector<std::pair<Word, Weight>> words;
    FoldedGraph g;
    naive::NaiveFoldedGraph2 g_naive;
    auto max_length = 0u;
    for (auto j = 0u; j < kWords; ++j) {
      words.emplace_back(rw(engine), random_weight(engine));
#ifdef DEBUG_PRINT
      std::cout << ::testing::PrintToString(words.back()) << "\n";
#endif
      proc_begin = std::chrono::high_resolution_clock::now();
      g.PushCycle(words.back().first, &g.root(), words.back().second);
      harvest_folded_duration += (std::chrono::high_resolution_clock::now() - proc_begin);

      proc_begin = std::chrono::high_resolution_clock::now();
      g_naive.PushCycle(g_naive.root(), words.back().first, words.back().second);
      harvest_naive_duration += (std::chrono::high_resolution_clock::now() - proc_begin);
      if (words.back().first.size() > max_length) {
        max_length = words.back().first.size();
      }
    }

    if (g.modulus().modulus() != 1) {
      ++non_trivial;
    }

    proc_begin = std::chrono::high_resolution_clock::now();
    g_naive.Fold();
    harvest_naive_duration += (std::chrono::high_resolution_clock::now() - proc_begin);

    proc_begin = std::chrono::high_resolution_clock::now();
    auto harvest_folded = Harvest(max_length + 2, 1, &g);
    harvest_folded_duration += (std::chrono::high_resolution_clock::now() - proc_begin);

    proc_begin = std::chrono::high_resolution_clock::now();
    auto harvest_naive = g_naive.Harvest(max_length + 2, 1);
    harvest_naive_duration += (std::chrono::high_resolution_clock::now() - proc_begin);

    ASSERT_EQ(MinCycle(std::move(harvest_naive)), MinCycle(std::move(harvest_folded)))
    << "Pushed " << ::testing::PrintToString(words);

  }
  std::cout << std::string(13, ' ') << repeat << " repeats" << std::endl;
  std::cout << std::string(13, ' ') << non_trivial << " times modulus != 1" << std::endl;
  std::cout << std::string(13, ' ')
      << std::chrono::duration_cast<std::chrono::milliseconds>(harvest_folded_duration).count()
      << " vs "
      << std::chrono::duration_cast<std::chrono::milliseconds>(harvest_naive_duration).count()
      << std::endl;
}



} }