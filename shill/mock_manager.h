// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MANAGER_
#define SHILL_MOCK_MANAGER_

#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>

#include "shill/manager.h"

namespace shill {

class ControlInterface;
class DeviceInfo;
class EventDispatcher;
class GLib;
class PropertyStore;

class MockManager : public Manager {
 public:
  MockManager(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
              GLib *glib)
      : Manager(control_interface, dispatcher, glib, "", "", "") {
  }
  ~MockManager() {}

  MOCK_METHOD0(device_info, DeviceInfo*(void));
  MOCK_METHOD0(store, PropertyStore*(void));
  MOCK_METHOD1(UpdateService, void(const ServiceConstRefPtr &to_update));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_MANAGER_
