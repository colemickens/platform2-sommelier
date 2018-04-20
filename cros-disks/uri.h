// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_URI_H_
#define CROS_DISKS_URI_H_

#include <string>

namespace cros_disks {

// Wrapper for string representing URI. By no mean it's a complete
// implementation of what should be in such class, just to group some
// related utilities.
class Uri {
 public:
  Uri(const std::string& scheme, const std::string& path);

  bool operator==(const Uri& other) const { return value() == other.value(); }

  std::string value() const;
  const std::string& scheme() const { return scheme_; }
  const std::string& path() const { return path_; }

  // Returns true if the given string is URI, i.e. <scheme>://[something].
  // It checks only the scheme part and doesn't verify validity of the path.
  static bool IsUri(const std::string& s);

  static Uri Parse(const std::string& s);

 private:
  std::string scheme_;
  std::string path_;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_URI_H_
