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
    std::for_each(tokens.begin(), tokens.end(), [](std::string& str) {
      base::TrimWhitespaceASCII(str, base::TRIM_ALL, &str);
    });
    if (purge_empty_strings) {
      // We might have removed all the characters from a string if they were
      // all whitespaces. If we don't want empty strings, make sure we remove
      // them.
      tokens.erase(std::remove(tokens.begin(), tokens.end(), std::string{}),
                   tokens.end());
    }
  }

  return tokens;
}

bool SplitAtFirst(const std::string& str,
                  char delimiter,
                  std::string* left_part,
                  std::string* right_part,
                  bool trim_whitespaces) {
  if (delimiter == 0) {
    left_part->clear();
    right_part->clear();
    return false;
  }

  bool delimiter_found = false;
  const char* sz = str.c_str();
  const char* szNext = strchr(sz, delimiter);
  if (szNext) {
    *left_part = std::string(sz, szNext);
    *right_part = std::string(szNext + 1);
    delimiter_found = true;
  } else {
    *left_part = str;
  }

  if (trim_whitespaces) {
    base::TrimWhitespaceASCII(*left_part, base::TRIM_ALL, left_part);
    base::TrimWhitespaceASCII(*right_part, base::TRIM_ALL, right_part);
  }

  return delimiter_found;
}

std::pair<std::string, std::string> SplitAtFirst(const std::string& str,
                                                 char delimiter,
                                                 bool trim_whitespaces) {
  std::pair<std::string, std::string> pair;
  SplitAtFirst(str, delimiter, &pair.first, &pair.second, trim_whitespaces);
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
                 const std::string& str1,
                 const std::string& str2) {
  return str1 + delimiter + str2;
}

std::string Join(const std::string& delimiter,
                 const std::string& str1,
                 const std::string& str2) {
  return str1 + delimiter + str2;
}

std::string ToString(double value) {
  return base::StringPrintf("%g", value);
}

std::string ToString(bool value) {
  return value ? "true" : "false";
}

std::string GetBytesAsString(const std::vector<uint8_t>& buffer) {
  auto ptr = reinterpret_cast<const char*>(buffer.data());
  return std::string{ptr, ptr + buffer.size()};
}

std::vector<uint8_t> GetStringAsBytes(const std::string& str) {
  auto ptr = reinterpret_cast<const uint8_t*>(str.data());
  return std::vector<uint8_t>{ptr, ptr + str.size()};
}

}  // namespace string_utils
}  // namespace chromeos
