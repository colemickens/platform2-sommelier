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

#ifndef SHILL_DBUS_PROPERTY_MATCHERS_H_
#define SHILL_DBUS_PROPERTY_MATCHERS_H_

#include <gmock/gmock.h>

#include "shill/dbus_properties.h"

MATCHER_P2(HasDBusPropertyWithValueU32, key, value, "") {
  shill::DBusPropertiesMap::const_iterator it = arg.find(key);
  return it != arg.end() && value == it->second.reader().get_uint32();
}

MATCHER_P2(HasDBusPropertyWithValueI32, key, value, "") {
  shill::DBusPropertiesMap::const_iterator it = arg.find(key);
  return it != arg.end() && value == it->second.reader().get_int32();
}

#endif  // SHILL_DBUS_PROPERTY_MATCHERS_H_
