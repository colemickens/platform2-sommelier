// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/indented_text.h"

#include <string>
#include <utility>
#include <vector>

#include <base/logging.h>
#include <chromeos/strings/string_utils.h>

using std::string;
using std::vector;

namespace chromeos_dbus_bindings {

IndentedText::IndentedText() : offset_(0) {}

void IndentedText::AddBlankLine() {
  AddLine("");
}

void IndentedText::AddBlock(const IndentedText& block) {
  AddBlockWithOffset(block, 0);
}

void IndentedText::AddBlockWithOffset(const IndentedText& block, size_t shift) {
  for (const auto& member : block.contents_) {
    AddLineWithOffset(member.first, member.second + shift);
  }
}

void IndentedText::AddLine(const std::string& line) {
  AddLineWithOffset(line, 0);
}

void IndentedText::AddLineWithOffset(const std::string& line, size_t shift) {
  contents_.emplace_back(line, shift + offset_);
}

void IndentedText::AddComments(const std::string& doc_string) {
  // Split at \n, trim all whitespaces and eliminate empty strings.
  auto lines = chromeos::string_utils::Split(doc_string, '\n', true, true);
  for (const auto& line : lines) {
    AddLine("// " + line);
  }
}

string IndentedText::GetContents() const {
  string output;
  for (const auto& member : contents_) {
    const string& line = member.first;
    size_t shift = line.empty() ? 0 : member.second;
    string indent(shift, ' ');
    output.append(indent + line + "\n");
  }
  return output;
}

void IndentedText::PushOffset(size_t shift) {
  offset_ += shift;
  offset_history_.push_back(shift);
}

void IndentedText::PopOffset() {
  CHECK(!offset_history_.empty());
  offset_ -= offset_history_.back();
  offset_history_.pop_back();
}

void IndentedText::Reset() {
  offset_ = 0;
  offset_history_.clear();
  contents_.clear();
}

}  // namespace chromeos_dbus_bindings
