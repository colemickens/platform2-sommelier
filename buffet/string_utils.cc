// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/string_utils.h"

#include <algorithm>
#include <string.h>
#include <base/strings/string_util.h>

namespace chromeos {
namespace string_utils {

std::vector<std::string> Split(std::string const& str,
                               char delimiter,
                               bool trim_whitespaces,
                               bool purge_empty_strings) {
  std::vector<std::string> tokens;
  if (delimiter == 0)
    return tokens;

  char const* sz = str.c_str();
  if (sz) {
    char const* szNext = strchr(sz, delimiter);
    while (szNext) {
      if (szNext != sz || !purge_empty_strings)
        tokens.emplace_back(sz, szNext - sz);
      sz = szNext + 1;
      szNext = strchr(sz, delimiter);
    }
    if (*sz != 0 || !purge_empty_strings)
      tokens.emplace_back(sz);
  }

  if (trim_whitespaces) {
    std::for_each(tokens.begin(), tokens.end(), [](std::string& str) {
      TrimWhitespaceASCII(str, TRIM_ALL, &str); });
  }

  return tokens;
}

std::pair<std::string, std::string> SplitAtFirst(std::string const& str,
                                                 char delimiter,
                                                 bool trim_whitespaces) {
  std::pair<std::string, std::string> pair;
  if (delimiter == 0)
    return pair;

  char const* sz = str.c_str();
  char const* szNext = strchr(sz, delimiter);
  if (szNext) {
    pair.first = std::string(sz, szNext);
    pair.second = std::string(szNext + 1);
  } else {
    pair.first = str;
  }

  if (trim_whitespaces) {
    TrimWhitespaceASCII(pair.first, TRIM_ALL, &pair.first);
    TrimWhitespaceASCII(pair.second, TRIM_ALL, &pair.second);
  }

  return pair;
}

std::string Join(char delimiter, std::vector<std::string> const& strings) {
  return JoinString(strings, delimiter);
}

std::string Join(std::string const& delimiter,
                 std::vector<std::string> const& strings) {
  return JoinString(strings, delimiter);
}

std::string Join(char delimiter,
                 std::string const& str1, std::string const& str2) {
  return str1 + delimiter + str2;
}

std::string Join(std::string const& delimiter,
                 std::string const& str1, std::string const& str2) {
  return str1 + delimiter + str2;
}

} // namespace string_utils
} // namespace chromeos
