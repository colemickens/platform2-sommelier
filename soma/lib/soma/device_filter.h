// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_DEVICE_FILTER_H_
#define SOMA_LIB_SOMA_DEVICE_FILTER_H_

#include <set>

#include <base/files/file_path.h>

namespace base {
class ListValue;
}  // namespace base

namespace soma {
namespace parser {

// NB: These are copyable and assignable!
class DevicePathFilter {
 public:
  struct Comparator {
    bool operator()(const DevicePathFilter& a, const DevicePathFilter& b);
  };
  using Set = std::set<DevicePathFilter, Comparator>;

  explicit DevicePathFilter(const base::FilePath& path);
  DevicePathFilter(const DevicePathFilter& that);
  virtual ~DevicePathFilter();
  DevicePathFilter& operator=(const DevicePathFilter& that);

  bool Allows(const base::FilePath& rhs) const;
  const base::FilePath& filter() const { return filter_; }

  // Returns true if |filters| can be successfully parsed into |out|.
  // False is returned on failure and |out| may be in an inconsistent state.
  static bool ParseList(const base::ListValue& filters, Set* out);

 private:
  base::FilePath filter_;
  // Take care when adding more data members, as this class allows copy/assign.
};

// NB: These are copyable and assignable!
class DeviceNodeFilter {
 public:
  struct Comparator {
    bool operator()(const DeviceNodeFilter& a, const DeviceNodeFilter& b);
  };
  using Set = std::set<DeviceNodeFilter, Comparator>;

  DeviceNodeFilter(int major, int minor);
  DeviceNodeFilter(const DeviceNodeFilter& that) = default;
  virtual ~DeviceNodeFilter() = default;
  DeviceNodeFilter& operator=(const DeviceNodeFilter& that) = default;

  bool Allows(int major, int minor) const;
  int major() const { return major_; }
  int minor() const { return minor_; }

  // Returns true if |filters| can be successfully parsed into |out|.
  // False is returned on failure and |out| may be in an inconsistent state.
  static bool ParseList(const base::ListValue& filters, Set* out);

 private:
  int major_;
  int minor_;
  // Take care when adding more data members, as this class allows copy/assign.
};

}  // namespace parser
}  // namespace soma
#endif  // SOMA_LIB_SOMA_DEVICE_FILTER_H_
