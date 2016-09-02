//
// Created by dpantele on 5/23/16.
//

#include "ACIndex.h"
#include "acc_classes.h"

#include <regex>
#include <crag/compressed_word/endomorphism.h>

using namespace crag;

void ACClasses::AddClass(const ACPair& a) {
  thread_local auto x_xy = Endomorphism(CWord("xy"), CWord("y"));
  thread_local auto x_y = Endomorphism(CWord("y"), CWord("x"));
  thread_local auto y_Y = Endomorphism(CWord("x"), CWord("Y"));

  classes_.emplace_back(classes_.size(), a, a, ACClass::AutKind::Ident, logger_);
  classes_.emplace_back(classes_.size(), a, Apply(x_xy, a), ACClass::AutKind::x_xy, logger_);
  classes_.emplace_back(classes_.size(), a, Apply(x_y, a), ACClass::AutKind::x_y, logger_);
  classes_.emplace_back(classes_.size(), a, Apply(y_Y, a), ACClass::AutKind::y_Y, logger_);
}

void ACClasses::RestoreMerges() {
  fs::ifstream merges_log(config_.classes_merges_in());
  if (merges_log.fail()) {
    return;
  }

  std::regex log_format("^(\\d+) (\\d+)$");
  std::string log_line;
  std::smatch log_line_parsed;

  while(std::getline(merges_log, log_line)) {
    if (!std::regex_match(log_line, log_line_parsed, log_format)) {
      throw std::runtime_error("Corrupted merges dump");
    }
    auto class_a = std::stoul(log_line_parsed[1]);
    auto class_b = std::stoul(log_line_parsed[2]);
    Merge(class_a, class_b);
  }
}

void ACClasses::RestoreMinimums() {
  fs::ifstream minimums_log(config_.classes_minimums_in());
  if (minimums_log.fail()) {
    return;
  }

  std::regex log_format("^(\\d+) ([xXyY]+) ([xXyY]+)$");
  std::string log_line;
  std::smatch log_line_parsed;

  while(std::getline(minimums_log, log_line)) {
    if (!std::regex_match(log_line, log_line_parsed, log_format)) {
      throw std::runtime_error("Corrupted minimums dump");
    }
    auto id = std::stoul(log_line_parsed[1]);
    ACPair min{CWord(log_line_parsed[2]), CWord(log_line_parsed[3])};
    AddPair(id, min);
  }
}

void ACClasses::Merge(ACClasses::ClassId first_id, ACClasses::ClassId second_id) {
  auto first = at(first_id);
  assert(first->IsPrimary());

  auto second = at(second_id);
  assert(second->IsPrimary());

  if (first == second) {
    return;
  }

  if (first->id_ > second->id_) {
    std::swap(first, second);
  }

  second->merged_with_ = first->id_;

  //update minimal and aut_type
  if (second->minimal_ < first->minimal_) {
    first->minimal_ = second->minimal_;
  }

  first->aut_types_ |= second->aut_types_;
  first->pairs_count_ += second->pairs_count_;

  logger_->Merge(*first, *second);
}

void ACClasses::AddPair(ACClasses::ClassId id, ACPair pair) {
  ACClass* canonical = at(id);

  logger_->DumpPairClass(pair, *canonical);
  canonical->pairs_count_ += 1;

  if (pair < canonical->minimal_) {
    canonical->minimal_ = pair;
    logger_->NewMinimum(*canonical, pair);
  }
}

void ACClasses::InitACIndex(ACIndex* index) {
  std::map<ACPair, ACClass*> pairs_classes;
  for (auto&& c : classes_) {
    auto was_inserted = pairs_classes.emplace(minimal_in(c.id_), &c);
    if (!was_inserted.second) {
      Merge(c.id_, was_inserted.first->second->id_);
    }
  }

  auto initial_batch = index->NewBatch();

  for (auto&& p : pairs_classes) {
    initial_batch.Push(p.first, p.second->id_);
  }

  initial_batch.Execute();

  // make sure data is committed
  while (index->GetData().size() < pairs_classes.size()) {
    std::this_thread::yield();
  }
}





