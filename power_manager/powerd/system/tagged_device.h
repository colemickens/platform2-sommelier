// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_TAGGED_DEVICE_H_
#define POWER_MANAGER_POWERD_SYSTEM_TAGGED_DEVICE_H_

#include <string>
#include <unordered_set>

namespace power_manager {
namespace system {

// Represents a udev device with powerd tags associated to it.
class TaggedDevice {
 public:
  // Default constructor for easier use with std::map.
  TaggedDevice();
  TaggedDevice(const std::string& syspath, const std::string& tags);
  ~TaggedDevice();

  const std::string& syspath() const { return syspath_; }
  const std::unordered_set<std::string> tags() const { return tags_; }

  // Returns true if the device has the given tag.
  bool HasTag(const std::string& tag) const;

 private:
  std::string syspath_;
  std::unordered_set<std::string> tags_;
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_TAGGED_DEVICE_H_
