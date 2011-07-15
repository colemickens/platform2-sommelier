// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"
#include "shill/mock_modem_manager_proxy.h"
#include "shill/modem_manager.h"
#include "shill/proxy_factory.h"

using std::string;
using testing::_;
using testing::Return;
using testing::StrEq;
using testing::Test;

namespace shill {

class ModemManagerTest : public Test {
 public:
  ModemManagerTest()
      : manager_(&control_interface_, &dispatcher_, &glib_),
        modem_manager_(kService,
                       kPath,
                       &control_interface_,
                       &dispatcher_,
                       &manager_,
                       &glib_),
        proxy_factory_(&proxy_) {
    ProxyFactory::set_factory(&proxy_factory_);
  }

  virtual void TearDown() {
    modem_manager_.watcher_id_ = 0;
    ModemManagerProxyInterface *proxy = modem_manager_.proxy_.release();
    EXPECT_TRUE(proxy == NULL || proxy == &proxy_);
    ProxyFactory::set_factory(NULL);
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    TestProxyFactory(ModemManagerProxyInterface *proxy) : proxy_(proxy) {}

    virtual ModemManagerProxyInterface *CreateModemManagerProxy(
        ModemManager *manager,
        const string &path,
        const string &service) {
      return proxy_;
    }

   private:
    ModemManagerProxyInterface *proxy_;
  };

  static const char kService[];
  static const char kPath[];

  MockGLib glib_;
  MockControl control_interface_;
  EventDispatcher dispatcher_;
  Manager manager_;
  ModemManager modem_manager_;
  MockModemManagerProxy proxy_;
  TestProxyFactory proxy_factory_;
};

const char ModemManagerTest::kService[] = "test.dbus.service";
const char ModemManagerTest::kPath[] = "/dbus/service/test/path";

TEST_F(ModemManagerTest, Start) {
  const int kWatcher = 123;
  EXPECT_CALL(glib_, BusWatchName(G_BUS_TYPE_SYSTEM,
                                  StrEq(kService),
                                  G_BUS_NAME_WATCHER_FLAGS_NONE,
                                  ModemManager::OnAppear,
                                  ModemManager::OnVanish,
                                  &modem_manager_,
                                  NULL))
      .WillOnce(Return(kWatcher));
  EXPECT_EQ(0, modem_manager_.watcher_id_);
  modem_manager_.Start();
  EXPECT_EQ(kWatcher, modem_manager_.watcher_id_);
}

TEST_F(ModemManagerTest, Stop) {
  const int kWatcher = 345;
  static const char kOwner[] = ":1.30";
  modem_manager_.watcher_id_ = kWatcher;
  modem_manager_.owner_ = kOwner;
  EXPECT_CALL(glib_, BusUnwatchName(kWatcher)).Times(1);
  modem_manager_.Stop();
  EXPECT_EQ(0, modem_manager_.watcher_id_);
  EXPECT_EQ("", modem_manager_.owner_);
}

TEST_F(ModemManagerTest, Connect) {
  EXPECT_EQ("", modem_manager_.owner_);
  static const char kOwner[] = ":1.20";
  modem_manager_.Connect(kOwner);
  EXPECT_EQ(kOwner, modem_manager_.owner_);
}

TEST_F(ModemManagerTest, Disconnect) {
  static const char kOwner[] = ":1.22";
  modem_manager_.owner_ = kOwner;
  modem_manager_.Disconnect();
  EXPECT_EQ("", modem_manager_.owner_);
}

TEST_F(ModemManagerTest, OnAppear) {
  EXPECT_EQ("", modem_manager_.owner_);
  static const char kOwner[] = ":1.17";
  ModemManager::OnAppear(NULL, NULL, kOwner, &modem_manager_);
  EXPECT_EQ(kOwner, modem_manager_.owner_);
}

TEST_F(ModemManagerTest, OnVanish) {
  static const char kOwner[] = ":1.18";
  modem_manager_.owner_ = kOwner;
  ModemManager::OnVanish(NULL, NULL, &modem_manager_);
  EXPECT_EQ("", modem_manager_.owner_);
}

}  // namespace shill
