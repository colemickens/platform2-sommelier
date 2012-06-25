// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_manager.h"

#include <base/bind.h>
#include <base/memory/weak_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/mock_dbus_service_proxy.h"
#include "shill/proxy_factory.h"

using base::Bind;
using std::string;
using testing::_;

namespace shill {

class DBusManagerTest : public testing::Test {
 public:
  DBusManagerTest()
      : proxy_(new MockDBusServiceProxy()),
        proxy_factory_(this) {}

  virtual void SetUp() {
    // Replaces the real proxy factory with a local one providing mock proxies.
    dbus_manager_.proxy_factory_ = &proxy_factory_;
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(DBusManagerTest *test) : test_(test) {}

    virtual DBusServiceProxyInterface *CreateDBusServiceProxy() {
      return test_->proxy_.release();
    }

   private:
    DBusManagerTest *test_;

    DISALLOW_COPY_AND_ASSIGN(TestProxyFactory);
  };

  class NameWatcher : public base::SupportsWeakPtr<NameWatcher> {
   public:
    NameWatcher(const string &name)
        : on_appear_(Bind(&NameWatcher::OnAppear, AsWeakPtr(), name)),
          on_vanish_(Bind(&NameWatcher::OnVanish, AsWeakPtr(), name)) {}
    virtual ~NameWatcher() {}

    MOCK_METHOD2(OnAppear, void(const string &name, const string &owner));
    MOCK_METHOD1(OnVanish, void(const string &name));

    const DBusManager::AppearedCallback &on_appear() const {
      return on_appear_;
    }

    const DBusManager::VanishedCallback &on_vanish() const {
      return on_vanish_;
    }

    DBusManager::NameWatcher GetWatcher() const {
      return DBusManager::NameWatcher(on_appear_, on_vanish_);
    }

    DBusManager::NameWatcher GetAppearWatcher() const {
      return DBusManager::NameWatcher(on_appear_,
                                      DBusManager::VanishedCallback());
    }

    DBusManager::NameWatcher GetVanishWatcher() const {
      return DBusManager::NameWatcher(DBusManager::AppearedCallback(),
                                      on_vanish_);
    }

   private:
    DBusManager::AppearedCallback on_appear_;
    DBusManager::VanishedCallback on_vanish_;

    DISALLOW_COPY_AND_ASSIGN(NameWatcher);
  };

  scoped_ptr<MockDBusServiceProxy> proxy_;
  TestProxyFactory proxy_factory_;
  DBusManager dbus_manager_;
};

TEST_F(DBusManagerTest, NameWatchers) {
  static const char kName1[] = "org.chromium.Service1";
  static const char kOwner1[] = ":1.17";
  static const char kName2[] = "org.chromium.Service2";
  static const char kOwner2[] = ":1.27";

  MockDBusServiceProxy *proxy = proxy_.get();

  // Start the DBus service manager.
  EXPECT_CALL(*proxy, set_name_owner_changed_callback(_));
  dbus_manager_.Start();
  EXPECT_TRUE(dbus_manager_.proxy_.get());

  // Expect no crash.
  dbus_manager_.Start();

  // Register a name watcher 1A for kName1.
  scoped_ptr<NameWatcher> watcher1a(new NameWatcher(kName1));
  EXPECT_CALL(*proxy, GetNameOwner(kName1, _, _, _));
  dbus_manager_.WatchName(
      kName1, watcher1a->on_appear(), watcher1a->on_vanish());

  // 1A should be notified on the initial owner.
  EXPECT_CALL(*watcher1a, OnAppear(kName1, kOwner1));
  EXPECT_CALL(*watcher1a, OnVanish(_)).Times(0);
  dbus_manager_.OnGetNameOwnerComplete(
      watcher1a->GetWatcher(), kOwner1, Error());

  // Register an appear-only watcher 1B for kName1.
  scoped_ptr<NameWatcher> watcher1b(new NameWatcher(kName1));
  EXPECT_CALL(*proxy, GetNameOwner(kName1, _, _, _));
  dbus_manager_.WatchName(
      kName1, watcher1b->on_appear(), DBusManager::VanishedCallback());

  // 1B should be notified on the initial owner. 1A should not get any
  // notification.
  EXPECT_CALL(*watcher1a, OnAppear(_, _)).Times(0);
  EXPECT_CALL(*watcher1b, OnAppear(kName1, kOwner1));
  dbus_manager_.OnGetNameOwnerComplete(
      watcher1b->GetAppearWatcher(), kOwner1, Error());

  // Register a name watcher 2A for kName2.
  scoped_ptr<NameWatcher> watcher2a(new NameWatcher(kName2));
  EXPECT_CALL(*proxy, GetNameOwner(kName2, _, _, _));
  dbus_manager_.WatchName(
      kName2, watcher2a->on_appear(), watcher2a->on_vanish());

  // 2A should be notified on the lack of initial owner.
  EXPECT_CALL(*watcher2a, OnAppear(_, _)).Times(0);
  EXPECT_CALL(*watcher2a, OnVanish(kName2));
  dbus_manager_.OnGetNameOwnerComplete(
      watcher2a->GetWatcher(), string(), Error());

  // Register a vanish-only watcher 2B for kName2.
  scoped_ptr<NameWatcher> watcher2b(new NameWatcher(kName2));
  EXPECT_CALL(*proxy, GetNameOwner(kName2, _, _, _));
  dbus_manager_.WatchName(
      kName2, DBusManager::AppearedCallback(), watcher2b->on_vanish());

  // 2B should be notified on the lack of initial owner. 2A should not get any
  // notification.
  EXPECT_CALL(*watcher2a, OnVanish(_)).Times(0);
  EXPECT_CALL(*watcher2b, OnVanish(kName2));
  dbus_manager_.OnGetNameOwnerComplete(
      watcher2b->GetVanishWatcher(), string(), Error());

  EXPECT_EQ(2, dbus_manager_.name_watchers_[kName1].size());
  EXPECT_EQ(2, dbus_manager_.name_watchers_[kName2].size());

  // Toggle kName1 owner.
  EXPECT_CALL(*watcher1a, OnVanish(kName1));
  EXPECT_CALL(*watcher1b, OnVanish(_)).Times(0);
  dbus_manager_.OnNameOwnerChanged(kName1, kOwner1, string());
  EXPECT_CALL(*watcher1a, OnAppear(kName1, kOwner1));
  EXPECT_CALL(*watcher1b, OnAppear(kName1, kOwner1));
  dbus_manager_.OnNameOwnerChanged(kName1, string(), kOwner1);

  // Toggle kName2 owner.
  EXPECT_CALL(*watcher2a, OnAppear(kName2, kOwner2));
  EXPECT_CALL(*watcher2b, OnAppear(_, _)).Times(0);
  dbus_manager_.OnNameOwnerChanged(kName2, string(), kOwner2);
  EXPECT_CALL(*watcher2a, OnVanish(kName2));
  EXPECT_CALL(*watcher2b, OnVanish(kName2));
  dbus_manager_.OnNameOwnerChanged(kName2, kOwner2, string());

  // Invalidate kName1 callbacks, ensure no crashes.
  watcher1a.reset();
  watcher1b.reset();
  dbus_manager_.OnNameOwnerChanged(kName1, kOwner1, string());
  dbus_manager_.OnNameOwnerChanged(kName1, string(), kOwner1);

  // Stop the DBus service manager.
  dbus_manager_.Stop();
  EXPECT_FALSE(dbus_manager_.proxy_.get());
  EXPECT_TRUE(dbus_manager_.name_watchers_.empty());

  // Ensure no crash.
  dbus_manager_.Stop();
}

}  // namespace shill
