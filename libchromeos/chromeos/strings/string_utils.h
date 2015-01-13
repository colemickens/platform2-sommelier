// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_STRINGS_STRING_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_STRINGS_STRING_UTILS_H_

#include <string>
#include <utility>
#include <vector>

#include <chromeos/chromeos_export.h>

namespace chromeos {
namespace string_utils {

// Treats the string as a delimited list of substrings and returns the array
// of original elements of the list.
// |trim_whitespaces| causes each element to have all whitespaces trimmed off.
// |purge_empty_strings| specifies whether empty elements from the original
// string should be omitted.
CHROMEOS_EXPORT std::vector<std::string> Split(const std::string& str,
                                               char delimiter,
                                               bool trim_whitespaces,
                                               bool purge_empty_strings);
// Splits the string, trims all whitespaces, omits empty string parts.
inline std::vector<std::string> Split(const std::string& str, char delimiter) {
  return Split(str, delimiter, true, true);
}
// Splits the string, omits empty string parts.
inline std::vector<std::string> Split(const std::string& str,
                                      char delimiter,
                                      bool trim_whitespaces) {
  return Split(str, delimiter, trim_whitespaces, true);
}

// Splits the string into two pieces at the first position of the specified
// delimiter.
CHROMEOS_EXPORT std::pair<std::string, std::string>
SplitAtFirst(const std::string& str, char delimiter, bool trim_whitespaces);
// Splits the string into two pieces at the first position of the specified
// delimiter. Both parts have all whitespaces trimmed off.
inline std::pair<std::string, std::string> SplitAtFirst(const std::string& str,
                                                        char delimiter) {
  return SplitAtFirst(str, delimiter, true);
}

// The following overload returns false if the delimiter was not found in the
// source string. In this case, |left_part| will be set to |str| and
// |right_part| will be empty.
CHROMEOS_EXPORT bool SplitAtFirst(const std::string& str,
                                  char delimiter,
                                  std::string* left_part,
                                  std::string* right_part,
                                  bool trim_whitespaces);
// Always trims the white spaces in the split parts.
inline bool SplitAtFirst(const std::string& str,
                         char delimiter,
                         std::string* left_part,
                         std::string* right_part) {
  return SplitAtFirst(str, delimiter, left_part, right_part, true);
}

// Joins an array of strings into a single string separated by |delimiter|.
CHROMEOS_EXPORT std::string Join(char delimiter,
                                 const std::vector<std::string>& strings);
CHROMEOS_EXPORT std::string Join(const std::string& delimiter,
                                 const std::vector<std::string>& strings);
CHROMEOS_EXPORT std::string Join(char delimiter,
                                 const std::string& str1,
                                 const std::string& str2);
CHROMEOS_EXPORT std::string Join(const std::string& delimiter,
                                 const std::string& str1,
                                 const std::string& str2);

// string_utils::ToString() is a helper function to convert any scalar type
// to a string. In most cases, it redirects the call to std::to_string with
// two exceptions: for std::string itself and for double and bool.
template<typename T>
inline std::string ToString(T value) {
  return std::to_string(value);
}
// Having the following overload is handy for templates where the type
// of template parameter isn't known and could be a string itself.
inline std::string ToString(std::string value) {
  return value;
}
// We overload this for double because std::to_string(double) uses %f to
// format the value and I would like to use a shorter %g format instead.
CHROMEOS_EXPORT std::string ToString(double value);
// And the bool to be converted as true/false instead of 1/0.
CHROMEOS_EXPORT std::string ToString(bool value);

}  // namespace string_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_STRINGS_STRING_UTILS_H_
