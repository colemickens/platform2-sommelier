// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
