#include <deque>

#include "disjoint_subsets.h"
#include "gtest/gtest.h"

namespace crag {
namespace {

TEST(DisjointSubset, SingleElem) {
  DisjointSubset<int> a(3);

  EXPECT_EQ(1, a.size());
  EXPECT_EQ(3, a.root());
}

TEST(DisjointSubset, Null) {
  DisjointSubset<int> a(nullptr);

  EXPECT_EQ(0, a.size());
  EXPECT_EQ(int(), a.root());
}

struct A {
  A() = delete;

  int a_;

  A(int a)
    : a_(a)
  { }
};

TEST(DisjointSubset, NoConsturct) {
  DisjointSubset<A> a(1);

  EXPECT_EQ(1, a.size());
  EXPECT_EQ(1, a.root().a_);
}

struct NonCopyable {
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable(NonCopyable&&) = delete;

  int a_;

  NonCopyable(int a)
    : a_(a)
  { }
};

TEST(DisjointSubset, PlacementConstruct) {
  DisjointSubset<NonCopyable> a(1);

  EXPECT_EQ(1, a.size());
  EXPECT_EQ(1, a.root().a_);
}


TEST(DisjointSubset, Merge) {
  DisjointSubset<int> a(1);
  DisjointSubset<int> b(2);

  EXPECT_EQ(1, Merge(&a, &b));

  EXPECT_EQ(2, a.size());
  EXPECT_EQ(1, a.root());
  EXPECT_EQ(2, b.size());
  EXPECT_EQ(1, b.root());
}

TEST(DisjointSubset, ConstLabelMerge) {
  DisjointSubset<const int> a(1);
  DisjointSubset<const int> b(2);

  EXPECT_EQ(1, Merge(&a, &b));

  EXPECT_EQ(2, a.size());
  EXPECT_EQ(1, a.root());
  EXPECT_EQ(2, b.size());
  EXPECT_EQ(1, b.root());
}

TEST(DisjointSubset, ConstSubsetRoot) {
  DisjointSubset<int> a(1);
  DisjointSubset<int> b(2);

  EXPECT_EQ(1, Merge(&a, &b));

  EXPECT_EQ(2, a.size());
  EXPECT_EQ(1, a.root());
  EXPECT_EQ(2, b.size());
  EXPECT_EQ(1, b.root());

  const DisjointSubset<int>& c = a;

  EXPECT_EQ(2, c.size());
  EXPECT_EQ(1, c.root());

  //should not compile
  //c.Merge(&a);
}


TEST(DisjointSubset, MergeLinear) {
  std::deque<DisjointSubset<int>> elems;

  for (auto i = 0u; i < 16; ++i) {
    elems.emplace_back(i);
  }

  for (auto i = 0u; i < 16; ++i) {
    EXPECT_EQ(i == 0 ? 0 : 1, Merge(&elems[0], &elems[i]));
    EXPECT_EQ(i + 1, elems[i].size());
    EXPECT_EQ(0, elems[i].root());
    EXPECT_EQ(i + 1, elems[0].size());
    EXPECT_EQ(0, elems[0].root());
  }
}

TEST(DisjointSubset, MergeLinearFromSmall) {
  std::deque<DisjointSubset<int>> elems;

  for (auto i = 0u; i < 16; ++i) {
    elems.emplace_back(i);
  }

  for (auto i = 1u; i < 16; ++i) {
    EXPECT_EQ(1, elems[i].MergeWith(&elems[0]).first);
    EXPECT_EQ(i + 1, elems[i].size());
    EXPECT_EQ(1, elems[i].root());
    EXPECT_EQ(i + 1, elems[0].size());
    EXPECT_EQ(1, elems[0].root());
  }
}


TEST(DisjointSubset, MergeLinearNeighbours) {
  std::deque<DisjointSubset<int>> elems;

  for (auto i = 0u; i < 16; ++i) {
    elems.emplace_back(i);
  }

  for (auto i = 0u; i < 15; ++i) {
    EXPECT_EQ(0, elems[i].MergeWith(&elems[i + 1]).first);
    EXPECT_EQ(i + 2, elems[i].size());
    EXPECT_EQ(0, elems[i].root());
    EXPECT_EQ(i + 2, elems[0].size());
    EXPECT_EQ(0, elems[0].root());
  }
}

TEST(DisjointSubset, MergeBinary) {
  std::deque<DisjointSubset<int>> elems;

  for (auto i = 0u; i < 16; ++i) {
    elems.emplace_back(i);
  }

  for (auto i = 1u; i <= 4; ++i) {
    for (auto j = 0u; j < 16u; j += (1u << i)) {
      EXPECT_EQ(j, elems[j].MergeWith(&elems[j + (1u << (i - 1))]).first);
      EXPECT_EQ((1u << i), elems[j].size());
    }
  }
}

} //namespace
} //namespace crag
