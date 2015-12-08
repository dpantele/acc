//
// Created by dpantele on 12/8/15.
//

#ifndef ACC_CANONICAL_WORD_MAPPING_H
#define ACC_CANONICAL_WORD_MAPPING_H

#include <deque>

#include <compressed_word/compressed_word.h>

namespace crag {

class CanonicalMapping {

  struct CanonicalWord {
    CWord word_;
    CWord canonical_word_;
    CanonicalWord* root_ = nullptr;
    size_t set_size_ = 1;

    CanonicalWord* root() {
      if (root_ == nullptr) {
        return this;
      }
      root_ = root_->root();
      return root_;
    }

    void Merge(CanonicalWord* other);

    CanonicalWord(CWord word)
      : word_(word)
      , canonical_word_(word)
    { }
  };
 public:
  CanonicalMapping()=default;

  CanonicalMapping(CWord::size_type min_length, CWord::size_type end_length);


  const auto& mapping() const {
    return mapping_;
  }
 private:
  std::deque<CanonicalWord> mapping_;


};

}

#endif //ACC_CANONICAL_WORD_MAPPING_H
