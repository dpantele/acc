//
// Created by dpantele on 2/15/16.
//

#include <compressed_word/enumerate_words.h>
#include <compressed_word/tuple_normal_form.h>
#include <fstream>
#include <compressed_word/longest_common_subword_cyclic.h>

#include <folded_graph/complete.h>
#include <folded_graph/folded_graph.h>
#include <folded_graph/harvest.h>

using namespace crag;

std::pair<int, int> lettersPower(CWord w) {
  auto x_power = 0;
  auto y_power = 0;
  while (!w.Empty()) {
    switch (w.GetFront().AsChar()) {
      case 'x':
        x_power += 1;
        break;
      case 'X':
        x_power -= 1;
        break;
      case 'y':
        y_power += 1;
        break;
      case 'Y':
        y_power -= 1;
        break;
      default:
        assert(false);
    }
    w.PopFront();
  }

  return std::make_pair(x_power, y_power);
}

bool hasTrivialAbelianisation(const CWord& u, const CWord& v) {
  auto u_powers = lettersPower(u);
  auto v_powers = lettersPower(v);
  return std::abs(u_powers.first * v_powers.second - v_powers.first * u_powers.second) == 1;
}

bool ConjugationLengthReduction(const CWord& u, const CWord& v) {
  static auto i = 0u;
  FoldedGraph g;
  g.PushCycle(u, 1);
  CompleteWith(v, &g);
  CompleteWith(v, &g);
  auto result = Harvest(u.size(), 1, &g);
  if (result.empty()) {
    return false;
  }

  for (auto&& u_p : result) {
    if (u_p.size() > u.size()) {
      break;
    }
    auto normal = ConjugationInverseNormalForm(u_p);
    if (normal < u) {
      return true;
    }
  }

  return false;
}

bool HasRootCycle(const CWord& u, const FoldedGraph& g) {
  auto u_path = g.ReadWord(u, g.root());
  if (!u_path.unread_word_part().Empty()) {
    return false;
  }
  return u_path.terminus() == g.root();
}

enum IsTrivialResult {
  kTrivial,
  kNonTrivial,
  kUnknown
};

const char* IsTrivialToString(IsTrivialResult v) {
  switch (v) {
    case kTrivial:
      return "Trivial";
    case kNonTrivial:
      return "NonTrivial";
    case kUnknown:
      return "Unknown";
  }
  assert(false);
}

bool isTrivial(const CWord& u, const CWord& v) {
  FoldedGraph g;
  bool is_complete = false;
  for (auto i = 0u; i < 14 && !is_complete; ++i) {
    auto size = g.size();
    CompleteWith(u, size, &g);
    CompleteWith(v, size, &g);
    if (size == g.size()) {
      is_complete = true;
    }
    if (i >= 6) {
      std::cerr << "Takes too long for " << u << " " << v << "(" << i << ", " << size << "->" << g.size() << ")" << std::endl;
    }
  }
  return HasRootCycle(CWord("x"), g) && HasRootCycle(CWord("y"), g);
}

bool AllowAutoReduction(const CWord& u, const CWord& v) {
  auto tuple = CWordTuple<2>{u, v};
  if (WhiteheadReduce(tuple)) {
    return true;
  }
  for (auto&& image : ShortestAutomorphicImages(tuple)) {
    auto min = ConjugationInverseNormalForm(image);
    if (min < tuple || min.GetReversed() < tuple) {
      return true;
    }
  }
  return false;
}

void PrintAutoNormalForm(const CWord& u, const CWord& v) {
  auto tuple = CWordTuple<2>{u, v};
  auto min = tuple;
  for (auto&& image : ShortestAutomorphicImages(tuple)) {
    auto image_min = ConjugationInverseNormalForm(image);
    if (Length(image_min) < Length(min) || image_min < min) {
      min = image_min;
    }
    image_min.Reverse();

    if (image_min < min) {
      min = image_min;
    }
  }
  std::cout << min[0] << " " << min[1] << std::endl;
}

int main(int argc, char* argv[]) {
  //we will consider words of type x..., y...
  auto total_length = 14u;

  std::ofstream trivial_abel("trivial_abel.txt");
  std::ofstream no_simple_reduction("no_simple_reduction.txt");
  std::ofstream no_conjugation_reduction("no_conjugation_reduction.txt");
  std::ofstream trivial("trivial.txt");

  auto count = 0u;
  auto count_nontrivial = 0u;

  for(auto u : EnumerateWords(2, total_length - 1)) {
    if (u.GetFront() == u.GetBack().Inverse()) {
      continue;
    }
    if (ConjugationInverseNormalForm(u) != u) {
      continue;
    }
    for (auto v : EnumerateWords(total_length - u.size())) {
      if (v.GetFront() == v.GetBack().Inverse()) {
        continue;
      }
      if (ConjugationInverseNormalForm(v) != v) {
        continue;
      }
      if (!hasTrivialAbelianisation(u, v)) {
        continue;
      }
      if (v < u) {
        continue;
      }

      ++count;

      trivial_abel << count << " " << u << " " << v << "\n";

      CWord::size_type common_part_length, common_u_begin, common_v_begin;
      std::tie(common_u_begin, common_v_begin, common_part_length) = LongestCommonSubwordCyclic(u, v);
      if (common_part_length > v.size() / 2 || common_part_length > u.size() / 2) {
        continue;
      }

      no_simple_reduction << count << " " << u << " " << v << "\n";

      if (ConjugationLengthReduction(u, v) || ConjugationLengthReduction(v, u)) {
        continue;
      }

      no_conjugation_reduction << count << " " << u << " " << v << "\n";

      if (!isTrivial(u, v)) {
        std::cout << count_nontrivial << std::endl;
        std::cout << u << ' ' << v << std::endl;
        std::cout << AllowAutoReduction(u, v) << " ";
        PrintAutoNormalForm(u, v);
        continue;
      }

      trivial << count << " " << u << " " << v << "\n";
    }
  }

}