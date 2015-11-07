#include <array>

#include "gtest/gtest.h"
#include "xy_letter.h"

namespace crag {
namespace {

////CLion fix
//#ifndef TEST
//#define TEST(TEST_CASE_NAME, TEST_NAME) void TEST_CASE_NAME##__##TEST_NAME()
//#define EXPECT_EQ(LHS, RHS) if (LHS != RHS) throw 0;
//#endif

TEST(XYLetter, CharShortConvert) {
  EXPECT_EQ('x', XYLetter('x').AsChar());
  EXPECT_EQ('y', XYLetter('y').AsChar());
  EXPECT_EQ('X', XYLetter('X').AsChar());
  EXPECT_EQ('Y', XYLetter('Y').AsChar());
}

TEST(XYLetter, ShortToCharConvert) {
  EXPECT_EQ('x', XYLetter(0).AsChar());
  EXPECT_EQ('y', XYLetter(2).AsChar());
  EXPECT_EQ('X', XYLetter(1).AsChar());
  EXPECT_EQ('Y', XYLetter(3).AsChar());
}

TEST(XYLetter, ArrayInit) {
  std::array<XYLetter, 4> word{0, 3, 2, 1};
  EXPECT_EQ(XYLetter(0), word[0]);
  EXPECT_EQ(XYLetter(3), word[1]);
  EXPECT_EQ(XYLetter(2), word[2]);
  EXPECT_EQ(XYLetter(1), word[3]);
}

TEST(XYLetter, Invert) {
  EXPECT_EQ(XYLetter('x'), XYLetter('X').Inverse());
  EXPECT_EQ(XYLetter('X'), XYLetter('x').Inverse());
  EXPECT_EQ(XYLetter('y'), XYLetter('Y').Inverse());
  EXPECT_EQ(XYLetter('Y'), XYLetter('y').Inverse());
}


} //anonymous
} //crag