//
// Created by dpantele on 5/23/16.
//

#include <fmt/format.h>

#include "acc_class.h"
#include "acc_classes.h"
#include "state_dump.h"


void ACClass::DescribeForLog(std::ostream* out) const {
  *out << id_ << ": " << initial_ << "(";
  switch(init_kind()) {
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
  *out << "): ";
  if (merged_with_ != id_) {
    *out << "=> " << merged_with_;
  } else {
    *out << minimal_;

    if (id_ % 4 == 0) {
      *out << ", " << aut_types_;
    }
  }
}

void ACClass::DescribeForLog(fmt::MemoryWriter *out) const {
  out->write("{}: {}(", id_, initial_);
  switch (init_kind()) {
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

  if (id_ != merged_with_) {
    out->write("): => ", merged_with_);
  } else if (id_ % 4 == 0) {
    out->write("): {}, {}", minimal_, aut_types_);
  } else {
    out->write("): {}", minimal_);
  }
}

ACClass::ACClass(size_t id, ACPair initial, ACPair image, AutKind kind, ACStateDump* logger)
  : initial_(std::move(initial))
  , aut_types_(1u << static_cast<int>(kind))
  , minimal_(crag::ConjugationInverseFlipNormalForm(image))
  , merged_with_(id)
  , id_(id)
  , logger_(logger)
{
  assert(kind == init_kind());
  logger_->DumpPairClass(minimal_, *this);
  logger_->NewMinimum(*this, minimal_);
}