// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <sys/socket.h>
#include <linux/if.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.

#include <base/bind.h>
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
#include "shill/mock_metrics.h"
#include "shill/mock_modem_cdma_proxy.h"
#include "shill/mock_modem_gsm_card_proxy.h"
#include "shill/mock_modem_gsm_network_proxy.h"
#include "shill/mock_modem_proxy.h"
#include "shill/mock_modem_simple_proxy.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/nice_mock_control.h"
#include "shill/property_store_unittest.h"
#include "shill/proxy_factory.h"

using base::Bind;
using base::Unretained;
using std::map;
using std::string;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;
using testing::Unused;

namespace shill {

MATCHER(IsSuccess, "") {
  return arg.IsSuccess();
}

class CellularPropertyTest : public PropertyStoreTest {
 public:
  CellularPropertyTest()
      : device_(new Cellular(control_interface(),
                             NULL,
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

TEST_F(CellularPropertyTest, SetProperty) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::SetProperty(
        device_->mutable_store(),
        flimflam::kCellularAllowRoamingProperty,
        PropertyStoreTest::kBoolV,
        &error));
  }
  // Ensure that attempting to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                          flimflam::kAddressProperty,
                                          PropertyStoreTest::kStringV,
                                          &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::SetProperty(device_->mutable_store(),
                                          flimflam::kCarrierProperty,
                                          PropertyStoreTest::kStringV,
                                          &error));
    EXPECT_EQ(invalid_args(), error.name());
  }
}

class CellularTest : public testing::Test {
 public:
  CellularTest()
      : manager_(&control_interface_, &dispatcher_, &metrics_, &glib_),
        device_info_(&control_interface_, &dispatcher_, &metrics_, &manager_),
        dhcp_config_(new MockDHCPConfig(&control_interface_,
                                        &dispatcher_,
                                        &dhcp_provider_,
                                        kTestDeviceName,
                                        "",
                                        &glib_)),
        device_(new Cellular(&control_interface_,
                             &dispatcher_,
                             &metrics_,
                             &manager_,
                             kTestDeviceName,
                             kTestDeviceAddress,
                             3,
                             Cellular::kTypeGSM,
                             kDBusOwner,
                             kDBusPath,
                             NULL)),
        proxy_(new MockModemProxy()),
        simple_proxy_(new MockModemSimpleProxy()),
        cdma_proxy_(new MockModemCDMAProxy()),
        gsm_card_proxy_(new MockModemGSMCardProxy()),
        gsm_network_proxy_(new MockModemGSMNetworkProxy()),
        proxy_factory_(this),
        provider_db_(NULL) {
    device_->capability_->proxy_factory_ = &proxy_factory_;
  }

  virtual ~CellularTest() {
    mobile_provider_close_db(provider_db_);
    provider_db_ = NULL;
  }

  virtual void SetUp() {
    static_cast<Device *>(device_)->rtnl_handler_ = &rtnl_handler_;
    device_->set_dhcp_provider(&dhcp_provider_);
    EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&device_info_));
    EXPECT_CALL(manager_, DeregisterService(_)).Times(AnyNumber());
  }

  virtual void TearDown() {
    device_->DestroyIPConfig();
    device_->state_ = Cellular::kStateDisabled;
    device_->capability_->ReleaseProxies();
    device_->set_dhcp_provider(NULL);
  }

  void InvokeEnable(bool enable, Error *error,
                    const ResultCallback &callback, int timeout) {
    callback.Run(Error());
  }
  void InvokeGetSignalQuality(Error *error,
                              const SignalQualityCallback &callback,
                              int timeout) {
    callback.Run(kStrength, Error());
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
    static const char kManufacturer[] = "Company";
    static const char kModelID[] = "Gobi 2000";
    static const char kHWRev[] = "A00B1234";
    ModemHardwareInfo info;
    info._1 = kManufacturer;
    info._2 = kModelID;
    info._3 = kHWRev;
    callback.Run(info, Error());
  }
  void InvokeGetRegistrationState1X(Error *error,
                                    const RegistrationStateCallback &callback,
                                    int timeout) {
    callback.Run(MM_MODEM_CDMA_REGISTRATION_STATE_HOME,
                 MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN,
                 Error());
  }
  void InvokeGetIMEI(Error *error, const GSMIdentifierCallback &callback,
                     int timeout) {
    callback.Run(kIMEI, Error());
  }
  void InvokeGetIMSI(Error *error, const GSMIdentifierCallback &callback,
                     int timeout) {
    callback.Run(kIMSI, Error());
  }
  void InvokeGetMSISDN(Error *error, const GSMIdentifierCallback &callback,
                       int timeout) {
    callback.Run(kMSISDN, Error());
  }
  void InvokeGetSPN(Error *error, const GSMIdentifierCallback &callback,
                    int timeout) {
    callback.Run(kTestCarrier, Error());
  }
  void InvokeGetRegistrationInfo(Error *error,
                                 const RegistrationInfoCallback &callback,
                                 int timeout) {
    static const char kNetworkID[] = "22803";
    callback.Run(MM_MODEM_GSM_NETWORK_REG_STATUS_ROAMING,
                 kNetworkID, kTestCarrier, Error());
  }
  void InvokeRegister(const string &network_id,
                      Error *error,
                      const ResultCallback &callback,
                      int timeout) {
    callback.Run(Error());
  }
  void InvokeGetRegistrationState(Error *error,
                                  const RegistrationStateCallback &callback,
                                  int timeout) {
    callback.Run(MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED,
                 MM_MODEM_CDMA_REGISTRATION_STATE_HOME,
                 Error());
  }
  void InvokeConnect(DBusPropertiesMap props, Error *error,
                     const ResultCallback &callback, int timeout) {
    EXPECT_EQ(Service::kStateAssociating, device_->service_->state());
    callback.Run(Error());
  }
  void InvokeConnectFail(DBusPropertiesMap props, Error *error,
                     const ResultCallback &callback, int timeout) {
    EXPECT_EQ(Service::kStateAssociating, device_->service_->state());
    callback.Run(Error(Error::kNotOnHomeNetwork));
  }
  void InvokeDisconnect(Error *error, const ResultCallback &callback,
                        int timeout) {
    if (!callback.is_null())
      callback.Run(Error());
  }

  void ExpectCdmaStartModem(string network_technology) {
    EXPECT_CALL(*proxy_, Enable(true, _, _, CellularCapability::kTimeoutEnable))
        .WillOnce(Invoke(this, &CellularTest::InvokeEnable));
    EXPECT_CALL(*simple_proxy_,
                GetModemStatus(_, _, CellularCapability::kTimeoutDefault))
        .WillOnce(Invoke(this, &CellularTest::InvokeGetModemStatus));
    EXPECT_CALL(*proxy_,
                GetModemInfo(_, _, CellularCapability::kTimeoutDefault))
        .WillOnce(Invoke(this, &CellularTest::InvokeGetModemInfo));
    if (network_technology == flimflam::kNetworkTechnology1Xrtt)
      EXPECT_CALL(*cdma_proxy_, GetRegistrationState(NULL, _, _))
          .WillOnce(Invoke(this,
                           &CellularTest::InvokeGetRegistrationState1X));
    else
      EXPECT_CALL(*cdma_proxy_, GetRegistrationState(NULL, _, _))
          .WillOnce(Invoke(this,
                           &CellularTest::InvokeGetRegistrationState));
    EXPECT_CALL(*cdma_proxy_, GetSignalQuality(NULL, _, _))
        .Times(2)
        .WillRepeatedly(Invoke(this,
                               &CellularTest::InvokeGetSignalQuality));
    EXPECT_CALL(*this, TestCallback(IsSuccess()));
  }

  MOCK_METHOD1(TestCallback, void(const Error &error));

 protected:
  static const char kTestDeviceName[];
  static const char kTestDeviceAddress[];
  static const char kDBusOwner[];
  static const char kDBusPath[];
  static const char kTestCarrier[];
  static const char kMEID[];
  static const char kIMEI[];
  static const char kIMSI[];
  static const char kMSISDN[];
  static const char kTestMobileProviderDBPath[];
  static const int kStrength;

  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(CellularTest *test) : test_(test) {}

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
    CellularTest *test_;
  };
  void StartRTNLHandler();
  void StopRTNLHandler();

  void SetCellularType(Cellular::Type type) {
    device_->InitCapability(type, &proxy_factory_);
  }

  CellularCapabilityCDMA *GetCapabilityCDMA() {
    return dynamic_cast<CellularCapabilityCDMA *>(device_->capability_.get());
  }

  CellularCapabilityGSM *GetCapabilityGSM() {
    return dynamic_cast<CellularCapabilityGSM *>(device_->capability_.get());
  }

  NiceMockControl control_interface_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  NiceMock<MockRTNLHandler> rtnl_handler_;

  MockDHCPProvider dhcp_provider_;
  scoped_refptr<MockDHCPConfig> dhcp_config_;

  CellularRefPtr device_;
  scoped_ptr<MockModemProxy> proxy_;
  scoped_ptr<MockModemSimpleProxy> simple_proxy_;
  scoped_ptr<MockModemCDMAProxy> cdma_proxy_;
  scoped_ptr<MockModemGSMCardProxy> gsm_card_proxy_;
  scoped_ptr<MockModemGSMNetworkProxy> gsm_network_proxy_;
  TestProxyFactory proxy_factory_;
  mobile_provider_db *provider_db_;
};

const char CellularTest::kTestDeviceName[] = "usb0";
const char CellularTest::kTestDeviceAddress[] = "00:01:02:03:04:05";
const char CellularTest::kDBusOwner[] = ":1.19";
const char CellularTest::kDBusPath[] = "/org/chromium/ModemManager/Gobi/0";
const char CellularTest::kTestCarrier[] = "The Cellular Carrier";
const char CellularTest::kMEID[] = "01234567EF8901";
const char CellularTest::kIMEI[] = "987654321098765";
const char CellularTest::kIMSI[] = "123456789012345";
const char CellularTest::kMSISDN[] = "12345678901";
const char CellularTest::kTestMobileProviderDBPath[] =
    "provider_db_unittest.bfd";
const int CellularTest::kStrength = 90;

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
  SetCellularType(Cellular::kTypeCDMA);
  ExpectCdmaStartModem(flimflam::kNetworkTechnology1Xrtt);
  EXPECT_CALL(*cdma_proxy_, MEID()).WillOnce(Return(kMEID));
  Error error;
  device_->Start(&error, Bind(&CellularTest::TestCallback, Unretained(this)));
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(kMEID, device_->capability_->meid_);
  EXPECT_EQ(kTestCarrier, device_->capability_->carrier_);
  EXPECT_EQ(Cellular::kStateRegistered, device_->state_);
  ASSERT_TRUE(device_->service_.get());
  EXPECT_EQ(flimflam::kNetworkTechnology1Xrtt,
            device_->service_->network_technology());
  EXPECT_EQ(kStrength, device_->service_->strength());
  EXPECT_EQ(flimflam::kRoamingStateHome, device_->service_->roaming_state());
}

TEST_F(CellularTest, StartGSMRegister) {
  provider_db_ = mobile_provider_open_db(kTestMobileProviderDBPath);
  ASSERT_TRUE(provider_db_);
  device_->provider_db_ = provider_db_;
  EXPECT_CALL(*proxy_, Enable(true, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this, &CellularTest::InvokeEnable));
  EXPECT_CALL(*gsm_network_proxy_,
              Register("", _, _, CellularCapability::kTimeoutRegister))
      .WillOnce(Invoke(this, &CellularTest::InvokeRegister));
  EXPECT_CALL(*simple_proxy_,
              GetModemStatus(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularTest::InvokeGetModemStatus));
  EXPECT_CALL(*gsm_card_proxy_,
              GetIMEI(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularTest::InvokeGetIMEI));
  EXPECT_CALL(*gsm_card_proxy_,
              GetIMSI(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularTest::InvokeGetIMSI));
  EXPECT_CALL(*gsm_card_proxy_,
              GetSPN(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularTest::InvokeGetSPN));
  EXPECT_CALL(*gsm_card_proxy_,
              GetMSISDN(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularTest::InvokeGetMSISDN));
  EXPECT_CALL(*gsm_network_proxy_, AccessTechnology())
      .WillOnce(Return(MM_MODEM_GSM_ACCESS_TECH_EDGE));
  EXPECT_CALL(*gsm_card_proxy_, EnabledFacilityLocks())
      .WillOnce(Return(MM_MODEM_GSM_FACILITY_SIM));
  EXPECT_CALL(*proxy_, GetModemInfo(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularTest::InvokeGetModemInfo));
  static const char kNetworkID[] = "22803";
  EXPECT_CALL(*gsm_network_proxy_,
              GetRegistrationInfo(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularTest::InvokeGetRegistrationInfo));
  EXPECT_CALL(*gsm_network_proxy_, GetSignalQuality(NULL, _, _))
      .Times(2)
      .WillRepeatedly(Invoke(this,
                             &CellularTest::InvokeGetSignalQuality));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  Error error;
  device_->Start(&error, Bind(&CellularTest::TestCallback, Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(kIMEI, device_->capability_->imei_);
  EXPECT_EQ(kIMSI, device_->capability_->imsi_);
  EXPECT_EQ(kTestCarrier, GetCapabilityGSM()->spn_);
  EXPECT_EQ(kMSISDN, device_->capability_->mdn_);
  EXPECT_EQ(Cellular::kStateRegistered, device_->state_);
  ASSERT_TRUE(device_->service_.get());
  EXPECT_EQ(flimflam::kNetworkTechnologyEdge,
            device_->service_->network_technology());
  EXPECT_TRUE(GetCapabilityGSM()->sim_lock_status_.enabled);
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
  device_->capability_->meid_ = kMEID;
  ExpectCdmaStartModem(flimflam::kNetworkTechnologyEvdo);
  Error error;
  device_->Start(&error, Bind(&CellularTest::TestCallback, Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateConnected, device_->state_);
}

TEST_F(CellularTest, StartLinked) {
  EXPECT_CALL(device_info_, GetFlags(device_->interface_index(), _))
      .WillOnce(DoAll(SetArgumentPointee<1>(IFF_UP), Return(true)));
  SetCellularType(Cellular::kTypeCDMA);
  device_->set_modem_state(Cellular::kModemStateConnected);
  device_->capability_->meid_ = kMEID;
  ExpectCdmaStartModem(flimflam::kNetworkTechnologyEvdo);
  EXPECT_CALL(dhcp_provider_, CreateConfig(kTestDeviceName, _))
      .WillOnce(Return(dhcp_config_));
  EXPECT_CALL(*dhcp_config_, RequestIP()).WillOnce(Return(true));
  EXPECT_CALL(manager_, UpdateService(_)).Times(2);
  Error error;
  device_->Start(&error, Bind(&CellularTest::TestCallback, Unretained(this)));
  EXPECT_TRUE(error.IsSuccess());
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateLinked, device_->state_);
  EXPECT_EQ(Service::kStateConfiguring, device_->service_->state());
  device_->SelectService(NULL);
}

TEST_F(CellularTest, CreateService) {
  SetCellularType(Cellular::kTypeCDMA);
  static const char kPaymentURL[] = "https://payment.url";
  static const char kUsageURL[] = "https://usage.url";
  device_->home_provider_.SetName(kTestCarrier);
  GetCapabilityCDMA()->olp_.SetURL(kPaymentURL);
  GetCapabilityCDMA()->usage_url_ = kUsageURL;
  device_->CreateService();
  ASSERT_TRUE(device_->service_.get());
  EXPECT_EQ(kPaymentURL, device_->service_->olp().GetURL());
  EXPECT_EQ(kUsageURL, device_->service_->usage_url());
  EXPECT_EQ(kTestCarrier, device_->service_->serving_operator().GetName());
  EXPECT_TRUE(device_->service_->TechnologyIs(Technology::kCellular));
}

namespace {

MATCHER(ContainsPhoneNumber, "") {
  return ContainsKey(arg, CellularCapability::kConnectPropertyPhoneNumber);
}

}  // namespace

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
      &control_interface_, &dispatcher_, &metrics_, &manager_, device_);

  device_->capability_->allow_roaming_ = false;
  device_->service_->roaming_state_ = flimflam::kRoamingStateRoaming;
  device_->Connect(&error);
  EXPECT_EQ(Error::kNotOnHomeNetwork, error.type());

  error.Populate(Error::kSuccess);
  EXPECT_CALL(*simple_proxy_,
              Connect(ContainsPhoneNumber(), _, _,
                      CellularCapability::kTimeoutConnect))
                .Times(2)
                .WillRepeatedly(Invoke(this, &CellularTest::InvokeConnect));
  device_->capability_->simple_proxy_.reset(simple_proxy_.release());
  device_->service_->roaming_state_ = flimflam::kRoamingStateHome;
  device_->state_ = Cellular::kStateRegistered;
  device_->Connect(&error);
  EXPECT_TRUE(error.IsSuccess());
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateConnected, device_->state_);

  device_->capability_->allow_roaming_ = true;
  device_->service_->roaming_state_ = flimflam::kRoamingStateRoaming;
  device_->state_ = Cellular::kStateRegistered;
  device_->Connect(&error);
  EXPECT_TRUE(error.IsSuccess());
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateConnected, device_->state_);
}

TEST_F(CellularTest, Disconnect) {
  Error error;
  device_->state_ = Cellular::kStateRegistered;
  device_->Disconnect(&error);
  EXPECT_EQ(Error::kNotConnected, error.type());
  error.Reset();

  device_->state_ = Cellular::kStateConnected;
  EXPECT_CALL(*proxy_,
              Disconnect(_, _, CellularCapability::kTimeoutDefault))
      .WillOnce(Invoke(this, &CellularTest::InvokeDisconnect));
  device_->capability_->proxy_.reset(proxy_.release());
  device_->Disconnect(&error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(Cellular::kStateRegistered, device_->state_);
}

TEST_F(CellularTest, ConnectFailure) {
  SetCellularType(Cellular::kTypeCDMA);
  device_->state_ = Cellular::kStateRegistered;
  device_->service_ = new CellularService(
      &control_interface_, &dispatcher_, &metrics_, &manager_, device_);
  ASSERT_EQ(Service::kStateIdle, device_->service_->state());
  EXPECT_CALL(*simple_proxy_,
              Connect(_, _, _, CellularCapability::kTimeoutConnect))
                .WillOnce(Invoke(this, &CellularTest::InvokeConnectFail));
  device_->capability_->simple_proxy_.reset(simple_proxy_.release());
  Error error;
  device_->Connect(&error);
  EXPECT_EQ(Service::kStateFailure, device_->service_->state());
}

}  // namespace shill
