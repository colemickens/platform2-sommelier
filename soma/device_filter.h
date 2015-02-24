// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_DEVICE_FILTER_H_
#define SOMA_DEVICE_FILTER_H_

#include <base/files/file_path.h>

namespace soma {

class DevicePathFilter {
 public:
  // Will be useful if I put these in a std::set<>, which I might.
  using Comparator = bool(*)(const DevicePathFilter&, const DevicePathFilter&);
  static bool Comp(const DevicePathFilter& a, const DevicePathFilter& b) {
    return a.Precedes(b);
  }

  explicit DevicePathFilter(const base::FilePath& path);
  virtual ~DevicePathFilter();

  bool Precedes(const DevicePathFilter& rhs) const {
    return filter_.value() < rhs.filter_.value();
  }

  bool Allows(const base::FilePath& rhs) const {
    return filter_.value() == rhs.value();
  }

 private:
  const base::FilePath filter_;
  DISALLOW_COPY_AND_ASSIGN(DevicePathFilter);
};

class DeviceNodeFilter{
 public:
  // Will be useful if I put these in a std::set<>, which I might.
  using Comparator = bool(*)(const DeviceNodeFilter&, const DeviceNodeFilter&);
  static bool Comp(const DeviceNodeFilter& a, const DeviceNodeFilter& b) {
    return a.Precedes(b);
  }

  DeviceNodeFilter(int major, int minor);
  virtual ~DeviceNodeFilter();

  bool Precedes(const DeviceNodeFilter& rhs) const {
    return major_ < rhs.major_ || (major_ == rhs.major_ && minor_ < rhs.minor_);
  }

  // TODO(cmasone): handle wildcarding in both major and minor.
  bool Allows(int major, int minor) const {
    return major_ == major && minor_ == minor;
  }

 private:
  int major_;
  int minor_;
  DISALLOW_COPY_AND_ASSIGN(DeviceNodeFilter);
};

}  // namespace soma
#endif  // SOMA_DEVICE_FILTER_H_
