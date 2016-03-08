#include <compressed_word/compressed_word.h>
#include <compressed_word/tuple_normal_form.h>

#include <fstream>
#include <string>
#include <map>
#include <limits>

using namespace crag;

CWord conj(CWord u, CWord v) {
  return u.Inverse() + v.Inverse() + u + v;
}
int countX(CWord w) {
  if (w.Empty()) {
    return 0;
  }
  auto front = w.GetFront();
  w.PopFront();
  switch (front.AsInt()) {
    case XYLetter('x').AsInt():
      return 1 + countX(w);
    case XYLetter('X').AsInt():
      return -1 + countX(w);
    default:
      return countX(w);
  }
}

CWord random_x0(std::random_device& rd) {
  RandomWord w(9, 9);
  auto result = w(rd);
  while (countX(result) != 0) {
    result = w(rd);
  }
  return result;
}

CWordTuple<2> MinInvSwap(const CWordTuple<2>& a) {
  auto result = ConjugationInverseNormalForm(a);
  auto swapped = ConjugationInverseNormalForm(a.GetReversed());

  return result < swapped ? result : swapped;
}

int main() {
  constexpr auto n = 2;

  auto orbit = ShortestAutomorphicImages(CWordTuple<2>{
      CWord("X") + CWord(n, XYLetter('y')) + CWord("x") + CWord(n+1, XYLetter('Y')),
      CWord("Y") + CWord(n, XYLetter('x')) + CWord("y") + CWord(n+1, XYLetter('X'))
  });

  auto min = *orbit.begin();
  for (auto&& elem : orbit) {
    auto elem_min = MinInvSwap(elem);
    if (elem_min < min) {
      min = elem_min;
    }
  }

  std::cout << min << std::endl;

  return 0;

  std::ifstream in("/tmp/h20_unproc_words.txt");

  std::random_device rd;

  auto w = CWordTuple<2>{
      CWord("X") + CWord(n, XYLetter('y')) + CWord("x") + CWord(n+1, XYLetter('Y')),
      random_x0(rd)
      //RandomWord(7, 7)(rd)
  };
  auto shortest = ShortestAutomorphicImages(w);

  while (shortest.size() == 8) {
    w = CWordTuple<2>{
        CWord("X") + CWord(n, XYLetter('y')) + CWord("x") + CWord(n+1, XYLetter('Y')),
        random_x0(rd)
        //RandomWord(7, 7)(rd)
    };
    shortest = ShortestAutomorphicImages(w);
  }

  std::cout << w << std::endl;
  std::cout << "Size of orbit: " << shortest.size() << std::endl;
  std::cout << "Orbit: " << std::endl;
  for (auto&& w : shortest) {
    std::cout << w << std::endl;
  }

  return 0;

  CWord u, v;
  std::string line;
  auto split_line = [&]() {
    auto comma_count = 0u;
    u.Clear();
    v.Clear();
    getline(in, line);
    for(auto&& s : line) {
      if (s == ',') {
        ++comma_count;
        continue;
      }
      if (std::isspace(s)) {
        continue;
      }
      if (comma_count == 1) {
        u.PushBack(XYLetter(s));
      } else if (comma_count == 2) {
        v.PushBack(XYLetter(s));
      }
    }

    return comma_count == 2;
  };

  struct LenghtInfo {
    double total_length_ = 0;
    double total_count_ = 0;
    double min_size_ = std::numeric_limits<double>::max();
    double max_size_ = std::numeric_limits<double>::min();
  };

  std::map<CWord::size_type, LenghtInfo> stats;

  getline(in, line);
  auto count = 0u;
  while (split_line()) {
    if (++count % 1'000'000 == 0) {
      std::cout << count << std::endl;
    }
    auto& current_stats = stats.emplace(u.size() + v.size(), LenghtInfo()).first->second;
    current_stats.total_count_ += 1;
    auto current_size = ShortestAutomorphicImages(CWordTuple<2>{u, v}).size();
    current_stats.total_length_ += current_size;
    if (current_size < current_stats.min_size_) {
      current_stats.min_size_ = current_size;
    }
    if (current_size > current_stats.max_size_) {
      current_stats.max_size_ = current_size;
    }
  }

  for (auto&& stat : stats) {
    std::cout << stat.first << ": " << stat.second.total_length_
        << " " << stat.second.total_count_
        << " " << stat.second.total_length_ / stat.second.total_count_
        << " " << stat.second.min_size_
        << " " << stat.second.max_size_
        << "\n";
  }
}