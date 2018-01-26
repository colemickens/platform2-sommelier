// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_NEWBLUED_COMPLETE_MOCK_OBJECT_PROXY_H_
#define BLUETOOTH_NEWBLUED_COMPLETE_MOCK_OBJECT_PROXY_H_

#include "dbus/mock_object_proxy.h"

namespace bluetooth {

// Like MockObjectProxy, but with more mocking.
class CompleteMockObjectProxy : public dbus::MockObjectProxy {
 public:
  using MockObjectProxy::MockObjectProxy;
  MOCK_METHOD1(WaitForServiceToBeAvailable,
               void(ObjectProxy::WaitForServiceToBeAvailableCallback));
  MOCK_METHOD1(SetNameOwnerChangedCallback,
               void(ObjectProxy::NameOwnerChangedCallback));
};

}  // namespace bluetooth

#endif  // BLUETOOTH_NEWBLUED_COMPLETE_MOCK_OBJECT_PROXY_H_
