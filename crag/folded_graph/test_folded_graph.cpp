#include <chrono>
#include <iterator>

#include <gtest/gtest.h>
#include "folded_graph.h"

#include "internal/naive_folded_graph.h"
#include "internal/cycles.h"
#include "internal/cycles_examples.h"
#include "internal/folded_graph_internal_checks.h"

namespace crag {

namespace {

using Label = FoldedGraph::Label;
using Word = FoldedGraph::Word;
using Weight = FoldedGraph::Weight;

TEST(FoldedGraph, JustRoot) {
  FoldedGraph g;
  EXPECT_EQ(1, g.size());
  EXPECT_EQ(g.root(), g[0]);
  EXPECT_EQ(g.root(), *g.begin());
  EXPECT_FALSE(g.root().edge(0));
  EXPECT_FALSE(g.root().edge(1));
  EXPECT_FALSE(g.root().edge(2));
  EXPECT_FALSE(g.root().edge(3));
  EXPECT_TRUE(g.root().begin() == g.root().end());
  EXPECT_FALSE(g.root().IsMerged());
}

TEST(FoldedGraph, SingleEdge) {
  FoldedGraph g;

  auto& another_vertex = g.CreateVertex();
  EXPECT_FALSE(another_vertex.IsMerged());

  g.root().AddEdge(Label('x'), &another_vertex, 1);

  EXPECT_FALSE(g.root().begin() == g.root().end());
  EXPECT_FALSE(another_vertex.begin() == another_vertex.end());
  EXPECT_TRUE(g.root().edge(Label('x')));
  EXPECT_TRUE(another_vertex.edge(Label('X')));

  auto x_edge = *g.root().begin();
  EXPECT_EQ(another_vertex, x_edge.terminus());
  EXPECT_EQ(1, x_edge.weight());

  auto X_edge = *another_vertex.begin();
  EXPECT_EQ(g.root(), X_edge.terminus());
  EXPECT_EQ(-1, X_edge.weight());
}

TEST(FoldedGraph, EmptyVertexCombine) {
  FoldedGraph g;
  g.Combine(&g.CreateVertex(), &g.root(), 0);
  EXPECT_TRUE(g.root() == g[0] || g.root() == g[1]);
  EXPECT_FALSE(g.root() == g[0] && g.root() == g[1]);
  EXPECT_TRUE(*g.begin() == g.root());
  EXPECT_TRUE(g.root().begin() == g.root().end());
}

TEST(FoldedGraph, SingleEdgeVertexCombine) {
  FoldedGraph g;
  auto& v1 = g.CreateVertex();
  g.root().AddEdge(Label('y'), &v1, 2);

  g.Combine(&g.root(), &v1, 1);

  EXPECT_TRUE(g.root() == g[0] || g.root() == g[1]);
  EXPECT_FALSE(g.root() == g[0] && g.root() == g[1]);
  EXPECT_TRUE(*g.begin() == g.root());

  auto edge = g.root().begin();
  ASSERT_FALSE(edge++ == g.root().end());
  ASSERT_FALSE(edge++ == g.root().end());
  ASSERT_TRUE(edge == g.root().end());

  auto first_edge = g.root().begin();
  EXPECT_EQ(Label('y'), first_edge->label());
  EXPECT_EQ(g.root(), first_edge->terminus());
  EXPECT_EQ(1, first_edge->weight());
  auto second_edge = std::next(first_edge);
  EXPECT_EQ(Label('Y'), second_edge->label());
  EXPECT_EQ(g.root(), second_edge->terminus());
  EXPECT_EQ(-1, second_edge->weight());
}

TEST(FoldedGraph, PacmanFold) {
  FoldedGraph g;
  auto v1 = &g.CreateVertex();
  auto v2 = &g.CreateVertex();

  v1->AddEdge(Label('x'), v2, 1);
  v2->AddEdge(Label('y'), v1, 0);

  auto v3 = &g.CreateVertex();
  auto v4 = &g.CreateVertex();

  v3->AddEdge(Label('x'), v4, 0);
  v4->AddEdge(Label('Y'), v3, 0);

  g.Combine(v1, v3, 0);

  auto v5 = &v1->Parent();
  EXPECT_TRUE(v5 == v1 || v5 == v3);
  EXPECT_TRUE(v1->Parent() == v3->Parent());

  auto v6 = &v2->Parent();
  EXPECT_TRUE(v6 == v2 || v6 == v4);
  EXPECT_TRUE(v2->Parent() == v4->Parent());

  EXPECT_TRUE(v5->edge(Label('x')));
  EXPECT_FALSE(v5->edge(Label('X')));
  EXPECT_TRUE(v5->edge(Label('y')));
  EXPECT_TRUE(v5->edge(Label('Y')));

  EXPECT_TRUE(v6->edge(Label('X')));
  EXPECT_FALSE(v6->edge(Label('x')));
  EXPECT_TRUE(v6->edge(Label('y')));
  EXPECT_TRUE(v6->edge(Label('Y')));

  EXPECT_EQ(1, v5->edge(Label('x')).weight() + v6->edge(Label('y')).weight());
  EXPECT_EQ(0, v5->edge(Label('x')).weight() + v6->edge(Label('Y')).weight());
}

TEST(FoldedGraph, PacmanFoldModulus) {
  FoldedGraph g;
  auto v1 = &g.CreateVertex();
  auto v2 = &g.CreateVertex();

  v1->AddEdge(Label('x'), v2, 1);
  v2->AddEdge(Label('y'), v1, 0);

  auto v3 = &g.CreateVertex();
  auto v4 = &g.CreateVertex();

  v3->AddEdge(Label('x'), v4, 0);
  v4->AddEdge(Label('y'), v3, 0);

  g.Combine(v1, v3, 0);

  auto v5 = &v1->Parent();
  EXPECT_TRUE(v5 == v1 || v5 == v3);
  EXPECT_TRUE(v1->Parent() == v3->Parent());

  auto v6 = &v2->Parent();
  EXPECT_TRUE(v6 == v2 || v6 == v4);
  EXPECT_TRUE(v2->Parent() == v4->Parent());

  EXPECT_TRUE(v5->edge(Label('x')));
  EXPECT_FALSE(v5->edge(Label('X')));
  EXPECT_FALSE(v5->edge(Label('y')));
  EXPECT_TRUE(v5->edge(Label('Y')));

  EXPECT_TRUE(v6->edge(Label('X')));
  EXPECT_FALSE(v6->edge(Label('x')));
  EXPECT_TRUE(v6->edge(Label('y')));
  EXPECT_FALSE(v6->edge(Label('Y')));


  EXPECT_EQ(1, g.modulus().modulus());

  EXPECT_TRUE(
      v5->edge(Label('x')).weight() + v6->edge(Label('y')).weight() == 0
          || v5->edge(Label('x')).weight() + v6->edge(Label('y')).weight() == 1
  );
}

TEST(FoldedGraph, PacmanCycles) {
  FoldedGraph g;
  g.PushCycle(Word("xy"), 1);

  auto path = g.ReadWord(Word("xy"), g.root());
  EXPECT_EQ(g.root(), path.terminus());
  EXPECT_EQ(1, path.weight());
  EXPECT_EQ(Word(), path.unread_word_part());

  path = g.ReadWord(Word("YX"), g.root());
  EXPECT_EQ(g.root(), path.terminus());
  EXPECT_EQ(-1, path.weight());
  EXPECT_EQ(Word(), path.unread_word_part());

  path = g.ReadWord(Word("xY"), g.root());
  EXPECT_EQ(g[1], path.terminus());
  EXPECT_TRUE(path.weight() == 1 || path.weight() == 0);
  EXPECT_EQ(Word("Y"), path.unread_word_part());

  g.PushCycle(Word("xY"));

  path = g.ReadWord(Word("xy"), g.root());
  EXPECT_EQ(g.root(), path.terminus());
  EXPECT_EQ(1, path.weight());
  EXPECT_EQ(Word(), path.unread_word_part());

  path = g.ReadWord(Word("YX"), g.root());
  EXPECT_EQ(g.root(), path.terminus());
  EXPECT_EQ(-1, path.weight());
  EXPECT_EQ(Word(), path.unread_word_part());

  path = g.ReadWord(Word("xY"), g.root());
  EXPECT_EQ(g.root(), path.terminus());
  EXPECT_EQ(0, path.weight());
  EXPECT_EQ(Word(), path.unread_word_part());

  EXPECT_TRUE(g.modulus().infinite());
}

TEST(FoldedGraph, PacmanCyclesModulus) {
  FoldedGraph g;
  g.PushCycle(Word("xy"), 1);

  auto path = g.ReadWord(Word("xy"), g.root());
  EXPECT_EQ(g.root(), path.terminus());
  EXPECT_EQ(1, path.weight());
  EXPECT_EQ(Word(), path.unread_word_part());

  path = g.ReadWord(Word("YX"), g.root());
  EXPECT_EQ(g.root(), path.terminus());
  EXPECT_EQ(-1, path.weight());
  EXPECT_EQ(Word(), path.unread_word_part());

  path = g.ReadWord(Word("xY"), g.root());
  EXPECT_EQ(g[1], path.terminus());
  EXPECT_TRUE(path.weight() == 1 || path.weight() == 0);
  EXPECT_EQ(Word("Y"), path.unread_word_part());

  g.PushCycle(Word("xy"));

  path = g.ReadWord(Word("xy"), g.root());
  EXPECT_EQ(g.root(), path.terminus());
  EXPECT_EQ(0, path.weight());
  EXPECT_EQ(Word(), path.unread_word_part());

  path = g.ReadWord(Word("YX"), g.root());
  EXPECT_EQ(g.root(), path.terminus());
  EXPECT_EQ(0, path.weight());
  EXPECT_EQ(Word(), path.unread_word_part());

  path = g.ReadWord(Word("xY"), g.root());
  EXPECT_EQ(g[1], path.terminus());
  EXPECT_TRUE(path.weight() == 1 || path.weight() == 0);
  EXPECT_EQ(Word("Y"), path.unread_word_part());

  EXPECT_EQ(1, g.modulus().modulus());
}

TEST(FoldedGraph, PushCycleWithStem) {
  FoldedGraph g;

  Word w("xxyXX");

  g.PushCycle(w, &g.root(), 1);

  EXPECT_EQ(g.root(), g.ReadWord(w, g.root()).terminus());
  EXPECT_EQ(1, g.ReadWord(w, g.root()).weight());
  EXPECT_EQ(g.root(), g.ReadWord(w.Inverse(), g.root()).terminus());
  EXPECT_EQ(-1, g.ReadWord(w.Inverse(), g.root()).weight());
}

TEST(FoldedGraph, PushCycleEmpty) {
  FoldedGraph g;

  g.PushCycle(Word());
  EXPECT_TRUE(g.modulus().infinite());

  g.PushCycle(Word(), 1);
  EXPECT_EQ(1, g.modulus().modulus());
}

class GraphsPushReadCycles: public ::testing::TestWithParam<PushReadCyclesParam>
{
};

FoldedGraph GetFolded(const std::vector<Cycle>& cycles) {
  FoldedGraph g;
  for (auto&& cycle : cycles) {
    g.PushCycle(cycle.word(), &g.root(), cycle.weight());
    auto result = FoldedGraphInternalChecks::Check(g);
    EXPECT_TRUE(result);
  }

  return g;
}

using Path = FoldedGraph::Path;

TEST_P(GraphsPushReadCycles, FoldedGraphPushRead) {
  auto g = GetFolded(GetParam().first);

  if (HasFatalFailure()) {
    return;
  }

  EXPECT_EQ(GetParam().second, g.modulus().modulus());

  for (auto&& cycle : GetParam().first) {
    auto path = g.ReadWord(cycle.word(), g.root());

    EXPECT_EQ(g.root(), path.origin());
    EXPECT_EQ(g.root(), path.terminus());
    EXPECT_EQ(Word(), path.unread_word_part());

    EXPECT_TRUE(g.modulus().AreEqual(cycle.weight(), path.weight()))
              << path.weight() << " vs " << cycle.weight() << " mod " << g.modulus().modulus();
  }
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

TEST_P(GraphsPushReadCycles, NaiveFoldedGraph2PushRead) {
  auto g = GetNaiveFolded(GetParam().first);

  EXPECT_EQ(GetParam().second, g.modulus_);


  for(auto&& cycle : GetParam().first) {
    Weight w = 0;
    Word to_read(cycle.word());
    auto end = g.ReadWord(g.root(), &to_read, &w);

    EXPECT_EQ(g.root(), end);
    EXPECT_EQ(0, to_read.size());
    EXPECT_EQ(0, naive::WeightMod(w - cycle.weight(), g.modulus_));
  }
}

INSTANTIATE_TEST_CASE_P(Examples, GraphsPushReadCycles, ::testing::ValuesIn(push_read_cycles_params));

using Clock = std::chrono::steady_clock;
using std::chrono::seconds;

struct NaiveStressFolding {
  NaiveFoldedGraph2 g;

  static const char* TypeString() {
    return "naive";
  }

  NaiveStressFolding(const std::vector<Cycle>& words) {
    for (auto&& w : words) {
      Word word(w.word());
      g.PushCycle(g.root(), &word, w.weight());
    }
    g.Fold();
  }

  ::testing::AssertionResult ReadWord(const Cycle& c) {
    auto to_read = c.word();
    Weight read_weight = {};

    auto stopped_at = g.ReadWord(g.root(), &to_read, &read_weight);

    if (stopped_at != g.root()) {
      return ::testing::AssertionFailure() << "stopped not on in root: " << stopped_at;
    }

    if (!to_read.Empty()) {
      return ::testing::AssertionFailure() << "haven't read the whole word, " << to_read << " left";
    }

    if (WeightMod(read_weight - c.weight(), g.modulus_) != 0) {
      return ::testing::AssertionFailure() << "weight is different: "
             << read_weight << " vs " << c.weight()
             << " mod " << g.modulus_;
    }

    return ::testing::AssertionSuccess();
  }
};

struct NormalStressFolding {
  FoldedGraph g;

  static const char* TypeString() {
    return "fast";
  }

  NormalStressFolding(const std::vector<Cycle>& words) {
    for (auto&& w : words) {
      g.PushCycle(w.word(), &g.root(), w.weight());

#ifndef NDEBUG
     EXPECT_TRUE(FoldedGraphInternalChecks::Check(g));
#endif
    }
  }

  ::testing::AssertionResult ReadWord(const Cycle& c) {
    auto read_path = g.ReadWord(c.word(), g.root());

    if (read_path.terminus() != g.root()) {
      return ::testing::AssertionFailure() << " stopped not on in root";
    }

    if (!read_path.unread_word_part().Empty()) {
      return ::testing::AssertionFailure() << "haven't read the whole word, " << read_path.unread_word_part() << " left";
    }

    if (!g.modulus().AreEqual(read_path.weight(), c.weight())) {
      return ::testing::AssertionFailure() << "weight is different: "
             << read_path.weight() << " vs " << c.weight() << " mod " << g.modulus().modulus();
    }

    return ::testing::AssertionSuccess();
  }
};

template <typename T>
class GraphFolding : public ::testing::Test { };

typedef ::testing::Types<NaiveStressFolding, NormalStressFolding> GraphImpl;
TYPED_TEST_CASE(GraphFolding, GraphImpl);

//#define DEBUG_PRINT

TYPED_TEST(GraphFolding, StressTest) {
  auto test_duration = std::chrono::seconds(7);

  std::uniform_int_distribution<> words_count(4, 10);
  std::mt19937_64 engine(17);
  RandomWord rw(2, 15);
  std::discrete_distribution<Weight> random_weight({0.6, 0.4, 0.1});

  auto test_start = Clock::now();
  auto current_iteration = 0u;

  while (Clock::now() - test_start < test_duration) {
    ++current_iteration;
#ifdef DEBUG_PRINT
    std::cout << "== " << current_iteration << " ==========================" << std::endl;
#endif
    std::vector<Cycle> words;

    auto kWords = words_count(engine);

    for (auto j = kWords; j != 0; --j) {
      words.emplace_back(rw(engine), random_weight(engine));
#ifdef DEBUG_PRINT
      std::cout << words.back() << "\n";
#endif
    }

    auto test_impl = TypeParam(words);

    if (this->HasFatalFailure()) {
      return;
    }

    for (auto&& w : words) {
      ASSERT_TRUE(test_impl.ReadWord(w)) << "Pushed " << ::testing::PrintToString(words) << ", fail on " << w;
    }
  }

  std::cout << TypeParam::TypeString() << ": "
    << current_iteration << " iterations in " << test_duration.count() << " seconds" << std::endl;
}

}
}