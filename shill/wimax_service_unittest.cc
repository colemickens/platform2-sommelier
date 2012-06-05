// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_service.h"

#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_store.h"
#include "shill/mock_wimax.h"
#include "shill/mock_wimax_network_proxy.h"
#include "shill/mock_wimax_provider.h"

using std::string;
using testing::_;
using testing::NiceMock;
using testing::Return;
using wimax_manager::kEAPAnonymousIdentity;
using wimax_manager::kEAPUserIdentity;
using wimax_manager::kEAPUserPassword;

namespace shill {

namespace {

const char kTestLinkName[] = "wm0";
const char kTestAddress[] = "0123456789AB";
const int kTestInterfaceIndex = 5;
const char kTestPath[] = "/org/chromium/WiMaxManager/Device/wm7";
const char kTestName[] = "Test WiMAX Network";
const char kTestNetworkId[] = "1234abcd";

}  // namespace

class WiMaxServiceTest : public testing::Test {
 public:
  WiMaxServiceTest()
      : proxy_(new MockWiMaxNetworkProxy()),
        manager_(&control_, NULL, NULL, NULL),
        device_(new MockWiMax(&control_, NULL, &metrics_, &manager_,
                              kTestLinkName, kTestAddress, kTestInterfaceIndex,
                              kTestPath)),
        service_(new WiMaxService(&control_, NULL, &metrics_, &manager_)) {
    service_->set_friendly_name(kTestName);
    service_->set_network_id(kTestNetworkId);
    service_->InitStorageIdentifier();
  }

  virtual ~WiMaxServiceTest() {}

 protected:
  virtual void TearDown() {
    service_->device_ = NULL;
  }

  scoped_ptr<MockWiMaxNetworkProxy> proxy_;
  NiceMockControl control_;
  MockManager manager_;
  MockMetrics metrics_;
  scoped_refptr<MockWiMax> device_;
  WiMaxServiceRefPtr service_;
};

TEST_F(WiMaxServiceTest, GetConnectParameters) {
  {
    KeyValueStore parameters;
    service_->GetConnectParameters(&parameters);

    EXPECT_FALSE(parameters.ContainsString(kEAPAnonymousIdentity));
    EXPECT_FALSE(parameters.ContainsString(kEAPUserIdentity));
    EXPECT_FALSE(parameters.ContainsString(kEAPUserPassword));
  }
  {
    Service::EapCredentials eap;
    eap.anonymous_identity = "TestAnonymousIdentity";
    eap.identity = "TestUserIdentity";
    eap.password = "TestPassword";
    service_->set_eap(eap);

    KeyValueStore parameters;
    service_->GetConnectParameters(&parameters);

    EXPECT_EQ(eap.anonymous_identity,
              parameters.LookupString(kEAPAnonymousIdentity, ""));
    EXPECT_EQ(eap.identity, parameters.LookupString(kEAPUserIdentity, ""));
    EXPECT_EQ(eap.password, parameters.LookupString(kEAPUserPassword, ""));
  }
}

TEST_F(WiMaxServiceTest, GetDeviceRpcId) {
  Error error;
  EXPECT_EQ("/", service_->GetDeviceRpcId(&error));
  EXPECT_EQ(Error::kNotSupported, error.type());
  service_->device_ = device_;
  error.Reset();
  EXPECT_EQ(DeviceMockAdaptor::kRpcId, service_->GetDeviceRpcId(&error));
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(WiMaxServiceTest, OnSignalStrengthChanged) {
  const int kStrength = 55;
  service_->OnSignalStrengthChanged(kStrength);
  EXPECT_EQ(kStrength, service_->strength());
}

TEST_F(WiMaxServiceTest, StartStop) {
  static const char kName[] = "My WiMAX Network";
  const uint32 kIdentifier = 0x1234abcd;
  const int kStrength = 66;
  EXPECT_FALSE(service_->connectable());
  EXPECT_FALSE(service_->IsStarted());
  EXPECT_EQ(0, service_->strength());
  EXPECT_FALSE(service_->proxy_.get());
  EXPECT_CALL(*proxy_, Name(_)).WillOnce(Return(kName));
  EXPECT_CALL(*proxy_, Identifier(_)).WillOnce(Return(kIdentifier));
  EXPECT_CALL(*proxy_, SignalStrength(_)).WillOnce(Return(kStrength));
  EXPECT_CALL(*proxy_, set_signal_strength_changed_callback(_));
  service_->need_passphrase_ = false;
  EXPECT_TRUE(service_->Start(proxy_.release()));
  EXPECT_TRUE(service_->IsStarted());
  EXPECT_EQ(kStrength, service_->strength());
  EXPECT_EQ(kName, service_->network_name());
  EXPECT_EQ(kTestName, service_->friendly_name());
  EXPECT_EQ(kTestNetworkId, service_->network_id());
  EXPECT_TRUE(service_->connectable());
  EXPECT_TRUE(service_->proxy_.get());

  service_->device_ = device_;
  EXPECT_CALL(*device_, OnServiceStopped(_));
  service_->Stop();
  EXPECT_FALSE(service_->IsStarted());
  EXPECT_EQ(0, service_->strength());
  EXPECT_FALSE(service_->proxy_.get());
}

TEST_F(WiMaxServiceTest, SetEAP) {
  ServiceRefPtr base_service = service_;
  EXPECT_TRUE(base_service->Is8021x());
  EXPECT_TRUE(service_->need_passphrase_);
  EXPECT_FALSE(base_service->connectable());

  // No password.
  Service::EapCredentials eap;
  eap.identity = "TestIdentity";
  base_service->set_eap(eap);
  EXPECT_TRUE(service_->need_passphrase_);
  EXPECT_FALSE(base_service->connectable());

  // Not started.
  eap.password = "TestPassword";
  base_service->set_eap(eap);
  EXPECT_FALSE(service_->need_passphrase_);
  EXPECT_FALSE(base_service->connectable());

  // Connectable.
  service_->proxy_.reset(proxy_.release());
  base_service->set_eap(eap);
  EXPECT_FALSE(service_->need_passphrase_);
  EXPECT_TRUE(base_service->connectable());

  // Reset password.
  service_->ClearPassphrase();
  EXPECT_TRUE(service_->need_passphrase_);
  EXPECT_FALSE(base_service->connectable());
  EXPECT_TRUE(base_service->eap().password.empty());
}

TEST_F(WiMaxServiceTest, ConvertIdentifierToNetworkId) {
  EXPECT_EQ("00000000", WiMaxService::ConvertIdentifierToNetworkId(0));
  EXPECT_EQ("abcd1234", WiMaxService::ConvertIdentifierToNetworkId(0xabcd1234));
  EXPECT_EQ("ffffffff", WiMaxService::ConvertIdentifierToNetworkId(0xffffffff));
}

TEST_F(WiMaxServiceTest, StorageIdentifier) {
  static const char kStorageId[] = "wimax_test_wimax_network_1234abcd";
  EXPECT_EQ(kStorageId, service_->GetStorageIdentifier());
  EXPECT_EQ(kStorageId,
            WiMaxService::CreateStorageIdentifier(kTestNetworkId, kTestName));
}

TEST_F(WiMaxServiceTest, Save) {
  NiceMock<MockStore> storage;
  string storage_id = service_->GetStorageIdentifier();
  EXPECT_CALL(storage, SetString(storage_id, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(storage, DeleteKey(storage_id, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(storage, SetString(storage_id,
                                 WiMaxService::kStorageNetworkId,
                                 kTestNetworkId));
  EXPECT_TRUE(service_->Save(&storage));
}

TEST_F(WiMaxServiceTest, Connect) {
  // Connect but not connectable.
  Error error;
  EXPECT_FALSE(service_->connectable());
  service_->Connect(&error);
  EXPECT_EQ(Error::kOperationFailed, error.type());
  service_->set_connectable(true);

  // No carrier device available.
  MockWiMaxProvider provider;
  scoped_refptr<MockWiMax> null_device;
  EXPECT_CALL(manager_, wimax_provider()).WillOnce(Return(&provider));
  EXPECT_CALL(provider, SelectCarrier(_)).WillOnce(Return(null_device));
  error.Reset();
  service_->Connect(&error);
  EXPECT_EQ(Error::kNoCarrier, error.type());

  // Successful connect.
  EXPECT_CALL(manager_, wimax_provider()).WillOnce(Return(&provider));
  EXPECT_CALL(provider, SelectCarrier(_)).WillOnce(Return(device_));
  EXPECT_CALL(*device_, ConnectTo(_, _));
  error.Reset();
  service_->Connect(&error);
  EXPECT_TRUE(error.IsSuccess());

  // Connect while already connected.
  service_->Connect(&error);
  EXPECT_EQ(Error::kAlreadyConnected, error.type());

  // Successful disconnect.
  EXPECT_CALL(*device_, DisconnectFrom(_, _));
  error.Reset();
  service_->Disconnect(&error);
  EXPECT_TRUE(error.IsSuccess());

  // Disconnect while not connected.
  service_->Disconnect(&error);
  EXPECT_EQ(Error::kNotConnected, error.type());
}

TEST_F(WiMaxServiceTest, Unload) {
  MockWiMaxProvider provider;
  EXPECT_CALL(manager_, wimax_provider())
      .Times(2)
      .WillRepeatedly(Return(&provider));
  Service::EapCredentials eap;
  eap.identity = "TestUserIdentity";
  service_->set_eap(eap);
  EXPECT_CALL(provider, OnServiceUnloaded(_)).WillOnce(Return(false));
  EXPECT_FALSE(service_->Unload());
  EXPECT_TRUE(service_->eap().identity.empty());
  eap.identity = "TestUserIdentity";
  service_->set_eap(eap);
  EXPECT_CALL(provider, OnServiceUnloaded(_)).WillOnce(Return(true));
  EXPECT_TRUE(service_->Unload());
  EXPECT_TRUE(service_->eap().identity.empty());
}

TEST_F(WiMaxServiceTest, SetState) {
  service_->device_ = device_;
  ServiceRefPtr base_service = service_;
  EXPECT_EQ(Service::kStateIdle, service_->state());

  EXPECT_CALL(manager_, UpdateService(_));
  base_service->SetState(Service::kStateAssociating);
  EXPECT_EQ(Service::kStateAssociating, service_->state());
  EXPECT_TRUE(service_->device_);

  EXPECT_CALL(manager_, UpdateService(_));
  base_service->SetState(Service::kStateFailure);
  EXPECT_EQ(Service::kStateFailure, service_->state());
  EXPECT_FALSE(service_->device_);
}

}  // namespace shill
