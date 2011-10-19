// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <sys/socket.h>
#include <linux/if.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.

#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>

#include "shill/cellular_service.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_device_info.h"
#include "shill/mock_dhcp_config.h"
#include "shill/mock_dhcp_provider.h"
#include "shill/mock_manager.h"
#include "shill/mock_modem_cdma_proxy.h"
#include "shill/mock_modem_gsm_card_proxy.h"
#include "shill/mock_modem_gsm_network_proxy.h"
#include "shill/mock_modem_proxy.h"
#include "shill/mock_modem_simple_proxy.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/nice_mock_control.h"
#include "shill/property_store_unittest.h"
#include "shill/proxy_factory.h"

using std::map;
using std::string;
using testing::_;
using testing::AnyNumber;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;

namespace shill {

class CellularPropertyTest : public PropertyStoreTest {
 public:
  CellularPropertyTest()
      : device_(new Cellular(control_interface(),
                             NULL,
                             NULL,
                             "usb0",
                             "00:01:02:03:04:05",
                             3,
                             Cellular::kTypeGSM,
                             "",
                             "")) {}
  virtual ~CellularPropertyTest() {}

 protected:
  DeviceRefPtr device_;
};

TEST_F(CellularPropertyTest, Contains) {
  EXPECT_TRUE(device_->store().Contains(flimflam::kNameProperty));
  EXPECT_FALSE(device_->store().Contains(""));
}

TEST_F(CellularPropertyTest, Dispatch) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(
        device_->mutable_store(),
        flimflam::kCellularAllowRoamingProperty,
        PropertyStoreTest::kBoolV,
        &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_->mutable_store(),
                                            flimflam::kScanIntervalProperty,
                                            PropertyStoreTest::kUint16V,
                                            &error));
  }
  // Ensure that attempting to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_->mutable_store(),
                                             flimflam::kAddressProperty,
                                             PropertyStoreTest::kStringV,
                                             &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_->mutable_store(),
                                             flimflam::kCarrierProperty,
                                             PropertyStoreTest::kStringV,
                                             &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_->mutable_store(),
                                             flimflam::kPRLVersionProperty,
                                             PropertyStoreTest::kInt16V,
                                             &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
}

class CellularTest : public testing::Test {
 public:
  CellularTest()
      : manager_(&control_interface_, &dispatcher_, &glib_),
        device_info_(&control_interface_, &dispatcher_, &manager_),
        proxy_(new MockModemProxy()),
        simple_proxy_(new MockModemSimpleProxy()),
        cdma_proxy_(new MockModemCDMAProxy()),
        gsm_card_proxy_(new MockModemGSMCardProxy()),
        gsm_network_proxy_(new MockModemGSMNetworkProxy()),
        proxy_factory_(this),
        dhcp_config_(new MockDHCPConfig(&control_interface_,
                                        &dispatcher_,
                                        &dhcp_provider_,
                                        kTestDeviceName,
                                        &glib_)),
        device_(new Cellular(&control_interface_,
                             &dispatcher_,
                             &manager_,
                             kTestDeviceName,
                             kTestDeviceAddress,
                             3,
                             Cellular::kTypeGSM,
                             kDBusOwner,
                             kDBusPath)) {}
  virtual ~CellularTest() {}

  virtual void SetUp() {
    device_->proxy_factory_ = &proxy_factory_;
    static_cast<Device *>(device_)->rtnl_handler_ = &rtnl_handler_;
    device_->set_dhcp_provider(&dhcp_provider_);
    EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&device_info_));
    EXPECT_CALL(manager_, DeregisterService(_)).Times(AnyNumber());
  }

  virtual void TearDown() {
    device_->proxy_factory_ = NULL;
    device_->Stop();
    device_->set_dhcp_provider(NULL);
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(CellularTest *test) : test_(test) {}

    virtual ModemProxyInterface *CreateModemProxy(
        ModemProxyListener */*listener*/,
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
        ModemCDMAProxyListener */*listener*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->cdma_proxy_.release();
    }

    virtual ModemGSMCardProxyInterface *CreateModemGSMCardProxy(
        ModemGSMCardProxyListener */*listener*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->gsm_card_proxy_.release();
    }

    virtual ModemGSMNetworkProxyInterface *CreateModemGSMNetworkProxy(
        ModemGSMNetworkProxyListener */*listener*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->gsm_network_proxy_.release();
    }

   private:
    CellularTest *test_;
  };

  static const char kTestDeviceName[];
  static const char kTestDeviceAddress[];
  static const int kTestSocket;
  static const char kDBusOwner[];
  static const char kDBusPath[];
  static const char kTestCarrier[];
  static const char kMEID[];
  static const char kIMEI[];
  static const char kIMSI[];
  static const char kMSISDN[];
  static const char kPIN[];
  static const char kPUK[];

  void StartRTNLHandler();
  void StopRTNLHandler();

  NiceMockControl control_interface_;
  EventDispatcher dispatcher_;
  MockGLib glib_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  NiceMock<MockRTNLHandler> rtnl_handler_;

  scoped_ptr<MockModemProxy> proxy_;
  scoped_ptr<MockModemSimpleProxy> simple_proxy_;
  scoped_ptr<MockModemCDMAProxy> cdma_proxy_;
  scoped_ptr<MockModemGSMCardProxy> gsm_card_proxy_;
  scoped_ptr<MockModemGSMNetworkProxy> gsm_network_proxy_;
  TestProxyFactory proxy_factory_;

  MockDHCPProvider dhcp_provider_;
  scoped_refptr<MockDHCPConfig> dhcp_config_;

  CellularRefPtr device_;
};

const char CellularTest::kTestDeviceName[] = "usb0";
const char CellularTest::kTestDeviceAddress[] = "00:01:02:03:04:05";
const char CellularTest::kDBusOwner[] = ":1.19";
const char CellularTest::kDBusPath[] = "/org/chromium/ModemManager/Gobi/0";
const char CellularTest::kTestCarrier[] = "The Cellular Carrier";
const char CellularTest::kMEID[] = "D1234567EF8901";
const char CellularTest::kIMEI[] = "987654321098765";
const char CellularTest::kIMSI[] = "123456789012345";
const char CellularTest::kMSISDN[] = "12345678901";
const char CellularTest::kPIN[] = "9876";
const char CellularTest::kPUK[] = "8765";

TEST_F(CellularTest, GetTypeString) {
  EXPECT_EQ("CellularTypeGSM", device_->GetTypeString());
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_EQ("CellularTypeCDMA", device_->GetTypeString());
}

TEST_F(CellularTest, GetCDMANetworkTechnologyString) {
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_EQ("", device_->GetNetworkTechnologyString());
  device_->cdma_.registration_state_evdo =
      MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  EXPECT_EQ(flimflam::kNetworkTechnologyEvdo,
            device_->GetNetworkTechnologyString());
  device_->cdma_.registration_state_evdo =
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  device_->cdma_.registration_state_1x =
      MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  EXPECT_EQ(flimflam::kNetworkTechnology1Xrtt,
            device_->GetNetworkTechnologyString());
}

TEST_F(CellularTest, GetGSMNetworkTechnologyString) {
  device_->type_ = Cellular::kTypeGSM;
  EXPECT_EQ("", device_->GetNetworkTechnologyString());
  device_->gsm_.access_technology = MM_MODEM_GSM_ACCESS_TECH_GSM;
  EXPECT_EQ("", device_->GetNetworkTechnologyString());
  device_->gsm_.registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_HOME;
  EXPECT_EQ(flimflam::kNetworkTechnologyGsm,
            device_->GetNetworkTechnologyString());
  device_->gsm_.access_technology = MM_MODEM_GSM_ACCESS_TECH_GSM_COMPACT;
  EXPECT_EQ(flimflam::kNetworkTechnologyGsm,
            device_->GetNetworkTechnologyString());
  device_->gsm_.access_technology = MM_MODEM_GSM_ACCESS_TECH_GPRS;
  EXPECT_EQ(flimflam::kNetworkTechnologyGprs,
            device_->GetNetworkTechnologyString());
  device_->gsm_.access_technology = MM_MODEM_GSM_ACCESS_TECH_EDGE;
  EXPECT_EQ(flimflam::kNetworkTechnologyEdge,
            device_->GetNetworkTechnologyString());
  device_->gsm_.access_technology = MM_MODEM_GSM_ACCESS_TECH_UMTS;
  EXPECT_EQ(flimflam::kNetworkTechnologyUmts,
            device_->GetNetworkTechnologyString());
  device_->gsm_.registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING;
  device_->gsm_.access_technology = MM_MODEM_GSM_ACCESS_TECH_HSDPA;
  EXPECT_EQ(flimflam::kNetworkTechnologyHspa,
            device_->GetNetworkTechnologyString());
  device_->gsm_.access_technology = MM_MODEM_GSM_ACCESS_TECH_HSUPA;
  EXPECT_EQ(flimflam::kNetworkTechnologyHspa,
            device_->GetNetworkTechnologyString());
  device_->gsm_.access_technology = MM_MODEM_GSM_ACCESS_TECH_HSPA;
  EXPECT_EQ(flimflam::kNetworkTechnologyHspa,
            device_->GetNetworkTechnologyString());
  device_->gsm_.access_technology = MM_MODEM_GSM_ACCESS_TECH_HSPA_PLUS;
  EXPECT_EQ(flimflam::kNetworkTechnologyHspaPlus,
            device_->GetNetworkTechnologyString());
}

TEST_F(CellularTest, GetCDMARoamingStateString) {
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_EQ(flimflam::kRoamingStateUnknown, device_->GetRoamingStateString());
  device_->cdma_.registration_state_evdo =
      MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED;
  EXPECT_EQ(flimflam::kRoamingStateUnknown, device_->GetRoamingStateString());
  device_->cdma_.registration_state_evdo =
      MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  EXPECT_EQ(flimflam::kRoamingStateHome, device_->GetRoamingStateString());
  device_->cdma_.registration_state_evdo =
      MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
  EXPECT_EQ(flimflam::kRoamingStateRoaming, device_->GetRoamingStateString());
  device_->cdma_.registration_state_evdo =
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN;
  device_->cdma_.registration_state_1x =
      MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED;
  EXPECT_EQ(flimflam::kRoamingStateUnknown, device_->GetRoamingStateString());
  device_->cdma_.registration_state_1x =
      MM_MODEM_CDMA_REGISTRATION_STATE_HOME;
  EXPECT_EQ(flimflam::kRoamingStateHome, device_->GetRoamingStateString());
  device_->cdma_.registration_state_1x =
      MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING;
  EXPECT_EQ(flimflam::kRoamingStateRoaming, device_->GetRoamingStateString());
}

TEST_F(CellularTest, GetGSMRoamingStateString) {
  device_->type_ = Cellular::kTypeGSM;
  EXPECT_EQ(flimflam::kRoamingStateUnknown, device_->GetRoamingStateString());
  device_->gsm_.registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_HOME;
  EXPECT_EQ(flimflam::kRoamingStateHome, device_->GetRoamingStateString());
  device_->gsm_.registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING;
  EXPECT_EQ(flimflam::kRoamingStateRoaming, device_->GetRoamingStateString());
  device_->gsm_.registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_SEARCHING;
  EXPECT_EQ(flimflam::kRoamingStateUnknown, device_->GetRoamingStateString());
  device_->gsm_.registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_DENIED;
  EXPECT_EQ(flimflam::kRoamingStateUnknown, device_->GetRoamingStateString());
  device_->gsm_.registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_IDLE;
  EXPECT_EQ(flimflam::kRoamingStateUnknown, device_->GetRoamingStateString());
}

TEST_F(CellularTest, GetStateString) {
  EXPECT_EQ("CellularStateDisabled",
            device_->GetStateString(Cellular::kStateDisabled));
  EXPECT_EQ("CellularStateEnabled",
            device_->GetStateString(Cellular::kStateEnabled));
  EXPECT_EQ("CellularStateRegistered",
            device_->GetStateString(Cellular::kStateRegistered));
  EXPECT_EQ("CellularStateConnected",
            device_->GetStateString(Cellular::kStateConnected));
  EXPECT_EQ("CellularStateLinked",
            device_->GetStateString(Cellular::kStateLinked));
}

TEST_F(CellularTest, GetCDMAActivationStateString) {
  EXPECT_EQ(flimflam::kActivationStateActivated,
            device_->GetCDMAActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATED));
  EXPECT_EQ(flimflam::kActivationStateActivating,
            device_->GetCDMAActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING));
  EXPECT_EQ(flimflam::kActivationStateNotActivated,
            device_->GetCDMAActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED));
  EXPECT_EQ(flimflam::kActivationStatePartiallyActivated,
            device_->GetCDMAActivationStateString(
                MM_MODEM_CDMA_ACTIVATION_STATE_PARTIALLY_ACTIVATED));
  EXPECT_EQ(flimflam::kActivationStateUnknown,
            device_->GetCDMAActivationStateString(123));
}

TEST_F(CellularTest, GetCDMAActivationErrorString) {
  EXPECT_EQ(flimflam::kErrorNeedEvdo,
            device_->GetCDMAActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_WRONG_RADIO_INTERFACE));
  EXPECT_EQ(flimflam::kErrorNeedHomeNetwork,
            device_->GetCDMAActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_ROAMING));
  EXPECT_EQ(flimflam::kErrorOtaspFailed,
            device_->GetCDMAActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_COULD_NOT_CONNECT));
  EXPECT_EQ(flimflam::kErrorOtaspFailed,
            device_->GetCDMAActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_SECURITY_AUTHENTICATION_FAILED));
  EXPECT_EQ(flimflam::kErrorOtaspFailed,
            device_->GetCDMAActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_PROVISIONING_FAILED));
  EXPECT_EQ("",
            device_->GetCDMAActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR));
  EXPECT_EQ(flimflam::kErrorActivationFailed,
            device_->GetCDMAActivationErrorString(
                MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL));
  EXPECT_EQ(flimflam::kErrorActivationFailed,
            device_->GetCDMAActivationErrorString(1234));
}

TEST_F(CellularTest, StartCDMARegister) {
  const int kStrength = 90;
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_CALL(*proxy_, Enable(true)).Times(1);
  EXPECT_CALL(*simple_proxy_, GetStatus())
      .WillOnce(Return(DBusPropertiesMap()));
  EXPECT_CALL(*cdma_proxy_, MEID()).WillOnce(Return(kMEID));
  EXPECT_CALL(*proxy_, GetInfo()).WillOnce(Return(ModemProxyInterface::Info()));
  EXPECT_CALL(*cdma_proxy_, GetRegistrationState(_, _))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(MM_MODEM_CDMA_REGISTRATION_STATE_HOME),
          SetArgumentPointee<1>(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN)));
  EXPECT_CALL(*cdma_proxy_, GetSignalQuality()).WillOnce(Return(kStrength));
  device_->Start();
  EXPECT_EQ(kMEID, device_->meid_);
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateRegistered, device_->state_);
  ASSERT_TRUE(device_->service_.get());
  EXPECT_EQ(flimflam::kNetworkTechnology1Xrtt,
            device_->service_->network_tech());
  EXPECT_EQ(kStrength, device_->service_->strength());
  EXPECT_EQ(flimflam::kRoamingStateHome, device_->service_->roaming_state());
}

TEST_F(CellularTest, StartGSMRegister) {
  device_->type_ = Cellular::kTypeGSM;
  static const char kNetwork[] = "My Favorite GSM Network";
  const int kStrength = 70;
  device_->selected_network_ = kNetwork;
  EXPECT_CALL(*proxy_, Enable(true)).Times(1);
  EXPECT_CALL(*gsm_network_proxy_, Register(kNetwork)).Times(1);
  EXPECT_CALL(*simple_proxy_, GetStatus())
      .WillOnce(Return(DBusPropertiesMap()));
  EXPECT_CALL(*gsm_card_proxy_, GetIMEI()).WillOnce(Return(kIMEI));
  EXPECT_CALL(*gsm_card_proxy_, GetIMSI()).WillOnce(Return(kIMSI));
  EXPECT_CALL(*gsm_card_proxy_, GetSPN()).WillOnce(Return(kTestCarrier));
  EXPECT_CALL(*gsm_card_proxy_, GetMSISDN()).WillOnce(Return(kMSISDN));
  EXPECT_CALL(*gsm_network_proxy_, AccessTechnology())
      .WillOnce(Return(MM_MODEM_GSM_ACCESS_TECH_EDGE));
  EXPECT_CALL(*proxy_, GetInfo()).WillOnce(Return(ModemProxyInterface::Info()));
  ModemGSMNetworkProxyInterface::RegistrationInfo reg_info;
  reg_info._1 = MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING;
  EXPECT_CALL(*gsm_network_proxy_, GetRegistrationInfo())
      .WillOnce(Return(reg_info));
  EXPECT_CALL(*gsm_network_proxy_, GetSignalQuality())
      .WillOnce(Return(kStrength));
  device_->Start();
  EXPECT_EQ(kIMEI, device_->imei_);
  EXPECT_EQ(kIMSI, device_->imsi_);
  EXPECT_EQ(kTestCarrier, device_->gsm_.spn);
  EXPECT_EQ(kMSISDN, device_->mdn_);
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateRegistered, device_->state_);
  ASSERT_TRUE(device_->service_.get());
  EXPECT_EQ(flimflam::kNetworkTechnologyEdge,
            device_->service_->network_tech());
  EXPECT_EQ(kStrength, device_->service_->strength());
  EXPECT_EQ(flimflam::kRoamingStateRoaming, device_->service_->roaming_state());
}

TEST_F(CellularTest, StartConnected) {
  EXPECT_CALL(device_info_, GetFlags(device_->interface_index(), _))
      .WillOnce(Return(true));
  device_->type_ = Cellular::kTypeCDMA;
  device_->set_modem_state(Cellular::kModemStateConnected);
  device_->meid_ = kMEID;
  EXPECT_CALL(*proxy_, Enable(true)).Times(1);
  EXPECT_CALL(*simple_proxy_, GetStatus())
      .WillOnce(Return(DBusPropertiesMap()));
  EXPECT_CALL(*proxy_, GetInfo()).WillOnce(Return(ModemProxyInterface::Info()));
  EXPECT_CALL(*cdma_proxy_, GetRegistrationState(_, _))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED),
          SetArgumentPointee<1>(MM_MODEM_CDMA_REGISTRATION_STATE_HOME)));
  EXPECT_CALL(*cdma_proxy_, GetSignalQuality()).WillOnce(Return(90));
  device_->Start();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateConnected, device_->state_);
}

TEST_F(CellularTest, StartLinked) {
  EXPECT_CALL(device_info_, GetFlags(device_->interface_index(), _))
      .WillOnce(DoAll(SetArgumentPointee<1>(IFF_UP), Return(true)));
  device_->type_ = Cellular::kTypeCDMA;
  device_->set_modem_state(Cellular::kModemStateConnected);
  device_->meid_ = kMEID;
  EXPECT_CALL(*proxy_, Enable(true)).Times(1);
  EXPECT_CALL(*simple_proxy_, GetStatus())
      .WillOnce(Return(DBusPropertiesMap()));
  EXPECT_CALL(*proxy_, GetInfo()).WillOnce(Return(ModemProxyInterface::Info()));
  EXPECT_CALL(*cdma_proxy_, GetRegistrationState(_, _))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED),
          SetArgumentPointee<1>(MM_MODEM_CDMA_REGISTRATION_STATE_HOME)));
  EXPECT_CALL(*cdma_proxy_, GetSignalQuality()).WillOnce(Return(90));
  EXPECT_CALL(dhcp_provider_, CreateConfig(kTestDeviceName))
      .WillOnce(Return(dhcp_config_));
  EXPECT_CALL(*dhcp_config_, RequestIP()).WillOnce(Return(true));
  EXPECT_CALL(manager_, UpdateService(_)).Times(2);
  device_->Start();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateLinked, device_->state_);
  EXPECT_EQ(Service::kStateConfiguring, device_->service_->state());
  device_->SelectService(NULL);
}

TEST_F(CellularTest, InitProxiesCDMA) {
  device_->type_ = Cellular::kTypeCDMA;
  device_->InitProxies();
  EXPECT_TRUE(device_->proxy_.get());
  EXPECT_TRUE(device_->simple_proxy_.get());
  EXPECT_TRUE(device_->cdma_proxy_.get());
  EXPECT_FALSE(device_->gsm_card_proxy_.get());
  EXPECT_FALSE(device_->gsm_network_proxy_.get());
}

TEST_F(CellularTest, InitProxiesGSM) {
  device_->type_ = Cellular::kTypeGSM;
  device_->InitProxies();
  EXPECT_TRUE(device_->proxy_.get());
  EXPECT_TRUE(device_->simple_proxy_.get());
  EXPECT_TRUE(device_->gsm_card_proxy_.get());
  EXPECT_TRUE(device_->gsm_network_proxy_.get());
  EXPECT_FALSE(device_->cdma_proxy_.get());
}

TEST_F(CellularTest, GetModemStatus) {
  device_->type_ = Cellular::kTypeCDMA;
  DBusPropertiesMap props;
  props["carrier"].writer().append_string(kTestCarrier);
  props["unknown-property"].writer().append_string("irrelevant-value");
  EXPECT_CALL(*simple_proxy_, GetStatus()).WillOnce(Return(props));
  device_->simple_proxy_.reset(simple_proxy_.release());
  device_->state_ = Cellular::kStateEnabled;
  device_->GetModemStatus();
  EXPECT_EQ(kTestCarrier, device_->carrier_);
  EXPECT_EQ(kTestCarrier, device_->home_provider_.GetName());
}

TEST_F(CellularTest, GetModemInfo) {
  static const char kManufacturer[] = "Company";
  static const char kModelID[] = "Gobi 2000";
  static const char kHWRev[] = "A00B1234";
  ModemProxyInterface::Info info;
  info._1 = kManufacturer;
  info._2 = kModelID;
  info._3 = kHWRev;
  EXPECT_CALL(*proxy_, GetInfo()).WillOnce(Return(info));
  device_->proxy_.reset(proxy_.release());
  device_->GetModemInfo();
  EXPECT_EQ(kManufacturer, device_->manufacturer_);
  EXPECT_EQ(kModelID, device_->model_id_);
  EXPECT_EQ(kHWRev, device_->hardware_revision_);
}

TEST_F(CellularTest, GetCDMARegistrationState) {
  EXPECT_FALSE(device_->service_.get());
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            device_->cdma_.registration_state_1x);
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
            device_->cdma_.registration_state_evdo);
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_CALL(*cdma_proxy_, GetRegistrationState(_, _))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED),
          SetArgumentPointee<1>(MM_MODEM_CDMA_REGISTRATION_STATE_HOME)));
  EXPECT_CALL(*cdma_proxy_, GetSignalQuality()).WillOnce(Return(90));
  device_->cdma_proxy_.reset(cdma_proxy_.release());
  device_->GetModemRegistrationState();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED,
            device_->cdma_.registration_state_1x);
  EXPECT_EQ(MM_MODEM_CDMA_REGISTRATION_STATE_HOME,
            device_->cdma_.registration_state_evdo);
  ASSERT_TRUE(device_->service_.get());
}

TEST_F(CellularTest, GetCDMASignalQuality) {
  const int kStrength = 90;
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_CALL(*cdma_proxy_, GetSignalQuality())
      .Times(2)
      .WillRepeatedly(Return(kStrength));
  device_->cdma_proxy_.reset(cdma_proxy_.release());

  EXPECT_FALSE(device_->service_.get());
  device_->GetModemSignalQuality();

  device_->service_ = new CellularService(
      &control_interface_, &dispatcher_, &manager_, device_);
  EXPECT_EQ(0, device_->service_->strength());
  device_->GetModemSignalQuality();
  EXPECT_EQ(kStrength, device_->service_->strength());
}

TEST_F(CellularTest, GetGSMSignalQuality) {
  const int kStrength = 80;
  device_->type_ = Cellular::kTypeGSM;
  EXPECT_CALL(*gsm_network_proxy_, GetSignalQuality())
      .Times(2)
      .WillRepeatedly(Return(kStrength));
  device_->gsm_network_proxy_.reset(gsm_network_proxy_.release());

  EXPECT_FALSE(device_->service_.get());
  device_->GetModemSignalQuality();

  device_->service_ = new CellularService(
      &control_interface_, &dispatcher_, &manager_, device_);
  EXPECT_EQ(0, device_->service_->strength());
  device_->GetModemSignalQuality();
  EXPECT_EQ(kStrength, device_->service_->strength());
}

TEST_F(CellularTest, GetCDMAIdentifiers) {
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_CALL(*cdma_proxy_, MEID()).WillOnce(Return(kMEID));
  device_->cdma_proxy_.reset(cdma_proxy_.release());
  device_->GetModemIdentifiers();
  EXPECT_EQ(kMEID, device_->meid_);
  device_->GetModemIdentifiers();
  EXPECT_EQ(kMEID, device_->meid_);
}

TEST_F(CellularTest, GetGSMIdentifiers) {
  device_->type_ = Cellular::kTypeGSM;
  EXPECT_CALL(*gsm_card_proxy_, GetIMEI()).WillOnce(Return(kIMEI));
  EXPECT_CALL(*gsm_card_proxy_, GetIMSI()).WillOnce(Return(kIMSI));
  EXPECT_CALL(*gsm_card_proxy_, GetSPN()).WillOnce(Return(kTestCarrier));
  EXPECT_CALL(*gsm_card_proxy_, GetMSISDN()).WillOnce(Return(kMSISDN));
  device_->gsm_card_proxy_.reset(gsm_card_proxy_.release());
  device_->GetModemIdentifiers();
  EXPECT_EQ(kIMEI, device_->imei_);
  EXPECT_EQ(kIMSI, device_->imsi_);
  EXPECT_EQ(kTestCarrier, device_->gsm_.spn);
  EXPECT_EQ(kMSISDN, device_->mdn_);
  device_->GetModemIdentifiers();
  EXPECT_EQ(kIMEI, device_->imei_);
  EXPECT_EQ(kIMSI, device_->imsi_);
  EXPECT_EQ(kTestCarrier, device_->gsm_.spn);
  EXPECT_EQ(kMSISDN, device_->mdn_);
}

TEST_F(CellularTest, CreateService) {
  device_->type_ = Cellular::kTypeCDMA;
  static const char kPaymentURL[] = "https://payment.url";
  static const char kUsageURL[] = "https://usage.url";
  device_->cdma_.payment_url = kPaymentURL;
  device_->cdma_.usage_url = kUsageURL;
  device_->home_provider_.SetName(kTestCarrier);
  device_->CreateService();
  ASSERT_TRUE(device_->service_.get());
  EXPECT_EQ(kPaymentURL, device_->service_->payment_url());
  EXPECT_EQ(kUsageURL, device_->service_->usage_url());
  EXPECT_EQ(kTestCarrier, device_->service_->serving_operator().GetName());
  EXPECT_TRUE(device_->service_->TechnologyIs(Technology::kCellular));
}

namespace {

MATCHER(ContainsPhoneNumber, "") {
  return ContainsKey(arg, Cellular::kConnectPropertyPhoneNumber);
}

}  // namespace {}

TEST_F(CellularTest, Connect) {
  Error error;
  EXPECT_CALL(device_info_, GetFlags(device_->interface_index(), _))
      .Times(2)
      .WillRepeatedly(Return(true));
  device_->state_ = Cellular::kStateConnected;
  device_->Connect(&error);
  EXPECT_EQ(Error::kAlreadyConnected, error.type());
  error.Populate(Error::kSuccess);

  device_->state_ = Cellular::kStateLinked;
  device_->Connect(&error);
  EXPECT_EQ(Error::kAlreadyConnected, error.type());

  device_->state_ = Cellular::kStateRegistered;
  device_->service_ = new CellularService(
      &control_interface_, &dispatcher_, &manager_, device_);

  device_->allow_roaming_ = false;
  device_->service_->set_roaming_state(flimflam::kRoamingStateRoaming);
  device_->Connect(&error);
  ASSERT_TRUE(device_->task_factory_.empty());
  EXPECT_EQ(Error::kNotOnHomeNetwork, error.type());
  error.Populate(Error::kSuccess);

  device_->service_->set_roaming_state(flimflam::kRoamingStateHome);
  device_->Connect(&error);
  ASSERT_FALSE(device_->task_factory_.empty());
  EXPECT_TRUE(error.IsSuccess());

  device_->allow_roaming_ = true;
  device_->service_->set_roaming_state(flimflam::kRoamingStateRoaming);
  device_->Connect(&error);
  EXPECT_TRUE(error.IsSuccess());

  DBusPropertiesMap properties;
  properties[Cellular::kConnectPropertyPhoneNumber].writer().append_string(
      Cellular::kPhoneNumberGSM);
  EXPECT_CALL(*simple_proxy_, Connect(ContainsPhoneNumber())).Times(2);
  device_->simple_proxy_.reset(simple_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularTest, RegisterOnNetwork) {
  Error error;
  device_->type_ = Cellular::kTypeGSM;
  EXPECT_CALL(*gsm_network_proxy_, Register(kTestCarrier)).Times(1);
  device_->RegisterOnNetwork(kTestCarrier, &error);
  EXPECT_TRUE(error.IsSuccess());
  device_->gsm_network_proxy_.reset(gsm_network_proxy_.release());
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(kTestCarrier, device_->selected_network_);
}

TEST_F(CellularTest, RegisterOnNetworkError) {
  Error error;
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_CALL(*gsm_network_proxy_, Register(_)).Times(0);
  device_->RegisterOnNetwork(kTestCarrier, &error);
  EXPECT_EQ(Error::kNotSupported, error.type());
  device_->gsm_network_proxy_.reset(gsm_network_proxy_.release());
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ("", device_->selected_network_);
}

TEST_F(CellularTest, RequirePIN) {
  Error error;
  device_->type_ = Cellular::kTypeGSM;
  EXPECT_CALL(*gsm_card_proxy_, EnablePIN(kPIN, true)).Times(1);
  device_->RequirePIN(kPIN, true, &error);
  EXPECT_TRUE(error.IsSuccess());
  device_->gsm_card_proxy_.reset(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularTest, RequirePINError) {
  Error error;
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_CALL(*gsm_card_proxy_, EnablePIN(_, _)).Times(0);
  device_->RequirePIN(kPIN, true, &error);
  EXPECT_EQ(Error::kNotSupported, error.type());
  device_->gsm_card_proxy_.reset(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularTest, EnterPIN) {
  Error error;
  device_->type_ = Cellular::kTypeGSM;
  EXPECT_CALL(*gsm_card_proxy_, SendPIN(kPIN)).Times(1);
  device_->EnterPIN(kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  device_->gsm_card_proxy_.reset(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularTest, EnterPINError) {
  Error error;
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_CALL(*gsm_card_proxy_, SendPIN(_)).Times(0);
  device_->EnterPIN(kPIN, &error);
  EXPECT_EQ(Error::kNotSupported, error.type());
  device_->gsm_card_proxy_.reset(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularTest, UnblockPIN) {
  Error error;
  device_->type_ = Cellular::kTypeGSM;
  EXPECT_CALL(*gsm_card_proxy_, SendPUK(kPUK, kPIN)).Times(1);
  device_->UnblockPIN(kPUK, kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  device_->gsm_card_proxy_.reset(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularTest, UnblockPINError) {
  Error error;
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_CALL(*gsm_card_proxy_, SendPUK(_, _)).Times(0);
  device_->UnblockPIN(kPUK, kPIN, &error);
  EXPECT_EQ(Error::kNotSupported, error.type());
  device_->gsm_card_proxy_.reset(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularTest, ChangePIN) {
  static const char kOldPIN[] = "1111";
  Error error;
  device_->type_ = Cellular::kTypeGSM;
  EXPECT_CALL(*gsm_card_proxy_, ChangePIN(kOldPIN, kPIN)).Times(1);
  device_->ChangePIN(kOldPIN, kPIN, &error);
  EXPECT_TRUE(error.IsSuccess());
  device_->gsm_card_proxy_.reset(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularTest, ChangePINError) {
  Error error;
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_CALL(*gsm_card_proxy_, ChangePIN(_, _)).Times(0);
  device_->ChangePIN(kPIN, kPIN, &error);
  EXPECT_EQ(Error::kNotSupported, error.type());
  device_->gsm_card_proxy_.reset(gsm_card_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

TEST_F(CellularTest, Scan) {
  static const char kID0[] = "123";
  static const char kID1[] = "456";
  Error error;
  device_->type_ = Cellular::kTypeGSM;
  device_->Scan(&error);
  EXPECT_TRUE(error.IsSuccess());
  ModemGSMNetworkProxyInterface::ScanResults results;
  results.push_back(ModemGSMNetworkProxyInterface::ScanResult());
  results[0][Cellular::kNetworkPropertyID] = kID0;
  results.push_back(ModemGSMNetworkProxyInterface::ScanResult());
  results[1][Cellular::kNetworkPropertyID] = kID1;
  EXPECT_CALL(*gsm_network_proxy_, Scan()).WillOnce(Return(results));
  device_->gsm_network_proxy_.reset(gsm_network_proxy_.release());
  device_->found_networks_.resize(2, Stringmap());
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(2, device_->found_networks_.size());
  EXPECT_EQ(kID0, device_->found_networks_[0][flimflam::kNetworkIdProperty]);
  EXPECT_EQ(kID1, device_->found_networks_[1][flimflam::kNetworkIdProperty]);
}

TEST_F(CellularTest, ParseScanResult) {
  static const char kID[] = "123";
  static const char kLongName[] = "long name";
  static const char kShortName[] = "short name";
  ModemGSMNetworkProxyInterface::ScanResult result;
  result[Cellular::kNetworkPropertyStatus] = "1";
  result[Cellular::kNetworkPropertyID] = kID;
  result[Cellular::kNetworkPropertyLongName] = kLongName;
  result[Cellular::kNetworkPropertyShortName] = kShortName;
  result[Cellular::kNetworkPropertyAccessTechnology] = "3";
  result["unknown property"] = "random value";
  Stringmap parsed = device_->ParseScanResult(result);
  EXPECT_EQ(5, parsed.size());
  EXPECT_EQ("available", parsed[flimflam::kStatusProperty]);
  EXPECT_EQ(kID, parsed[flimflam::kNetworkIdProperty]);
  EXPECT_EQ(kLongName, parsed[flimflam::kLongNameProperty]);
  EXPECT_EQ(kShortName, parsed[flimflam::kShortNameProperty]);
  EXPECT_EQ(flimflam::kNetworkTechnologyEdge,
            parsed[flimflam::kTechnologyProperty]);
}

TEST_F(CellularTest, Activate) {
  Error error;
  device_->type_ = Cellular::kTypeCDMA;
  device_->state_ = Cellular::kStateEnabled;
  EXPECT_CALL(*cdma_proxy_, Activate(kTestCarrier))
      .WillOnce(Return(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_ERROR));
  device_->Activate(kTestCarrier, &error);
  EXPECT_TRUE(error.IsSuccess());
  device_->cdma_proxy_.reset(cdma_proxy_.release());
  device_->CreateService();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_ACTIVATING,
            device_->cdma_.activation_state);
  EXPECT_EQ(flimflam::kActivationStateActivating,
            device_->service_->activation_state());
  EXPECT_EQ("", device_->service_->error());
}

TEST_F(CellularTest, ActivateError) {
  Error error;
  device_->type_ = Cellular::kTypeGSM;
  device_->Activate(kTestCarrier, &error);
  EXPECT_EQ(Error::kInvalidArguments, error.type());

  error.Populate(Error::kSuccess);
  device_->type_ = Cellular::kTypeCDMA;
  device_->Activate(kTestCarrier, &error);
  EXPECT_EQ(Error::kInvalidArguments, error.type());

  error.Populate(Error::kSuccess);
  device_->state_ = Cellular::kStateRegistered;
  EXPECT_CALL(*cdma_proxy_, Activate(kTestCarrier))
      .WillOnce(Return(MM_MODEM_CDMA_ACTIVATION_ERROR_NO_SIGNAL));
  device_->Activate(kTestCarrier, &error);
  EXPECT_TRUE(error.IsSuccess());
  device_->cdma_proxy_.reset(cdma_proxy_.release());
  device_->CreateService();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_CDMA_ACTIVATION_STATE_NOT_ACTIVATED,
            device_->cdma_.activation_state);
  EXPECT_EQ(flimflam::kActivationStateNotActivated,
            device_->service_->activation_state());
  EXPECT_EQ(flimflam::kErrorActivationFailed,
            device_->service_->error());
}

TEST_F(CellularTest, SetGSMAccessTechnology) {
  device_->type_ = Cellular::kTypeGSM;
  device_->SetGSMAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GSM);
  EXPECT_EQ(MM_MODEM_GSM_ACCESS_TECH_GSM, device_->gsm_.access_technology);
  device_->service_ = new CellularService(
      &control_interface_, &dispatcher_, &manager_, device_);
  device_->gsm_.registration_state = MM_MODEM_GSM_NETWORK_REG_STATUS_HOME;
  device_->SetGSMAccessTechnology(MM_MODEM_GSM_ACCESS_TECH_GPRS);
  EXPECT_EQ(MM_MODEM_GSM_ACCESS_TECH_GPRS, device_->gsm_.access_technology);
  EXPECT_EQ(flimflam::kNetworkTechnologyGprs,
            device_->service_->network_tech());
}

}  // namespace shill
