// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_modem_cdma_proxy.h"
#include "shill/mock_modem_gsm_card_proxy.h"
#include "shill/mock_modem_gsm_network_proxy.h"
#include "shill/mock_modem_proxy.h"
#include "shill/mock_modem_simple_proxy.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using base::Bind;
using base::Unretained;
using std::string;
using testing::InSequence;
using testing::NiceMock;
using testing::_;

namespace shill {

MATCHER(IsSuccess, "") {
  return arg.IsSuccess();
}
MATCHER(IsFailure, "") {
  return arg.IsFailure();
}

class CellularCapabilityTest : public testing::Test {
 public:
  CellularCapabilityTest()
      : manager_(&control_, &dispatcher_, &metrics_, &glib_),
        cellular_(new Cellular(&control_,
                               &dispatcher_,
                               NULL,
                               &manager_,
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
    static_cast<Device *>(cellular_)->rtnl_handler_ = &rtnl_handler_;
    capability_ = cellular_->capability_.get();
    device_adaptor_ =
        dynamic_cast<NiceMock<DeviceMockAdaptor> *>(cellular_->adaptor());
  }

  virtual void TearDown() {
    capability_->proxy_factory_ = NULL;
  }

  void InvokeEnable(bool enable, Error *error,
                    const ResultCallback &callback, int timeout) {
    callback.Run(Error());
  }
  void InvokeEnableFail(bool enable, Error *error,
                        const ResultCallback &callback, int timeout) {
    callback.Run(Error(Error::kOperationFailed));
  }
  void InvokeDisconnect(Error *error, const ResultCallback &callback,
                        int timeout) {
    callback.Run(Error());
  }
  void InvokeGetModemStatus(Error *error,
                            const DBusPropertyMapCallback &callback,
                            int timeout) {
    DBusPropertiesMap props;
    props["carrier"].writer().append_string(kTestCarrier);
    props["unknown-property"].writer().append_string("irrelevant-value");
    callback.Run(props, Error());
  }
  void InvokeGetModemInfo(Error *error, const ModemInfoCallback &callback,
                          int timeout) {
    ModemHardwareInfo info;
    info._1 = kManufacturer;
    info._2 = kModelID;
    info._3 = kHWRev;
    callback.Run(info, Error());
  }

  MOCK_METHOD1(TestCallback, void(const Error &error));

 protected:
  static const char kTestCarrier[];
  static const char kManufacturer[];
  static const char kModelID[];
  static const char kHWRev[];

  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(CellularCapabilityTest *test) : test_(test) {}

    virtual ModemProxyInterface *CreateModemProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->proxy_.release();
    }

    virtual ModemSimpleProxyInterface *CreateModemSimpleProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->simple_proxy_.release();
    }

    virtual ModemCDMAProxyInterface *CreateModemCDMAProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->cdma_proxy_.release();
    }

    virtual ModemGSMCardProxyInterface *CreateModemGSMCardProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->gsm_card_proxy_.release();
    }

    virtual ModemGSMNetworkProxyInterface *CreateModemGSMNetworkProxy(
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

  void SetGSMNetworkProxy() {
    CellularCapabilityGSM *gsm_capability =
        dynamic_cast<CellularCapabilityGSM *>(cellular_->capability_.get());
    gsm_capability->network_proxy_.reset(gsm_network_proxy_.release());
  }

  void SetCellularType(Cellular::Type type) {
    cellular_->InitCapability(type, &proxy_factory_);
    capability_ = cellular_->capability_.get();
  }

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;
  CellularRefPtr cellular_;
  MockRTNLHandler rtnl_handler_;
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
const char CellularCapabilityTest::kManufacturer[] = "Company";
const char CellularCapabilityTest::kModelID[] = "Gobi 2000";
const char CellularCapabilityTest::kHWRev[] = "A00B1234";

TEST_F(CellularCapabilityTest, GetModemStatus) {
  SetCellularType(Cellular::kTypeCDMA);
  EXPECT_CALL(*simple_proxy_,
              GetModemStatus(_, _, CellularCapability::kTimeoutDefault)).
      WillOnce(Invoke(this, &CellularCapabilityTest::InvokeGetModemStatus));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetSimpleProxy();
  ResultCallback callback =
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this));
  capability_->GetModemStatus(callback);
  EXPECT_EQ(kTestCarrier, capability_->carrier_);
  EXPECT_EQ(kTestCarrier, cellular_->home_provider_.GetName());
}

TEST_F(CellularCapabilityTest, GetModemInfo) {
  EXPECT_CALL(*proxy_, GetModemInfo(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeGetModemInfo));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetProxy();
  ResultCallback callback =
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this));
  capability_->GetModemInfo(callback);
  EXPECT_EQ(kManufacturer, capability_->manufacturer_);
  EXPECT_EQ(kModelID, capability_->model_id_);
  EXPECT_EQ(kHWRev, capability_->hardware_revision_);
}

TEST_F(CellularCapabilityTest, EnableModemSucceed) {
  EXPECT_CALL(*proxy_, Enable(true, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeEnable));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  ResultCallback callback =
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this));
  SetProxy();
  capability_->EnableModem(callback);
}

TEST_F(CellularCapabilityTest, EnableModemFail) {
  EXPECT_CALL(*proxy_, Enable(true, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeEnableFail));
  EXPECT_CALL(*this, TestCallback(IsFailure()));
  ResultCallback callback =
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this));
  SetProxy();
  capability_->EnableModem(callback);
}

TEST_F(CellularCapabilityTest, FinishEnable) {
  EXPECT_CALL(*gsm_network_proxy_,
              GetRegistrationInfo(NULL, _,
                                  CellularCapability::kTimeoutDefault));
  EXPECT_CALL(*gsm_network_proxy_,
              GetSignalQuality(NULL, _, CellularCapability::kTimeoutDefault));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  SetGSMNetworkProxy();
  capability_->FinishEnable(
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this)));
}

TEST_F(CellularCapabilityTest, UnsupportedOperation) {
  Error error;
  EXPECT_CALL(*this, TestCallback(IsSuccess())).Times(0);
  capability_->CellularCapability::Scan(&error,
      Bind(&CellularCapabilityTest::TestCallback, Unretained(this)));
  EXPECT_TRUE(error.IsFailure());
  EXPECT_EQ(Error::kNotSupported, error.type());
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

  EXPECT_CALL(*proxy_, Disconnect(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularCapabilityTest::InvokeDisconnect));
  SetProxy();
  cellular_->state_ = Cellular::kStateConnected;
  capability_->SetAllowRoaming(false, NULL);
  EXPECT_FALSE(capability_->GetAllowRoaming(NULL));
  EXPECT_EQ(Cellular::kStateRegistered, cellular_->state_);
}

}  // namespace shill
