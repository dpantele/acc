//
// Created by dpantele on 5/23/16.
//

#include "acc_classes.h"

#include <regex>
#include <crag/compressed_word/endomorphism.h>

using namespace crag;

void ACClasses::AddClass(const ACPair& a) {
  thread_local auto x_xy = Endomorphism(CWord("xy"), CWord("y"));
  thread_local auto x_y = Endomorphism(CWord("y"), CWord("x"));
  thread_local auto y_Y = Endomorphism(CWord("x"), CWord("Y"));

  classes_.emplace_back(classes_.size(), a, ACClass::AutKind::Ident, logger_);
  classes_.emplace_back(classes_.size(), Apply(x_xy, a), ACClass::AutKind::x_xy, logger_);
  classes_.emplace_back(classes_.size(), Apply(x_y, a), ACClass::AutKind::x_y, logger_);
  classes_.emplace_back(classes_.size(), Apply(y_Y, a), ACClass::AutKind::y_Y, logger_);
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
    classes_.at(class_a).Merge(&classes_.at(class_b));
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
    classes_.at(id).AddPair(min);
  }
}





