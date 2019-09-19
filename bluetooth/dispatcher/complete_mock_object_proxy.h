// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_COMPLETE_MOCK_OBJECT_PROXY_H_
#define BLUETOOTH_DISPATCHER_COMPLETE_MOCK_OBJECT_PROXY_H_

#include "dbus/mock_object_proxy.h"

namespace bluetooth {

// Like MockObjectProxy, but with more mocking.
class CompleteMockObjectProxy : public dbus::MockObjectProxy {
 public:
  using MockObjectProxy::MockObjectProxy;
  MOCK_METHOD(void,
              WaitForServiceToBeAvailable,
              (ObjectProxy::WaitForServiceToBeAvailableCallback),
              (override));
  MOCK_METHOD(void,
              SetNameOwnerChangedCallback,
              (ObjectProxy::NameOwnerChangedCallback),
              (override));
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_COMPLETE_MOCK_OBJECT_PROXY_H_
