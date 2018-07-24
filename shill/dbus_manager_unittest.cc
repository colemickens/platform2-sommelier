// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_manager.h"

#include <base/bind.h>
#include <base/memory/weak_ptr.h>

#include "shill/error.h"
#include "shill/mock_control.h"
#include "shill/mock_dbus_service_proxy.h"
#include "shill/testing.h"

using base::Bind;
using base::Unretained;
using std::string;
using std::unique_ptr;
using testing::Invoke;
using testing::SaveArg;
using testing::WithArg;
using testing::_;

namespace shill {

namespace {

const char kName1[] = "org.chromium.Service1";
const char kOwner1[] = ":1.17";
const char kName2[] = "org.chromium.Service2";
const char kOwner2[] = ":1.27";

void SetErrorOperationFailed(Error* error) {
  error->Populate(Error::kOperationFailed);
}

}  // namespace

class DBusManagerTest : public testing::Test {
 public:
  DBusManagerTest()
      : proxy_(new MockDBusServiceProxy()),
        manager_(new DBusManager(&control_)) {}

 protected:
  class DBusNameWatcherCallbackObserver {
   public:
    DBusNameWatcherCallbackObserver()
        : name_appeared_callback_(
              Bind(&DBusNameWatcherCallbackObserver::OnNameAppeared,
                   Unretained(this))),
          name_vanished_callback_(
              Bind(&DBusNameWatcherCallbackObserver::OnNameVanished,
                   Unretained(this))) {}

    virtual ~DBusNameWatcherCallbackObserver() {}

    MOCK_CONST_METHOD2(OnNameAppeared, void(const string& name,
                                            const string& owner));
    MOCK_CONST_METHOD1(OnNameVanished, void(const string& name));

    const DBusNameWatcher::NameAppearedCallback& name_appeared_callback()
        const {
      return name_appeared_callback_;
    }

    const DBusNameWatcher::NameVanishedCallback& name_vanished_callback()
        const {
      return name_vanished_callback_;
    }

   private:
    DBusNameWatcher::NameAppearedCallback name_appeared_callback_;
    DBusNameWatcher::NameVanishedCallback name_vanished_callback_;

    DISALLOW_COPY_AND_ASSIGN(DBusNameWatcherCallbackObserver);
  };

  MockDBusServiceProxy* ExpectCreateDBusServiceProxy() {
    EXPECT_CALL(control_, CreateDBusServiceProxy())
        .WillOnce(ReturnAndReleasePointee(&proxy_));
    return proxy_.get();
  }

  unique_ptr<MockDBusServiceProxy> proxy_;
  MockControl control_;
  unique_ptr<DBusManager> manager_;
};

TEST_F(DBusManagerTest, GetNameOwnerFails) {
  MockDBusServiceProxy* proxy = ExpectCreateDBusServiceProxy();

  EXPECT_CALL(*proxy, set_name_owner_changed_callback(_));
  manager_->Start();

  EXPECT_CALL(*proxy, GetNameOwner(kName1, _, _, _))
      .WillOnce(WithArg<1>(Invoke(SetErrorOperationFailed)));

  DBusNameWatcherCallbackObserver observer;
  EXPECT_CALL(observer, OnNameAppeared(_, _)).Times(0);
  EXPECT_CALL(observer, OnNameVanished(kName1));

  unique_ptr<DBusNameWatcher> watcher(
      manager_->CreateNameWatcher(kName1,
                                  observer.name_appeared_callback(),
                                  observer.name_vanished_callback()));
}

TEST_F(DBusManagerTest,
       GetNameOwnerReturnsAfterDBusManagerAndNameWatcherDestroyed) {
  MockDBusServiceProxy* proxy = ExpectCreateDBusServiceProxy();

  EXPECT_CALL(*proxy, set_name_owner_changed_callback(_));
  manager_->Start();

  StringCallback get_name_owner_callback;
  EXPECT_CALL(*proxy, GetNameOwner(kName1, _, _, _))
      .WillOnce(SaveArg<2>(&get_name_owner_callback));
  unique_ptr<DBusNameWatcher> watcher(
      manager_->CreateNameWatcher(kName1,
                                  DBusNameWatcher::NameAppearedCallback(),
                                  DBusNameWatcher::NameVanishedCallback()));

  // Expect no crash if the GetNameOwner callback is invoked after the
  // DBusNameWatcher object is destroyed.
  watcher.reset();
  get_name_owner_callback.Run(kOwner1, Error());

  // Expect no crash if the GetNameOwner callback is invoked after the
  // DBusManager object is destroyed.
  manager_.reset();
  get_name_owner_callback.Run(kOwner1, Error());
}

TEST_F(DBusManagerTest, NameWatchers) {
  MockDBusServiceProxy* proxy = ExpectCreateDBusServiceProxy();

  // Start the DBus service manager.
  EXPECT_CALL(*proxy, set_name_owner_changed_callback(_));
  manager_->Start();
  EXPECT_TRUE(manager_->proxy_.get());

  // Expect no crash when DBusManager::Start() is invoked again.
  manager_->Start();

  StringCallback get_name_owner_callback;

  // Register a name watcher 1a for kName1.
  EXPECT_CALL(*proxy, GetNameOwner(kName1, _, _, _))
      .WillOnce(SaveArg<2>(&get_name_owner_callback));
  DBusNameWatcherCallbackObserver observer1a;
  unique_ptr<DBusNameWatcher> watcher1a(
      manager_->CreateNameWatcher(kName1,
                                  observer1a.name_appeared_callback(),
                                  observer1a.name_vanished_callback()));

  // Observer 1a should be notified on the initial owner.
  EXPECT_CALL(observer1a, OnNameAppeared(kName1, kOwner1));
  EXPECT_CALL(observer1a, OnNameVanished(_)).Times(0);
  get_name_owner_callback.Run(kOwner1, Error());

  // Register an appear-only watcher 1b for kName1.
  EXPECT_CALL(*proxy, GetNameOwner(kName1, _, _, _))
      .WillOnce(SaveArg<2>(&get_name_owner_callback));
  DBusNameWatcherCallbackObserver observer1b;
  unique_ptr<DBusNameWatcher> watcher1b(
      manager_->CreateNameWatcher(kName1,
                                  observer1b.name_appeared_callback(),
                                  DBusNameWatcher::NameVanishedCallback()));

  // Observer 1b should be notified on the initial owner. Observer 1a should
  // not get any notification.
  EXPECT_CALL(observer1a, OnNameAppeared(_, _)).Times(0);
  EXPECT_CALL(observer1b, OnNameAppeared(kName1, kOwner1));
  get_name_owner_callback.Run(kOwner1, Error());

  // Register a name watcher 2a for kName2.
  EXPECT_CALL(*proxy, GetNameOwner(kName2, _, _, _))
      .WillOnce(SaveArg<2>(&get_name_owner_callback));
  DBusNameWatcherCallbackObserver observer2a;
  unique_ptr<DBusNameWatcher> watcher2a(
      manager_->CreateNameWatcher(kName2,
                                  observer2a.name_appeared_callback(),
                                  observer2a.name_vanished_callback()));

  // Observer 2a should be notified on the lack of initial owner.
  EXPECT_CALL(observer2a, OnNameAppeared(_, _)).Times(0);
  EXPECT_CALL(observer2a, OnNameVanished(kName2));
  get_name_owner_callback.Run(string(), Error());

  // Register a vanish-only watcher 2b for kName2.
  EXPECT_CALL(*proxy, GetNameOwner(kName2, _, _, _))
      .WillOnce(SaveArg<2>(&get_name_owner_callback));
  DBusNameWatcherCallbackObserver observer2b;
  unique_ptr<DBusNameWatcher> watcher2b(
      manager_->CreateNameWatcher(kName2,
                                  DBusNameWatcher::NameAppearedCallback(),
                                  observer2b.name_vanished_callback()));

  // Observer 2b should be notified on the lack of initial owner. Observer 2a
  // should not get any notification.
  EXPECT_CALL(observer2a, OnNameVanished(_)).Times(0);
  EXPECT_CALL(observer2b, OnNameVanished(kName2));
  get_name_owner_callback.Run(string(), Error());

  EXPECT_EQ(2, manager_->name_watchers_[kName1].size());
  EXPECT_EQ(2, manager_->name_watchers_[kName2].size());

  // Toggle kName1 owner.
  EXPECT_CALL(observer1a, OnNameVanished(kName1));
  EXPECT_CALL(observer1b, OnNameVanished(_)).Times(0);
  manager_->OnNameOwnerChanged(kName1, kOwner1, string());
  EXPECT_CALL(observer1a, OnNameAppeared(kName1, kOwner1));
  EXPECT_CALL(observer1b, OnNameAppeared(kName1, kOwner1));
  manager_->OnNameOwnerChanged(kName1, string(), kOwner1);

  // Toggle kName2 owner.
  EXPECT_CALL(observer2a, OnNameAppeared(kName2, kOwner2));
  EXPECT_CALL(observer2b, OnNameAppeared(_, _)).Times(0);
  manager_->OnNameOwnerChanged(kName2, string(), kOwner2);
  EXPECT_CALL(observer2a, OnNameVanished(kName2));
  EXPECT_CALL(observer2b, OnNameVanished(kName2));
  manager_->OnNameOwnerChanged(kName2, kOwner2, string());

  // Invalidate kName1 callbacks, ensure no crashes.
  watcher1a.reset();
  watcher1b.reset();
  manager_->OnNameOwnerChanged(kName1, kOwner1, string());
  manager_->OnNameOwnerChanged(kName1, string(), kOwner1);

  // Stop the DBus service manager.
  manager_->Stop();
  EXPECT_FALSE(manager_->proxy_.get());
  EXPECT_TRUE(manager_->name_watchers_.empty());

  // Ensure no crash if DBusManager::Stop() is inovked again.
  manager_->Stop();
}

}  // namespace shill
