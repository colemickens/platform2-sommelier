// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/stl_util.h>
#include <ModemManager/ModemManager.h>

#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_dbus_objectmanager_proxy.h"
#include "shill/mock_dbus_service_proxy.h"
#include "shill/mock_manager.h"
#include "shill/mock_modem.h"
#include "shill/mock_modem_info.h"
#include "shill/mock_modem_manager_proxy.h"
#include "shill/mock_proxy_factory.h"
#include "shill/modem.h"
#include "shill/modem_manager.h"
#include "shill/testing.h"

using std::string;
using std::shared_ptr;
using std::vector;
using testing::_;
using testing::Invoke;
using testing::Pointee;
using testing::Return;
using testing::SaveArg;
using testing::StrEq;
using testing::Test;

namespace shill {

class ModemManagerTest : public Test {
 public:
  ModemManagerTest()
      : manager_(&control_, &dispatcher_, NULL, NULL),
        modem_info_(&control_, &dispatcher_, NULL, &manager_, NULL),
        dbus_service_proxy_(NULL) {}

  virtual void SetUp() {
    modem_.reset(new StrictModem(kOwner, kService, kModemPath, &modem_info_));
    manager_.dbus_manager_.reset(new DBusManager());
    dbus_service_proxy_ = new MockDBusServiceProxy();
    // Ownership  of |dbus_service_proxy_| is transferred to
    // |manager_.dbus_manager_|.
    manager_.dbus_manager_->proxy_.reset(dbus_service_proxy_);
  }

 protected:
  static const char kService[];
  static const char kPath[];
  static const char kOwner[];
  static const char kModemPath[];

  shared_ptr<StrictModem> modem_;

  EventDispatcher dispatcher_;
  MockControl control_;
  MockManager manager_;
  MockModemInfo modem_info_;
  MockProxyFactory proxy_factory_;
  MockDBusServiceProxy *dbus_service_proxy_;
};

const char ModemManagerTest::kService[] = "org.chromium.ModemManager";
const char ModemManagerTest::kPath[] = "/org/chromium/ModemManager";
const char ModemManagerTest::kOwner[] = ":1.17";
const char ModemManagerTest::kModemPath[] = "/org/blah/Modem/blah/0";

class ModemManagerCoreTest : public ModemManagerTest {
 public:
  ModemManagerCoreTest()
      : ModemManagerTest(),
        modem_manager_(kService, kPath, &modem_info_) {}

 protected:
  ModemManager modem_manager_;
};

TEST_F(ModemManagerCoreTest, StartStopWithModemManagerServiceAbsent) {
  StringCallback get_name_owner_callback;
  EXPECT_CALL(*dbus_service_proxy_, GetNameOwner(kService, _, _, _))
      .WillOnce(SaveArg<2>(&get_name_owner_callback));
  modem_manager_.Start();
  get_name_owner_callback.Run("", Error());
  EXPECT_EQ("", modem_manager_.owner_);

  modem_manager_.Stop();
  EXPECT_EQ("", modem_manager_.owner_);
}

TEST_F(ModemManagerCoreTest, StartStopWithModemManagerServicePresent) {
  StringCallback get_name_owner_callback;
  EXPECT_CALL(*dbus_service_proxy_, GetNameOwner(kService, _, _, _))
      .WillOnce(SaveArg<2>(&get_name_owner_callback));
  modem_manager_.Start();
  get_name_owner_callback.Run(kOwner, Error());
  EXPECT_EQ(kOwner, modem_manager_.owner_);

  modem_manager_.Stop();
  EXPECT_EQ("", modem_manager_.owner_);
}

TEST_F(ModemManagerCoreTest, OnAppearVanish) {
  EXPECT_CALL(*dbus_service_proxy_, GetNameOwner(kService, _, _, _));
  modem_manager_.Start();
  EXPECT_EQ("", modem_manager_.owner_);

  manager_.dbus_manager()->OnNameOwnerChanged(kService, "", kOwner);
  EXPECT_EQ(kOwner, modem_manager_.owner_);

  manager_.dbus_manager()->OnNameOwnerChanged(kService, kOwner, "");
  EXPECT_EQ("", modem_manager_.owner_);
}

TEST_F(ModemManagerCoreTest, ConnectDisconnect) {
  EXPECT_EQ("", modem_manager_.owner_);
  modem_manager_.Connect(kOwner);
  EXPECT_EQ(kOwner, modem_manager_.owner_);
  EXPECT_EQ(0, modem_manager_.modems_.size());

  modem_manager_.RecordAddedModem(modem_);
  EXPECT_EQ(1, modem_manager_.modems_.size());

  modem_manager_.ModemManager::Disconnect();
  EXPECT_EQ("", modem_manager_.owner_);
  EXPECT_EQ(0, modem_manager_.modems_.size());
}

TEST_F(ModemManagerCoreTest, AddRemoveModem) {
  modem_manager_.Connect(kOwner);
  EXPECT_FALSE(modem_manager_.ModemExists(kModemPath));

  // Remove non-existent modem path.
  modem_manager_.RemoveModem(kModemPath);
  EXPECT_FALSE(modem_manager_.ModemExists(kModemPath));

  modem_manager_.RecordAddedModem(modem_);
  EXPECT_TRUE(modem_manager_.ModemExists(kModemPath));

  // Add an already added modem.
  modem_manager_.RecordAddedModem(modem_);
  EXPECT_TRUE(modem_manager_.ModemExists(kModemPath));

  modem_manager_.RemoveModem(kModemPath);
  EXPECT_FALSE(modem_manager_.ModemExists(kModemPath));

  // Remove an already removed modem path.
  modem_manager_.RemoveModem(kModemPath);
  EXPECT_FALSE(modem_manager_.ModemExists(kModemPath));
}

class ModemManagerClassicMockInit : public ModemManagerClassic {
 public:
  ModemManagerClassicMockInit(const string &service,
                              const string &path,
                              ModemInfo *modem_info_) :
      ModemManagerClassic(service, path, modem_info_) {}

  MOCK_METHOD1(InitModemClassic, void(shared_ptr<ModemClassic>));
};

class ModemManagerClassicTest : public ModemManagerTest {
 public:
  ModemManagerClassicTest()
      : ModemManagerTest(),
        modem_manager_(kService, kPath, &modem_info_),
        proxy_(new MockModemManagerProxy()) {}

 protected:
  virtual void SetUp() {
    modem_manager_.proxy_factory_ = &proxy_factory_;
  }

  virtual void TearDown() {
    modem_manager_.proxy_factory_ = NULL;
  }

  ModemManagerClassicMockInit modem_manager_;
  scoped_ptr<MockModemManagerProxy> proxy_;
};

TEST_F(ModemManagerClassicTest, Connect) {
  EXPECT_EQ("", modem_manager_.owner_);

  EXPECT_CALL(proxy_factory_, CreateModemManagerProxy(_, kPath, kOwner))
      .WillOnce(ReturnAndReleasePointee(&proxy_));
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
                        ModemInfo *modem_info_) :
      ModemManager1(service, path, modem_info_) {}
  MOCK_METHOD2(InitModem1, void(shared_ptr<Modem1>,
                                const DBusInterfaceToProperties &));
};


class ModemManager1Test : public ModemManagerTest {
 public:
  ModemManager1Test()
      : ModemManagerTest(),
        modem_manager_(kService, kPath, &modem_info_),
        proxy_(new MockDBusObjectManagerProxy()) {}

 protected:
  virtual void SetUp() {
    modem_manager_.proxy_factory_ = &proxy_factory_;
    proxy_->IgnoreSetCallbacks();
  }

  virtual void TearDown() {
    modem_manager_.proxy_factory_ = NULL;
  }

  void Connect(const DBusObjectsWithProperties &expected_objects) {
    EXPECT_CALL(proxy_factory_, CreateDBusObjectManagerProxy(kPath, kOwner))
        .WillOnce(ReturnAndReleasePointee(&proxy_));
    EXPECT_CALL(*proxy_, set_interfaces_added_callback(_));
    EXPECT_CALL(*proxy_, set_interfaces_removed_callback(_));
    ManagedObjectsCallback get_managed_objects_callback;
    EXPECT_CALL(*proxy_, GetManagedObjects(_, _, _))
        .WillOnce(SaveArg<1>(&get_managed_objects_callback));
    modem_manager_.Connect(kOwner);
    get_managed_objects_callback.Run(expected_objects, Error());
  }

  static DBusObjectsWithProperties GetModemWithProperties() {
    DBusPropertiesMap o_fd_mm1_modem;

    DBusInterfaceToProperties properties;
    properties[MM_DBUS_INTERFACE_MODEM] = o_fd_mm1_modem;

    DBusObjectsWithProperties objects_with_properties;
    objects_with_properties[kModemPath] = properties;

    return objects_with_properties;
  }

  ModemManager1MockInit modem_manager_;
  scoped_ptr<MockDBusObjectManagerProxy> proxy_;
  MockProxyFactory proxy_factory_;
};

TEST_F(ModemManager1Test, Connect) {
  Connect(GetModemWithProperties());
  EXPECT_EQ(1, modem_manager_.modems_.size());
  EXPECT_TRUE(ContainsKey(modem_manager_.modems_, kModemPath));
}

TEST_F(ModemManager1Test, AddRemoveInterfaces) {
  // Have nothing come back from GetManagedObjects
  Connect(DBusObjectsWithProperties());
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
  modem_manager_.OnInterfacesRemovedSignal(kModemPath, with_modem_interface);
  EXPECT_EQ(0, modem_manager_.modems_.size());
}

}  // namespace shill
