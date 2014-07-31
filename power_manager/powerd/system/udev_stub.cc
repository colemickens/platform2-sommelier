// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/udev_stub.h"

namespace power_manager {
namespace system {

UdevStub::UdevStub() {}

UdevStub::~UdevStub() {}

bool UdevStub::HasObserver(const std::string& subsystem,
                           UdevObserver* observer) const {
  ObserverMap::const_iterator it = observers_.find(subsystem);
  return it != observers_.end() && it->second.count(observer);
}

void UdevStub::AddObserver(const std::string& subsystem,
                           UdevObserver* observer) {
  observers_[subsystem].insert(observer);
}

void UdevStub::RemoveObserver(const std::string& subsystem,
                              UdevObserver* observer) {
  observers_[subsystem].erase(observer);
}

bool UdevStub::GetSysattr(const std::string& syspath,
                          const std::string& sysattr,
                          std::string* value) {
  SysattrMap::iterator it = map_.find(std::make_pair(syspath, sysattr));
  if (it == map_.end())
    return false;
  *value = it->second;
  return true;
}

bool UdevStub::SetSysattr(const std::string& syspath,
                          const std::string& sysattr,
                          const std::string& value) {
  // Allows arbitrary attributes to be created using SetSysattr, which differs
  // from UdevSysattr. For all reasonable testing scenarios, this should be
  // fine.
  map_[std::make_pair(syspath, sysattr)] = value;
  return true;
}

bool UdevStub::FindParentWithSysattr(const std::string& syspath,
                                     const std::string& sysattr,
                                     std::string* parent_syspath) {
  *parent_syspath = syspath;
  return true;
}

}  // namespace system
}  // namespace power_manager
