// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_DATA_ENCODING_H_
#define LIBCHROMEOS_CHROMEOS_DATA_ENCODING_H_

#include <string>
#include <utility>
#include <vector>

namespace chromeos {
namespace data_encoding {

typedef std::vector<std::pair<std::string, std::string>> WebParamList;

// Encode/escape string to be used in the query portion of a URL.
// If |encodeSpaceAsPlus| is set to true, spaces are encoded as '+' instead
// of "%20"
std::string UrlEncode(const char* data, bool encodeSpaceAsPlus);

inline std::string UrlEncode(const char* data) {
  return UrlEncode(data, true);
}

// Decodes/unescapes a URL. Replaces all %XX sequences with actual characters.
// Also replaces '+' with spaces.
std::string UrlDecode(const char* data);

// Converts a list of key-value pairs into a string compatible with
// 'application/x-www-form-urlencoded' content encoding.
std::string WebParamsEncode(const WebParamList& params, bool encodeSpaceAsPlus);

inline std::string WebParamsEncode(const WebParamList& params) {
  return WebParamsEncode(params, true);
}

// Parses a string of '&'-delimited key-value pairs (separated by '=') and
// encoded in a way compatible with 'application/x-www-form-urlencoded'
// content encoding.
WebParamList WebParamsDecode(const std::string& data);

}  // namespace data_encoding
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DATA_ENCODING_H_
