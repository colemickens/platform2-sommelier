// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/device_filter.h"

#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/values.h>

namespace soma {
namespace parser {

const char DevicePathFilter::kListKey[] = "device path filters";
const char DeviceNodeFilter::kListKey[] = "device node filters";

DevicePathFilter::DevicePathFilter(const base::FilePath& path) : filter_(path) {
}

// static
bool DevicePathFilter::Comp(const DevicePathFilter& a,
                            const DevicePathFilter& b) {
  return a.filter_ < b.filter_;
}

bool DevicePathFilter::Allows(const base::FilePath& rhs) const {
  return filter_ == rhs;
}

DevicePathFilterSet::DevicePathFilterSet()
    : std::set<DevicePathFilter, DevicePathFilter::Comparator>(
          &DevicePathFilter::Comp) {
}

// static
bool DevicePathFilterSet::Parse(const base::ListValue* filters,
                                DevicePathFilterSet* out) {
  DCHECK(out);
  std::string temp_filter_string;
  for (base::Value* filter : *filters) {
    if (!filter->GetAsString(&temp_filter_string)) {
      LOG(ERROR) << "Device path filters must be strings, not " << filter;
      return false;
    }
    out->insert(DevicePathFilter(base::FilePath(temp_filter_string)));
  }
  return true;
}

DeviceNodeFilter::DeviceNodeFilter(int major, int minor)
    : major_(major), minor_(minor) {
}

// static
bool DeviceNodeFilter::Comp(const DeviceNodeFilter& a,
                            const DeviceNodeFilter& b) {
  return a.major_ < b.major_ || (a.major_ == b.major_ && a.minor_ < b.minor_);
}

// TODO(cmasone): handle wildcarding in both major and minor.
bool DeviceNodeFilter::Allows(int major, int minor) const {
  return major_ == major && minor_ == minor;
}

DeviceNodeFilterSet::DeviceNodeFilterSet()
    : std::set<DeviceNodeFilter, DeviceNodeFilter::Comparator>(
          &DeviceNodeFilter::Comp) {
}

namespace {
// Helper function that parses a list of integer pairs.
std::vector<std::pair<int, int>> ParseIntegerPairs(
    const base::ListValue* filters) {
  std::vector<std::pair<int, int>> to_return;
  for (base::Value* filter : *filters) {
    base::ListValue* nested = nullptr;
    if (!(filter->GetAsList(&nested) && nested->GetSize() == 2)) {
      LOG(ERROR) << "Device node filter must be a list of 2 elements.";
      continue;
    }
    int major, minor;
    if (!nested->GetInteger(0, &major) || !nested->GetInteger(1, &minor)) {
      LOG(ERROR) << "Device node filter must contain 2 ints.";
      continue;
    }
    to_return.push_back(std::make_pair(major, minor));
  }
  return to_return;
}
}  // anonymous namespace

// static
bool DeviceNodeFilterSet::Parse(const base::ListValue* filters,
                                DeviceNodeFilterSet* out) {
  DCHECK(out);
  out->clear();
  if (filters->GetSize() == 0)
    return true;
  for (const auto& num_pair : ParseIntegerPairs(filters)) {
    out->insert(DeviceNodeFilter(num_pair.first, num_pair.second));
  }
  return !out->empty();
}

}  // namespace parser
}  // namespace soma
