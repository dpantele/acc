//
// Created by dpantele on 11/9/15.
//

#include <ostream>
#include <utility>

#include <compressed_word/compressed_word.h>


#ifndef ACC_CYCLES_H_H
#define ACC_CYCLES_H_H

namespace crag {
namespace {

struct Cycle
{
  using Word = CWord;
  using Weight = int64_t;
  std::pair<Word, Weight> data_;

  Cycle(const char* word, Weight weight)
      : data_(Word(word), weight) {
  }

  Cycle(Word word, Weight weight)
      : data_(std::move(word), std::move(weight)) {
  }

  const Word& word() const {
    return data_.first;
  }

  Weight weight() const {
    return data_.second;
  }
};

std::ostream& operator<<(std::ostream& out, const Cycle& c) {
  return out << "{ " << c.word() << " , " << c.weight() << " }";
}


}
}

#endif //ACC_CYCLES_H_H
