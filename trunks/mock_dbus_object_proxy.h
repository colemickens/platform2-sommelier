//
// Copyright (C) 2017 The Android Open Source Project
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
