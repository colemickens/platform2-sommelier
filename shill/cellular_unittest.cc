// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <sys/socket.h>
#include <linux/if.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.

#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>
#include <mobile_provider.h>

#include "shill/cellular_capability_cdma.h"
#include "shill/cellular_capability_gsm.h"
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
                             "",
                             NULL)) {}
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
                             kDBusPath,
                             NULL)),
        provider_db_(NULL) {}

  virtual ~CellularTest() {
    mobile_provider_close_db(provider_db_);
    provider_db_ = NULL;
  }

  virtual void SetUp() {
    device_->proxy_factory_ = &proxy_factory_;
    device_->capability_->proxy_factory_ = &proxy_factory_;
    static_cast<Device *>(device_)->rtnl_handler_ = &rtnl_handler_;
    device_->set_dhcp_provider(&dhcp_provider_);
    EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&device_info_));
    EXPECT_CALL(manager_, DeregisterService(_)).Times(AnyNumber());
  }

  virtual void TearDown() {
    device_->capability_->proxy_factory_ = NULL;
    device_->proxy_factory_ = NULL;
    device_->Stop();
    device_->set_dhcp_provider(NULL);
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(CellularTest *test) : test_(test) {}

    virtual ModemProxyInterface *CreateModemProxy(
        ModemProxyDelegate */*delegate*/,
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
  static const char kTestMobileProviderDBPath[];

  void StartRTNLHandler();
  void StopRTNLHandler();

  void SetCellularType(Cellular::Type type) {
    device_->type_ = type;
    device_->InitCapability();
  }

  CellularCapabilityCDMA *GetCapabilityCDMA() {
    return dynamic_cast<CellularCapabilityCDMA *>(device_->capability_.get());
  }

  CellularCapabilityGSM *GetCapabilityGSM() {
    return dynamic_cast<CellularCapabilityGSM *>(device_->capability_.get());
  }

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
  mobile_provider_db *provider_db_;
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
const char CellularTest::kTestMobileProviderDBPath[] =
    "provider_db_unittest.bfd";

TEST_F(CellularTest, GetTypeString) {
  EXPECT_EQ("CellularTypeGSM", device_->GetTypeString());
  SetCellularType(Cellular::kTypeCDMA);
  EXPECT_EQ("CellularTypeCDMA", device_->GetTypeString());
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

TEST_F(CellularTest, StartCDMARegister) {
  const int kStrength = 90;
  SetCellularType(Cellular::kTypeCDMA);
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
  provider_db_ = mobile_provider_open_db(kTestMobileProviderDBPath);
  ASSERT_TRUE(provider_db_);
  device_->provider_db_ = provider_db_;
  const int kStrength = 70;
  EXPECT_CALL(*proxy_, Enable(true)).Times(1);
  EXPECT_CALL(*gsm_network_proxy_, Register("")).Times(1);
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
  static const char kNetworkID[] = "22803";
  reg_info._1 = MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING;
  reg_info._2 = kNetworkID;
  EXPECT_CALL(*gsm_network_proxy_, GetRegistrationInfo())
      .WillOnce(Return(reg_info));
  EXPECT_CALL(*gsm_network_proxy_, GetSignalQuality())
      .WillOnce(Return(kStrength));
  device_->Start();
  EXPECT_EQ(kIMEI, device_->imei_);
  EXPECT_EQ(kIMSI, device_->imsi_);
  EXPECT_EQ(kTestCarrier, GetCapabilityGSM()->spn());
  EXPECT_EQ(kMSISDN, device_->mdn_);
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateRegistered, device_->state_);
  ASSERT_TRUE(device_->service_.get());
  EXPECT_EQ(flimflam::kNetworkTechnologyEdge,
            device_->service_->network_tech());
  EXPECT_EQ(kStrength, device_->service_->strength());
  EXPECT_EQ(flimflam::kRoamingStateRoaming, device_->service_->roaming_state());
  EXPECT_EQ(kNetworkID, device_->service_->serving_operator().GetCode());
  EXPECT_EQ("Orange", device_->service_->serving_operator().GetName());
  EXPECT_EQ("ch", device_->service_->serving_operator().GetCountry());
}

TEST_F(CellularTest, StartConnected) {
  EXPECT_CALL(device_info_, GetFlags(device_->interface_index(), _))
      .WillOnce(Return(true));
  SetCellularType(Cellular::kTypeCDMA);
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
  SetCellularType(Cellular::kTypeCDMA);
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

TEST_F(CellularTest, InitProxies) {
  device_->InitProxies();
  EXPECT_TRUE(device_->proxy_.get());
  EXPECT_TRUE(device_->simple_proxy_.get());
}

TEST_F(CellularTest, GetModemStatus) {
  SetCellularType(Cellular::kTypeCDMA);
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

TEST_F(CellularTest, CreateService) {
  SetCellularType(Cellular::kTypeCDMA);
  static const char kPaymentURL[] = "https://payment.url";
  static const char kUsageURL[] = "https://usage.url";
  device_->home_provider_.SetName(kTestCarrier);
  GetCapabilityCDMA()->payment_url_ = kPaymentURL;
  GetCapabilityCDMA()->usage_url_ = kUsageURL;
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

  EXPECT_CALL(*simple_proxy_, Connect(ContainsPhoneNumber())).Times(2);
  device_->simple_proxy_.reset(simple_proxy_.release());
  dispatcher_.DispatchPendingEvents();
}

}  // namespace shill
