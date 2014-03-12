// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_UDEV_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_UDEV_STUB_H_

#include <map>
#include <set>
#include <string>

#include <base/basictypes.h>
#include <base/compiler_specific.h>

#include "power_manager/powerd/system/udev.h"

namespace power_manager {
namespace system {

// Stub implementation of UdevInterface for use in tests.
class UdevStub : public UdevInterface {
 public:
  UdevStub();
  virtual ~UdevStub();

  // Returns true if |observer| is registered for |subsystem|.
  bool HasObserver(const std::string& subsystem, UdevObserver* observer) const;

  // UdevInterface implementation:
  virtual void AddObserver(const std::string& subsystem,
                           UdevObserver* observer) OVERRIDE;
  virtual void RemoveObserver(const std::string& subsystem,
                              UdevObserver* observer) OVERRIDE;

 private:
  // Registered observers keyed by subsystem.
  typedef std::map<std::string, std::set<UdevObserver*> > ObserverMap;
  ObserverMap observers_;

  DISALLOW_COPY_AND_ASSIGN(UdevStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_UDEV_H_
