// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_UDEV_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_UDEV_STUB_H_

#include <map>
#include <set>
#include <string>
#include <utility>

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
  void AddObserver(const std::string& subsystem,
                   UdevObserver* observer) override;
  void RemoveObserver(const std::string& subsystem,
                      UdevObserver* observer) override;
  bool GetSysattr(const std::string& syspath,
                  const std::string& sysattr,
                  std::string* value) override;
  bool SetSysattr(const std::string& syspath,
                  const std::string& sysattr,
                  const std::string& value) override;
  bool FindParentWithSysattr(const std::string& syspath,
                             const std::string& sysattr,
                             std::string* parent_syspath) override;

 private:
  // Registered observers keyed by subsystem.
  typedef std::map<std::string, std::set<UdevObserver*> > ObserverMap;
  ObserverMap observers_;

  // Maps a pair (device syspath, sysattr name) to the corresponding sysattr
  // value.
  typedef std::map<std::pair<std::string, std::string>, std::string> SysattrMap;
  SysattrMap map_;

  DISALLOW_COPY_AND_ASSIGN(UdevStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_UDEV_STUB_H_
