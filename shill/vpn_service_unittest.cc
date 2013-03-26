// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_service.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_connection.h"
#include "shill/mock_device_info.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_profile.h"
#include "shill/mock_sockets.h"
#include "shill/mock_store.h"
#include "shill/mock_vpn_driver.h"
#include "shill/mock_vpn_provider.h"

using std::string;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace shill {

class VPNServiceTest : public testing::Test {
 public:
  VPNServiceTest()
      : interface_name_("test-interface"),
        driver_(new MockVPNDriver()),
        manager_(&control_, NULL, NULL, NULL),
        metrics_(NULL),
        device_info_(&control_, NULL, NULL, NULL),
        connection_(new NiceMock<MockConnection>(&device_info_)),
        sockets_(new MockSockets()),
        service_(new VPNService(&control_, NULL, &metrics_, &manager_,
                                driver_)) {
    service_->sockets_.reset(sockets_);  // Passes ownership.
  }

  virtual ~VPNServiceTest() {}

 protected:
  virtual void SetUp() {
    ON_CALL(*connection_, interface_name())
        .WillByDefault(ReturnRef(interface_name_));
    ON_CALL(*connection_, ipconfig_rpc_identifier())
        .WillByDefault(ReturnRef(ipconfig_rpc_identifier_));
  }

  virtual void TearDown() {
    EXPECT_CALL(device_info_, FlushAddresses(0));
  }

  void SetServiceState(Service::ConnectState state) {
    service_->state_ = state;
  }

  void SetHasEverConnected(bool connected) {
    service_->has_ever_connected_ = connected;
  }

  void SetConnectable(bool connectable) {
    service_->connectable_ = connectable;
  }

  const char *GetAutoConnOffline() {
    return Service::kAutoConnOffline;
  }

  const char *GetAutoConnNeverConnected() {
    return VPNService::kAutoConnNeverConnected;
  }

  const char *GetAutoConnVPNAlreadyActive() {
    return VPNService::kAutoConnVPNAlreadyActive;
  }

  bool IsAutoConnectable(const char **reason) const {
    return service_->IsAutoConnectable(reason);
  }

  // Takes ownership of |provider|.
  void SetVPNProvider(VPNProvider *provider) {
    manager_.vpn_provider_.reset(provider);
  }

  std::string interface_name_;
  std::string ipconfig_rpc_identifier_;
  MockVPNDriver *driver_;  // Owned by |service_|.
  NiceMockControl control_;
  MockManager manager_;
  MockMetrics metrics_;
  MockDeviceInfo device_info_;
  scoped_refptr<NiceMock<MockConnection> > connection_;
  MockSockets *sockets_;  // Owned by |service_|.
  VPNServiceRefPtr service_;
};

TEST_F(VPNServiceTest, Connect) {
  EXPECT_TRUE(service_->connectable());
  Error error;
  EXPECT_CALL(*driver_, Connect(_, &error));
  service_->Connect(&error);
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(VPNServiceTest, ConnectAlreadyConnected) {
  Error error;
  EXPECT_CALL(*driver_, Connect(_, _)).Times(0);
  SetServiceState(Service::kStateOnline);
  service_->Connect(&error);
  EXPECT_EQ(Error::kAlreadyConnected, error.type());
  error.Reset();
  SetServiceState(Service::kStateConfiguring);
  service_->Connect(&error);
  EXPECT_EQ(Error::kAlreadyConnected, error.type());
}

TEST_F(VPNServiceTest, Disconnect) {
  Error error;
  EXPECT_CALL(*driver_, Disconnect());
  service_->Disconnect(&error);
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(VPNServiceTest, CreateStorageIdentifierNoHost) {
  KeyValueStore args;
  Error error;
  args.SetString(flimflam::kProviderNameProperty, "vpn-name");
  EXPECT_EQ("", VPNService::CreateStorageIdentifier(args, &error));
  EXPECT_EQ(Error::kInvalidProperty, error.type());
}

TEST_F(VPNServiceTest, CreateStorageIdentifierNoName) {
  KeyValueStore args;
  Error error;
  args.SetString(flimflam::kProviderHostProperty, "10.8.0.1");
  EXPECT_EQ("", VPNService::CreateStorageIdentifier(args, &error));
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_F(VPNServiceTest, CreateStorageIdentifier) {
  KeyValueStore args;
  Error error;
  args.SetString(flimflam::kProviderNameProperty, "vpn-name");
  args.SetString(flimflam::kProviderHostProperty, "10.8.0.1");
  EXPECT_EQ("vpn_10_8_0_1_vpn_name",
            VPNService::CreateStorageIdentifier(args, &error));
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(VPNServiceTest, GetStorageIdentifier) {
  EXPECT_EQ("", service_->GetStorageIdentifier());
  service_->set_storage_id("foo");
  EXPECT_EQ("foo", service_->GetStorageIdentifier());
}

TEST_F(VPNServiceTest, GetDeviceRpcId) {
  Error error;
  EXPECT_EQ("/", service_->GetDeviceRpcId(&error));
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_F(VPNServiceTest, Load) {
  NiceMock<MockStore> storage;
  static const char kStorageID[] = "storage-id";
  service_->set_storage_id(kStorageID);
  EXPECT_CALL(storage, ContainsGroup(kStorageID)).WillOnce(Return(true));
  EXPECT_CALL(*driver_, Load(&storage, kStorageID))
      .WillOnce(Return(true));
  EXPECT_TRUE(service_->Load(&storage));
}

TEST_F(VPNServiceTest, Save) {
  NiceMock<MockStore> storage;
  static const char kStorageID[] = "storage-id";
  service_->set_storage_id(kStorageID);
  EXPECT_CALL(*driver_, Save(&storage, kStorageID, false))
      .WillOnce(Return(true));
  EXPECT_TRUE(service_->Save(&storage));
}

TEST_F(VPNServiceTest, SaveCredentials) {
  NiceMock<MockStore> storage;
  static const char kStorageID[] = "storage-id";
  service_->set_storage_id(kStorageID);
  service_->set_save_credentials(true);
  EXPECT_CALL(*driver_, Save(&storage, kStorageID, true))
      .WillOnce(Return(true));
  EXPECT_TRUE(service_->Save(&storage));
}

TEST_F(VPNServiceTest, Unload) {
  service_->set_auto_connect(true);
  service_->set_save_credentials(true);
  EXPECT_CALL(*driver_, Disconnect());
  EXPECT_CALL(*driver_, UnloadCredentials());
  MockVPNProvider *provider = new MockVPNProvider;
  SetVPNProvider(provider);
  provider->services_.push_back(service_);
  service_->Unload();
  EXPECT_FALSE(service_->auto_connect());
  EXPECT_FALSE(service_->save_credentials());
  EXPECT_TRUE(provider->services_.empty());
}

TEST_F(VPNServiceTest, InitPropertyStore) {
  EXPECT_CALL(*driver_, InitPropertyStore(service_->mutable_store()));
  service_->InitDriverPropertyStore();
}

TEST_F(VPNServiceTest, MakeFavorite) {
  EXPECT_FALSE(service_->favorite());
  EXPECT_FALSE(service_->auto_connect());
  service_->MakeFavorite();
  EXPECT_TRUE(service_->favorite());
  EXPECT_FALSE(service_->auto_connect());
}

TEST_F(VPNServiceTest, SetConnection) {
  EXPECT_FALSE(service_->connection_binder_.get());
  EXPECT_FALSE(service_->connection());
  EXPECT_CALL(*sockets_, Socket(_, _, _)).WillOnce(Return(-1));
  service_->SetConnection(connection_);
  ASSERT_TRUE(service_->connection_binder_.get());
  EXPECT_EQ(connection_.get(),
            service_->connection_binder_->connection().get());
  EXPECT_EQ(connection_.get(), service_->connection().get());
  EXPECT_CALL(*driver_, OnConnectionDisconnected()).Times(0);
}

TEST_F(VPNServiceTest, OnConnectionDisconnected) {
  EXPECT_CALL(*sockets_, Socket(_, _, _)).WillOnce(Return(-1));
  service_->SetConnection(connection_);
  EXPECT_CALL(*driver_, OnConnectionDisconnected()).Times(1);
  connection_->OnLowerDisconnect();
}

TEST_F(VPNServiceTest, IsAutoConnectableOffline) {
  EXPECT_TRUE(service_->connectable());
  const char *reason = NULL;
  EXPECT_CALL(manager_, IsOnline()).WillOnce(Return(false));
  EXPECT_FALSE(IsAutoConnectable(&reason));
  EXPECT_STREQ(GetAutoConnOffline(), reason);
}

TEST_F(VPNServiceTest, IsAutoConnectableNeverConnected) {
  EXPECT_TRUE(service_->connectable());
  EXPECT_FALSE(service_->has_ever_connected());
  const char *reason = NULL;
  EXPECT_CALL(manager_, IsOnline()).WillOnce(Return(true));
  EXPECT_FALSE(IsAutoConnectable(&reason));
  EXPECT_STREQ(GetAutoConnNeverConnected(), reason);
}

TEST_F(VPNServiceTest, IsAutoConnectableVPNAlreadyActive) {
  EXPECT_TRUE(service_->connectable());
  SetHasEverConnected(true);
  EXPECT_CALL(manager_, IsOnline()).WillOnce(Return(true));
  MockVPNProvider *provider = new MockVPNProvider;
  SetVPNProvider(provider);
  EXPECT_CALL(*provider, HasActiveService()).WillOnce(Return(true));
  const char *reason = NULL;
  EXPECT_FALSE(IsAutoConnectable(&reason));
  EXPECT_STREQ(GetAutoConnVPNAlreadyActive(), reason);
}

TEST_F(VPNServiceTest, IsAutoConnectableNotConnectable) {
  const char *reason = NULL;
  SetConnectable(false);
  EXPECT_FALSE(IsAutoConnectable(&reason));
}

TEST_F(VPNServiceTest, IsAutoConnectable) {
  EXPECT_TRUE(service_->connectable());
  SetHasEverConnected(true);
  EXPECT_CALL(manager_, IsOnline()).WillOnce(Return(true));
  MockVPNProvider *provider = new MockVPNProvider;
  SetVPNProvider(provider);
  EXPECT_CALL(*provider, HasActiveService()).WillOnce(Return(false));
  const char *reason = NULL;
  EXPECT_TRUE(IsAutoConnectable(&reason));
  EXPECT_FALSE(reason);
}

TEST_F(VPNServiceTest, SetNamePropertyTrivial) {
  DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                       flimflam::kNameProperty,
                                       DBusAdaptor::StringToVariant(
                                           service_->friendly_name()),
                                       &error));
}

TEST_F(VPNServiceTest, SetNameProperty) {
  const string kHost = "1.2.3.4";
  driver_->args()->SetString(flimflam::kProviderHostProperty, kHost);
  string kOldId = service_->GetStorageIdentifier();
  DBus::Error error;
  const string kName = "New Name";
  scoped_refptr<MockProfile> profile(
      new MockProfile(&control_, &metrics_, &manager_));
  EXPECT_CALL(*profile, DeleteEntry(kOldId, _));
  EXPECT_CALL(*profile, UpdateService(_));
  service_->set_profile(profile);
  EXPECT_TRUE(DBusAdaptor::SetProperty(service_->mutable_store(),
                                       flimflam::kNameProperty,
                                       DBusAdaptor::StringToVariant(kName),
                                       &error));
  EXPECT_NE(service_->GetStorageIdentifier(), kOldId);
  EXPECT_EQ(kName, service_->friendly_name());
}

}  // namespace shill
