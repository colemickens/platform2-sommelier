// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/property_store_unittest.h"

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
#include "shill/property_store.h"
#include "shill/shill_event.h"

using std::string;
using std::map;
using std::vector;

namespace shill {

// static
const ::DBus::Variant PropertyStoreTest::kBoolV = DBusAdaptor::BoolToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kByteV = DBusAdaptor::ByteToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kInt16V =
    DBusAdaptor::Int16ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kInt32V =
    DBusAdaptor::Int32ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kStringV =
    DBusAdaptor::StringToVariant("");
// static
const ::DBus::Variant PropertyStoreTest::kStringmapV =
    DBusAdaptor::StringmapToVariant(map<string, string>());
// static
const ::DBus::Variant PropertyStoreTest::kStringmapsV =
    DBusAdaptor::StringmapsToVariant(vector<map<string, string> >());
// static
const ::DBus::Variant PropertyStoreTest::kStringsV =
    DBusAdaptor::StringsToVariant(vector<string>(1, ""));
// static
const ::DBus::Variant PropertyStoreTest::kUint16V =
    DBusAdaptor::Uint16ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kUint32V =
    DBusAdaptor::Uint32ToVariant(0);

PropertyStoreTest::PropertyStoreTest()
    : manager_(&control_interface_, &dispatcher_),
      invalid_args_(Error::kErrorNames[Error::kInvalidArguments]),
      invalid_prop_(Error::kErrorNames[Error::kInvalidProperty]) {
}

PropertyStoreTest::~PropertyStoreTest() {}

}  // namespace shill
