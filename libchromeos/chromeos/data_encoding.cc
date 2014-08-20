// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/data_encoding.h>

#include <base/strings/stringprintf.h>
#include <chromeos/string_utils.h>
#include <cstring>

namespace {

inline int HexToDec(int hex) {
  int dec = -1;
  if (hex >= '0' && hex <= '9') {
    dec = hex - '0';
  } else if (hex >= 'A' && hex <= 'F') {
    dec = hex - 'A' + 10;
  } else if (hex >= 'a' && hex <= 'f') {
    dec = hex - 'a' + 10;
  }
  return dec;
}

}  // namespace

/////////////////////////////////////////////////////////////////////////
namespace chromeos {
namespace data_encoding {

std::string UrlEncode(const char* data, bool encodeSpaceAsPlus) {
  std::string result;

  while (*data) {
    char c = *data++;
    // According to RFC3986 (http://www.faqs.org/rfcs/rfc3986.html),
    // section 2.3. - Unreserved Characters
    if ((c >= '0' && c <= '9') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') ||
        c == '-' || c == '.' || c == '_' || c == '~') {
      result += c;
    } else if (c == ' ' && encodeSpaceAsPlus) {
      // For historical reasons, some URLs have spaces encoded as '+',
      // this also applies to form data encoded as
      // 'application/x-www-form-urlencoded'
      result += '+';
    } else {
      base::StringAppendF(&result, "%%%02X",
                          static_cast<unsigned char>(c));  // Encode as %NN
    }
  }
  return result;
}

std::string UrlDecode(const char* data) {
  std::string result;
  while (*data) {
    char c = *data++;
    int part1 = 0, part2 = 0;
    // HexToDec would return -1 even for character 0 (end of string),
    // so it is safe to access data[0] and data[1] without overrunning the buf.
    if (c == '%' &&
        (part1 = HexToDec(data[0])) >= 0 && (part2 = HexToDec(data[1])) >= 0) {
      c = static_cast<char>((part1 << 4) | part2);
      data += 2;
    } else if (c == '+') {
      c = ' ';
    }
    result += c;
  }
  return result;
}

std::string WebParamsEncode(const WebParamList& params,
                            bool encodeSpaceAsPlus) {
  std::vector<std::string> pairs;
  pairs.reserve(params.size());
  for (const auto& p : params) {
    std::string key = UrlEncode(p.first.c_str(), encodeSpaceAsPlus);
    std::string value = UrlEncode(p.second.c_str(), encodeSpaceAsPlus);
    pairs.push_back(chromeos::string_utils::Join('=', key, value));
  }

  return chromeos::string_utils::Join('&', pairs);
}

WebParamList WebParamsDecode(const std::string& data) {
  WebParamList result;
  std::vector<std::string> params = chromeos::string_utils::Split(data, '&');
  for (const auto& p : params) {
    auto pair = chromeos::string_utils::SplitAtFirst(p, '=');
    result.emplace_back(UrlDecode(pair.first.c_str()),
                        UrlDecode(pair.second.c_str()));
  }
  return result;
}

}  // namespace data_encoding
}  // namespace chromeos
