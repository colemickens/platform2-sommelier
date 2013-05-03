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
#include "shill/mock_eap_credentials.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_store.h"
#include "shill/mock_wimax.h"
#include "shill/mock_wimax_network_proxy.h"
#include "shill/mock_wimax_provider.h"
#include "shill/service_property_change_test.h"

using std::string;
using testing::_;
using testing::Mock;
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
        metrics_(static_cast<EventDispatcher *>(NULL)),
        device_(new MockWiMax(&control_, NULL, &metrics_, &manager_,
                              kTestLinkName, kTestAddress, kTestInterfaceIndex,
                              kTestPath)),
        service_(new WiMaxService(&control_, NULL, &metrics_, &manager_)),
        eap_(new MockEapCredentials()) {
    service_->set_friendly_name(kTestName);
    service_->set_network_id(kTestNetworkId);
    service_->InitStorageIdentifier();
    service_->eap_.reset(eap_);  // Passes ownership.
  }

  virtual ~WiMaxServiceTest() {}

 protected:
  virtual void TearDown() {
    service_->device_ = NULL;
  }

  void ExpectUpdateService() {
    EXPECT_CALL(manager_, HasService(_)).WillOnce(Return(true));
    EXPECT_CALL(manager_, UpdateService(_));
  }

  void SetConnectable(bool connectable) {
    service_->connectable_ = connectable;
  }

  void SetDevice(WiMaxRefPtr device) {
    service_->SetDevice(device);
  }

  ServiceMockAdaptor *GetAdaptor() {
    return dynamic_cast<ServiceMockAdaptor *>(service_->adaptor());
  }

  scoped_ptr<MockWiMaxNetworkProxy> proxy_;
  NiceMockControl control_;
  MockManager manager_;
  NiceMock<MockMetrics> metrics_;
  scoped_refptr<MockWiMax> device_;
  WiMaxServiceRefPtr service_;
  MockEapCredentials *eap_;  // Owned by |service_|.
};

TEST_F(WiMaxServiceTest, GetConnectParameters) {
  KeyValueStore parameters;
  EXPECT_CALL(*eap_, PopulateWiMaxProperties(&parameters));
  service_->GetConnectParameters(&parameters);
}

TEST_F(WiMaxServiceTest, GetDeviceRpcId) {
  Error error;
  EXPECT_EQ("/", service_->GetDeviceRpcId(&error));
  EXPECT_EQ(Error::kNotFound, error.type());
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
  EXPECT_FALSE(service_->IsVisible());
  EXPECT_EQ(0, service_->strength());
  EXPECT_FALSE(service_->proxy_.get());
  EXPECT_CALL(*proxy_, Name(_)).WillOnce(Return(kName));
  EXPECT_CALL(*proxy_, Identifier(_)).WillOnce(Return(kIdentifier));
  EXPECT_CALL(*proxy_, SignalStrength(_)).WillOnce(Return(kStrength));
  EXPECT_CALL(*proxy_, set_signal_strength_changed_callback(_));
  ExpectUpdateService();
  service_->need_passphrase_ = false;
  EXPECT_TRUE(service_->Start(proxy_.release()));
  EXPECT_TRUE(service_->IsStarted());
  EXPECT_TRUE(service_->IsVisible());
  EXPECT_EQ(kStrength, service_->strength());
  EXPECT_EQ(kName, service_->network_name());
  EXPECT_EQ(kTestName, service_->friendly_name());
  EXPECT_EQ(kTestNetworkId, service_->network_id());
  EXPECT_TRUE(service_->connectable());
  EXPECT_TRUE(service_->proxy_.get());

  service_->device_ = device_;
  EXPECT_CALL(*device_, OnServiceStopped(_));
  ExpectUpdateService();
  service_->Stop();
  EXPECT_FALSE(service_->IsStarted());
  EXPECT_FALSE(service_->IsVisible());
  EXPECT_EQ(0, service_->strength());
  EXPECT_FALSE(service_->proxy_.get());
}

TEST_F(WiMaxServiceTest, Connectable) {
  EXPECT_TRUE(service_->Is8021x());
  EXPECT_TRUE(service_->need_passphrase_);
  EXPECT_FALSE(service_->connectable());

  EXPECT_CALL(*eap_, IsConnectableUsingPassphrase())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));

  // No WiMaxCredentials.
  service_->OnEapCredentialsChanged();
  EXPECT_TRUE(service_->need_passphrase_);
  EXPECT_FALSE(service_->connectable());

  // Not started (no proxy).
  service_->OnEapCredentialsChanged();
  EXPECT_FALSE(service_->need_passphrase_);
  EXPECT_FALSE(service_->connectable());

  // Connectable.
  service_->proxy_.reset(proxy_.release());
  ExpectUpdateService();
  service_->OnEapCredentialsChanged();
  EXPECT_FALSE(service_->need_passphrase_);
  EXPECT_TRUE(service_->connectable());

  // Reset WimaxConnectable state.
  Mock::VerifyAndClearExpectations(eap_);
  EXPECT_CALL(*eap_, set_password(""));
  EXPECT_CALL(*eap_, IsConnectableUsingPassphrase())
      .WillRepeatedly(Return(false));
  ExpectUpdateService();
  service_->ClearPassphrase();
  EXPECT_TRUE(service_->need_passphrase_);
  EXPECT_FALSE(service_->connectable());
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
  service_->Connect(&error, "in test");
  EXPECT_EQ(Error::kOperationFailed, error.type());
  SetConnectable(true);

  // No carrier device available.
  MockWiMaxProvider provider;
  scoped_refptr<MockWiMax> null_device;
  EXPECT_CALL(manager_, wimax_provider()).WillOnce(Return(&provider));
  EXPECT_CALL(provider, SelectCarrier(_)).WillOnce(Return(null_device));
  error.Reset();
  service_->Connect(&error, "in test");
  EXPECT_EQ(Error::kNoCarrier, error.type());

  // Successful connect.
  EXPECT_CALL(manager_, wimax_provider()).WillOnce(Return(&provider));
  EXPECT_CALL(provider, SelectCarrier(_)).WillOnce(Return(device_));
  EXPECT_CALL(*device_, ConnectTo(_, _));
  error.Reset();
  service_->Connect(&error, "in test");
  EXPECT_TRUE(error.IsSuccess());

  // Connect while already connected.
  // TODO(benchan): Check for error if we populate error again after changing
  // the way that Chrome handles Error::kAlreadyConnected situation.
  service_->Connect(&error, "in test");

  // Successful disconnect.
  EXPECT_CALL(*device_, DisconnectFrom(_, _));
  EXPECT_CALL(*eap_, set_password(""));
  EXPECT_CALL(*eap_, IsConnectableUsingPassphrase())
      .WillRepeatedly(Return(false));
  error.Reset();
  ExpectUpdateService();
  service_->Disconnect(&error);
  EXPECT_TRUE(error.IsSuccess());

  // Verify that the EAP passphrase is cleared after the service is explicitly
  // disconnected.
  // TODO(benchan): Remove this check once WiMaxService no longer uses this
  // workaroud to prompt the user for EAP credentials.
  EXPECT_TRUE(service_->need_passphrase_);
  EXPECT_FALSE(service_->connectable());

  // Disconnect while not connected.
  service_->Disconnect(&error);
  EXPECT_EQ(Error::kNotConnected, error.type());
}

TEST_F(WiMaxServiceTest, Unload) {
  MockWiMaxProvider provider;
  EXPECT_CALL(manager_, wimax_provider())
      .Times(2)
      .WillRepeatedly(Return(&provider));
  EXPECT_CALL(*eap_, Reset());
  EXPECT_CALL(*eap_, set_password(""));
  EXPECT_CALL(*eap_, IsConnectableUsingPassphrase())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider, OnServiceUnloaded(_)).WillOnce(Return(false));
  EXPECT_FALSE(service_->Unload());
  Mock::VerifyAndClearExpectations(eap_);

  EXPECT_CALL(*eap_, Reset());
  EXPECT_CALL(*eap_, set_password(""));
  EXPECT_CALL(*eap_, IsConnectableUsingPassphrase())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(provider, OnServiceUnloaded(_)).WillOnce(Return(true));
  EXPECT_TRUE(service_->Unload());
}

TEST_F(WiMaxServiceTest, SetState) {
  service_->device_ = device_;
  EXPECT_EQ(Service::kStateIdle, service_->state());

  EXPECT_CALL(manager_, UpdateService(_));
  service_->SetState(Service::kStateAssociating);
  EXPECT_EQ(Service::kStateAssociating, service_->state());
  EXPECT_TRUE(service_->device_);

  EXPECT_CALL(manager_, UpdateService(_));
  service_->SetState(Service::kStateFailure);
  EXPECT_EQ(Service::kStateFailure, service_->state());
  EXPECT_FALSE(service_->device_);
}

TEST_F(WiMaxServiceTest, IsAutoConnectable) {
  EXPECT_FALSE(service_->connectable());
  const char *reason = "";

  EXPECT_FALSE(service_->IsAutoConnectable(&reason));

  MockWiMaxProvider provider;
  EXPECT_CALL(manager_, wimax_provider())
      .Times(2)
      .WillRepeatedly(Return(&provider));

  SetConnectable(true);
  EXPECT_CALL(provider, SelectCarrier(_)).WillOnce(Return(device_));
  EXPECT_CALL(*device_, IsIdle()).WillOnce(Return(false));
  reason = "";
  EXPECT_FALSE(service_->IsAutoConnectable(&reason));
  EXPECT_EQ(WiMaxService::kAutoConnBusy, reason);

  EXPECT_CALL(provider, SelectCarrier(_)).WillOnce(Return(device_));
  EXPECT_CALL(*device_, IsIdle()).WillOnce(Return(true));
  reason = "";
  EXPECT_TRUE(service_->IsAutoConnectable(&reason));
  EXPECT_STREQ("", reason);
}

TEST_F(WiMaxServiceTest, PropertyChanges) {
  ServiceMockAdaptor *adaptor = GetAdaptor();
  TestCommonPropertyChanges(service_, adaptor);
  TestAutoConnectPropertyChange(service_, adaptor);

  EXPECT_CALL(*adaptor,
              EmitRpcIdentifierChanged(flimflam::kDeviceProperty, _));
  SetDevice(device_);
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_CALL(*adaptor,
              EmitRpcIdentifierChanged(flimflam::kDeviceProperty, _));
  SetDevice(NULL);
  Mock::VerifyAndClearExpectations(adaptor);
}

// Custom property setters should return false, and make no changes, if
// the new value is the same as the old value.
TEST_F(WiMaxServiceTest, CustomSetterNoopChange) {
  TestCustomSetterNoopChange(service_, &manager_);
}

}  // namespace shill
