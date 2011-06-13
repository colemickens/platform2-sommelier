// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_PROPERTY_STORE_UNITTEST_H_
#define SHILL_PROPERTY_STORE_UNITTEST_H_

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/property_store_interface.h"
#include "shill/shill_event.h"

namespace shill {

class PropertyStoreTest : public ::testing::Test {
 public:
  PropertyStoreTest()
      : bool_v_(DBusAdaptor::BoolToVariant(0)),
        byte_v_(DBusAdaptor::ByteToVariant(0)),
        uint16_v_(DBusAdaptor::Uint16ToVariant(0)),
        uint32_v_(DBusAdaptor::Uint32ToVariant(0)),
        int16_v_(DBusAdaptor::Int16ToVariant(0)),
        int32_v_(DBusAdaptor::Int32ToVariant(0)),
        string_v_(DBusAdaptor::StringToVariant("")),
        stringmap_v_(DBusAdaptor::StringmapToVariant(
            std::map<std::string, std::string>())),
        strings_v_(DBusAdaptor::StringsToVariant(
            std::vector<std::string>(1, ""))),
        manager_(&control_interface_, &dispatcher_),
        invalid_args_(Error::kErrorNames[Error::kInvalidArguments]),
        invalid_prop_(Error::kErrorNames[Error::kInvalidProperty]) {
  }

  virtual ~PropertyStoreTest() {}

 protected:
  ::DBus::Variant bool_v_;
  ::DBus::Variant byte_v_;
  ::DBus::Variant uint16_v_;
  ::DBus::Variant uint32_v_;
  ::DBus::Variant int16_v_;
  ::DBus::Variant int32_v_;
  ::DBus::Variant string_v_;
  ::DBus::Variant stringmap_v_;
  ::DBus::Variant strings_v_;

  MockControl control_interface_;
  EventDispatcher dispatcher_;
  Manager manager_;
  std::string invalid_args_;
  std::string invalid_prop_;
};

}  // namespace shill
#endif  // SHILL_PROPERTY_STORE_UNITTEST_H_
