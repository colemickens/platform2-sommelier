// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_COMPLETE_MOCK_OBJECT_PROXY_H_
#define PEERD_COMPLETE_MOCK_OBJECT_PROXY_H_

#include "dbus/mock_object_proxy.h"

namespace peerd {

// Like MockObjectProxy, but with more mocking.
class CompleteMockObjectProxy : public dbus::MockObjectProxy {
 public:
  using MockObjectProxy::MockObjectProxy;
  MOCK_METHOD1(WaitForServiceToBeAvailable,
               void(ObjectProxy::WaitForServiceToBeAvailableCallback));
};

}  // namespace peerd

#endif  // PEERD_COMPLETE_MOCK_OBJECT_PROXY_H_
