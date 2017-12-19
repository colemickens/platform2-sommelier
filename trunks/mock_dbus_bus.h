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

#ifndef TRUNKS_MOCK_DBUS_H_
#define TRUNKS_MOCK_DBUS_H_

#include <string>

#include <dbus/bus.h>
#include <gmock/gmock.h>

namespace trunks {

class MockDBusBus : public dbus::Bus {
 public:
  MockDBusBus() : dbus::Bus(dbus::Bus::Options()) {}

  MOCK_METHOD0(Connect, bool());
  MOCK_METHOD0(ShutdownAndBlock, void());
  MOCK_METHOD2(GetServiceOwnerAndBlock,
               std::string(const std::string&,
                           dbus::Bus::GetServiceOwnerOption));
  MOCK_METHOD2(GetObjectProxy,
               dbus::ObjectProxy*(const std::string&, const dbus::ObjectPath&));
};

}  // namespace trunks

#endif  // TRUNKS_MOCK_DBUS_H_
