// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_DATA_ENCODING__H_
#define BUFFET_DATA_ENCODING__H_

#include <vector>
#include <string>

namespace chromeos {
namespace data_encoding {

typedef std::vector<std::pair<std::string, std::string>> WebParamList;

// Encode/escape string to be used in the query portion of a URL.
// If |encodeSpaceAsPlus| is set to true, spaces are encoded as '+' instead
// of "%20"
std::string UrlEncode(char const* data, bool encodeSpaceAsPlus);

inline std::string UrlEncode(char const* data) {
  return UrlEncode(data, true);
}

// Decodes/unescapes a URL. Replaces all %XX sequences with actual characters.
// Also replaces '+' with spaces.
std::string UrlDecode(char const* data);

// Converts a list of key-value pairs into a string compatible with
// 'application/x-www-form-urlencoded' content encoding.
std::string WebParamsEncode(WebParamList const& params, bool encodeSpaceAsPlus);

inline std::string WebParamsEncode(WebParamList const& params) {
  return WebParamsEncode(params, true);
}

// Parses a string of '&'-delimited key-value pairs (separated by '=') and
// encoded in a way compatible with 'application/x-www-form-urlencoded'
// content encoding.
WebParamList WebParamsDecode(std::string const& data);

} // namespace data_encoding
} // namespace chromeos

#endif // BUFFET_DATA_ENCODING__H_
