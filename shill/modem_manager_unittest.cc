// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/stl_util.h>
#include <gtest/gtest.h>
#include <mm/ModemManager-names.h>

#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_dbus_objectmanager_proxy.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_modem.h"
#include "shill/mock_modem_manager_proxy.h"
#include "shill/modem.h"
#include "shill/modem_manager.h"
#include "shill/proxy_factory.h"

using std::string;
using std::tr1::shared_ptr;
using std::vector;
using testing::_;
using testing::Invoke;
using testing::Pointee;
using testing::Return;
using testing::StrEq;
using testing::Test;

namespace shill {

// A testing subclass of ModemManager.
class ModemManagerCore : public ModemManager {
 public:
  ModemManagerCore(const string &service,
                   const string &path,
                   ControlInterface *control_interface,
                   EventDispatcher *dispatcher,
                   Metrics *metrics,
                   Manager *manager,
                   GLib *glib,
                   mobile_provider_db *provider_db)
      : ModemManager(service,
                     path,
                     control_interface,
                     dispatcher,
                     metrics,
                     manager,
                     glib,
                     provider_db) {}

  virtual ~ModemManagerCore() {}

  MOCK_METHOD1(Connect, void(const string &owner));
  MOCK_METHOD0(Disconnect, void());
};

class ModemManagerTest : public Test {
 public:
  ModemManagerTest()
      : manager_(&control_interface_, &dispatcher_, &metrics_, &glib_) {
  }

  virtual void SetUp() {
    modem_.reset(new StrictModem(
        kOwner, kService, kModemPath, &control_interface_, &dispatcher_,
        &metrics_, &manager_, static_cast<mobile_provider_db *>(NULL)));
  }
 protected:
  static const char kService[];
  static const char kPath[];
  static const char kOwner[];
  static const char kModemPath[];

  shared_ptr<StrictModem> modem_;

  MockGLib glib_;
  MockControl control_interface_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockManager manager_;
};

const char ModemManagerTest::kService[] = "org.chromium.ModemManager";
const char ModemManagerTest::kPath[] = "/org/chromium/ModemManager";
const char ModemManagerTest::kOwner[] = ":1.17";
const char ModemManagerTest::kModemPath[] = "/org/blah/Modem/blah/0";

class ModemManagerCoreTest : public ModemManagerTest {
 public:
  ModemManagerCoreTest()
      : ModemManagerTest(),
        modem_manager_(kService,
                       kPath,
                       &control_interface_,
                       &dispatcher_,
                       &metrics_,
                       &manager_,
                       &glib_,
                       NULL) {}

  virtual void TearDown() {
    modem_manager_.watcher_id_ = 0;
  }

 protected:
  ModemManagerCore modem_manager_;
};

TEST_F(ModemManagerCoreTest, Start) {
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

TEST_F(ModemManagerCoreTest, Stop) {
  const int kWatcher = 345;
  modem_manager_.watcher_id_ = kWatcher;
  modem_manager_.owner_ = kOwner;
  EXPECT_CALL(glib_, BusUnwatchName(kWatcher)).Times(1);
  EXPECT_CALL(modem_manager_, Disconnect());
  modem_manager_.Stop();
}

TEST_F(ModemManagerCoreTest, OnAppearVanish) {
  EXPECT_EQ("", modem_manager_.owner_);
  EXPECT_CALL(modem_manager_, Connect(kOwner));
  EXPECT_CALL(modem_manager_, Disconnect());
  ModemManager::OnAppear(NULL, kService, kOwner, &modem_manager_);
  ModemManager::OnVanish(NULL, kService, &modem_manager_);
}

TEST_F(ModemManagerCoreTest, Connect) {
  EXPECT_EQ("", modem_manager_.owner_);
  modem_manager_.ModemManager::Connect(kOwner);
  EXPECT_EQ(kOwner, modem_manager_.owner_);
}

TEST_F(ModemManagerCoreTest, Disconnect) {
  modem_manager_.owner_ = kOwner;
  modem_manager_.RecordAddedModem(modem_);
  EXPECT_EQ(1, modem_manager_.modems_.size());

  modem_manager_.ModemManager::Disconnect();
  EXPECT_EQ("", modem_manager_.owner_);
  EXPECT_EQ(0, modem_manager_.modems_.size());
}

TEST_F(ModemManagerCoreTest, ModemExists) {
  modem_manager_.owner_ = kOwner;

  EXPECT_FALSE(modem_manager_.ModemExists(kModemPath));
  modem_manager_.RecordAddedModem(modem_);
  EXPECT_TRUE(modem_manager_.ModemExists(kModemPath));
}

class ModemManagerClassicMockInit : public ModemManagerClassic {
 public:
  ModemManagerClassicMockInit(const string &service,
                              const string &path,
                              ControlInterface *control_interface,
                              EventDispatcher *dispatcher,
                              Metrics *metrics,
                              Manager *manager,
                              GLib *glib,
                              mobile_provider_db *provider_db) :
      ModemManagerClassic(service,
                          path,
                          control_interface,
                          dispatcher,
                          metrics,
                          manager,
                          glib,
                          provider_db) {}
  MOCK_METHOD1(InitModemClassic, void(shared_ptr<ModemClassic>));
};

class ModemManagerClassicTest : public ModemManagerTest {
 public:
  ModemManagerClassicTest()
      : ModemManagerTest(),
        modem_manager_(kService,
                       kPath,
                       &control_interface_,
                       &dispatcher_,
                       &metrics_,
                       &manager_,
                       &glib_,
                       NULL),
        proxy_(new MockModemManagerProxy()),
        proxy_factory_(this) {
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(ModemManagerClassicTest *test) : test_(test) {}

    virtual ModemManagerProxyInterface *CreateModemManagerProxy(
        ModemManagerClassic */*manager*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->proxy_.release();
    }

   private:
    ModemManagerClassicTest *test_;
  };

  virtual void SetUp() {
    modem_manager_.proxy_factory_ = &proxy_factory_;
  }

  virtual void TearDown() {
    modem_manager_.proxy_factory_ = NULL;
  }

  ModemManagerClassicMockInit modem_manager_;
  scoped_ptr<MockModemManagerProxy> proxy_;
  TestProxyFactory proxy_factory_;
};

TEST_F(ModemManagerClassicTest, Connect) {
  EXPECT_EQ("", modem_manager_.owner_);

  EXPECT_CALL(*proxy_, EnumerateDevices())
      .WillOnce(Return(vector<DBus::Path>(1, kModemPath)));

  EXPECT_CALL(modem_manager_,
              InitModemClassic(
                  Pointee(Field(&Modem::path_, StrEq(kModemPath)))));

  modem_manager_.Connect(kOwner);
  EXPECT_EQ(kOwner, modem_manager_.owner_);
  EXPECT_EQ(1, modem_manager_.modems_.size());
  ASSERT_TRUE(ContainsKey(modem_manager_.modems_, kModemPath));
}


class ModemManager1MockInit : public ModemManager1 {
 public:
  ModemManager1MockInit(const string &service,
                        const string &path,
                        ControlInterface *control_interface,
                        EventDispatcher *dispatcher,
                        Metrics *metrics,
                        Manager *manager,
                        GLib *glib,
                        mobile_provider_db *provider_db) :
      ModemManager1(service,
                    path,
                    control_interface,
                    dispatcher,
                    metrics,
                    manager,
                    glib,
                    provider_db) {}
  MOCK_METHOD2(InitModem1, void(shared_ptr<Modem1>,
                                const DBusInterfaceToProperties &));
};


class ModemManager1Test : public ModemManagerTest {
 public:
  ModemManager1Test()
      : ModemManagerTest(),
        modem_manager_(kService,
                       kPath,
                       &control_interface_,
                       &dispatcher_,
                       &metrics_,
                       &manager_,
                       &glib_,
                       NULL),
        proxy_(new MockDBusObjectManagerProxy()),
        proxy_factory_(this) {
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(ModemManager1Test *test) : test_(test) {}

    virtual DBusObjectManagerProxyInterface *CreateDBusObjectManagerProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->proxy_.release();
    }

   private:
    ModemManager1Test *test_;
  };

  virtual void SetUp() {
    modem_manager_.proxy_factory_ = &proxy_factory_;
    proxy_->IgnoreSetCallbacks();
  }

  virtual void TearDown() {
    modem_manager_.proxy_factory_ = NULL;
  }

  static DBusObjectsWithProperties GetModemWithProperties() {
    DBusPropertiesMap o_fd_mm1_modem;

    DBusInterfaceToProperties i_to_p;
    i_to_p[MM_DBUS_INTERFACE_MODEM] = o_fd_mm1_modem;

    DBusObjectsWithProperties objects_with_properties;
    objects_with_properties[kModemPath] = i_to_p;

    return objects_with_properties;
  }

  ModemManager1MockInit modem_manager_;
  scoped_ptr<MockDBusObjectManagerProxy> proxy_;
  TestProxyFactory proxy_factory_;
};

TEST_F(ModemManager1Test, Connect) {
  Error e;

  EXPECT_CALL(*proxy_, GetManagedObjects(_, _, _));
  EXPECT_CALL(modem_manager_,
              InitModem1(
                  Pointee(Field(&Modem::path_, StrEq(kModemPath))),
                  _));

  modem_manager_.Connect(kOwner);
  modem_manager_.OnGetManagedObjectsReply(GetModemWithProperties(), e);
  EXPECT_EQ(1, modem_manager_.modems_.size());
  EXPECT_TRUE(ContainsKey(modem_manager_.modems_, kModemPath));
}


TEST_F(ModemManager1Test, AddRemoveInterfaces) {
  EXPECT_CALL(*proxy_, GetManagedObjects(_, _, _));
  modem_manager_.Connect(kOwner);

  // Have nothing come back from GetManagedObjects
  modem_manager_.OnGetManagedObjectsReply(DBusObjectsWithProperties(), Error());
  EXPECT_EQ(0, modem_manager_.modems_.size());

  // Add an object that doesn't have a modem interface.  Nothing should be added
  EXPECT_CALL(modem_manager_, InitModem1(_, _)).Times(0);
  modem_manager_.OnInterfacesAddedSignal(kModemPath,
                                         DBusInterfaceToProperties());
  EXPECT_EQ(0, modem_manager_.modems_.size());

  // Actually add a modem
  EXPECT_CALL(modem_manager_, InitModem1(_, _)).Times(1);
  modem_manager_.OnInterfacesAddedSignal(kModemPath,
                                         GetModemWithProperties()[kModemPath]);
  EXPECT_EQ(1, modem_manager_.modems_.size());

  // Remove an irrelevant interface
  vector<string> not_including_modem_interface;
  not_including_modem_interface.push_back("not.a.modem.interface");
  modem_manager_.OnInterfacesRemovedSignal(kModemPath,
                                           not_including_modem_interface);
  EXPECT_EQ(1, modem_manager_.modems_.size());

  // Remove the modem
  vector<string> with_modem_interface;
  with_modem_interface.push_back(MM_DBUS_INTERFACE_MODEM);
  modem_manager_.OnInterfacesRemovedSignal(kModemPath,
                                           with_modem_interface);
  EXPECT_EQ(0, modem_manager_.modems_.size());
}

}  // namespace shill
