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

}  // namespace system
}  // namespace power_manager
