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
#include "shill/mock_device_info.h"
#include "shill/mock_dhcp_config.h"
#include "shill/mock_dhcp_provider.h"
#include "shill/mock_manager.h"
#include "shill/mock_modem_cdma_proxy.h"
#include "shill/mock_modem_proxy.h"
#include "shill/mock_modem_simple_proxy.h"
#include "shill/mock_sockets.h"
#include "shill/nice_mock_control.h"
#include "shill/rtnl_handler.h"
#include "shill/property_store_unittest.h"
#include "shill/proxy_factory.h"

using std::string;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;

namespace shill {

class TestEventDispatcher : public EventDispatcher {
 public:
  virtual IOInputHandler *CreateInputHandler(
      int fd,
      Callback1<InputData*>::Type *callback) {
    return NULL;
  }
};

class CellularPropertyTest : public PropertyStoreTest {
 public:
  CellularPropertyTest()
      : device_(new Cellular(&control_interface_,
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
  EXPECT_TRUE(device_->store()->Contains(flimflam::kNameProperty));
  EXPECT_FALSE(device_->store()->Contains(""));
}

TEST_F(CellularPropertyTest, Dispatch) {
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(
        device_->store(),
        flimflam::kCellularAllowRoamingProperty,
        PropertyStoreTest::kBoolV,
        &error));
  }
  {
    ::DBus::Error error;
    EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_->store(),
                                            flimflam::kScanIntervalProperty,
                                            PropertyStoreTest::kUint16V,
                                            &error));
  }
  // Ensure that attempting to write a R/O property returns InvalidArgs error.
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_->store(),
                                             flimflam::kAddressProperty,
                                             PropertyStoreTest::kStringV,
                                             &error));
    EXPECT_EQ(invalid_args_, error.name());
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_->store(),
                                             flimflam::kCarrierProperty,
                                             PropertyStoreTest::kStringV,
                                             &error));
    EXPECT_EQ(invalid_args_, error.name());
  }
  {
    ::DBus::Error error;
    EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_->store(),
                                             flimflam::kPRLVersionProperty,
                                             PropertyStoreTest::kInt16V,
                                             &error));
    EXPECT_EQ(invalid_args_, error.name());
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
    ProxyFactory::set_factory(&proxy_factory_);
    StartRTNLHandler();
    device_->set_dhcp_provider(&dhcp_provider_);
    EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&device_info_));
  }

  virtual void TearDown() {
    ProxyFactory::set_factory(NULL);
    device_->Stop();
    StopRTNLHandler();
    device_->set_dhcp_provider(NULL);
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    TestProxyFactory(CellularTest *test) : test_(test) {}

    virtual ModemProxyInterface *CreateModemProxy(ModemProxyListener *listener,
                                                  const string &path,
                                                  const string &service) {
      return test_->proxy_.release();
    }

    virtual ModemSimpleProxyInterface *CreateModemSimpleProxy(
        const string &path,
        const string &service) {
      return test_->simple_proxy_.release();
    }

    virtual ModemCDMAProxyInterface *CreateModemCDMAProxy(
        ModemCDMAProxyListener *listener,
        const string &path,
        const string &service) {
      return test_->cdma_proxy_.release();
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

  void StartRTNLHandler();
  void StopRTNLHandler();

  NiceMockControl control_interface_;
  TestEventDispatcher dispatcher_;
  MockGLib glib_;
  MockManager manager_;
  MockDeviceInfo device_info_;
  NiceMock<MockSockets> sockets_;
  scoped_ptr<MockModemProxy> proxy_;
  scoped_ptr<MockModemSimpleProxy> simple_proxy_;
  scoped_ptr<MockModemCDMAProxy> cdma_proxy_;
  TestProxyFactory proxy_factory_;

  MockDHCPProvider dhcp_provider_;
  scoped_refptr<MockDHCPConfig> dhcp_config_;

  CellularRefPtr device_;
};

const char CellularTest::kTestDeviceName[] = "usb0";
const char CellularTest::kTestDeviceAddress[] = "00:01:02:03:04:05";
const int CellularTest::kTestSocket = 123;
const char CellularTest::kDBusOwner[] = ":1.19";
const char CellularTest::kDBusPath[] = "/org/chromium/ModemManager/Gobi/0";
const char CellularTest::kTestCarrier[] = "The Cellular Carrier";

void CellularTest::StartRTNLHandler() {
  EXPECT_CALL(sockets_, Socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE))
      .WillOnce(Return(kTestSocket));
  EXPECT_CALL(sockets_, Bind(kTestSocket, _, sizeof(sockaddr_nl)))
      .WillOnce(Return(0));
  RTNLHandler::GetInstance()->Start(&dispatcher_, &sockets_);
}

void CellularTest::StopRTNLHandler() {
  EXPECT_CALL(sockets_, Close(kTestSocket)).WillOnce(Return(0));
  RTNLHandler::GetInstance()->Stop();
}

TEST_F(CellularTest, GetTypeString) {
  EXPECT_EQ("CellularTypeGSM", device_->GetTypeString());
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_EQ("CellularTypeCDMA", device_->GetTypeString());
}

TEST_F(CellularTest, GetNetworkTechnologyString) {
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

TEST_F(CellularTest, GetRoamingStateString) {
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

TEST_F(CellularTest, Start) {
  EXPECT_CALL(*proxy_, Enable(true)).Times(1);
  EXPECT_CALL(*simple_proxy_, GetStatus())
      .WillOnce(Return(DBusPropertiesMap()));
  EXPECT_CALL(*proxy_, GetInfo()).WillOnce(Return(ModemProxyInterface::Info()));
  device_->Start();
  EXPECT_EQ(Cellular::kStateEnabled, device_->state_);
}

TEST_F(CellularTest, StartRegister) {
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_CALL(*proxy_, Enable(true)).Times(1);
  EXPECT_CALL(*simple_proxy_, GetStatus())
      .WillOnce(Return(DBusPropertiesMap()));
  EXPECT_CALL(*proxy_, GetInfo()).WillOnce(Return(ModemProxyInterface::Info()));
  EXPECT_CALL(*cdma_proxy_, GetRegistrationState(_, _))
      .WillOnce(DoAll(
          SetArgumentPointee<0>(MM_MODEM_CDMA_REGISTRATION_STATE_HOME),
          SetArgumentPointee<1>(MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN)));
  EXPECT_CALL(*cdma_proxy_, GetSignalQuality()).WillOnce(Return(90));
  device_->Start();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateRegistered, device_->state_);
  ASSERT_TRUE(device_->service_.get());
  EXPECT_EQ(flimflam::kNetworkTechnology1Xrtt,
            device_->service_->network_tech());
  EXPECT_EQ(flimflam::kRoamingStateHome, device_->service_->roaming_state());
}

TEST_F(CellularTest, StartConnected) {
  EXPECT_CALL(device_info_, GetFlags(device_->interface_index(), _))
      .WillOnce(Return(true));
  device_->type_ = Cellular::kTypeCDMA;
  device_->set_modem_state(Cellular::kModemStateConnected);
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
  device_->Start();
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(Cellular::kStateLinked, device_->state_);
}

TEST_F(CellularTest, InitProxiesCDMA) {
  device_->type_ = Cellular::kTypeCDMA;
  device_->InitProxies();
  EXPECT_TRUE(device_->proxy_.get());
  EXPECT_TRUE(device_->simple_proxy_.get());
  EXPECT_TRUE(device_->cdma_proxy_.get());
}

TEST_F(CellularTest, InitProxiesGSM) {
  device_->type_ = Cellular::kTypeGSM;
  device_->InitProxies();
  EXPECT_TRUE(device_->proxy_.get());
  EXPECT_TRUE(device_->simple_proxy_.get());
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
}

namespace {

MATCHER(ContainsPhoneNumber, "") {
  return ContainsKey(arg, Cellular::kConnectPropertyPhoneNumber);
}

}  // namespace {}

TEST_F(CellularTest, Connect) {
  Error error;
  EXPECT_CALL(device_info_, GetFlags(device_->interface_index(), _))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
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

}  // namespace shill
