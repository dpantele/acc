//
// Created by dpantele on 5/23/16.
//

#ifndef ACC_ACC_CLASSES_H
#define ACC_ACC_CLASSES_H

#include <map>

#include "acc_class.h"
#include "state_dump.h"
#include "config.h"

class ACIndex;
class ACClasses {
 public:
  ACClasses(const Config &c, ACStateDump *logger)
      : config_(c)
      , logger_(logger) {}

  std::shared_ptr<ACClasses> Clone() const {
    auto result = std::make_shared<ACClasses>(*this);
#ifndef NDEBUG
    ++result->version_;
#endif
    return result;
  }

  void AddClass(const ACPair &a);

  template <typename C>
  void AddClasses(const C &container) {
    for (auto &&pair : container) {
      AddClass(pair);
    }
  }

  //! Restores the sequence of merges
  void RestoreMerges();
  void RestoreMinimums();

  typedef size_t ClassId;

  void Merge(ClassId first_id, ClassId second_id);
  void AddPair(ClassId id, ACPair pair);

  void InitACIndex(ACIndex *index);;

  std::vector<ACClass>::iterator
  begin() {
    return classes_.begin();
  }

  std::vector<ACClass>::const_iterator
  begin() const {
    return classes_.begin();
  }

  std::vector<ACClass>::iterator
  end() {
    return classes_.end();
  }

  std::vector<ACClass>::const_iterator
  end() const {
    return classes_.end();
  }

  ACClass *at(size_t id) {
    auto canonical = &classes_.at(id);
    while (!canonical->IsPrimary()) {
      canonical = &classes_[canonical->merged_with_];
    }
    return canonical;
  }

  //! Replace every merged_with_ with the final id
  /**
   * Should be called after the modification batch
   */
  void Normalize() {
    for (auto &&c : classes_) {
      c.merged_with_ = at(c.id_)->id_;

      if (c.id_ % 4 == 3) {
        auto& original = classes_.at(c.id_ - 3);
        auto& canonical = classes_.at(original.merged_with_);
        for (auto i = 1u; i < 4; ++i) {
          auto& image = classes_.at(original.id_ + i);
          if (original.merged_with_ == image.merged_with_) {
            // make sure it is allowed for the canonical
            canonical.aut_types_.set(i);
          }
        }
      }
    }
  }

  const ACClass *at(size_t id) const {
    return static_cast<const ACClass *>(const_cast<ACClasses *>(this)->at(id));
  }

  //! Get the 'canonical' representative for the class
  const crag::CWordTuple<2> &minimal_in(ClassId id) const {
    return at(id)->minimal_;
  }

  //! Check if automorphisms may be used for this class
  /**
   * This become true as soon as it gets merged with all 3 automorphic 'relatives',
   * i.e. there is are 3 pairs of words in this class such that \phi(u, v) ~(ACM) (u, v)
   */
  bool AllowsAutMoves(ClassId id) const {
    return at(IdentityImageFor(id))->aut_types_.all();
  }

  bool AreMerged(ClassId first, ClassId second) const {
    return at(first) == at(second);
  }

  static constexpr bool IsIdent(ClassId id) {
    return id % 4u == 0;
  }

  static constexpr ClassId IdentityImageFor(ClassId id) {
    return id - (id % 4u);
  }

  bool AllowsAutomorphism(ClassId id, ACClass::AutKind type) const {
    return at(id)->aut_types_.test(static_cast<size_t>(type));
  }

#ifndef NDEBUG
  size_t version_ = 0u; //!< Special debugging field to make sure we don't overwrite some changes
#endif

 private:
  std::vector<ACClass> classes_;
  const Config &config_;
  ACStateDump *logger_;
};

#endif //ACC_ACC_CLASSES_H
