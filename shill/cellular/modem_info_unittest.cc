// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/modem_info.h"

#include <base/stl_util.h>
#include <gtest/gtest.h>

#include "shill/cellular/modem_manager.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_dbus_service_proxy.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"

using testing::_;
using testing::Return;
using testing::Test;

namespace shill {

class ModemInfoTest : public Test {
 public:
  ModemInfoTest()
      : metrics_(&dispatcher_),
        manager_(&control_interface_, &dispatcher_, &metrics_, &glib_),
        dbus_service_proxy_(nullptr),
        modem_info_(&control_interface_, &dispatcher_, &metrics_, &manager_,
                    &glib_) {}

  virtual void SetUp() {
    manager_.dbus_manager_.reset(new DBusManager());
    dbus_service_proxy_ = new MockDBusServiceProxy();
    // Ownership  of |dbus_service_proxy_| is transferred to
    // |manager_.dbus_manager_|.
    manager_.dbus_manager_->proxy_.reset(dbus_service_proxy_);
  }

 protected:
  MockGLib glib_;
  MockControl control_interface_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockManager manager_;
  MockDBusServiceProxy *dbus_service_proxy_;
  ModemInfo modem_info_;
};

TEST_F(ModemInfoTest, StartStop) {
  EXPECT_EQ(0, modem_info_.modem_managers_.size());
  EXPECT_CALL(*dbus_service_proxy_,
              GetNameOwner("org.chromium.ModemManager", _, _, _));
  EXPECT_CALL(*dbus_service_proxy_,
              GetNameOwner("org.freedesktop.ModemManager1", _, _, _));
  modem_info_.Start();
  EXPECT_EQ(2, modem_info_.modem_managers_.size());
  modem_info_.Stop();
  EXPECT_EQ(0, modem_info_.modem_managers_.size());
}

TEST_F(ModemInfoTest, RegisterModemManager) {
  static const char kService[] = "some.dbus.service";
  EXPECT_CALL(*dbus_service_proxy_, GetNameOwner(kService, _, _, _));
  modem_info_.RegisterModemManager(
      new ModemManagerClassic(kService, "/dbus/service/path", &modem_info_));
  ASSERT_EQ(1, modem_info_.modem_managers_.size());
  ModemManager *manager = modem_info_.modem_managers_[0];
  EXPECT_EQ(kService, manager->service_);
  EXPECT_EQ(&modem_info_, manager->modem_info_);
}

}  // namespace shill
