// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/stl_util-inl.h>
#include <gtest/gtest.h>
#include <mobile_provider.h>

#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/modem_info.h"
#include "shill/modem_manager.h"

using testing::_;
using testing::Return;
using testing::Test;

namespace shill {

class ModemInfoTest : public Test {
 public:
  ModemInfoTest()
      : manager_(&control_interface_, &dispatcher_, &glib_),
        modem_info_(&control_interface_, &dispatcher_, &manager_, &glib_) {}

 protected:
  static const char kTestMobileProviderDBPath[];

  MockGLib glib_;
  MockControl control_interface_;
  EventDispatcher dispatcher_;
  MockManager manager_;
  ModemInfo modem_info_;
};

const char ModemInfoTest::kTestMobileProviderDBPath[] =
    "provider_db_unittest.bfd";

TEST_F(ModemInfoTest, StartStop) {
  const int kWatcher1 = 123;
  const int kWatcher2 = 456;
  EXPECT_EQ(0, modem_info_.modem_managers_.size());
  EXPECT_CALL(glib_, BusWatchName(_, _, _, _, _, _, _))
      .WillOnce(Return(kWatcher1))
      .WillOnce(Return(kWatcher2));
  modem_info_.provider_db_path_ = kTestMobileProviderDBPath;
  modem_info_.Start();
  EXPECT_EQ(2, modem_info_.modem_managers_.size());
  EXPECT_TRUE(modem_info_.provider_db_);
  EXPECT_TRUE(mobile_provider_lookup_by_name(modem_info_.provider_db_, "AT&T"));
  EXPECT_FALSE(mobile_provider_lookup_by_name(modem_info_.provider_db_, "xyz"));

  EXPECT_CALL(glib_, BusUnwatchName(kWatcher1)).Times(1);
  EXPECT_CALL(glib_, BusUnwatchName(kWatcher2)).Times(1);
  modem_info_.Stop();
  EXPECT_EQ(0, modem_info_.modem_managers_.size());
  EXPECT_FALSE(modem_info_.provider_db_);
}

TEST_F(ModemInfoTest, RegisterModemManager) {
  const int kWatcher = 123;
  static const char kService[] = "some.dbus.service";
  EXPECT_CALL(glib_, BusWatchName(_, _, _, _, _, _, _))
      .WillOnce(Return(kWatcher));
  // Passes ownership of the database.
  modem_info_.provider_db_ = mobile_provider_open_db(kTestMobileProviderDBPath);
  EXPECT_TRUE(modem_info_.provider_db_);
  modem_info_.RegisterModemManager(kService, "/dbus/service/path");
  ASSERT_EQ(1, modem_info_.modem_managers_.size());
  ModemManager *manager = modem_info_.modem_managers_[0];
  EXPECT_EQ(kService, manager->service_);
  EXPECT_EQ(kWatcher, manager->watcher_id_);
  EXPECT_EQ(modem_info_.provider_db_, manager->provider_db_);
  manager->watcher_id_ = 0;
}

}  // namespace shill
