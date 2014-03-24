// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_STRING_UTILS_H_
#define BUFFET_STRING_UTILS_H_

#include <string>
#include <vector>

namespace chromeos {
namespace string_utils {

// Treats the string as a delimited list of substrings and returns the array
// of original elements of the list.
// By default, empty elements from the original string are omitted and
// each element has all whitespaces trimmed off.
std::vector<std::string> Split(std::string const& str,
                               char delimiter,
                               bool trim_whitespaces = true,
                               bool purge_empty_strings = true);

// Splits the string into two pieces at the first position of the specified
// delimiter. By default, each part has all whitespaces trimmed off.
std::pair<std::string, std::string> SplitAtFirst(std::string const& str,
                                                 char delimiter,
                                                 bool trim_whitespaces = true);

// Joins an array of strings into a single string separated by |delimiter|.
std::string Join(char delimiter, std::vector<std::string> const& strings);
std::string Join(std::string const& delimiter,
                 std::vector<std::string> const& strings);
std::string Join(char delimiter,
                 std::string const& str1, std::string const& str2);
std::string Join(std::string const& delimiter,
                 std::string const& str1, std::string const& str2);

} // namespace string_utils
} // namespace chromeos

#endif // BUFFET_STRING_UTILS_H_
