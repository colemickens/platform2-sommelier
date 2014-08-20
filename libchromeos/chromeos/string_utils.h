// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_STRING_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_STRING_UTILS_H_

#include <string>
#include <utility>
#include <vector>

namespace chromeos {
namespace string_utils {

// Treats the string as a delimited list of substrings and returns the array
// of original elements of the list.
// By default, empty elements from the original string are omitted and
// each element has all whitespaces trimmed off.
std::vector<std::string> Split(const std::string& str,
                               char delimiter,
                               bool trim_whitespaces = true,
                               bool purge_empty_strings = true);

// Splits the string into two pieces at the first position of the specified
// delimiter. By default, each part has all whitespaces trimmed off.
std::pair<std::string, std::string> SplitAtFirst(const std::string& str,
                                                 char delimiter,
                                                 bool trim_whitespaces = true);

// Joins an array of strings into a single string separated by |delimiter|.
std::string Join(char delimiter, const std::vector<std::string>& strings);
std::string Join(const std::string& delimiter,
                 const std::vector<std::string>& strings);
std::string Join(char delimiter,
                 const std::string& str1, const std::string& str2);
std::string Join(const std::string& delimiter,
                 const std::string& str1, const std::string& str2);

// string_utils::ToString() is a helper function to convert any scalar type
// to a string. In most cases, it redirects the call to std::to_string with
// two exceptions: for std::string itself and for double and bool.
template<typename T>
inline std::string ToString(T value) { return std::to_string(value); }
// Having the following overload is handy for templates where the type
// of template parameter isn't known and could be a string itself.
inline std::string ToString(std::string value) { return value; }
// We overload this for double because std::to_string(double) uses %f to
// format the value and I would like to use a shorter %g format instead.
std::string ToString(double value);
// And the bool to be converted as true/false instead of 1/0.
std::string ToString(bool value);

}  // namespace string_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_STRING_UTILS_H_
