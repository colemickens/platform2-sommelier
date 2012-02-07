// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_modem_cdma_proxy.h"
#include "shill/mock_modem_gsm_card_proxy.h"
#include "shill/mock_modem_gsm_network_proxy.h"
#include "shill/mock_modem_proxy.h"
#include "shill/mock_modem_simple_proxy.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using std::string;
using testing::InSequence;
using testing::NiceMock;

namespace shill {

class CellularCapabilityTest : public testing::Test {
 public:
  CellularCapabilityTest()
      : cellular_(new Cellular(&control_,
                               &dispatcher_,
                               NULL,
                               NULL,
                               "",
                               "",
                               0,
                               Cellular::kTypeGSM,
                               "",
                               "",
                               NULL)),
        proxy_(new MockModemProxy()),
        simple_proxy_(new MockModemSimpleProxy()),
        cdma_proxy_(new MockModemCDMAProxy()),
        gsm_card_proxy_(new MockModemGSMCardProxy()),
        gsm_network_proxy_(new MockModemGSMNetworkProxy()),
        proxy_factory_(this),
        capability_(NULL),
        device_adaptor_(NULL) {}

  virtual ~CellularCapabilityTest() {
    cellular_->service_ = NULL;
    capability_ = NULL;
    device_adaptor_ = NULL;
  }

  virtual void SetUp() {
    capability_ = cellular_->capability_.get();
    device_adaptor_ =
        dynamic_cast<NiceMock<DeviceMockAdaptor> *>(cellular_->adaptor());
  }

  virtual void TearDown() {
    capability_->proxy_factory_ = NULL;
  }

 protected:
  static const char kTestCarrier[];

  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(CellularCapabilityTest *test) : test_(test) {}

    virtual ModemProxyInterface *CreateModemProxy(
        ModemProxyDelegate */*delegate*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->proxy_.release();
    }

    virtual ModemSimpleProxyInterface *CreateModemSimpleProxy(
        ModemSimpleProxyDelegate */*delegate*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->simple_proxy_.release();
    }

    virtual ModemCDMAProxyInterface *CreateModemCDMAProxy(
        ModemCDMAProxyDelegate */*delegate*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->cdma_proxy_.release();
    }

    virtual ModemGSMCardProxyInterface *CreateModemGSMCardProxy(
        ModemGSMCardProxyDelegate */*delegate*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->gsm_card_proxy_.release();
    }

    virtual ModemGSMNetworkProxyInterface *CreateModemGSMNetworkProxy(
        ModemGSMNetworkProxyDelegate */*delegate*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->gsm_network_proxy_.release();
    }

   private:
    CellularCapabilityTest *test_;
  };

  void SetProxy() {
    capability_->proxy_.reset(proxy_.release());
  }

  void SetSimpleProxy() {
    capability_->simple_proxy_.reset(simple_proxy_.release());
  }

  void SetCellularType(Cellular::Type type) {
    cellular_->InitCapability(type, &proxy_factory_);
    capability_ = cellular_->capability_.get();
  }

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  CellularRefPtr cellular_;
  scoped_ptr<MockModemProxy> proxy_;
  scoped_ptr<MockModemSimpleProxy> simple_proxy_;
  scoped_ptr<MockModemCDMAProxy> cdma_proxy_;
  scoped_ptr<MockModemGSMCardProxy> gsm_card_proxy_;
  scoped_ptr<MockModemGSMNetworkProxy> gsm_network_proxy_;
  TestProxyFactory proxy_factory_;
  CellularCapability *capability_;  // Owned by |cellular_|.
  NiceMock<DeviceMockAdaptor> *device_adaptor_;  // Owned by |cellular_|.
};

const char CellularCapabilityTest::kTestCarrier[] = "The Cellular Carrier";

TEST_F(CellularCapabilityTest, GetModemStatus) {
  SetCellularType(Cellular::kTypeCDMA);
  DBusPropertiesMap props;
  props["carrier"].writer().append_string(kTestCarrier);
  props["unknown-property"].writer().append_string("irrelevant-value");
  EXPECT_CALL(*simple_proxy_,
              GetModemStatus(NULL, CellularCapability::kTimeoutDefault));
  SetSimpleProxy();
  capability_->GetModemStatus(NULL);
  capability_->OnGetModemStatusCallback(props, Error(), NULL);
  EXPECT_EQ(kTestCarrier, capability_->carrier_);
  EXPECT_EQ(kTestCarrier, cellular_->home_provider_.GetName());
}

TEST_F(CellularCapabilityTest, GetModemInfo) {
  static const char kManufacturer[] = "Company";
  static const char kModelID[] = "Gobi 2000";
  static const char kHWRev[] = "A00B1234";
  ModemHardwareInfo info;
  info._1 = kManufacturer;
  info._2 = kModelID;
  info._3 = kHWRev;
  EXPECT_CALL(*proxy_, GetModemInfo(NULL, CellularCapability::kTimeoutDefault));
  SetProxy();
  capability_->GetModemInfo(NULL);
  capability_->OnGetModemInfoCallback(info, Error(), NULL);
  EXPECT_EQ(kManufacturer, capability_->manufacturer_);
  EXPECT_EQ(kModelID, capability_->model_id_);
  EXPECT_EQ(kHWRev, capability_->hardware_revision_);
}

TEST_F(CellularCapabilityTest, EnableModemSucceed) {
  EXPECT_CALL(*proxy_, Enable(true, NULL, CellularCapability::kTimeoutDefault));
  ASSERT_EQ(Cellular::kStateDisabled, cellular_->state_);
  SetProxy();
  capability_->EnableModem(NULL);
  capability_->OnModemEnableCallback(Error(), NULL);
  EXPECT_EQ(Cellular::kStateEnabled, cellular_->state_);
}

TEST_F(CellularCapabilityTest, EnableModemFail) {
  EXPECT_CALL(*proxy_, Enable(true, NULL, CellularCapability::kTimeoutDefault));
  ASSERT_EQ(Cellular::kStateDisabled, cellular_->state_);
  SetProxy();
  capability_->EnableModem(NULL);
  capability_->OnModemEnableCallback(Error(Error::kOperationFailed), NULL);
  EXPECT_EQ(Cellular::kStateDisabled, cellular_->state_);
}

TEST_F(CellularCapabilityTest, AllowRoaming) {
  EXPECT_FALSE(capability_->GetAllowRoaming(NULL));
  capability_->SetAllowRoaming(false, NULL);
  EXPECT_FALSE(capability_->GetAllowRoaming(NULL));

  {
    InSequence seq;
    EXPECT_CALL(*device_adaptor_, EmitBoolChanged(
        flimflam::kCellularAllowRoamingProperty, true));
    EXPECT_CALL(*device_adaptor_, EmitBoolChanged(
        flimflam::kCellularAllowRoamingProperty, false));
  }

  cellular_->state_ = Cellular::kStateConnected;
  dynamic_cast<CellularCapabilityGSM *>(capability_)->registration_state_ =
      MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING;
  capability_->SetAllowRoaming(true, NULL);
  EXPECT_TRUE(capability_->GetAllowRoaming(NULL));
  EXPECT_EQ(Cellular::kStateConnected, cellular_->state_);

  EXPECT_CALL(*proxy_, Disconnect());
  SetProxy();
  cellular_->state_ = Cellular::kStateConnected;
  capability_->SetAllowRoaming(false, NULL);
  EXPECT_FALSE(capability_->GetAllowRoaming(NULL));
  EXPECT_EQ(Cellular::kStateRegistered, cellular_->state_);
}

}  // namespace shill
