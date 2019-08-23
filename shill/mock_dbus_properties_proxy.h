// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_DBUS_PROPERTIES_PROXY_H_
#define SHILL_MOCK_DBUS_PROPERTIES_PROXY_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/dbus_properties_proxy_interface.h"

namespace shill {

class MockDBusPropertiesProxy : public DBusPropertiesProxyInterface {
 public:
  MockDBusPropertiesProxy();
  ~MockDBusPropertiesProxy() override;

  MOCK_METHOD1(GetAll, KeyValueStore(const std::string& interface_name));
  MOCK_METHOD2(Get,
               brillo::Any(const std::string& interface_name,
                           const std::string& property));
  MOCK_METHOD1(set_properties_changed_callback,
               void(const PropertiesChangedCallback& callback));
  MOCK_METHOD1(set_modem_manager_properties_changed_callback,
               void(const ModemManagerPropertiesChangedCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDBusPropertiesProxy);
};

}  // namespace shill

#endif  // SHILL_MOCK_DBUS_PROPERTIES_PROXY_H_
