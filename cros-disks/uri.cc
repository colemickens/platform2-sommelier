// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/uri.h"

#include <base/logging.h>

namespace cros_disks {

namespace {

const char kUriDelimiter[] = "://";

}  // namespace

Uri::Uri(const std::string& scheme, const std::string& path)
    : scheme_(scheme), path_(path) {}

std::string Uri::value() const {
  return scheme_ + kUriDelimiter + path_;
}

bool Uri::IsUri(const std::string& s) {
  size_t pos = s.find(kUriDelimiter);
  if (pos == std::string::npos || pos == 0)
    return false;
  // RFC 3986, section 3.1
  if (!isalpha(s[0]))
    return false;
  for (auto c : s.substr(0, pos)) {
    if (!isalnum(c) && c != '-' && c != '+' && c != '.')
      return false;
  }
  return true;
}

Uri Uri::Parse(const std::string& s) {
  CHECK(IsUri(s));
  size_t pos = s.find(kUriDelimiter);
  return Uri(s.substr(0, pos), s.substr(pos + strlen(kUriDelimiter)));
}

}  // namespace cros_disks
