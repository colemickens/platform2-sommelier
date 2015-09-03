//
// Copyright (C) 2012 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
