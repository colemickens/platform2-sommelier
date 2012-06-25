// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DBUS_MANAGER_H_
#define SHILL_MOCK_DBUS_MANAGER_H_

#include <gmock/gmock.h>

#include "shill/dbus_manager.h"

namespace shill {

class MockDBusManager : public DBusManager {
 public:
  MockDBusManager();
  virtual ~MockDBusManager();

  MOCK_METHOD3(WatchName, void(const std::string &name,
                               const AppearedCallback &on_appear,
                               const VanishedCallback &on_vanish));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDBusManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_DBUS_MANAGER_H_
