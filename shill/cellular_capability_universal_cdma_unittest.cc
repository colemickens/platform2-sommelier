// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_universal_cdma.h"

#include <string>
#include <vector>

#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
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
#include "shill/mock_mm1_modem_modem3gpp_proxy.h"
#include "shill/mock_mm1_modem_modemcdma_proxy.h"
#include "shill/mock_mm1_modem_proxy.h"
#include "shill/mock_mm1_modem_simple_proxy.h"
#include "shill/mock_mm1_sim_proxy.h"
#include "shill/mock_modem_info.h"
#include "shill/mock_pending_activation_store.h"
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
  CellularCapabilityUniversalCDMATest(EventDispatcher *dispatcher)
      : capability_(NULL),
        device_adaptor_(NULL),
        modem_info_(NULL, dispatcher, NULL, NULL, NULL),
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

  void ClearService() {
    cellular_->service_ = NULL;
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
  MockModemInfo modem_info_;
  MockGLib glib_;
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

class CellularCapabilityUniversalCDMAMainTest
    : public CellularCapabilityUniversalCDMATest {
 public:
  CellularCapabilityUniversalCDMAMainTest()
      : CellularCapabilityUniversalCDMATest(&dispatcher_) {}

 private:
  EventDispatcher dispatcher_;
};

class CellularCapabilityUniversalCDMADispatcherTest
    : public CellularCapabilityUniversalCDMATest {
 public:
  CellularCapabilityUniversalCDMADispatcherTest()
      : CellularCapabilityUniversalCDMATest(NULL) {}
};

TEST_F(CellularCapabilityUniversalCDMAMainTest, PropertiesChanged) {
  // Set up mock modem CDMA properties.
  DBusPropertiesMap modem_cdma_properties;
  modem_cdma_properties[MM_MODEM_MODEMCDMA_PROPERTY_MEID].
      writer().append_string(kMeid);
  modem_cdma_properties[MM_MODEM_MODEMCDMA_PROPERTY_ESN].
      writer().append_string(kEsn);

  SetUp();

  EXPECT_TRUE(cellular_->meid().empty());
  EXPECT_TRUE(cellular_->esn().empty());

  // Changing properties on wrong interface will not have an effect
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM,
                                       modem_cdma_properties,
                                       vector<string>());
  EXPECT_TRUE(cellular_->meid().empty());
  EXPECT_TRUE(cellular_->esn().empty());

  // Changing properties on the right interface gets reflected in the
  // capabilities object
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM_MODEMCDMA,
                                       modem_cdma_properties,
                                       vector<string>());
  EXPECT_EQ(kMeid, cellular_->meid());
  EXPECT_EQ(kEsn, cellular_->esn());
}

TEST_F(CellularCapabilityUniversalCDMAMainTest, OnCDMARegistrationChanged) {
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

TEST_F(CellularCapabilityUniversalCDMAMainTest, UpdateOperatorInfo) {
  EXPECT_EQ("", capability_->provider_.GetCode());
  EXPECT_EQ("", capability_->provider_.GetName());
  EXPECT_EQ("", capability_->provider_.GetCountry());
  EXPECT_TRUE(capability_->activation_code_.empty());

  capability_->UpdateOperatorInfo();
  EXPECT_EQ("", capability_->provider_.GetCode());
  EXPECT_EQ("", capability_->provider_.GetName());
  EXPECT_EQ("", capability_->provider_.GetCountry());
  EXPECT_TRUE(capability_->activation_code_.empty());

  capability_->UpdateOperatorInfo();
  EXPECT_EQ("", capability_->provider_.GetCode());
  EXPECT_EQ("", capability_->provider_.GetName());
  EXPECT_EQ("", capability_->provider_.GetCountry());
  EXPECT_TRUE(capability_->activation_code_.empty());

  capability_->sid_ = 1;
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
              GetCellularOperatorBySID(_))
      .WillOnce(Return((const CellularOperatorInfo::CellularOperator *)NULL));

  capability_->UpdateOperatorInfo();
  EXPECT_EQ("1", capability_->provider_.GetCode());
  EXPECT_EQ("", capability_->provider_.GetName());
  EXPECT_EQ("", capability_->provider_.GetCountry());
  EXPECT_TRUE(capability_->activation_code_.empty());

  CellularOperatorInfo::CellularOperator *provider =
      new CellularOperatorInfo::CellularOperator();

  provider->country_ = "us";
  provider->is_primary_ = true;
  provider->activation_code_ = "1234";
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
  EXPECT_EQ("1234", capability_->activation_code_);
  EXPECT_EQ("1", cellular_->service()->serving_operator().GetCode());
  EXPECT_EQ("Test", cellular_->service()->serving_operator().GetName());
  EXPECT_EQ("us", cellular_->service()->serving_operator().GetCountry());
  EXPECT_EQ("Test", cellular_->service()->friendly_name());

  capability_->sid_ = 1;
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
              GetCellularOperatorBySID(_))
      .WillOnce(Return(nullptr));

  capability_->UpdateOperatorInfo();
  EXPECT_EQ("1", capability_->provider_.GetCode());
  EXPECT_EQ("", capability_->provider_.GetName());
  EXPECT_EQ("", capability_->provider_.GetCountry());
  EXPECT_TRUE(capability_->activation_code_.empty());

}

TEST_F(CellularCapabilityUniversalCDMAMainTest, CreateFriendlyServiceName) {
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

TEST_F(CellularCapabilityUniversalCDMAMainTest, UpdateOLP) {
  CellularOperatorInfo::CellularOperator cellular_operator;
  CellularService::OLP test_olp;
  test_olp.SetURL("http://testurl");
  test_olp.SetMethod("POST");
  test_olp.SetPostData("esn=${esn}&mdn=${mdn}&meid=${meid}");

  cellular_->set_esn("0");
  cellular_->set_mdn("10123456789");
  cellular_->set_meid("4");
  capability_->sid_ = 1;

  string sid_string = base::StringPrintf("%u", capability_->sid_);
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
              GetCellularOperatorBySID(sid_string))
      .WillRepeatedly(Return(&cellular_operator));
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
              GetOLPBySID(sid_string))
      .WillRepeatedly(Return(&test_olp));

  SetService();

  cellular_operator.identifier_ = "vzw";
  capability_->UpdateOLP();
  const CellularService::OLP &vzw_olp = cellular_->service()->olp();
  EXPECT_EQ("http://testurl", vzw_olp.GetURL());
  EXPECT_EQ("POST", vzw_olp.GetMethod());
  EXPECT_EQ("esn=0&mdn=0123456789&meid=4", vzw_olp.GetPostData());

  cellular_operator.identifier_ = "foo";
  capability_->UpdateOLP();
  const CellularService::OLP &olp = cellular_->service()->olp();
  EXPECT_EQ("http://testurl", olp.GetURL());
  EXPECT_EQ("POST", olp.GetMethod());
  EXPECT_EQ("esn=0&mdn=10123456789&meid=4", olp.GetPostData());
}

TEST_F(CellularCapabilityUniversalCDMAMainTest, ActivateAutomatic) {
  mm1::MockModemModemCdmaProxy *cdma_proxy = modem_cdma_proxy_.get();
  SetUp();
  capability_->InitProxies();

  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(_,_))
      .Times(0);
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(_,_,_))
      .Times(0);
  EXPECT_CALL(*cdma_proxy, Activate(_,_,_,_)).Times(0);
  capability_->ActivateAutomatic();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(modem_cdma_proxy_.get());

  capability_->activation_code_ = "1234";

  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierMEID, _))
      .WillOnce(Return(PendingActivationStore::kStatePending))
      .WillOnce(Return(PendingActivationStore::kStateActivated));
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(_,_,_))
      .Times(0);
  EXPECT_CALL(*cdma_proxy, Activate(_,_,_,_)).Times(0);
  capability_->ActivateAutomatic();
  capability_->ActivateAutomatic();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(modem_cdma_proxy_.get());

  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierMEID, _))
      .WillOnce(Return(PendingActivationStore::kStateUnknown))
      .WillOnce(Return(PendingActivationStore::kStateFailureRetry));
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(_,_, PendingActivationStore::kStatePending))
      .Times(2);
  EXPECT_CALL(*cdma_proxy, Activate(_,_,_,_)).Times(2);
  capability_->ActivateAutomatic();
  capability_->ActivateAutomatic();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(modem_cdma_proxy_.get());
}

TEST_F(CellularCapabilityUniversalCDMAMainTest, IsServiceActivationRequired) {
  CellularService::OLP olp;
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(), GetOLPBySID(_))
      .WillOnce(Return((const CellularService::OLP *)NULL))
      .WillRepeatedly(Return(&olp));
  capability_->activation_state_ =
      MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED;
  EXPECT_FALSE(capability_->IsServiceActivationRequired());
  EXPECT_TRUE(capability_->IsServiceActivationRequired());
  capability_->activation_state_ =
      MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
  EXPECT_FALSE(capability_->IsServiceActivationRequired());
  capability_->activation_state_ =
      MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED;
  EXPECT_FALSE(capability_->IsServiceActivationRequired());
}

TEST_F(CellularCapabilityUniversalCDMAMainTest,
       UpdateServiceActivationStateProperty) {
  CellularService::OLP olp;
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(), GetOLPBySID(_))
      .WillRepeatedly(Return(&olp));
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(_, _))
      .WillOnce(Return(PendingActivationStore::kStatePending))
      .WillRepeatedly(Return(PendingActivationStore::kStateUnknown));

  capability_->activation_state_ = MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED;
  EXPECT_CALL(*service_, SetActivationState(kActivationStateActivating))
      .Times(1);
  capability_->UpdateServiceActivationStateProperty();
  Mock::VerifyAndClearExpectations(service_);

  EXPECT_CALL(*service_, SetActivationState(kActivationStateNotActivated))
      .Times(1);
  capability_->UpdateServiceActivationStateProperty();
  Mock::VerifyAndClearExpectations(service_);

  capability_->activation_state_ = MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
  EXPECT_CALL(*service_, SetActivationState(kActivationStateActivating))
      .Times(1);
  capability_->UpdateServiceActivationStateProperty();
  Mock::VerifyAndClearExpectations(service_);

  capability_->activation_state_ = MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED;
  EXPECT_CALL(*service_, SetActivationState(kActivationStateActivated))
      .Times(1);
  capability_->UpdateServiceActivationStateProperty();
  Mock::VerifyAndClearExpectations(service_);
  Mock::VerifyAndClearExpectations(modem_info_.mock_cellular_operator_info());
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
}

TEST_F(CellularCapabilityUniversalCDMAMainTest, IsActivating) {
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(_,_))
      .WillOnce(Return(PendingActivationStore::kStatePending))
      .WillOnce(Return(PendingActivationStore::kStatePending))
      .WillOnce(Return(PendingActivationStore::kStateFailureRetry))
      .WillRepeatedly(Return(PendingActivationStore::kStateUnknown));

  capability_->activation_state_ =
      MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED;
  EXPECT_TRUE(capability_->IsActivating());
  EXPECT_TRUE(capability_->IsActivating());
  capability_->activation_state_ = MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
  EXPECT_TRUE(capability_->IsActivating());
  EXPECT_TRUE(capability_->IsActivating());
  capability_->activation_state_ =
      MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED;
  EXPECT_FALSE(capability_->IsActivating());
}

TEST_F(CellularCapabilityUniversalCDMAMainTest, IsRegistered) {
  capability_->cdma_1x_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  EXPECT_FALSE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_1x_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED;
  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_1x_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_1x_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  EXPECT_TRUE(capability_->IsRegistered());

  capability_->cdma_evdo_registration_state_ =
      MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
  EXPECT_TRUE(capability_->IsRegistered());
}

TEST_F(CellularCapabilityUniversalCDMAMainTest, SetupConnectProperties) {
  DBusPropertiesMap map;
  capability_->SetupConnectProperties(&map);
  EXPECT_EQ(1, map.size());
  EXPECT_STREQ("#777", map["number"].reader().get_string());
}

TEST_F(CellularCapabilityUniversalCDMAMainTest, UpdateStorageIdentifier) {
  ClearService();
  EXPECT_FALSE(cellular_->service().get());
  capability_->UpdateStorageIdentifier();
  EXPECT_FALSE(cellular_->service().get());

  SetService();
  EXPECT_TRUE(cellular_->service().get());

  const string kPrefix =
      string(shill::kTypeCellular) + "_" + string(kMachineAddress) + "_";
  const string kDefaultIdentifierPattern = kPrefix + "CDMANetwork*";

  // GetCellularOperatorBySID returns NULL.
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
              GetCellularOperatorBySID(_))
      .WillOnce(Return(nullptr));
  capability_->UpdateStorageIdentifier();
  EXPECT_TRUE(::MatchPattern(cellular_->service()->GetStorageIdentifier(),
                             kDefaultIdentifierPattern));
  Mock::VerifyAndClearExpectations(modem_info_.mock_cellular_operator_info());

  CellularOperatorInfo::CellularOperator provider;
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
              GetCellularOperatorBySID(_))
      .Times(2)
      .WillRepeatedly(Return(&provider));

  // |provider.identifier_| is empty.
  capability_->UpdateStorageIdentifier();
  EXPECT_TRUE(::MatchPattern(cellular_->service()->GetStorageIdentifier(),
                             kDefaultIdentifierPattern));

  // Success.
  provider.identifier_ = "testidentifier";
  capability_->UpdateStorageIdentifier();
  EXPECT_EQ(kPrefix + "testidentifier",
            cellular_->service()->GetStorageIdentifier());
}

TEST_F(CellularCapabilityUniversalCDMADispatcherTest,
       UpdatePendingActivationState) {
  capability_->activation_state_ = MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(), RemoveEntry(_,_))
      .Times(1);
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(_,_))
      .Times(0);
  EXPECT_CALL(*modem_info_.mock_dispatcher(), PostTask(_)).Times(0);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(modem_info_.mock_dispatcher());

  capability_->activation_state_ = MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(), RemoveEntry(_,_))
      .Times(0);
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(_,_))
      .Times(2)
      .WillRepeatedly(Return(PendingActivationStore::kStateUnknown));
  EXPECT_CALL(*modem_info_.mock_dispatcher(), PostTask(_)).Times(0);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(modem_info_.mock_dispatcher());

  capability_->activation_state_ = MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(), RemoveEntry(_,_))
      .Times(0);
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(_,_))
      .Times(2)
      .WillRepeatedly(Return(PendingActivationStore::kStatePending));
  EXPECT_CALL(*modem_info_.mock_dispatcher(), PostTask(_)).Times(0);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(modem_info_.mock_dispatcher());

  EXPECT_CALL(*modem_info_.mock_pending_activation_store(), RemoveEntry(_,_))
      .Times(0);
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(_,_))
      .Times(2)
      .WillRepeatedly(Return(PendingActivationStore::kStateFailureRetry));
  EXPECT_CALL(*modem_info_.mock_dispatcher(), PostTask(_)).Times(1);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(modem_info_.mock_dispatcher());

  EXPECT_CALL(*modem_info_.mock_pending_activation_store(), RemoveEntry(_,_))
      .Times(0);
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(_,_))
      .Times(4)
      .WillOnce(Return(PendingActivationStore::kStateActivated))
      .WillOnce(Return(PendingActivationStore::kStateActivated))
      .WillOnce(Return(PendingActivationStore::kStateUnknown))
      .WillOnce(Return(PendingActivationStore::kStateUnknown));
  EXPECT_CALL(*modem_info_.mock_dispatcher(), PostTask(_)).Times(0);
  capability_->UpdatePendingActivationState();
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(modem_info_.mock_dispatcher());
}

}  // namespace shill
