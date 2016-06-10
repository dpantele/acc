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
#include <map>
#include <unordered_set>
#include <chrono>

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

bool HasCycle(const CWord& u, const FoldedGraph& g, const FoldedGraph::Vertex& v) {
  auto u_path = g.ReadWord(u, v);
  if (!u_path.unread_word_part().Empty()) {
    return false;
  }

  return u_path.terminus() == v;
}

bool HasRootCycle(const CWord& u, const FoldedGraph& g) {
  return HasCycle(u, g, g.root());
}

long GroupSize(const CWord& u, const CWord& v, const size_t max_complete=6) {
  FoldedGraph g;
  bool is_complete = false;
  for (auto i = 0u; i < max_complete && !is_complete; ++i) {
    auto size = g.size();
    CompleteWith(u, size, &g);
    CompleteWith(v, size, &g);
    if (size == g.size()) {
      is_complete = true;
    }
  }
  if (is_complete) {
    return std::distance(g.begin(), g.end());
  } else {
    return 0;
  }
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

CWordTuple<2> AutoNormalForm(const CWord& u, const CWord& v) {
  auto tuple = CWordTuple<2>{u, v};
  auto min = tuple;
  for (auto&& image : ShortestAutomorphicImages(tuple)) {
    auto image_min = ConjugationInverseFlipNormalForm(image);
    if (image_min < min) {
      min = image_min;
    }
  }
  return min;
}

template<typename Generator, typename Iter>
std::vector<Iter> sample(size_t k, Generator& g, Iter begin, Iter end) {
  std::vector<Iter> result;
  result.reserve(k);

  while (result.size() < k) {
    if (begin == end) {
      return result;
    }

    result.push_back(begin);
    ++begin;
  }

  size_t consumed_count = result.size();

  while (begin != end) {
    auto to_swap = std::uniform_int_distribution<size_t>(0, consumed_count)(g);
    if (to_swap < k) {
      result[to_swap] = begin;
    }
    ++consumed_count;
    ++begin;
  }

  return result;
}

bool CanCompleteWith(FoldedGraph::Word r, const  FoldedGraph& g) {
  for (size_t shift = 0; shift < r.size(); ++shift, r.CyclicLeftShift()) {
    for (const auto& vertex : g) {
      if (!g.HasCycle(r, vertex)) {
        return true;
      }
    }
  }
  return false;
}

CWord FindPath(
    const FoldedGraph::Vertex& from, const FoldedGraph::Vertex& to
    , std::unordered_set<const FoldedGraph::Vertex*>* visited) {
  auto is_visited = visited->insert(&to);
  if (!is_visited.second) {
    return CWord();
  }
  CWord min;
  for (auto&& edge : to) {
    if (edge.terminus() == from) {
      return CWord(1, edge.label().Inverse());
    }
    auto path_to_terminus = FindPath(from, edge.terminus(), visited);
    if (!path_to_terminus.Empty()) {
      if (min.Empty() || min.size() > path_to_terminus.size() + 1) {
        min = path_to_terminus;
        min.PushBack(edge.label().Inverse());
      }
    }
  }
  return min;
}


CWord FindPath(const FoldedGraph::Vertex& from, const FoldedGraph::Vertex& to) {
  if (to.IsMerged()) {
    return FindPath(from, to.Parent());
  }
  std::unordered_set<const FoldedGraph::Vertex*> visited;
  return FindPath(from, to, &visited);
}

auto kCompleteCount = 2;
auto kFirstMin = 6u;
auto kFirstMax = 10u;

std::pair<std::vector<CWord>, size_t> findNonTrivialFinIndexSubgroup(const CWord& u, const CWord& v) {
  FoldedGraph g;

  std::random_device d;
  auto seed = d();
//  std::cout << "Seed: " << seed << "\n";
  std::mt19937_64 generator(seed); //2512030257


  RandomWord init_word(kFirstMin, kFirstMax);

  std::vector<CWord> combined = {init_word(generator)};

  g.PushCycle(combined.front());

  bool is_complete = false;
  for (auto i = 0u; i < kCompleteCount && !is_complete; ++i) {
    auto size = g.size();
    CompleteWith(u, size, &g);
    CompleteWith(v, size, &g);
    if (size == g.size()) {
      is_complete = true;
    }
  }


  while (!is_complete) {
    auto to_combine = sample(1, generator, g.begin(), g.end());
    if (to_combine.size() != 1) {
      std::cerr << "For some reason got trivial graph in a wrong place" << std::endl;
      break;
    }

    if (*to_combine[0] == g.root()) {
      continue;
    }

//    std::cout << (to_combine[0] - g.begin())  << " " << "\n";
    combined.push_back(g.FindShortestPath(g.root(), *(to_combine[0])).value());
    g.Combine(&g.root(), &*(to_combine[0]), 0);

    if (!CanCompleteWith(u, g) && !CanCompleteWith(v, g)) {
      is_complete = true;
    }
  }

  auto count = std::distance(g.begin(), g.end());
  if(!HasRootCycle(CWord("x"), g) || !HasRootCycle(CWord("y"), g)) {
    return {std::move(combined), count};
  } else {
    return {};
  }
}

/*
CWord("xxyXy")
CWord("xyyyyyyxY"):

1
3: yxY
4: Xyyy
index: 8

2
1: x
5: yxyyy
index: 8

3
1: x
5: yyxYY

 */

size_t findBestSubgroup(const CWord& u, const CWord& v, std::vector<CWord>* resulting_shortest_rels=nullptr) {
  auto TotaLength = [](const std::vector<CWord>& words) {
    return std::accumulate(words.begin(), words.end(), 0u, [](unsigned int l, const CWord& w) { return l + w.size(); });
  };

  std::vector<CWord> shortest_rels;
  auto shortest_length = 0;
  size_t index = 0u;
  for (auto i = 0u; i < 100; ++i) {
    auto new_relations = findNonTrivialFinIndexSubgroup(u, v);
    if (new_relations.first.empty()) {
      continue;
    }
    auto new_relations_length = TotaLength(new_relations.first);
    if (shortest_length == 0
        || new_relations_length < shortest_length
        || (new_relations_length == shortest_length && new_relations.second < index)
        || (new_relations_length == shortest_length
            && new_relations.second == index
            && std::lexicographical_compare(
                new_relations.first.begin()
                , new_relations.first.end()
                , shortest_rels.begin()
                , shortest_rels.end()))
        ) {
      shortest_length = new_relations_length;
      shortest_rels = std::move(new_relations.first);
      index = new_relations.second;
    }
  }

  if (index != 0u)  {
    if (resulting_shortest_rels) {
      *resulting_shortest_rels = std::move(shortest_rels);
    }
    else {
      for (auto&& w : shortest_rels) {
        std::cout << w << "\n";
      }

      std::cout << "index: " << index << "\n";
    }
  }

  return index;
}

/*
 * CWord("xxxyxYxy"), CWord("xxxYXyyXY")
 * 1: x
 * 4: yXyy
 * index: 9
 *
 * CWord("xxyxyXXY"), CWord("xyyyyxYxY")
 * 2: xx
 * 4: yxYx
 * index: 22
 *
 * CWord("xxxxxyXXy"), CWord("xxyyyyxxY")
 * 9: YYxyXyxxY
 * 7: xYYXXXY
 * 10: XYYYxYXYXX
 * 5: Xyyyx
 * index: 8
 */

/*
 * crag.enumerate.trivial_presentaions 3 xxxxxyXXy xxyyyyxxY
9: YXyyXYYYx
11: XyxYXYYXYYY
11: YXYXyXyyXXy
9: YxYXyxxyx
10: YXYYxYxyxx
index: 21
 */

struct PairsInfoData {
  std::ostream* trivial_abel;
  std::ostream* no_simple_reduction;
  std::ostream* no_conjugation_reduction;
  std::ostream* trivial;
  std::ostream* non_trivial_finite;
  std::ostream* non_trivial_with_fis;
  std::ostream* unknown;

  size_t count{};
};

bool CheckPair(const CWord& u, const CWord& v, PairsInfoData& data) {
  if (!hasTrivialAbelianisation(u, v)) {
    return false;
  }

  if (v < u) {
    return false;
  }

  ++data.count;

  (*data.trivial_abel) << data.count << " " << u << " " << v << "\n";

  CWord::size_type common_part_length, common_u_begin, common_v_begin;
  std::tie(common_u_begin, common_v_begin, common_part_length) = LongestCommonSubwordCyclic(u, v);
  if (common_part_length > v.size() / 2 || common_part_length > u.size() / 2) {
    return false;
  }

  (*data.no_simple_reduction) << data.count << " " << u << " " << v << "\n";

  if (ConjugationLengthReduction(u, v) || ConjugationLengthReduction(v, u)) {
    return false;
  }

  (*data.no_conjugation_reduction) << data.count << " " << u << " " << v << "\n";

  auto group_size = GroupSize(u, v);
  if (group_size == 1) {
    (*data.trivial) << data.count << " " << u << " " << v << "\n";
    return true;
  } else {
    auto auto_normal_form = AutoNormalForm(u, v);
    bool nf_conj_length_reduction =
        ConjugationLengthReduction(auto_normal_form[0], auto_normal_form[1])
            || ConjugationLengthReduction(auto_normal_form[1], auto_normal_form[0]);

    if (group_size != 0) {
      (*data.non_trivial_finite) <<
          data.count << " " << u << " " << v << " " << group_size << " " << auto_normal_form << " "
          << nf_conj_length_reduction << "\n";
    } else {
      auto index = findBestSubgroup(u, v);
      if (index != 0) {
        (*data.non_trivial_with_fis) <<
            data.count << " " << u << " " << v << " " << index << " " << auto_normal_form << " "
            << nf_conj_length_reduction << "\n";
      } else {
        (*data.unknown) << data.count << " " << u << " " << v << " " << auto_normal_form << " "
            << nf_conj_length_reduction << "\n";

      }
    }
    return false;
  }
}

size_t ProcessLength(size_t total_length) {
  std::ofstream trivial_abel(std::to_string(total_length) + "_trivial_abel.txt");
  std::ofstream no_simple_reduction(std::to_string(total_length) + "_no_simple_reduction.txt");
  std::ofstream no_conjugation_reduction(std::to_string(total_length) + "_no_conjugation_reduction.txt");
  std::ofstream trivial(std::to_string(total_length) + "_trivial.txt");
  std::ofstream non_trivial_finite(std::to_string(total_length) + "_non_trivial_finite.txt");
  std::ofstream non_trivial_with_fis(std::to_string(total_length) + "_non_trivial_with_fis.txt");
  std::ofstream unknown(std::to_string(total_length) + "_unknown.txt");

  std::ofstream enumerator_input("presentations_to_check.txt", std::ios::app);

  PairsInfoData data{
      &trivial_abel,
      &no_simple_reduction,
      &no_conjugation_reduction,
      &trivial,
      &non_trivial_finite,
      &non_trivial_with_fis,
      &unknown
  };

  // each word should be of length at least 4
  // othersize all presentatinos of a trivial group
  // with an element of length 4 are known to be AC-trivial
  for(auto u : EnumerateWords(4, total_length - 4)) {
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

      if (CheckPair(u, v, data)) {
        enumerator_input << ToString(u) << " " << ToString(v) << "\n";
      }
    }
  }

  return data.count;
}

void TestSinglePair(const char* u, const char* v) {
  std::ostringstream trivial_abel;
  std::ostringstream no_simple_reduction;
  std::ostringstream no_conjugation_reduction;
  std::ostringstream trivial;
  std::ostringstream non_trivial_finite;
  std::ostringstream non_trivial_with_fis;
  std::ostringstream unknown;

  PairsInfoData data{
      &trivial_abel
      , &no_simple_reduction
      , &no_conjugation_reduction
      , &trivial
      , &non_trivial_finite
      , &non_trivial_with_fis
      , &unknown
  };

  std::cout << u << " " << v << ": " << CheckPair(CWord(u), CWord(v), data) << "\n";

  std::cout << "trivial_abel: " << trivial_abel.str() << "\n";
  std::cout << "no_simple_reduction: " << no_simple_reduction.str() << "\n";
  std::cout << "no_conjugation_reduction: " << no_conjugation_reduction.str() << "\n";
  std::cout << "trivial: " << trivial.str() << "\n";
  std::cout << "non_trivial_finite: " << non_trivial_finite.str() << "\n";
  std::cout << "non_trivial_with_fis: " << non_trivial_with_fis.str() << "\n";
  std::cout << "unknown: " << unknown.str() << "\n";
}

int main(int argc, char* argv[]) {
  auto start = std::chrono::steady_clock::now();

  for (auto l : {13, 14, 15, 16, 17}) {
    std::clog << "Processing " << l << "... ";

    std::clog << ProcessLength(l) << " pairs total\n";
  }

  auto total_time = std::chrono::steady_clock::now() - start;

  std::clog << "Spent " << std::chrono::duration<double>(total_time).count() << " seconds\n";

  return 0;

//  CWord a;
//  CWord b;
//
//  kCompleteCount = std::stoi(argv[1]);
//
//
//  for (auto i = 2; i < argc; i += 4) {
//    a = CWord(argv[i]);
//    b = CWord(argv[i + 1]);
//    CWord c = CWord(argv[i + 2]);
//    CWord d = CWord(argv[i + 3]);
//    auto normalized1 = AutoNormalForm(a, b);
//    auto normalized2 = AutoNormalForm(c, d);
//
//    std::cout << (normalized1 == normalized2) << " " << normalized1 << " " << normalized2 << std::endl;
//  }
//
//
//  return 0;
//
//  while (true) {
//    auto subgroup = findNonTrivialFinIndexSubgroup(a, b);
//    if (subgroup.second != 0) {
//      for (auto&& w : subgroup.first) {
//        std::cout << w << "\n";
//      }
//      std::cout << "index: " << subgroup.second << std::endl;
//    }
//  }
//
//  return 0;
////  findBestSubgroup(CWord("xxxyxYxy"), CWord("xxxYXyyXY"));
//  /* findBestSubgroup(CWord("xxxxxyXXy"), CWord("xxyyyyxxY"));
//   * 10: YXyXyyyXXy
//9: YxYYYxYxx
//10: yyxxyxYYxx
//index: 8
//   */
//
//  /* findBestSubgroup(CWord("xxxxyXy"), CWord("xxYxyyyyyxY"));
//   * 2: xx
//9: YXyyXXYYY
//7: yxxyyxx
//index: 8
//   */
//
//
//  findBestSubgroup(CWord("xxxyXXYXY"), CWord("xyyXyXYYY"));
//  findBestSubgroup(CWord("xxxyXXYYY"), CWord("xyxYYXYxY"));
//  findBestSubgroup(CWord("xxxyxxYXY"), CWord("xyyXyXYYY"));
//  findBestSubgroup(CWord("xxxyxxYXY"), CWord("xyyyXYYxY"));
//  findBestSubgroup(CWord("xxxyxxYYY"), CWord("xyXYYXyXy"));
//  findBestSubgroup(CWord("xxxyxyXXY"), CWord("xyyyXYYxY"));
//  findBestSubgroup(CWord("xxxyxyyXY"), CWord("xxYxYXyyy"));
//
//  /*
//  findBestSubgroup(CWord("xxyXYxYXy"), CWord("xyXYYXyxY"));
//   7: yxYYxxx
//8: yXyxxYxx
//index: 9
//   */
//  /*
//  findBestSubgroup(CWord("xxyxxyXXy"), CWord("xyyXyyxYY"));
//   8: XYXYYYxY
//2: yX
//index: 96 */
//  return 0;

  //we will consider words of type x..., y...

//  auto start = std::chrono::steady_clock::now();
//
//  auto count = 0u;
////  auto count_nontrivial = 0u;
//
//  for(auto u : EnumerateWords(2, total_length - 1)) {
//    if (u.GetFront() == u.GetBack().Inverse()) {
//      continue;
//    }
//    if (ConjugationInverseNormalForm(u) != u) {
//      continue;
//    }
//    for (auto v : EnumerateWords(total_length - u.size())) {
//      if (v.GetFront() == v.GetBack().Inverse()) {
//        continue;
//      }
//      if (ConjugationInverseNormalForm(v) != v) {
//        continue;
//      }
//    }
//  }
//
//  auto total_time = std::chrono::steady_clock::now() - start;
//  std::cout << "Total time: " << std::chrono::duration_cast<std::chrono::milliseconds>(total_time).count() << " ms\n";
}