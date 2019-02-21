// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cromo/cromo_server.h"

#include <mm/mm-modem.h>

#include <base/logging.h>
#include <dbus-c++/glib-integration.h>
#include <gtest/gtest.h>

#include "cromo/carrier.h"

TEST(Carrier, Find) {
  DBus::Glib::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  dispatcher.attach(nullptr);

  DBus::Connection conn = DBus::Connection::SystemBus();

  std::string service_name =
      std::string(CromoServer::kServiceName) + ".UnitTest";
  ASSERT_TRUE(conn.acquire_name(service_name.c_str()))
      << "Failed to acquire D-Bus name " << service_name;

  static const char kTestName[] = "test_carrier";

  CromoServer* server = new CromoServer(conn);

  Carrier* not_found = server->FindCarrierByName(kTestName);
  EXPECT_EQ(nullptr, not_found);
  not_found = server->FindCarrierByCarrierId(38747);
  EXPECT_EQ(nullptr, not_found);

  Carrier* test_carrier = new Carrier(
      kTestName, "dir", 17, MM_MODEM_TYPE_CDMA, Carrier::kNone, "activation");

  server->AddCarrier(test_carrier);

  Carrier* by_id = server->FindCarrierByCarrierId(17);
  EXPECT_EQ(test_carrier, by_id);

  Carrier* by_name = server->FindCarrierByName(kTestName);
  EXPECT_EQ(test_carrier, by_name);

  // Testing carrier ctor, accessors
  EXPECT_STREQ(kTestName, by_name->name());
  EXPECT_STREQ("dir", by_name->firmware_directory());
  EXPECT_EQ(MM_MODEM_TYPE_CDMA, by_name->carrier_type());
  EXPECT_EQ(Carrier::kNone, by_name->activation_method());
  EXPECT_STREQ("activation", by_name->activation_code());
}
