// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet.h"

#include <netinet/ether.h>
#include <linux/if.h>  // Needs definitions from netinet/ether.h

#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/mock_device_info.h"
#include "shill/mock_dhcp_config.h"
#include "shill/mock_dhcp_provider.h"
#include "shill/mock_eap_listener.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/nice_mock_control.h"

using testing::_;
using testing::AnyNumber;
using testing::Eq;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

namespace shill {

class EthernetTest : public testing::Test {
 public:
  EthernetTest()
      : metrics_(NULL),
        manager_(&control_interface_, NULL, &metrics_, &glib_),
        device_info_(&control_interface_, &dispatcher_, &metrics_, &manager_),
        ethernet_(new Ethernet(&control_interface_,
                               &dispatcher_,
                               &metrics_,
                               &manager_,
                               kDeviceName,
                               kDeviceAddress,
                               kInterfaceIndex)),
        dhcp_config_(new MockDHCPConfig(&control_interface_,
                                        kDeviceName)),
        eap_listener_(new MockEapListener()) {
    ON_CALL(dhcp_provider_, CreateConfig(_, _, _, _)).
        WillByDefault(Return(dhcp_config_));
    ON_CALL(*dhcp_config_.get(), RequestIP()).
        WillByDefault(Return(true));
  }

  virtual void SetUp() {
    ethernet_->rtnl_handler_ = &rtnl_handler_;
    // Transfers ownership.
    ethernet_->eap_listener_.reset(eap_listener_);

    ethernet_->set_dhcp_provider(&dhcp_provider_);
    ON_CALL(manager_, device_info()).WillByDefault(Return(&device_info_));
    EXPECT_CALL(manager_, UpdateEnabledTechnologies()).Times(AnyNumber());
  }

  virtual void TearDown() {
    ethernet_->set_dhcp_provider(NULL);
    ethernet_->eap_listener_.reset();
  }

 protected:
  static const char kDeviceName[];
  static const char kDeviceAddress[];
  static const int kInterfaceIndex;

  void StartEthernet() {
    EXPECT_CALL(rtnl_handler_,
                SetInterfaceFlags(kInterfaceIndex, IFF_UP, IFF_UP));
    ethernet_->Start(NULL, EnabledStateChangedCallback());
  }
  bool GetLinkUp() { return ethernet_->link_up_; }
  bool GetIsEapDetected() { return ethernet_->is_eap_detected_; }
  void SetIsEapDetected(bool is_eap_detected) {
    ethernet_->is_eap_detected_ = is_eap_detected;
  }
  const PropertyStore &GetStore() { return ethernet_->store(); }
  const ServiceRefPtr &GetService() { return ethernet_->service_; }
  void TriggerOnEapDetected() { ethernet_->OnEapDetected(); }

  StrictMock<MockEventDispatcher> dispatcher_;
  MockGLib glib_;
  NiceMockControl control_interface_;
  MockMetrics metrics_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  EthernetRefPtr ethernet_;
  MockDHCPProvider dhcp_provider_;
  scoped_refptr<MockDHCPConfig> dhcp_config_;
  MockRTNLHandler rtnl_handler_;
  // Owned by Ethernet instance, but tracked here for expectations.
  MockEapListener *eap_listener_;
};

// static
const char EthernetTest::kDeviceName[] = "eth0";
const char EthernetTest::kDeviceAddress[] = "000102030405";
const int EthernetTest::kInterfaceIndex = 123;

TEST_F(EthernetTest, Construct) {
  EXPECT_FALSE(GetLinkUp());
  EXPECT_FALSE(GetIsEapDetected());
  EXPECT_TRUE(GetStore().Contains(kEapAuthenticatorDetectedProperty));
  EXPECT_EQ(NULL, GetService().get());
}

TEST_F(EthernetTest, StartStop) {
  StartEthernet();

  EXPECT_FALSE(GetService().get() == NULL);

  Service* service = GetService().get();
  EXPECT_CALL(manager_, DeregisterService(Eq(service)));
  ethernet_->Stop(NULL, EnabledStateChangedCallback());
  EXPECT_EQ(NULL, GetService().get());
}

TEST_F(EthernetTest, LinkEvent) {
  StartEthernet();

  // Link-down event while already down.
  EXPECT_CALL(manager_, DeregisterService(_)).Times(0);
  EXPECT_CALL(*eap_listener_, Start()).Times(0);
  ethernet_->LinkEvent(0, IFF_LOWER_UP);
  EXPECT_FALSE(GetLinkUp());
  EXPECT_FALSE(GetIsEapDetected());
  Mock::VerifyAndClearExpectations(&manager_);

  // Link-up event while down.
  EXPECT_CALL(manager_, RegisterService(GetService()));
  EXPECT_CALL(*eap_listener_, Start());
  ethernet_->LinkEvent(IFF_LOWER_UP, 0);
  EXPECT_TRUE(GetLinkUp());
  EXPECT_FALSE(GetIsEapDetected());
  Mock::VerifyAndClearExpectations(&manager_);

  // Link-up event while already up.
  EXPECT_CALL(manager_, RegisterService(GetService())).Times(0);
  EXPECT_CALL(*eap_listener_, Start()).Times(0);
  ethernet_->LinkEvent(IFF_LOWER_UP, 0);
  EXPECT_TRUE(GetLinkUp());
  EXPECT_FALSE(GetIsEapDetected());
  Mock::VerifyAndClearExpectations(&manager_);

  // Link-down event while up.
  SetIsEapDetected(true);
  EXPECT_CALL(manager_, DeregisterService(GetService()));
  EXPECT_CALL(*eap_listener_, Stop());
  ethernet_->LinkEvent(0, IFF_LOWER_UP);
  EXPECT_FALSE(GetLinkUp());
  EXPECT_FALSE(GetIsEapDetected());

  // Restore this expectation during shutdown.
  EXPECT_CALL(manager_, UpdateEnabledTechnologies()).Times(AnyNumber());
}

TEST_F(EthernetTest, OnEapDetected) {
  EXPECT_FALSE(GetIsEapDetected());
  EXPECT_CALL(*eap_listener_, Stop());
  TriggerOnEapDetected();
  EXPECT_TRUE(GetIsEapDetected());
}

}  // namespace shill
