// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DBUS_SERVICE_PROXY_H_
#define SHILL_MOCK_DBUS_SERVICE_PROXY_H_

#include <string>

#include <gmock/gmock.h>

#include "shill/dbus_service_proxy_interface.h"

namespace shill {

class MockDBusServiceProxy : public DBusServiceProxyInterface {
 public:
  MockDBusServiceProxy();
  ~MockDBusServiceProxy() override;

  MOCK_METHOD4(GetNameOwner, void(const std::string& name,
                                  Error* error,
                                  const StringCallback& callback,
                                  int timeout));

  MOCK_METHOD1(set_name_owner_changed_callback,
               void(const NameOwnerChangedCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDBusServiceProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_DBUS_SERVICE_PROXY_H_
