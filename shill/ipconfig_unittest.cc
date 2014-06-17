// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ipconfig.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_adaptors.h"
#include "shill/mock_control.h"
#include "shill/mock_store.h"
#include "shill/static_ip_parameters.h"

using base::Bind;
using base::Unretained;
using std::string;
using testing::_;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {
const char kDeviceName[] = "testdevice";
}  // namespace

class IPConfigTest : public Test {
 public:
  IPConfigTest() : ipconfig_(new IPConfig(&control_, kDeviceName)) {}
  void DropRef(const IPConfigRefPtr &/*ipconfig*/) {
    ipconfig_ = NULL;
  }

  MOCK_METHOD1(OnIPConfigUpdated, void(const IPConfigRefPtr &ipconfig));
  MOCK_METHOD1(OnIPConfigFailed, void(const IPConfigRefPtr &ipconfig));
  MOCK_METHOD1(OnIPConfigRefreshed, void(const IPConfigRefPtr &ipconfig));
  MOCK_METHOD1(OnIPConfigExpired, void(const IPConfigRefPtr &ipconfig));

 protected:
  IPConfigMockAdaptor *GetAdaptor() {
    return dynamic_cast<IPConfigMockAdaptor *>(ipconfig_->adaptor_.get());
  }

  void UpdateProperties(const IPConfig::Properties &properties) {
    ipconfig_->UpdateProperties(properties);
  }

  void NotifyFailure() {
    ipconfig_->NotifyFailure();
  }

  void NotifyExpiry() {
    ipconfig_->NotifyExpiry();
  }

  void ExpectPropertiesEqual(const IPConfig::Properties &properties) {
    EXPECT_EQ(properties.address, ipconfig_->properties().address);
    EXPECT_EQ(properties.subnet_prefix, ipconfig_->properties().subnet_prefix);
    EXPECT_EQ(properties.broadcast_address,
              ipconfig_->properties().broadcast_address);
    EXPECT_EQ(properties.dns_servers.size(),
              ipconfig_->properties().dns_servers.size());
    if (properties.dns_servers.size() ==
        ipconfig_->properties().dns_servers.size()) {
      for (size_t i = 0; i < properties.dns_servers.size(); ++i) {
        EXPECT_EQ(properties.dns_servers[i],
                  ipconfig_->properties().dns_servers[i]);
      }
    }
    EXPECT_EQ(properties.domain_search.size(),
              ipconfig_->properties().domain_search.size());
    if (properties.domain_search.size() ==
        ipconfig_->properties().domain_search.size()) {
      for (size_t i = 0; i < properties.domain_search.size(); ++i) {
        EXPECT_EQ(properties.domain_search[i],
                  ipconfig_->properties().domain_search[i]);
      }
    }
    EXPECT_EQ(properties.gateway, ipconfig_->properties().gateway);
    EXPECT_EQ(properties.blackhole_ipv6,
              ipconfig_->properties().blackhole_ipv6);
    EXPECT_EQ(properties.mtu, ipconfig_->properties().mtu);
  }

  MockControl control_;
  IPConfigRefPtr ipconfig_;
};

TEST_F(IPConfigTest, DeviceName) {
  EXPECT_EQ(kDeviceName, ipconfig_->device_name());
}

TEST_F(IPConfigTest, RequestIP) {
  EXPECT_FALSE(ipconfig_->RequestIP());
}

TEST_F(IPConfigTest, RenewIP) {
  EXPECT_FALSE(ipconfig_->RenewIP());
}

TEST_F(IPConfigTest, ReleaseIP) {
  EXPECT_FALSE(ipconfig_->ReleaseIP(IPConfig::kReleaseReasonDisconnect));
}

TEST_F(IPConfigTest, UpdateProperties) {
  IPConfig::Properties properties;
  properties.address = "1.2.3.4";
  properties.subnet_prefix = 24;
  properties.broadcast_address = "11.22.33.44";
  properties.dns_servers.push_back("10.20.30.40");
  properties.dns_servers.push_back("20.30.40.50");
  properties.domain_name = "foo.org";
  properties.domain_search.push_back("zoo.org");
  properties.domain_search.push_back("zoo.com");
  properties.gateway = "5.6.7.8";
  properties.blackhole_ipv6 = true;
  properties.mtu = 700;
  UpdateProperties(properties);
  ExpectPropertiesEqual(properties);

  // We should not reset on NotifyFailure.
  NotifyFailure();
  ExpectPropertiesEqual(properties);

  // We should not reset on NotifyExpiry.
  NotifyExpiry();
  ExpectPropertiesEqual(properties);

  // We should reset if ResetProperties is called.
  ipconfig_->ResetProperties();
  ExpectPropertiesEqual(IPConfig::Properties());
}

TEST_F(IPConfigTest, Callbacks) {
  ipconfig_->RegisterUpdateCallback(
      Bind(&IPConfigTest::OnIPConfigUpdated, Unretained(this)));
  ipconfig_->RegisterFailureCallback(
      Bind(&IPConfigTest::OnIPConfigFailed, Unretained(this)));
  ipconfig_->RegisterRefreshCallback(
      Bind(&IPConfigTest::OnIPConfigRefreshed, Unretained(this)));
  ipconfig_->RegisterExpireCallback(
      Bind(&IPConfigTest::OnIPConfigExpired, Unretained(this)));

  EXPECT_CALL(*this, OnIPConfigUpdated(ipconfig_));
  EXPECT_CALL(*this, OnIPConfigFailed(ipconfig_)).Times(0);
  EXPECT_CALL(*this, OnIPConfigRefreshed(ipconfig_)).Times(0);
  EXPECT_CALL(*this, OnIPConfigExpired(ipconfig_)).Times(0);
  UpdateProperties(IPConfig::Properties());
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnIPConfigUpdated(ipconfig_)).Times(0);
  EXPECT_CALL(*this, OnIPConfigFailed(ipconfig_));
  EXPECT_CALL(*this, OnIPConfigRefreshed(ipconfig_)).Times(0);
  EXPECT_CALL(*this, OnIPConfigExpired(ipconfig_)).Times(0);
  NotifyFailure();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnIPConfigUpdated(ipconfig_)).Times(0);
  EXPECT_CALL(*this, OnIPConfigFailed(ipconfig_)).Times(0);
  EXPECT_CALL(*this, OnIPConfigRefreshed(ipconfig_));
  EXPECT_CALL(*this, OnIPConfigExpired(ipconfig_)).Times(0);
  ipconfig_->Refresh(NULL);
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, OnIPConfigUpdated(ipconfig_)).Times(0);
  EXPECT_CALL(*this, OnIPConfigFailed(ipconfig_)).Times(0);
  EXPECT_CALL(*this, OnIPConfigRefreshed(ipconfig_)).Times(0);
  EXPECT_CALL(*this, OnIPConfigExpired(ipconfig_));
  NotifyExpiry();
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(IPConfigTest, UpdatePropertiesWithDropRef) {
  // The UpdateCallback should be able to drop a reference to the
  // IPConfig object without crashing.
  ipconfig_->RegisterUpdateCallback(
      Bind(&IPConfigTest::DropRef, Unretained(this)));
  UpdateProperties(IPConfig::Properties());
}

TEST_F(IPConfigTest, PropertyChanges) {
  IPConfigMockAdaptor *adaptor = GetAdaptor();

  StaticIPParameters static_ip_params;
  EXPECT_CALL(*adaptor, EmitStringChanged(kAddressProperty, _));
  EXPECT_CALL(*adaptor, EmitStringsChanged(kNameServersProperty, _));
  ipconfig_->ApplyStaticIPParameters(&static_ip_params);
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_CALL(*adaptor, EmitStringChanged(kAddressProperty, _));
  EXPECT_CALL(*adaptor, EmitStringsChanged(kNameServersProperty, _));
  ipconfig_->RestoreSavedIPParameters(&static_ip_params);
  Mock::VerifyAndClearExpectations(adaptor);

  IPConfig::Properties ip_properties;
  EXPECT_CALL(*adaptor, EmitStringChanged(kAddressProperty, _));
  EXPECT_CALL(*adaptor, EmitStringsChanged(kNameServersProperty, _));
  UpdateProperties(ip_properties);
  Mock::VerifyAndClearExpectations(adaptor);

  // It is the callback's responsibility for resetting the IPConfig
  // properties (via IPConfig::ResetProperties()).  Since NotifyFailure
  // by itself doesn't change any properties, it should not emit any
  // property change events either.
  EXPECT_CALL(*adaptor, EmitStringChanged(_, _)).Times(0);
  EXPECT_CALL(*adaptor, EmitStringsChanged(_, _)).Times(0);
  NotifyFailure();
  Mock::VerifyAndClearExpectations(adaptor);

  // Similarly, NotifyExpiry() should have no property change side effects.
  EXPECT_CALL(*adaptor, EmitStringChanged(_, _)).Times(0);
  EXPECT_CALL(*adaptor, EmitStringsChanged(_, _)).Times(0);
  NotifyExpiry();
  Mock::VerifyAndClearExpectations(adaptor);

  EXPECT_CALL(*adaptor, EmitStringChanged(kAddressProperty, _));
  EXPECT_CALL(*adaptor, EmitStringsChanged(kNameServersProperty, _));
  ipconfig_->ResetProperties();
  Mock::VerifyAndClearExpectations(adaptor);
}

}  // namespace shill
