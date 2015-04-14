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

const char DevicePathFilter::kListKey[] = "os/bruteus/device-path-filter-set";
const char DeviceNodeFilter::kListKey[] = "os/bruteus/device-node-filter-set";

DevicePathFilter::DevicePathFilter(const base::FilePath& path) : filter_(path) {
}

DevicePathFilter::DevicePathFilter(const DevicePathFilter& that) = default;

DevicePathFilter::~DevicePathFilter() = default;

DevicePathFilter& DevicePathFilter::operator=(const DevicePathFilter& that) =
  default;

bool DevicePathFilter::Comparator::operator()(const DevicePathFilter& a,
                                              const DevicePathFilter& b) {
  return a.filter_ < b.filter_;
}

bool DevicePathFilter::Allows(const base::FilePath& rhs) const {
  return filter_ == rhs;
}

// static
bool DevicePathFilter::ParseList(const base::ListValue& filters,
                                 DevicePathFilter::Set* out) {
  DCHECK(out);
  std::string temp_filter_string;
  for (base::Value* filter : filters) {
    if (!filter->GetAsString(&temp_filter_string)) {
      LOG(ERROR) << "Device path filters must be strings, not " << filter;
      return false;
    }
    out->emplace(base::FilePath(temp_filter_string));
  }
  return true;
}

DeviceNodeFilter::DeviceNodeFilter(int major, int minor)
    : major_(major), minor_(minor) {
}

bool DeviceNodeFilter::Comparator::operator()(const DeviceNodeFilter& a,
                                              const DeviceNodeFilter& b) {
  return a.major_ < b.major_ || (a.major_ == b.major_ && a.minor_ < b.minor_);
}

// TODO(cmasone): handle wildcarding in both major and minor.
bool DeviceNodeFilter::Allows(int major, int minor) const {
  return major_ == major && minor_ == minor;
}

namespace {
// Helper function that parses a list of integer pairs.
std::vector<std::pair<int, int>> ParseIntegerPairs(
    const base::ListValue& filters) {
  std::vector<std::pair<int, int>> to_return;
  for (base::Value* filter : filters) {
    base::DictionaryValue* nested = nullptr;
    if (!(filter->GetAsDictionary(&nested))) {
      LOG(ERROR) << "Device node filter must be a dictionary.";
      continue;
    }
    int major, minor;
    if (!nested->GetInteger("major", &major) ||
        !nested->GetInteger("minor", &minor)) {
      LOG(ERROR) << "Device node filter must contain 2 ints.";
      continue;
    }
    to_return.push_back(std::make_pair(major, minor));
  }
  return to_return;
}
}  // anonymous namespace

// static
bool DeviceNodeFilter::ParseList(const base::ListValue& filters,
                                 DeviceNodeFilter::Set* out) {
  DCHECK(out);
  out->clear();
  if (filters.GetSize() == 0)
    return true;
  for (const auto& num_pair : ParseIntegerPairs(filters)) {
    out->emplace(num_pair.first, num_pair.second);
  }
  return !out->empty();
}

}  // namespace parser
}  // namespace soma
