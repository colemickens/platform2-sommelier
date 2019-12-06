// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TRUNKS_MOCK_DBUS_OBJECT_PROXY_H_
#define TRUNKS_MOCK_DBUS_OBJECT_PROXY_H_

#include <base/bind.h>
#include <dbus/object_proxy.h>
#include <gmock/gmock.h>

namespace trunks {

class MockDBusObjectProxy : public dbus::ObjectProxy {
 public:
  MockDBusObjectProxy()
    : dbus::ObjectProxy(nullptr, "", dbus::ObjectPath(), 0) {}

  MOCK_METHOD1(WaitForServiceToBeAvailable,
               void(dbus::ObjectProxy::WaitForServiceToBeAvailableCallback));
  MOCK_METHOD1(SetNameOwnerChangedCallback,
               void(dbus::ObjectProxy::NameOwnerChangedCallback));
};

}  // namespace trunks

#endif  // TRUNKS_MOCK_DBUS_OBJECT_PROXY_H_
