#include "gtest/gtest.h"

#include "endomorphism.h"

#include <chrono>
#include <memory>

namespace crag {

namespace {

TEST(Endomorhism, Test1) {
  thread_local auto x_xy = Endomorphism(CWord("xy"), CWord("y"));

  EXPECT_EQ(CWord("xy"), x_xy.Apply(CWord("x")));
  EXPECT_EQ(CWord("YX"), x_xy.Apply(CWord("X")));
  EXPECT_EQ(CWord("y"), x_xy.Apply(CWord("y")));
  EXPECT_EQ(CWord("Y"), x_xy.Apply(CWord("Y")));
}

} //namespace

} //namespace crag
