// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_universal_cdma.h"

#include <string>
#include <vector>

#include <base/stringprintf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ModemManager/ModemManager.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_cellular_operator_info.h"
#include "shill/mock_cellular_service.h"
#include "shill/mock_dbus_properties_proxy.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_mm1_bearer_proxy.h"
#include "shill/mock_mm1_modem_modem3gpp_proxy.h"
#include "shill/mock_mm1_modem_modemcdma_proxy.h"
#include "shill/mock_mm1_modem_proxy.h"
#include "shill/mock_mm1_modem_simple_proxy.h"
#include "shill/mock_mm1_sim_proxy.h"
#include "shill/mock_modem_info.h"
#include "shill/nice_mock_control.h"
#include "shill/proxy_factory.h"

using base::StringPrintf;
using std::string;
using std::vector;
using testing::Invoke;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;
using testing::_;

namespace shill {

class CellularCapabilityUniversalCDMATest : public testing::Test {
 public:
  CellularCapabilityUniversalCDMATest()
      : capability_(NULL),
        device_adaptor_(NULL),
        modem_info_(NULL, &dispatcher_, NULL, NULL, NULL),
        bearer_proxy_(new mm1::MockBearerProxy()),
        modem_3gpp_proxy_(new mm1::MockModemModem3gppProxy()),
        modem_cdma_proxy_(new mm1::MockModemModemCdmaProxy()),
        modem_proxy_(new mm1::MockModemProxy()),
        modem_simple_proxy_(new mm1::MockModemSimpleProxy()),
        sim_proxy_(new mm1::MockSimProxy()),
        properties_proxy_(new MockDBusPropertiesProxy()),
        proxy_factory_(this),
        cellular_(new Cellular(&modem_info_,
                               "",
                               kMachineAddress,
                               0,
                               Cellular::kTypeUniversalCDMA,
                               "",
                               "",
                               "",
                               &proxy_factory_)),
        service_(new MockCellularService(&modem_info_,
                                         cellular_)) {}

  virtual ~CellularCapabilityUniversalCDMATest() {
    cellular_->service_ = NULL;
    capability_ = NULL;
    device_adaptor_ = NULL;
  }

  virtual void SetUp() {
    capability_ = dynamic_cast<CellularCapabilityUniversalCDMA *>(
        cellular_->capability_.get());
    device_adaptor_ =
        dynamic_cast<NiceMock<DeviceMockAdaptor> *>(cellular_->adaptor());
    cellular_->service_ = service_;
  }

  virtual void TearDown() {
    capability_->proxy_factory_ = NULL;
  }

  void SetService() {
    cellular_->service_ = new CellularService(&modem_info_, cellular_);
  }

  void ReleaseCapabilityProxies() {
    capability_->ReleaseProxies();
  }

  void SetCdmaProxy() {
    capability_->modem_cdma_proxy_.reset(modem_cdma_proxy_.release());
  }

  void SetSimpleProxy() {
    capability_->modem_simple_proxy_.reset(modem_simple_proxy_.release());
  }

 protected:
  static const char kEsn[];
  static const char kMachineAddress[];
  static const char kMeid[];

  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(CellularCapabilityUniversalCDMATest *test) :
        test_(test) {}

    // TODO(armansito): Some of these methods won't be necessary after 3GPP
    // gets refactored out of CellularCapabilityUniversal.
    virtual mm1::BearerProxyInterface *CreateBearerProxy(
        const std::string &path,
        const std::string &/*service*/) {
      return test_->bearer_proxy_.release();
    }

    virtual mm1::ModemModem3gppProxyInterface *CreateMM1ModemModem3gppProxy(
        const std::string &/*path*/,
        const std::string &/*service*/) {
      return test_->modem_3gpp_proxy_.release();
    }

    virtual mm1::ModemModemCdmaProxyInterface *CreateMM1ModemModemCdmaProxy(
        const std::string &/*path*/,
        const std::string &/*service*/) {
      return test_->modem_cdma_proxy_.release();
    }

    virtual mm1::ModemProxyInterface *CreateMM1ModemProxy(
        const std::string &/*path*/,
        const std::string &/*service*/) {
      return test_->modem_proxy_.release();
    }

    virtual mm1::ModemSimpleProxyInterface *CreateMM1ModemSimpleProxy(
        const std::string &/*path*/,
        const std::string &/*service*/) {
      return test_->modem_simple_proxy_.release();
    }

    virtual mm1::SimProxyInterface *CreateSimProxy(
        const std::string &/*path*/,
        const std::string &/*service*/) {
      return test_->sim_proxy_.release();
    }

    virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
        const std::string &/*path*/,
        const std::string &/*service*/) {
      return test_->properties_proxy_.release();
    }

   private:
    CellularCapabilityUniversalCDMATest *test_;
  };


  CellularCapabilityUniversalCDMA *capability_;
  NiceMock<DeviceMockAdaptor> *device_adaptor_;
  EventDispatcher dispatcher_;
  MockModemInfo modem_info_;
  MockGLib glib_;
  scoped_ptr<mm1::MockBearerProxy> bearer_proxy_;
  // TODO(armansito): Remove |modem_3gpp_proxy_| after refactor.
  scoped_ptr<mm1::MockModemModem3gppProxy> modem_3gpp_proxy_;
  scoped_ptr<mm1::MockModemModemCdmaProxy> modem_cdma_proxy_;
  scoped_ptr<mm1::MockModemProxy> modem_proxy_;
  scoped_ptr<mm1::MockModemSimpleProxy> modem_simple_proxy_;
  scoped_ptr<mm1::MockSimProxy> sim_proxy_;
  scoped_ptr<MockDBusPropertiesProxy> properties_proxy_;
  TestProxyFactory proxy_factory_;
  CellularRefPtr cellular_;
  MockCellularService *service_;
};

// static
const char CellularCapabilityUniversalCDMATest::kEsn[] = "0000";
// static
const char CellularCapabilityUniversalCDMATest::kMachineAddress[] =
    "TestMachineAddress";
// static
const char CellularCapabilityUniversalCDMATest::kMeid[] = "11111111111111";

TEST_F(CellularCapabilityUniversalCDMATest, PropertiesChanged) {
  // Set up mock modem CDMA properties.
  DBusPropertiesMap modem_cdma_properties;
  modem_cdma_properties[MM_MODEM_MODEMCDMA_PROPERTY_MEID].
      writer().append_string(kMeid);
  modem_cdma_properties[MM_MODEM_MODEMCDMA_PROPERTY_ESN].
      writer().append_string(kEsn);

  SetUp();

  EXPECT_TRUE(capability_->meid().empty());
  EXPECT_TRUE(capability_->esn().empty());

  // Changing properties on wrong interface will not have an effect
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM,
                                       modem_cdma_properties,
                                       vector<string>());
  EXPECT_TRUE(capability_->meid().empty());
  EXPECT_TRUE(capability_->esn().empty());

  // Changing properties on the right interface gets reflected in the
  // capabilities object
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM_MODEMCDMA,
                                       modem_cdma_properties,
                                       vector<string>());
  EXPECT_EQ(kMeid, capability_->meid());
  EXPECT_EQ(kEsn, capability_->esn());
}

TEST_F(CellularCapabilityUniversalCDMATest, OnCDMARegistrationChanged) {
  EXPECT_EQ(0, capability_->sid_);
  EXPECT_EQ(0, capability_->nid_);
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            capability_->cdma_1x_registration_state_);
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            capability_->cdma_evdo_registration_state_);

  EXPECT_EQ("", capability_->provider_.GetCode());
  EXPECT_EQ("", capability_->provider_.GetName());
  EXPECT_EQ("", capability_->provider_.GetCountry());

  CellularOperatorInfo::CellularOperator *provider =
      new CellularOperatorInfo::CellularOperator();

  provider->country_ = "us";
  provider->is_primary_ = true;
  provider->name_list_.push_back(
      CellularOperatorInfo::LocalizedName("Test", ""));

  scoped_ptr<const CellularOperatorInfo::CellularOperator> ptr(provider);

  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
              GetCellularOperatorBySID("2"))
      .WillOnce(Return(ptr.get()));

  capability_->OnCDMARegistrationChanged(
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
      MM_MODEM_CDMA_REGISTRATION_STATE_HOME,
      2,
      0);
  EXPECT_EQ(2, capability_->sid_);
  EXPECT_EQ(0, capability_->nid_);
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            capability_->cdma_1x_registration_state_);
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_HOME,
            capability_->cdma_evdo_registration_state_);

  EXPECT_TRUE(capability_->IsRegistered());
  EXPECT_EQ("2", capability_->provider_.GetCode());
  EXPECT_EQ("Test", capability_->provider_.GetName());
  EXPECT_EQ("us", capability_->provider_.GetCountry());
}

TEST_F(CellularCapabilityUniversalCDMATest, UpdateOperatorInfo) {
  EXPECT_EQ("", capability_->provider_.GetCode());
  EXPECT_EQ("", capability_->provider_.GetName());
  EXPECT_EQ("", capability_->provider_.GetCountry());

  capability_->UpdateOperatorInfo();
  EXPECT_EQ("", capability_->provider_.GetCode());
  EXPECT_EQ("", capability_->provider_.GetName());
  EXPECT_EQ("", capability_->provider_.GetCountry());


  capability_->UpdateOperatorInfo();
  EXPECT_EQ("", capability_->provider_.GetCode());
  EXPECT_EQ("", capability_->provider_.GetName());
  EXPECT_EQ("", capability_->provider_.GetCountry());

  capability_->sid_ = 1;
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
              GetCellularOperatorBySID(_))
      .WillOnce(Return((const CellularOperatorInfo::CellularOperator *)NULL));

  capability_->UpdateOperatorInfo();
  EXPECT_EQ("", capability_->provider_.GetCode());
  EXPECT_EQ("", capability_->provider_.GetName());
  EXPECT_EQ("", capability_->provider_.GetCountry());

  CellularOperatorInfo::CellularOperator *provider =
      new CellularOperatorInfo::CellularOperator();

  provider->country_ = "us";
  provider->is_primary_ = true;
  provider->name_list_.push_back(
      CellularOperatorInfo::LocalizedName("Test", ""));

  scoped_ptr<const CellularOperatorInfo::CellularOperator> ptr(provider);

  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
              GetCellularOperatorBySID(_))
      .WillOnce(Return(ptr.get()));

  capability_->UpdateOperatorInfo();

  EXPECT_EQ("1", capability_->provider_.GetCode());
  EXPECT_EQ("Test", capability_->provider_.GetName());
  EXPECT_EQ("us", capability_->provider_.GetCountry());
}

TEST_F(CellularCapabilityUniversalCDMATest, CreateFriendlyServiceName) {
  CellularCapabilityUniversalCDMA::friendly_service_name_id_cdma_ = 0;
  EXPECT_EQ(0, capability_->sid_);
  EXPECT_EQ("CDMANetwork0", capability_->CreateFriendlyServiceName());
  EXPECT_EQ("CDMANetwork1", capability_->CreateFriendlyServiceName());

  capability_->provider_.SetCode("0123");
  EXPECT_EQ("cellular_sid_0123", capability_->CreateFriendlyServiceName());

  CellularOperatorInfo::CellularOperator *provider =
      new CellularOperatorInfo::CellularOperator();

  capability_->sid_ = 1;
  provider->name_list_.push_back(
      CellularOperatorInfo::LocalizedName("Test", ""));
  capability_->provider_.SetCode("");

  scoped_ptr<const CellularOperatorInfo::CellularOperator> ptr(provider);

  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
              GetCellularOperatorBySID(_))
      .WillOnce(Return(ptr.get()));
  EXPECT_EQ("Test", capability_->CreateFriendlyServiceName());
}

TEST_F(CellularCapabilityUniversalCDMATest, UpdateOLP) {
  CellularService::OLP test_olp;
  test_olp.SetURL("http://testurl");
  test_olp.SetMethod("POST");
  test_olp.SetPostData("esn=${esn}&mdn=${mdn}&meid=${meid}");

  capability_->esn_ = "0";
  capability_->mdn_ = "3";
  capability_->meid_= "4";
  capability_->sid_ = 1;

  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
      GetOLPBySID(base::StringPrintf("%u", capability_->sid_)))
      .WillRepeatedly(Return(&test_olp));

  SetService();
  capability_->UpdateOLP();
  const CellularService::OLP &olp = cellular_->service()->olp();
  EXPECT_EQ("http://testurl", olp.GetURL());
  EXPECT_EQ("POST", olp.GetMethod());
  EXPECT_EQ("esn=0&mdn=3&meid=4", olp.GetPostData());
}

}  // namespace shill
