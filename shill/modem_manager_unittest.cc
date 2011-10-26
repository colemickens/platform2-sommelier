// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/stl_util-inl.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>

#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_modem_manager_proxy.h"
#include "shill/modem.h"
#include "shill/modem_manager.h"
#include "shill/proxy_factory.h"

using std::string;
using std::vector;
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
                       &glib_,
                       NULL),
        proxy_(new MockModemManagerProxy()),
        proxy_factory_(this) {
  }

  virtual void SetUp();
  virtual void TearDown();

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(ModemManagerTest *test) : test_(test) {}

    virtual ModemManagerProxyInterface *CreateModemManagerProxy(
        ModemManager */*manager*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->proxy_.release();
    }

   private:
    ModemManagerTest *test_;
  };

  static const char kService[];
  static const char kPath[];
  static const char kOwner[];
  static const char kModemPath[];

  MockGLib glib_;
  MockControl control_interface_;
  EventDispatcher dispatcher_;
  MockManager manager_;
  ModemManager modem_manager_;
  scoped_ptr<MockModemManagerProxy> proxy_;
  TestProxyFactory proxy_factory_;
};

const char ModemManagerTest::kService[] = "org.chromium.ModemManager";
const char ModemManagerTest::kPath[] = "/org/chromium/ModemManager";
const char ModemManagerTest::kOwner[] = ":1.17";
const char ModemManagerTest::kModemPath[] = "/org/chromium/ModemManager/Gobi/0";

void ModemManagerTest::SetUp() {
  modem_manager_.proxy_factory_ = &proxy_factory_;
}

void ModemManagerTest::TearDown() {
  modem_manager_.watcher_id_ = 0;
  modem_manager_.proxy_factory_ = NULL;
}

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
  modem_manager_.watcher_id_ = kWatcher;
  modem_manager_.owner_ = kOwner;
  EXPECT_CALL(glib_, BusUnwatchName(kWatcher)).Times(1);
  modem_manager_.Stop();
  EXPECT_EQ(0, modem_manager_.watcher_id_);
  EXPECT_EQ("", modem_manager_.owner_);
}

TEST_F(ModemManagerTest, Connect) {
  EXPECT_EQ("", modem_manager_.owner_);
  EXPECT_CALL(*proxy_, EnumerateDevices())
      .WillOnce(Return(vector<DBus::Path>(1, kModemPath)));
  modem_manager_.Connect(kOwner);
  EXPECT_EQ(kOwner, modem_manager_.owner_);
  EXPECT_EQ(1, modem_manager_.modems_.size());
  ASSERT_TRUE(ContainsKey(modem_manager_.modems_, kModemPath));
  EXPECT_FALSE(modem_manager_.modems_[kModemPath]->task_factory_.empty());
}

TEST_F(ModemManagerTest, Disconnect) {
  modem_manager_.owner_ = kOwner;
  modem_manager_.Disconnect();
  EXPECT_EQ("", modem_manager_.owner_);
}

TEST_F(ModemManagerTest, OnAppear) {
  EXPECT_EQ("", modem_manager_.owner_);
  EXPECT_CALL(*proxy_, EnumerateDevices())
      .WillOnce(Return(vector<DBus::Path>()));
  ModemManager::OnAppear(NULL, kService, kOwner, &modem_manager_);
  EXPECT_EQ(kOwner, modem_manager_.owner_);
}

TEST_F(ModemManagerTest, OnVanish) {
  modem_manager_.owner_ = kOwner;
  ModemManager::OnVanish(NULL, kService, &modem_manager_);
  EXPECT_EQ("", modem_manager_.owner_);
}

TEST_F(ModemManagerTest, AddRemoveModem) {
  modem_manager_.owner_ = kOwner;
  modem_manager_.AddModem(kModemPath);
  EXPECT_EQ(1, modem_manager_.modems_.size());
  ASSERT_TRUE(ContainsKey(modem_manager_.modems_, kModemPath));
  EXPECT_FALSE(modem_manager_.modems_[kModemPath]->task_factory_.empty());

  modem_manager_.RemoveModem(kModemPath);
  EXPECT_EQ(0, modem_manager_.modems_.size());
}

}  // namespace shill
