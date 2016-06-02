//
// Created by dpantele on 5/23/16.
//

#include <fmt/format.h>

#include "acc_class.h"
#include "state_dump.h"


void ACClass::DescribeForLog(std::ostream* out) {
  *out << "Class " << initial_ << "(";
  switch(init_kind_) {
    case AutKind::Ident:
      *out << "Id";
      break;
    case AutKind::x_xy:
      *out << "x->xy";
      break;
    case AutKind::x_y:
      *out << "x<->y";
      break;
    case AutKind::y_Y:
      *out << "y->Y";
      break;
  }
  *out << "): " << canonical_.root()->minimal_;
  *out << ", " << canonical_.root()->aut_types_;
}
void ACClass::Merge(ACClass* other) {
  auto merge_result = canonical_.MergeWith(&other->canonical_);
  if (merge_result.first == merge_result.second) {
    return;
  }
  //update minimal and aut_type
  if (merge_result.second->minimal_ < merge_result.first->minimal_) {
    merge_result.first->minimal_ = merge_result.second->minimal_;
  }

  merge_result.first->aut_types_ |= merge_result.second->aut_types_;

  merge_result.first->pairs_count_ += merge_result.second->pairs_count_;
  logger_->Merge(*this, *other);
}
void ACClass::AddPair(crag::CWordTuple<2> pair) {
  ACClass* canonical = canonical_.root();

  logger_->DumpPairClass(pair, *canonical);
  pairs_count_ += 1;

  if (pair < canonical->minimal_) {
    canonical->minimal_ = pair;
    logger_->NewMinimum(*canonical, pair);
  }
}
ACStateDump ACClass::CreateLogger(const Config& c) {
  static_assert(std::is_move_constructible<ACStateDump>(), "ACStateDump must be move-constructible");
  return ACStateDump(c);
}

ACClass::ACClass(size_t id, ACPair pair, AutKind kind, ACStateDump* logger)
  : initial_(std::move(pair))
    , init_kind_(kind)
    , aut_types_(1u << static_cast<int>(kind))
    , minimal_(crag::ConjugationInverseFlipNormalForm(initial_))
    , canonical_(this)
    , id_(id)
    , logger_(logger)
{
  logger_->DumpPairClass(minimal_, *this);
  logger_->NewMinimum(*this, minimal_);
}