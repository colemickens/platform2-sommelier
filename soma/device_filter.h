// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_DEVICE_FILTER_H_
#define SOMA_DEVICE_FILTER_H_

#include <set>

#include <base/files/file_path.h>

namespace base {
class ListValue;
}  // namespace base

namespace soma {

// NB: These are copyable and assignable!
class DevicePathFilter {
 public:
  static const char kListKey[];

  explicit DevicePathFilter(const base::FilePath& path);
  DevicePathFilter(const DevicePathFilter& that) = default;
  virtual ~DevicePathFilter() = default;
  DevicePathFilter& operator=(const DevicePathFilter& that) = default;

  bool Allows(const base::FilePath& rhs) const;

 private:
  friend class DevicePathFilterSet;
  // Used to support DevicePathFilterSet.
  using Comparator = bool(*)(const DevicePathFilter&, const DevicePathFilter&);
  static bool Comp(const DevicePathFilter& a, const DevicePathFilter& b);

  const base::FilePath filter_;
  // Take care when adding more data members, as this class allows copy/assign.
};

class DevicePathFilterSet
    : public std::set<DevicePathFilter, DevicePathFilter::Comparator> {
 public:
  DevicePathFilterSet();
  virtual ~DevicePathFilterSet() = default;
};

DevicePathFilterSet ParseDevicePathFilters(base::ListValue* filters);

// NB: These are copyable and assignable!
class DeviceNodeFilter{
 public:
  static const char kListKey[];

  DeviceNodeFilter(int major, int minor);
  DeviceNodeFilter(const DeviceNodeFilter& that) = default;
  virtual ~DeviceNodeFilter() = default;
  DeviceNodeFilter& operator=(const DeviceNodeFilter& that) = default;

  bool Allows(int major, int minor) const;

 private:
  friend class DeviceNodeFilterSet;
  // Used to support DeviceNodeFilterSet.
  using Comparator = bool(*)(const DeviceNodeFilter&, const DeviceNodeFilter&);
  static bool Comp(const DeviceNodeFilter& a, const DeviceNodeFilter& b);

  int major_;
  int minor_;
  // Take care when adding more data members, as this class allows copy/assign.
};

class DeviceNodeFilterSet
    : public std::set<DeviceNodeFilter, DeviceNodeFilter::Comparator> {
 public:
  DeviceNodeFilterSet();
  virtual ~DeviceNodeFilterSet() = default;
};

DeviceNodeFilterSet ParseDeviceNodeFilters(base::ListValue* filters);

}  // namespace soma
#endif  // SOMA_DEVICE_FILTER_H_
