// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DBUS_MANAGER_H_
#define SHILL_MOCK_DBUS_MANAGER_H_

#include "shill/dbus_manager.h"

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

namespace shill {

class MockDBusManager : public DBusManager {
 public:
  MockDBusManager();
  ~MockDBusManager() override;

  MOCK_METHOD3(CreateNameWatcher,
      DBusNameWatcher* (
          const std::string& name,
          const DBusNameWatcher::NameAppearedCallback& name_appeared_callback,
          const DBusNameWatcher::NameVanishedCallback& name_vanished_callback));
  MOCK_METHOD1(RemoveNameWatcher, void(DBusNameWatcher* name_watcher));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDBusManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_DBUS_MANAGER_H_
