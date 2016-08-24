//
// Created by dpantele on 5/23/16.
//

#include <fmt/format.h>

#include "acc_class.h"
#include "acc_classes.h"
#include "state_dump.h"


void ACClass::DescribeForLog(std::ostream* out) const {
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
  *out << "): " << minimal_;
  *out << ", " << aut_types_;
}

void ACClass::DescribeForLog(fmt::MemoryWriter* out) const {
  out->write("Class {}(", initial_);
  switch(init_kind_) {
  case AutKind::Ident:
    out->write("Id");
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
  out->write("): {}, {}", minimal_, aut_types_);
}

ACClass::ACClass(size_t id, ACPair pair, AutKind kind, ACStateDump* logger)
  : initial_(std::move(pair))
    , init_kind_(kind)
    , aut_types_(1u << static_cast<int>(kind))
    , minimal_(crag::ConjugationInverseFlipNormalForm(initial_))
    , merged_with_(id)
    , id_(id)
    , logger_(logger)
{
  logger_->DumpPairClass(minimal_, *this);
  logger_->NewMinimum(*this, minimal_);
}