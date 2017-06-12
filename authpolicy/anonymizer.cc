// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/anonymizer.h"

#include <algorithm>

#include <base/strings/string_util.h>

namespace {

constexpr char kNewLineChars[] = "\r\n";

// Searches the next occurrance of |search_keyword| in |str|, starting from
// |pos|, and sets |search_value| to the rest of the line, advances |pos| to
// after the search value and returns true. Returns false if |search_keyword|
// is not found.
bool FindNextSearchValue(const std::string& str,
                         const std::string& search_keyword,
                         size_t* pos,
                         std::string* search_value) {
  DCHECK(*pos <= str.size());
  *pos = str.find(search_keyword, *pos);
  if (*pos == std::string::npos)
    return false;
  *pos += search_keyword.size();
  DCHECK(*pos <= str.size());
  size_t end_pos = str.find_first_of(kNewLineChars, *pos);
  DCHECK(end_pos >= *pos);
  *search_value = str.substr(*pos, end_pos - *pos);
  base::TrimWhitespaceASCII(*search_value, base::TRIM_ALL, search_value);
  *pos = end_pos;
  return true;
}

}  // namespace

namespace authpolicy {

Anonymizer::Anonymizer() = default;

void Anonymizer::SetReplacement(const std::string& string_to_replace,
                                const std::string& replacement) {
  if (string_to_replace.empty())
    return;
  replacements_[string_to_replace] = replacement;
}

void Anonymizer::ReplaceSearchArg(const std::string& search_keyword,
                                  const std::string& replacement) {
  if (search_keyword.empty())
    return;
  search_replacements_[search_keyword + ":"] = replacement;
}

void Anonymizer::ResetSearchArgReplacements() {
  search_replacements_.clear();
}

std::string Anonymizer::Process(const std::string& input) {
  // Gather all search args and add them to replacements_.
  for (const auto& replacement : search_replacements_) {
    size_t pos = 0;
    const std::string& search_keyword = replacement.first;
    std::string string_to_replace;
    while (FindNextSearchValue(input, search_keyword, &pos, &string_to_replace))
      SetReplacement(string_to_replace, replacement.second);
  }

  // Now handle string replacements. Note: Iterate in reverse order. This
  // guarantees that keys are processed in reverse sorting order and prevents
  // that keys being substrings of longer keys are replaced first, e.g. we don't
  // want to replace 'KEY_1" before 'KEY_123'.
  std::string output = input;
  for (auto replacement = replacements_.rbegin(), rend = replacements_.rend();
       replacement != rend;
       ++replacement) {
    base::ReplaceSubstringsAfterOffset(
        &output, 0, replacement->first, replacement->second);
  }
  return output;
}

}  // namespace authpolicy
