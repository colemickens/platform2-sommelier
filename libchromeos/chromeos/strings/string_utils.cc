// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/strings/string_utils.h>

#include <algorithm>
#include <string.h>
#include <utility>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

namespace chromeos {
namespace string_utils {

std::vector<std::string> Split(const std::string& str,
                               char delimiter,
                               bool trim_whitespaces,
                               bool purge_empty_strings) {
  std::vector<std::string> tokens;
  if (delimiter == 0)
    return tokens;

  const char* sz = str.c_str();
  if (sz) {
    const char* szNext = strchr(sz, delimiter);
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
    std::for_each(tokens.begin(), tokens.end(),
                  [](std::string& str) {
      base::TrimWhitespaceASCII(str, base::TRIM_ALL, &str); });
  }

  return tokens;
}

std::pair<std::string, std::string> SplitAtFirst(const std::string& str,
                                                 char delimiter,
                                                 bool trim_whitespaces) {
  std::pair<std::string, std::string> pair;
  if (delimiter == 0)
    return pair;

  const char* sz = str.c_str();
  const char* szNext = strchr(sz, delimiter);
  if (szNext) {
    pair.first = std::string(sz, szNext);
    pair.second = std::string(szNext + 1);
  } else {
    pair.first = str;
  }

  if (trim_whitespaces) {
    base::TrimWhitespaceASCII(pair.first, base::TRIM_ALL, &pair.first);
    base::TrimWhitespaceASCII(pair.second, base::TRIM_ALL, &pair.second);
  }

  return pair;
}

std::string Join(char delimiter, const std::vector<std::string>& strings) {
  return JoinString(strings, delimiter);
}

std::string Join(const std::string& delimiter,
                 const std::vector<std::string>& strings) {
  return JoinString(strings, delimiter);
}

std::string Join(char delimiter,
                 const std::string& str1, const std::string& str2) {
  return str1 + delimiter + str2;
}

std::string Join(const std::string& delimiter,
                 const std::string& str1, const std::string& str2) {
  return str1 + delimiter + str2;
}

std::string ToString(double value) {
  return base::StringPrintf("%g", value);
}

std::string ToString(bool value) {
  return value ? "true" : "false";
}

}  // namespace string_utils
}  // namespace chromeos
