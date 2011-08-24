// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/property_store_unittest.h"

#include <map>
#include <string>
#include <utility>
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

using std::make_pair;
using std::map;
using std::string;
using std::vector;
using ::testing::Values;

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
    DBusAdaptor::StringmapToVariant(Stringmap());
// static
const ::DBus::Variant PropertyStoreTest::kStringmapsV =
    DBusAdaptor::StringmapsToVariant(Stringmaps());
// static
const ::DBus::Variant PropertyStoreTest::kStringsV =
    DBusAdaptor::StringsToVariant(Strings(1, ""));
// static
const ::DBus::Variant PropertyStoreTest::kStrIntPairV =
    DBusAdaptor::StrIntPairToVariant(StrIntPair(make_pair("", ""),
                                                make_pair("", 12)));
// static
const ::DBus::Variant PropertyStoreTest::kUint16V =
    DBusAdaptor::Uint16ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kUint32V =
    DBusAdaptor::Uint32ToVariant(0);

PropertyStoreTest::PropertyStoreTest()
    : manager_(&control_interface_, &dispatcher_, &glib_),
      invalid_args_(Error::kErrorNames[Error::kInvalidArguments]),
      invalid_prop_(Error::kErrorNames[Error::kInvalidProperty]) {
}

PropertyStoreTest::~PropertyStoreTest() {}

TEST_P(PropertyStoreTest, TestProperty) {
  // Ensure that an attempt to write unknown properties returns InvalidProperty.
  PropertyStore store;
  ::DBus::Error error;
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&store, "", GetParam(), &error));
  EXPECT_EQ(invalid_prop_, error.name());
}

INSTANTIATE_TEST_CASE_P(
    PropertyStoreTestInstance,
    PropertyStoreTest,
    Values(PropertyStoreTest::kBoolV,
           PropertyStoreTest::kByteV,
           PropertyStoreTest::kStringV,
           PropertyStoreTest::kInt16V,
           PropertyStoreTest::kInt32V,
           PropertyStoreTest::kStringmapV,
           PropertyStoreTest::kStringmapsV,
           PropertyStoreTest::kStringsV,
           PropertyStoreTest::kStrIntPairV,
           PropertyStoreTest::kUint16V,
           PropertyStoreTest::kUint32V));

}  // namespace shill
