// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_universal.h"

#include <string>
#include <vector>

#include <base/bind.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <mobile_provider.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_mm1_modem_modem3gpp_proxy.h"
#include "shill/mock_mm1_modem_modemcdma_proxy.h"
#include "shill/mock_mm1_modem_proxy.h"
#include "shill/mock_mm1_modem_simple_proxy.h"
#include "shill/mock_mm1_sim_proxy.h"
#include "shill/mock_profile.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using base::Bind;
using base::Unretained;
using std::string;
using std::vector;
using testing::InSequence;
using testing::NiceMock;
using testing::Return;
using testing::_;

namespace shill {

MATCHER(IsSuccess, "") {
  return arg.IsSuccess();
}
MATCHER(IsFailure, "") {
  return arg.IsFailure();
}

class CellularCapabilityUniversalTest : public testing::Test {
 public:
  CellularCapabilityUniversalTest()
      : manager_(&control_, &dispatcher_, &metrics_, &glib_),
        cellular_(new Cellular(&control_,
                               &dispatcher_,
                               NULL,
                               &manager_,
                               "",
                               "",
                               0,
                               Cellular::kTypeUniversal,
                               "",
                               "",
                               NULL)),
        modem_3gpp_proxy_(new mm1::MockModemModem3gppProxy()),
        modem_cdma_proxy_(new mm1::MockModemModemCdmaProxy()),
        modem_proxy_(new mm1::MockModemProxy()),
        modem_simple_proxy_(new mm1::MockModemSimpleProxy()),
        sim_proxy_(new mm1::MockSimProxy()),
        proxy_factory_(this),
        capability_(NULL),
        device_adaptor_(NULL) {}

  virtual ~CellularCapabilityUniversalTest() {
    cellular_->service_ = NULL;
    capability_ = NULL;
    device_adaptor_ = NULL;
  }

  virtual void SetUp() {
    capability_ = dynamic_cast<CellularCapabilityUniversal *>(
        cellular_->capability_.get());
    capability_->proxy_factory_ = &proxy_factory_;
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

  MOCK_METHOD1(TestCallback, void(const Error &error));

 protected:
  static const char kImei[];

  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(CellularCapabilityUniversalTest *test) :
        test_(test) {}

    virtual mm1::ModemModem3gppProxyInterface *CreateMM1ModemModem3gppProxy(
        const std::string &/* path */,
        const std::string &/* service */) {
      return test_->modem_3gpp_proxy_.release();
    }

    virtual mm1::ModemModemCdmaProxyInterface *CreateMM1ModemModemCdmaProxy(
        const std::string &/* path */,
        const std::string &/* service */) {
      return test_->modem_cdma_proxy_.release();
    }

    virtual mm1::ModemProxyInterface *CreateMM1ModemProxy(
        const std::string &/* path */,
        const std::string &/* service */) {
      return test_->modem_proxy_.release();
    }

    virtual mm1::ModemSimpleProxyInterface *CreateMM1ModemSimpleProxy(
        const std::string &/* path */,
        const std::string &/* service */) {
      return test_->modem_simple_proxy_.release();
    }

    virtual mm1::SimProxyInterface *CreateSimProxy(
        const std::string &/* path */,
        const std::string &/* service */) {
      return test_->sim_proxy_.release();
    }

   private:
    CellularCapabilityUniversalTest *test_;
  };

  NiceMockControl control_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;
  CellularRefPtr cellular_;
  scoped_ptr<mm1::MockModemModem3gppProxy> modem_3gpp_proxy_;
  scoped_ptr<mm1::MockModemModemCdmaProxy> modem_cdma_proxy_;
  scoped_ptr<mm1::MockModemProxy> modem_proxy_;
  scoped_ptr<mm1::MockModemSimpleProxy> modem_simple_proxy_;
  scoped_ptr<mm1::MockSimProxy> sim_proxy_;
  TestProxyFactory proxy_factory_;
  CellularCapabilityUniversal *capability_;  // Owned by |cellular_|.
  NiceMock<DeviceMockAdaptor> *device_adaptor_;  // Owned by |cellular_|.
};

const char CellularCapabilityUniversalTest::kImei[] = "999911110000";

TEST_F(CellularCapabilityUniversalTest, StartModem) {
  EXPECT_CALL(*modem_proxy_,
              Enable(true, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this, &CellularCapabilityUniversalTest::InvokeEnable));
  EXPECT_CALL(*modem_proxy_, AccessTechnologies())
      .WillOnce(Return(MM_MODEM_ACCESS_TECHNOLOGY_LTE|
                       MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS));
  EXPECT_CALL(*modem_3gpp_proxy_, EnabledFacilityLocks()).WillOnce(Return(0));
  ::DBus::Struct< uint32_t, bool > quality;
  quality._1 = 90;
  quality._2 = true;
  EXPECT_CALL(*modem_proxy_, SignalQuality()).WillOnce(Return(quality));
  EXPECT_CALL(*modem_proxy_, Sim()).WillOnce(Return(""));
  EXPECT_CALL(*modem_3gpp_proxy_, Imei()).WillOnce(Return(kImei));
  EXPECT_CALL(*modem_3gpp_proxy_, RegistrationState())
      .WillOnce(Return(MM_MODEM_3GPP_REGISTRATION_STATE_UNKNOWN));
  EXPECT_CALL(*modem_proxy_, OwnNumbers()).WillOnce(Return(vector<string>()));

  // After setup we lose pointers to the proxies, so it is hard to set
  // expectations.
  SetUp();

  Error error;
  ResultCallback callback =
      Bind(&CellularCapabilityUniversalTest::TestCallback, Unretained(this));
  capability_->StartModem(&error, callback);
  EXPECT_TRUE(error.IsSuccess());
}

TEST_F(CellularCapabilityUniversalTest, StartModemFail) {
  EXPECT_CALL(*modem_proxy_,
              Enable(true, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(
          Invoke(this, &CellularCapabilityUniversalTest::InvokeEnableFail));
  EXPECT_CALL(*this, TestCallback(IsFailure()));
  ResultCallback callback =
      Bind(&CellularCapabilityUniversalTest::TestCallback, Unretained(this));
  SetUp();

  Error error;
  capability_->StartModem(&error, callback);
  EXPECT_TRUE(error.IsSuccess());
}

}  // namespace shill
