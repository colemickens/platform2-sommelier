// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_universal.h"

#include <string>
#include <vector>

#include <base/bind.h>
#include <base/stringprintf.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mobile_provider.h>
#include <ModemManager/ModemManager.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_cellular_operator_info.h"
#include "shill/mock_cellular_service.h"
#include "shill/mock_dbus_properties_proxy.h"
#include "shill/mock_event_dispatcher.h"
#include "shill/mock_mm1_bearer_proxy.h"
#include "shill/mock_mm1_modem_modem3gpp_proxy.h"
#include "shill/mock_mm1_modem_modemcdma_proxy.h"
#include "shill/mock_mm1_modem_proxy.h"
#include "shill/mock_mm1_modem_simple_proxy.h"
#include "shill/mock_mm1_sim_proxy.h"
#include "shill/mock_modem_info.h"
#include "shill/mock_pending_activation_store.h"
#include "shill/mock_profile.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/proxy_factory.h"

using base::Bind;
using base::StringPrintf;
using base::Unretained;
using std::string;
using std::vector;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::NiceMock;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace shill {

MATCHER(IsSuccess, "") {
  return arg.IsSuccess();
}
MATCHER(IsFailure, "") {
  return arg.IsFailure();
}
MATCHER_P(HasApn, expected_apn, "") {
  string apn;
  return (DBusProperties::GetString(arg,
                                    CellularCapabilityUniversal::kConnectApn,
                                    &apn) &&
          apn == expected_apn);
}

class CellularCapabilityUniversalTest : public testing::TestWithParam<string> {
 public:
  CellularCapabilityUniversalTest(EventDispatcher *dispatcher)
      : modem_info_(NULL, dispatcher, NULL, NULL, NULL),
        bearer_proxy_(new mm1::MockBearerProxy()),
        modem_3gpp_proxy_(new mm1::MockModemModem3gppProxy()),
        modem_cdma_proxy_(new mm1::MockModemModemCdmaProxy()),
        modem_proxy_(new mm1::MockModemProxy()),
        modem_simple_proxy_(new mm1::MockModemSimpleProxy()),
        sim_proxy_(new mm1::MockSimProxy()),
        properties_proxy_(new MockDBusPropertiesProxy()),
        proxy_factory_(this),
        capability_(NULL),
        device_adaptor_(NULL),
        cellular_(new Cellular(&modem_info_,
                               "",
                               kMachineAddress,
                               0,
                               Cellular::kTypeUniversal,
                               "",
                               "",
                               "",
                               &proxy_factory_)),
        service_(new MockCellularService(&modem_info_, cellular_)) {
    modem_info_.metrics()->RegisterDevice(cellular_->interface_index(),
                                          Technology::kCellular);
  }

  virtual ~CellularCapabilityUniversalTest() {
    cellular_->service_ = NULL;
    capability_ = NULL;
    device_adaptor_ = NULL;
  }

  virtual void SetUp() {
    capability_ = dynamic_cast<CellularCapabilityUniversal *>(
        cellular_->capability_.get());
    device_adaptor_ =
        dynamic_cast<DeviceMockAdaptor *>(cellular_->adaptor());
    cellular_->service_ = service_;

    // kStateUnknown leads to minimal extra work in maintaining
    // activation state.
    ON_CALL(*modem_info_.mock_pending_activation_store(),
            GetActivationState(PendingActivationStore::kIdentifierICCID, _))
        .WillByDefault(Return(PendingActivationStore::kStateUnknown));
  }

  virtual void TearDown() {
    capability_->proxy_factory_ = NULL;
  }

  void InitProviderDB() {
    modem_info_.SetProviderDB(kTestMobileProviderDBPath);
  }

  void SetService() {
    cellular_->service_ = new CellularService(&modem_info_, cellular_);
  }

  void InvokeEnable(bool enable, Error *error,
                    const ResultCallback &callback, int timeout) {
    callback.Run(Error());
  }
  void InvokeEnableFail(bool enable, Error *error,
                        const ResultCallback &callback, int timeout) {
    callback.Run(Error(Error::kOperationFailed));
  }
  void InvokeRegister(const string &operator_id, Error *error,
                      const ResultCallback &callback, int timeout) {
    callback.Run(Error());
  }

  void InvokeScan(Error *error, const DBusPropertyMapsCallback &callback,
                  int timeout) {
    callback.Run(CellularCapabilityUniversal::ScanResults(), Error());
  }
  void ScanError(Error *error, const DBusPropertyMapsCallback &callback,
                 int timeout) {
    error->Populate(Error::kOperationFailed);
  }

  bool InvokeScanningOrSearchingTimeout() {
    capability_->OnScanningOrSearchingTimeout();
    return true;
  }

  void Set3gppProxy() {
    capability_->modem_3gpp_proxy_.reset(modem_3gpp_proxy_.release());
  }

  void SetSimpleProxy() {
    capability_->modem_simple_proxy_.reset(modem_simple_proxy_.release());
  }

  void ReleaseCapabilityProxies() {
    capability_->ReleaseProxies();
  }

  void SetRegistrationDroppedUpdateTimeout(int64 timeout_milliseconds) {
    capability_->registration_dropped_update_timeout_milliseconds_ =
        timeout_milliseconds;
  }

  MOCK_METHOD1(TestCallback, void(const Error &error));

  MOCK_METHOD0(DummyCallback, void(void));

  void SetMockRegistrationDroppedUpdateCallback() {
    capability_->registration_dropped_update_callback_.Reset(
        Bind(&CellularCapabilityUniversalTest::DummyCallback,
             Unretained(this)));
  }

 protected:
  static const char kActiveBearerPathPrefix[];
  static const char kImei[];
  static const char kInactiveBearerPathPrefix[];
  static const char kMachineAddress[];
  static const char kSimPath[];
  static const uint32 kAccessTechnologies;
  static const char kTestMobileProviderDBPath[];

  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(CellularCapabilityUniversalTest *test) :
        test_(test) {
      ::DBus::Variant ip_method_dhcp;
      ip_method_dhcp.writer().append_uint32(MM_BEARER_IP_METHOD_DHCP);
      bearer_ip4config_dhcp_[
          CellularCapabilityUniversal::kIpConfigPropertyMethod] =
          ip_method_dhcp;
    }

    virtual mm1::BearerProxyInterface *CreateBearerProxy(
        const std::string &path,
        const std::string &/*service*/) {
      mm1::MockBearerProxy *bearer_proxy = test_->bearer_proxy_.release();
      if (path.find(kActiveBearerPathPrefix) != std::string::npos)
        ON_CALL(*bearer_proxy, Connected()).WillByDefault(Return(true));
      else
        ON_CALL(*bearer_proxy, Connected()).WillByDefault(Return(false));
      ON_CALL(*bearer_proxy, Ip4Config()).WillByDefault(Return(
          bearer_ip4config_dhcp_));
      test_->bearer_proxy_.reset(new mm1::MockBearerProxy());
      return bearer_proxy;
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
      mm1::MockSimProxy *sim_proxy = test_->sim_proxy_.release();
      test_->sim_proxy_.reset(new mm1::MockSimProxy());
      return sim_proxy;
    }
    virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
        const std::string &/*path*/,
        const std::string &/*service*/) {
      MockDBusPropertiesProxy *properties_proxy =
          test_->properties_proxy_.release();
      test_->properties_proxy_.reset(new MockDBusPropertiesProxy());
      return properties_proxy;
    }

   private:
    CellularCapabilityUniversalTest *test_;
    DBusPropertiesMap bearer_ip4config_dhcp_;
  };

  MockModemInfo modem_info_;
  scoped_ptr<mm1::MockBearerProxy> bearer_proxy_;
  scoped_ptr<mm1::MockModemModem3gppProxy> modem_3gpp_proxy_;
  scoped_ptr<mm1::MockModemModemCdmaProxy> modem_cdma_proxy_;
  scoped_ptr<mm1::MockModemProxy> modem_proxy_;
  scoped_ptr<mm1::MockModemSimpleProxy> modem_simple_proxy_;
  scoped_ptr<mm1::MockSimProxy> sim_proxy_;
  scoped_ptr<MockDBusPropertiesProxy> properties_proxy_;
  TestProxyFactory proxy_factory_;
  CellularCapabilityUniversal *capability_;  // Owned by |cellular_|.
  DeviceMockAdaptor *device_adaptor_;  // Owned by |cellular_|.
  CellularRefPtr cellular_;
  MockCellularService *service_;  // owned by cellular_
  DBusPropertyMapsCallback scan_callback_;  // saved for testing scan operations
  DBusPathCallback connect_callback_;  // saved for testing connect operations
};

// Most of our tests involve using a real EventDispatcher object.
class CellularCapabilityUniversalMainTest
    : public CellularCapabilityUniversalTest {
 public:
  CellularCapabilityUniversalMainTest() :
      CellularCapabilityUniversalTest(&dispatcher_) {}

 protected:
  EventDispatcher dispatcher_;
};

// Tests that involve timers will (or may) use a mock of the event dispatcher
// instead of a real one.
class CellularCapabilityUniversalTimerTest
    : public CellularCapabilityUniversalTest {
 public:
  CellularCapabilityUniversalTimerTest()
      : CellularCapabilityUniversalTest(&mock_dispatcher_) {}

 protected:
  ::testing::StrictMock<MockEventDispatcher> mock_dispatcher_;
};

const char CellularCapabilityUniversalTest::kActiveBearerPathPrefix[] =
    "/bearer/active";
const char CellularCapabilityUniversalTest::kImei[] = "999911110000";
const char CellularCapabilityUniversalTest::kInactiveBearerPathPrefix[] =
    "/bearer/inactive";
const char CellularCapabilityUniversalTest::kMachineAddress[] =
    "TestMachineAddress";
const char CellularCapabilityUniversalTest::kSimPath[] = "/foo/sim";
const uint32 CellularCapabilityUniversalTest::kAccessTechnologies =
    MM_MODEM_ACCESS_TECHNOLOGY_LTE |
    MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS;
const char CellularCapabilityUniversalTest::kTestMobileProviderDBPath[] =
    "provider_db_unittest.bfd";

TEST_F(CellularCapabilityUniversalMainTest, StartModem) {
  // Set up mock modem properties
  DBusPropertiesMap modem_properties;
  string operator_name = "TestOperator";
  string operator_code = "001400";

  modem_properties[MM_MODEM_PROPERTY_ACCESSTECHNOLOGIES].
      writer().append_uint32(kAccessTechnologies);

  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  ::DBus::Struct< uint32_t, bool > quality;
  quality._1 = 90;
  quality._2 = true;
  writer << quality;
  modem_properties[MM_MODEM_PROPERTY_SIGNALQUALITY] = v;

  // Set up mock modem 3gpp properties
  DBusPropertiesMap modem3gpp_properties;
  modem3gpp_properties[MM_MODEM_MODEM3GPP_PROPERTY_ENABLEDFACILITYLOCKS].
      writer().append_uint32(0);
  modem3gpp_properties[MM_MODEM_MODEM3GPP_PROPERTY_IMEI].
      writer().append_string(kImei);

  EXPECT_CALL(*modem_proxy_,
              Enable(true, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(Invoke(this, &CellularCapabilityUniversalTest::InvokeEnable));
  EXPECT_CALL(*properties_proxy_,
              GetAll(MM_DBUS_INTERFACE_MODEM))
      .WillOnce(Return(modem_properties));
  EXPECT_CALL(*properties_proxy_,
              GetAll(MM_DBUS_INTERFACE_MODEM_MODEM3GPP))
      .WillOnce(Return(modem3gpp_properties));

  // Let the modem report that it is initializing.  StartModem() should defer
  // enabling the modem until its state changes to disabled.
  EXPECT_CALL(*modem_proxy_, State())
      .WillOnce(Return(Cellular::kModemStateInitializing));

  // After setup we lose pointers to the proxies, so it is hard to set
  // expectations.
  SetUp();

  Error error;
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  ResultCallback callback =
      Bind(&CellularCapabilityUniversalTest::TestCallback, Unretained(this));
  capability_->StartModem(&error, callback);

  // Verify that the modem has not been enabled.
  EXPECT_TRUE(capability_->imei_.empty());
  EXPECT_EQ(0, capability_->access_technologies_);
  Mock::VerifyAndClearExpectations(this);

  // Change the state to kModemStateDisabling and verify that it still has not
  // been enabled.
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  capability_->OnModemStateChangedSignal(Cellular::kModemStateInitializing,
                                         Cellular::kModemStateDisabling, 0);
  EXPECT_TRUE(capability_->imei_.empty());
  EXPECT_EQ(0, capability_->access_technologies_);
  Mock::VerifyAndClearExpectations(this);

  // Change the state of the modem to disabled and verify that it gets enabled.
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  capability_->OnModemStateChangedSignal(Cellular::kModemStateDisabling,
                                         Cellular::kModemStateDisabled, 0);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kImei, capability_->imei_);
  EXPECT_EQ(kAccessTechnologies, capability_->access_technologies_);
}

TEST_F(CellularCapabilityUniversalMainTest, StartModemFail) {
  EXPECT_CALL(*modem_proxy_, State())
          .WillOnce(Return(Cellular::kModemStateDisabled));
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
  EXPECT_TRUE(error.IsOngoing());
}

TEST_F(CellularCapabilityUniversalMainTest, StartModemAlreadyEnabled) {
  EXPECT_CALL(*modem_proxy_, State())
          .WillOnce(Return(Cellular::kModemStateEnabled));
  SetUp();
  capability_->cellular()->modem_state_ = Cellular::kModemStateConnected;

  // Make sure the call to StartModem() doesn't attempt to complete the
  // request synchronously, else it will crash DBus-C++.
  Error error(Error::kOperationInitiated);
  capability_->StartModem(&error, ResultCallback());
  EXPECT_TRUE(error.IsOngoing());
}

TEST_F(CellularCapabilityUniversalMainTest, StopModem) {
  // Save pointers to proxies before they are lost by the call to InitProxies
  mm1::MockModemProxy *modem_proxy = modem_proxy_.get();
  SetUp();
  EXPECT_CALL(*modem_proxy, set_state_changed_callback(_));
  capability_->InitProxies();

  Error error;
  ResultCallback callback =
      Bind(&CellularCapabilityUniversalTest::TestCallback, Unretained(this));
  capability_->StopModem(&error, callback);
  EXPECT_TRUE(error.IsSuccess());

  ResultCallback disable_callback;
  EXPECT_CALL(*modem_proxy,
              Enable(false, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(SaveArg<2>(&disable_callback));
  dispatcher_.DispatchPendingEvents();

  ResultCallback set_power_state_callback;
  EXPECT_CALL(
      *modem_proxy,
      SetPowerState(
          MM_MODEM_POWER_STATE_LOW, _, _,
          CellularCapabilityUniversal::kSetPowerStateTimeoutMilliseconds))
      .WillOnce(SaveArg<2>(&set_power_state_callback));
  disable_callback.Run(Error(Error::kSuccess));

  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  set_power_state_callback.Run(Error(Error::kSuccess));
  Mock::VerifyAndClearExpectations(this);

  // TestCallback should get called with success even if the power state
  // callback gets called with an error
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  set_power_state_callback.Run(Error(Error::kOperationFailed));
}

TEST_F(CellularCapabilityUniversalMainTest, StopModemConnected) {
  // Save pointers to proxies before they are lost by the call to InitProxies
  mm1::MockModemProxy *modem_proxy = modem_proxy_.get();
  mm1::MockModemSimpleProxy *modem_simple_proxy = modem_simple_proxy_.get();
  SetUp();
  EXPECT_CALL(*modem_proxy, set_state_changed_callback(_));
  capability_->InitProxies();

  ResultCallback disconnect_callback;
  Error error;
  ResultCallback callback =
      Bind(&CellularCapabilityUniversalTest::TestCallback, Unretained(this));
  EXPECT_CALL(*modem_simple_proxy,
              Disconnect(::DBus::Path("/"), _, _,
                         CellularCapability::kTimeoutDisconnect))
      .WillOnce(SaveArg<2>(&disconnect_callback));
  capability_->cellular()->modem_state_ = Cellular::kModemStateConnected;
  capability_->StopModem(&error, callback);
  EXPECT_TRUE(error.IsSuccess());

  ResultCallback disable_callback;
  EXPECT_CALL(*modem_proxy,
              Enable(false, _, _, CellularCapability::kTimeoutEnable))
      .WillOnce(SaveArg<2>(&disable_callback));
  disconnect_callback.Run(Error(Error::kSuccess));

  ResultCallback set_power_state_callback;
  EXPECT_CALL(
      *modem_proxy,
      SetPowerState(
          MM_MODEM_POWER_STATE_LOW, _, _,
          CellularCapabilityUniversal::kSetPowerStateTimeoutMilliseconds))
      .WillOnce(SaveArg<2>(&set_power_state_callback));

  disable_callback.Run(Error(Error::kSuccess));

  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  set_power_state_callback.Run(Error(Error::kSuccess));
}

TEST_F(CellularCapabilityUniversalMainTest, DisconnectModemNoBearer) {
  Error error;
  ResultCallback disconnect_callback;
  EXPECT_CALL(*modem_simple_proxy_,
              Disconnect(_, _, _, CellularCapability::kTimeoutDisconnect))
      .Times(0);
  capability_->Disconnect(&error, disconnect_callback);
}

TEST_F(CellularCapabilityUniversalMainTest, DisconnectNoProxy) {
  Error error;
  ResultCallback disconnect_callback;
  capability_->bearer_path_ = "/foo";
  EXPECT_CALL(*modem_simple_proxy_,
              Disconnect(_, _, _, CellularCapability::kTimeoutDisconnect))
      .Times(0);
  ReleaseCapabilityProxies();
  capability_->Disconnect(&error, disconnect_callback);
}

TEST_F(CellularCapabilityUniversalMainTest, DisconnectWithDeferredCallback) {
  Error error;
  ResultCallback disconnect_callback;
  capability_->bearer_path_ = "/foo";
  EXPECT_CALL(*modem_simple_proxy_,
              Disconnect(_, _, _, CellularCapability::kTimeoutDisconnect));
  SetSimpleProxy();
  SetMockRegistrationDroppedUpdateCallback();
  EXPECT_CALL(*this, DummyCallback());
  capability_->Disconnect(&error, disconnect_callback);
}

TEST_F(CellularCapabilityUniversalMainTest, SimLockStatusChanged) {
  // Set up mock SIM properties
  const char kImsi[] = "310100000001";
  const char kSimIdentifier[] = "9999888";
  const char kOperatorIdentifier[] = "310240";
  const char kOperatorName[] = "Custom SPN";
  DBusPropertiesMap sim_properties;
  sim_properties[MM_SIM_PROPERTY_IMSI].writer().append_string(kImsi);
  sim_properties[MM_SIM_PROPERTY_SIMIDENTIFIER].writer()
      .append_string(kSimIdentifier);
  sim_properties[MM_SIM_PROPERTY_OPERATORIDENTIFIER].writer()
      .append_string(kOperatorIdentifier);
  sim_properties[MM_SIM_PROPERTY_OPERATORNAME].writer()
      .append_string(kOperatorName);

  EXPECT_CALL(*properties_proxy_, GetAll(MM_DBUS_INTERFACE_SIM))
      .WillOnce(Return(sim_properties));
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID, _))
      .Times(1);

  SetUp();
  InitProviderDB();

  EXPECT_FALSE(capability_->sim_present_);
  EXPECT_TRUE(capability_->sim_proxy_ == NULL);

  capability_->OnSimPathChanged(kSimPath);
  EXPECT_TRUE(capability_->sim_present_);
  EXPECT_TRUE(capability_->sim_proxy_ != NULL);
  EXPECT_EQ(kSimPath, capability_->sim_path_);

  capability_->imsi_ = "";
  capability_->sim_identifier_ = "";
  capability_->operator_id_ = "";
  capability_->spn_ = "";

  // SIM is locked.
  capability_->sim_lock_status_.lock_type = MM_MODEM_LOCK_SIM_PIN;
  capability_->OnSimLockStatusChanged();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());

  EXPECT_EQ("", capability_->imsi_);
  EXPECT_EQ("", capability_->sim_identifier_);
  EXPECT_EQ("", capability_->operator_id_);
  EXPECT_EQ("", capability_->spn_);

  // SIM is unlocked.
  properties_proxy_.reset(new MockDBusPropertiesProxy());
  EXPECT_CALL(*properties_proxy_, GetAll(MM_DBUS_INTERFACE_SIM))
      .WillOnce(Return(sim_properties));
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID, _))
      .Times(1);

  capability_->sim_lock_status_.lock_type = MM_MODEM_LOCK_NONE;
  capability_->OnSimLockStatusChanged();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());

  EXPECT_EQ(kImsi, capability_->imsi_);
  EXPECT_EQ(kSimIdentifier, capability_->sim_identifier_);
  EXPECT_EQ(kOperatorIdentifier, capability_->operator_id_);
  EXPECT_EQ(kOperatorName, capability_->spn_);
}

TEST_F(CellularCapabilityUniversalMainTest, PropertiesChanged) {
  // Set up mock modem properties
  DBusPropertiesMap modem_properties;
  modem_properties[MM_MODEM_PROPERTY_ACCESSTECHNOLOGIES].
      writer().append_uint32(kAccessTechnologies);
  modem_properties[MM_MODEM_PROPERTY_SIM].
      writer().append_path(kSimPath);

  // Set up mock modem 3gpp properties
  DBusPropertiesMap modem3gpp_properties;
  modem3gpp_properties[MM_MODEM_MODEM3GPP_PROPERTY_ENABLEDFACILITYLOCKS].
      writer().append_uint32(0);
  modem3gpp_properties[MM_MODEM_MODEM3GPP_PROPERTY_IMEI].
      writer().append_string(kImei);

  // Set up mock modem sim properties
  DBusPropertiesMap sim_properties;

  // After setup we lose pointers to the proxies, so it is hard to set
  // expectations.
  EXPECT_CALL(*properties_proxy_,
              GetAll(MM_DBUS_INTERFACE_SIM))
      .WillOnce(Return(sim_properties));

  SetUp();

  EXPECT_EQ("", capability_->imei_);
  EXPECT_EQ(MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN,
            capability_->access_technologies_);
  EXPECT_FALSE(capability_->sim_proxy_.get());
  EXPECT_CALL(*device_adaptor_, EmitStringChanged(
      flimflam::kTechnologyFamilyProperty, flimflam::kTechnologyFamilyGsm));
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM,
                                       modem_properties, vector<string>());
  EXPECT_EQ(kAccessTechnologies, capability_->access_technologies_);
  EXPECT_EQ(kSimPath, capability_->sim_path_);
  EXPECT_TRUE(capability_->sim_proxy_.get());

  // Changing properties on wrong interface will not have an effect
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM,
                                       modem3gpp_properties,
                                       vector<string>());
  EXPECT_EQ("", capability_->imei_);

  // Changing properties on the right interface gets reflected in the
  // capabilities object
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM_MODEM3GPP,
                                       modem3gpp_properties,
                                       vector<string>());
  EXPECT_EQ(kImei, capability_->imei_);
  Mock::VerifyAndClearExpectations(device_adaptor_);

  // Expect to see changes when the family changes
  modem_properties.clear();
  modem_properties[MM_MODEM_PROPERTY_ACCESSTECHNOLOGIES].
      writer().append_uint32(MM_MODEM_ACCESS_TECHNOLOGY_1XRTT);
  EXPECT_CALL(*device_adaptor_, EmitStringChanged(
      flimflam::kTechnologyFamilyProperty, flimflam::kTechnologyFamilyCdma)).
      Times(1);
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM,
                                       modem_properties,
                                       vector<string>());
  Mock::VerifyAndClearExpectations(device_adaptor_);

  // Back to LTE
  modem_properties.clear();
  modem_properties[MM_MODEM_PROPERTY_ACCESSTECHNOLOGIES].
      writer().append_uint32(MM_MODEM_ACCESS_TECHNOLOGY_LTE);
  EXPECT_CALL(*device_adaptor_, EmitStringChanged(
      flimflam::kTechnologyFamilyProperty, flimflam::kTechnologyFamilyGsm)).
      Times(1);
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM,
                                       modem_properties,
                                       vector<string>());
  Mock::VerifyAndClearExpectations(device_adaptor_);

  // LTE & CDMA - the device adaptor should not be called!
  modem_properties.clear();
  modem_properties[MM_MODEM_PROPERTY_ACCESSTECHNOLOGIES].
      writer().append_uint32(MM_MODEM_ACCESS_TECHNOLOGY_LTE |
                             MM_MODEM_ACCESS_TECHNOLOGY_1XRTT);
  EXPECT_CALL(*device_adaptor_, EmitStringChanged(_, _)).Times(0);
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM,
                                       modem_properties,
                                       vector<string>());
}

TEST_F(CellularCapabilityUniversalMainTest, UpdateServiceName) {
  ::DBus::Struct<uint32_t, bool> data;
  data._1 = 100;
  data._2 = true;
  EXPECT_CALL(*modem_proxy_, SignalQuality()).WillRepeatedly(Return(data));

  SetUp();
  InitProviderDB();
  capability_->InitProxies();

  SetService();

  size_t len = strlen(CellularCapabilityUniversal::kGenericServiceNamePrefix);
  EXPECT_EQ(CellularCapabilityUniversal::kGenericServiceNamePrefix,
            cellular_->service_->friendly_name().substr(0, len));

  capability_->imsi_ = "310240123456789";
  capability_->SetHomeProvider();
  EXPECT_EQ("", capability_->spn_);
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
  EXPECT_EQ(CellularCapabilityUniversal::kGenericServiceNamePrefix,
            cellular_->service_->friendly_name().substr(0, len));

  capability_->registration_state_ = MM_MODEM_3GPP_REGISTRATION_STATE_HOME;
  capability_->SetHomeProvider();
  EXPECT_EQ("", capability_->spn_);
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
  EXPECT_EQ("T-Mobile", cellular_->service_->friendly_name());

  capability_->spn_ = "Test Home Provider";
  capability_->SetHomeProvider();
  EXPECT_EQ("Test Home Provider", cellular_->home_provider().GetName());
  EXPECT_EQ("Test Home Provider", cellular_->service_->friendly_name());

  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME, "", "OTA Name");
  EXPECT_EQ("OTA Name", cellular_->service_->friendly_name());

  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME, "123", "OTA Name 2");
  EXPECT_EQ("OTA Name 2", cellular_->service_->friendly_name());

  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME, "123", "");
  EXPECT_EQ("Test Home Provider", cellular_->service_->friendly_name());
}

TEST_F(CellularCapabilityUniversalMainTest, UpdateRegistrationState) {
  SetUp();
  InitProviderDB();
  capability_->InitProxies();

  SetService();
  capability_->imsi_ = "310240123456789";
  capability_->SetHomeProvider();
  cellular_->set_modem_state(Cellular::kModemStateConnected);
  SetRegistrationDroppedUpdateTimeout(0);

  string home_provider = cellular_->home_provider().GetName();
  string ota_name = cellular_->service_->friendly_name();

  // Home --> Roaming should be effective immediately.
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
            capability_->registration_state_);
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING,
            capability_->registration_state_);

  // Idle --> Roaming should be effective immediately.
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_IDLE,
      home_provider,
      ota_name);
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_IDLE,
            capability_->registration_state_);
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING,
            capability_->registration_state_);

  // Idle --> Searching should be effective immediately.
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_IDLE,
      home_provider,
      ota_name);
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_IDLE,
            capability_->registration_state_);
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING,
            capability_->registration_state_);

  // Home --> Searching --> Home should never see Searching.
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
            capability_->registration_state_);
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
            capability_->registration_state_);
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
            capability_->registration_state_);
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
            capability_->registration_state_);

  // Home --> Searching --> wait till dispatch should see Searching
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
            capability_->registration_state_);
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
            capability_->registration_state_);
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING,
            capability_->registration_state_);

  // Home --> Searching --> Searching --> wait till dispatch should see
  // Searching *and* the first callback should be cancelled.
  EXPECT_CALL(*this, DummyCallback()).Times(0);
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
            capability_->registration_state_);
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING,
      home_provider,
      ota_name);
  SetMockRegistrationDroppedUpdateCallback();
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
            capability_->registration_state_);
  dispatcher_.DispatchPendingEvents();
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING,
            capability_->registration_state_);

}

TEST_F(CellularCapabilityUniversalMainTest,
       UpdateRegistrationStateModemNotConnected) {
  SetUp();
  InitProviderDB();
  capability_->InitProxies();
  SetService();

  capability_->imsi_ = "310240123456789";
  capability_->SetHomeProvider();
  cellular_->set_modem_state(Cellular::kModemStateRegistered);
  SetRegistrationDroppedUpdateTimeout(0);

  string home_provider = cellular_->home_provider().GetName();
  string ota_name = cellular_->service_->friendly_name();

  // Home --> Searching should be effective immediately.
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_HOME,
            capability_->registration_state_);
  capability_->On3GPPRegistrationChanged(
      MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING,
      home_provider,
      ota_name);
  EXPECT_EQ(MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING,
            capability_->registration_state_);
}

TEST_F(CellularCapabilityUniversalMainTest, IsValidSimPath) {
  // Invalid paths
  EXPECT_FALSE(capability_->IsValidSimPath(""));
  EXPECT_FALSE(capability_->IsValidSimPath("/"));

  // A valid path
  EXPECT_TRUE(capability_->IsValidSimPath(
      "/org/freedesktop/ModemManager1/SIM/0"));

  // Note that any string that is not one of the above invalid paths is
  // currently regarded as valid, since the ModemManager spec doesn't impose
  // a strict format on the path. The validity of this is subject to change.
  EXPECT_TRUE(capability_->IsValidSimPath("path"));
}

TEST_F(CellularCapabilityUniversalMainTest, NormalizeMdn) {
  EXPECT_EQ("", capability_->NormalizeMdn(""));
  EXPECT_EQ("12345678901", capability_->NormalizeMdn("12345678901"));
  EXPECT_EQ("12345678901", capability_->NormalizeMdn("+1 234 567 8901"));
  EXPECT_EQ("12345678901", capability_->NormalizeMdn("+1-234-567-8901"));
  EXPECT_EQ("12345678901", capability_->NormalizeMdn("+1 (234) 567-8901"));
  EXPECT_EQ("12345678901", capability_->NormalizeMdn("1 234  567 8901 "));
  EXPECT_EQ("2345678901", capability_->NormalizeMdn("(234) 567-8901"));
}

TEST_F(CellularCapabilityUniversalMainTest, SimPathChanged) {
  // Set up mock modem SIM properties
  const char kImsi[] = "310100000001";
  const char kSimIdentifier[] = "9999888";
  const char kOperatorIdentifier[] = "310240";
  const char kOperatorName[] = "Custom SPN";
  DBusPropertiesMap sim_properties;
  sim_properties[MM_SIM_PROPERTY_IMSI].writer().append_string(kImsi);
  sim_properties[MM_SIM_PROPERTY_SIMIDENTIFIER].writer()
      .append_string(kSimIdentifier);
  sim_properties[MM_SIM_PROPERTY_OPERATORIDENTIFIER].writer()
      .append_string(kOperatorIdentifier);
  sim_properties[MM_SIM_PROPERTY_OPERATORNAME].writer()
      .append_string(kOperatorName);

  EXPECT_CALL(*properties_proxy_, GetAll(MM_DBUS_INTERFACE_SIM))
      .Times(1).WillOnce(Return(sim_properties));
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID, _))
      .Times(1);

  EXPECT_FALSE(capability_->sim_present_);
  EXPECT_TRUE(capability_->sim_proxy_ == NULL);
  EXPECT_EQ("", capability_->sim_path_);
  EXPECT_EQ("", capability_->imsi_);
  EXPECT_EQ("", capability_->sim_identifier_);
  EXPECT_EQ("", capability_->operator_id_);
  EXPECT_EQ("", capability_->spn_);

  capability_->OnSimPathChanged(kSimPath);
  EXPECT_TRUE(capability_->sim_present_);
  EXPECT_TRUE(capability_->sim_proxy_ != NULL);
  EXPECT_EQ(kSimPath, capability_->sim_path_);
  EXPECT_EQ(kImsi, capability_->imsi_);
  EXPECT_EQ(kSimIdentifier, capability_->sim_identifier_);
  EXPECT_EQ(kOperatorIdentifier, capability_->operator_id_);
  EXPECT_EQ(kOperatorName, capability_->spn_);

  // Changing to the same SIM path should be a no-op.
  capability_->OnSimPathChanged(kSimPath);
  EXPECT_TRUE(capability_->sim_present_);
  EXPECT_TRUE(capability_->sim_proxy_ != NULL);
  EXPECT_EQ(kSimPath, capability_->sim_path_);
  EXPECT_EQ(kImsi, capability_->imsi_);
  EXPECT_EQ(kSimIdentifier, capability_->sim_identifier_);
  EXPECT_EQ(kOperatorIdentifier, capability_->operator_id_);
  EXPECT_EQ(kOperatorName, capability_->spn_);

  capability_->OnSimPathChanged("");
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(properties_proxy_.get());
  EXPECT_FALSE(capability_->sim_present_);
  EXPECT_TRUE(capability_->sim_proxy_ == NULL);
  EXPECT_EQ("", capability_->sim_path_);
  EXPECT_EQ("", capability_->imsi_);
  EXPECT_EQ("", capability_->sim_identifier_);
  EXPECT_EQ("", capability_->operator_id_);
  EXPECT_EQ("", capability_->spn_);

  EXPECT_CALL(*properties_proxy_, GetAll(MM_DBUS_INTERFACE_SIM))
      .Times(1).WillOnce(Return(sim_properties));
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID, _))
      .Times(1);

  capability_->OnSimPathChanged(kSimPath);
  EXPECT_TRUE(capability_->sim_present_);
  EXPECT_TRUE(capability_->sim_proxy_ != NULL);
  EXPECT_EQ(kSimPath, capability_->sim_path_);
  EXPECT_EQ(kImsi, capability_->imsi_);
  EXPECT_EQ(kSimIdentifier, capability_->sim_identifier_);
  EXPECT_EQ(kOperatorIdentifier, capability_->operator_id_);
  EXPECT_EQ(kOperatorName, capability_->spn_);

  capability_->OnSimPathChanged("/");
  EXPECT_FALSE(capability_->sim_present_);
  EXPECT_TRUE(capability_->sim_proxy_ == NULL);
  EXPECT_EQ("/", capability_->sim_path_);
  EXPECT_EQ("", capability_->imsi_);
  EXPECT_EQ("", capability_->sim_identifier_);
  EXPECT_EQ("", capability_->operator_id_);
  EXPECT_EQ("", capability_->spn_);
}

TEST_F(CellularCapabilityUniversalMainTest, SimPropertiesChanged) {
  // Set up mock modem properties
  DBusPropertiesMap modem_properties;
  modem_properties[MM_MODEM_PROPERTY_SIM].writer().append_path(kSimPath);

  // Set up mock modem sim properties
  const char kImsi[] = "310100000001";
  DBusPropertiesMap sim_properties;
  sim_properties[MM_SIM_PROPERTY_IMSI].writer().append_string(kImsi);

  EXPECT_CALL(*properties_proxy_, GetAll(MM_DBUS_INTERFACE_SIM))
      .WillOnce(Return(sim_properties));
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID, _))
      .Times(0);
  // After setup we lose pointers to the proxies, so it is hard to set
  // expectations.
  SetUp();
  InitProviderDB();

  EXPECT_TRUE(cellular_->home_provider().GetName().empty());
  EXPECT_TRUE(cellular_->home_provider().GetCountry().empty());
  EXPECT_TRUE(cellular_->home_provider().GetCode().empty());
  EXPECT_FALSE(capability_->sim_proxy_.get());
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_MODEM,
                                       modem_properties, vector<string>());
  EXPECT_EQ(kSimPath, capability_->sim_path_);
  EXPECT_TRUE(capability_->sim_proxy_.get());
  EXPECT_EQ(kImsi, capability_->imsi_);
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());

  // Updating the SIM
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID, _))
      .Times(2);
  DBusPropertiesMap new_properties;
  const char kCountry[] = "us";
  const char kNewImsi[] = "310240123456789";
  const char kSimIdentifier[] = "9999888";
  const char kOperatorIdentifier[] = "310240";
  const char kOperatorName[] = "Custom SPN";
  new_properties[MM_SIM_PROPERTY_IMSI].writer().append_string(kNewImsi);
  new_properties[MM_SIM_PROPERTY_SIMIDENTIFIER].writer().
      append_string(kSimIdentifier);
  new_properties[MM_SIM_PROPERTY_OPERATORIDENTIFIER].writer().
      append_string(kOperatorIdentifier);
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_SIM,
                                       new_properties,
                                       vector<string>());
  EXPECT_EQ(kNewImsi, capability_->imsi_);
  EXPECT_EQ(kSimIdentifier, capability_->sim_identifier_);
  EXPECT_EQ(kOperatorIdentifier, capability_->operator_id_);
  EXPECT_EQ("", capability_->spn_);
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
  EXPECT_EQ(kCountry, cellular_->home_provider().GetCountry());
  EXPECT_EQ(kOperatorIdentifier, cellular_->home_provider().GetCode());
  EXPECT_EQ(4, capability_->apn_list_.size());

  new_properties[MM_SIM_PROPERTY_OPERATORNAME].writer().
      append_string(kOperatorName);
  capability_->OnDBusPropertiesChanged(MM_DBUS_INTERFACE_SIM,
                                       new_properties,
                                       vector<string>());
  EXPECT_EQ(kOperatorName, cellular_->home_provider().GetName());
  EXPECT_EQ(kOperatorName, capability_->spn_);
}

MATCHER_P(SizeIs, value, "") {
  return static_cast<size_t>(value) == arg.size();
}

TEST_F(CellularCapabilityUniversalMainTest, Reset) {
  // Save pointers to proxies before they are lost by the call to InitProxies
  mm1::MockModemProxy *modem_proxy = modem_proxy_.get();
  SetUp();
  EXPECT_CALL(*modem_proxy, set_state_changed_callback(_));
  capability_->InitProxies();

  Error error;
  ResultCallback reset_callback;

  EXPECT_CALL(*modem_proxy, Reset(_, _, CellularCapability::kTimeoutReset))
      .WillOnce(SaveArg<1>(&reset_callback));

  capability_->Reset(&error, ResultCallback());
  EXPECT_TRUE(capability_->resetting_);
  reset_callback.Run(error);
  EXPECT_FALSE(capability_->resetting_);
}

// Validates that OnScanReply does not crash with a null callback.
TEST_F(CellularCapabilityUniversalMainTest, ScanWithNullCallback) {
  Error error;
  EXPECT_CALL(*modem_3gpp_proxy_, Scan(_, _, CellularCapability::kTimeoutScan))
      .WillOnce(Invoke(this, &CellularCapabilityUniversalTest::InvokeScan));
  EXPECT_CALL(*device_adaptor_,
              EmitStringmapsChanged(flimflam::kFoundNetworksProperty,
                                    SizeIs(0)));
  Set3gppProxy();
  capability_->Scan(&error, ResultCallback());
  EXPECT_TRUE(error.IsSuccess());
}

// Validates that the scanning property is updated
TEST_F(CellularCapabilityUniversalMainTest, Scan) {
  Error error;

  EXPECT_CALL(*modem_3gpp_proxy_, Scan(_, _, CellularCapability::kTimeoutScan))
      .WillRepeatedly(SaveArg<1>(&scan_callback_));
  EXPECT_CALL(*device_adaptor_,
              EmitBoolChanged(flimflam::kScanningProperty, true));
  Set3gppProxy();
  capability_->Scan(&error, ResultCallback());
  EXPECT_TRUE(capability_->scanning_);
  Mock::VerifyAndClearExpectations(device_adaptor_);

  // Simulate the completion of the scan with 2 networks in the results.
  EXPECT_CALL(*device_adaptor_,
              EmitBoolChanged(flimflam::kScanningProperty, false));
  EXPECT_CALL(*device_adaptor_,
              EmitStringmapsChanged(flimflam::kFoundNetworksProperty,
                                    SizeIs(2)));
  vector<DBusPropertiesMap> results;
  const char kScanID0[] = "testID0";
  const char kScanID1[] = "testID1";
  results.push_back(DBusPropertiesMap());
  results[0][CellularCapabilityUniversal::kOperatorLongProperty].
      writer().append_string(kScanID0);
  results.push_back(DBusPropertiesMap());
  results[1][CellularCapabilityUniversal::kOperatorLongProperty].
      writer().append_string(kScanID1);
  scan_callback_.Run(results, error);
  EXPECT_FALSE(capability_->scanning_);
  Mock::VerifyAndClearExpectations(device_adaptor_);

  // Simulate the completion of the scan with no networks in the results.
  EXPECT_CALL(*device_adaptor_,
              EmitBoolChanged(flimflam::kScanningProperty, true));
  capability_->Scan(&error, ResultCallback());
  EXPECT_TRUE(capability_->scanning_);
  Mock::VerifyAndClearExpectations(device_adaptor_);

  EXPECT_CALL(*device_adaptor_,
              EmitBoolChanged(flimflam::kScanningProperty, false));
  EXPECT_CALL(*device_adaptor_,
              EmitStringmapsChanged(flimflam::kFoundNetworksProperty,
                                    SizeIs(0)));
  scan_callback_.Run(vector<DBusPropertiesMap>(), Error());
  EXPECT_FALSE(capability_->scanning_);
}

// Validates expected property updates when scan fails
TEST_F(CellularCapabilityUniversalMainTest, ScanFailure) {
  Error error;

  // Test immediate error
  {
    InSequence seq;
    EXPECT_CALL(*modem_3gpp_proxy_,
                Scan(_, _, CellularCapability::kTimeoutScan))
        .WillOnce(Invoke(this, &CellularCapabilityUniversalTest::ScanError));
    EXPECT_CALL(*modem_3gpp_proxy_,
                Scan(_, _, CellularCapability::kTimeoutScan))
        .WillOnce(SaveArg<1>(&scan_callback_));
  }
  Set3gppProxy();
  capability_->Scan(&error, ResultCallback());
  EXPECT_FALSE(capability_->scanning_);
  EXPECT_TRUE(error.IsFailure());

  // Initiate a scan
  error.Populate(Error::kSuccess);
  EXPECT_CALL(*device_adaptor_,
              EmitBoolChanged(flimflam::kScanningProperty, true));
  capability_->Scan(&error, ResultCallback());
  EXPECT_TRUE(capability_->scanning_);
  EXPECT_TRUE(error.IsSuccess());

  // Validate that error is returned if Scan is called while already scanning.
  capability_->Scan(&error, ResultCallback());
  EXPECT_TRUE(capability_->scanning_);
  EXPECT_TRUE(error.IsFailure());
  Mock::VerifyAndClearExpectations(device_adaptor_);

  // Validate that signals are emitted even if an error is reported.
  capability_->found_networks_.clear();
  capability_->found_networks_.push_back(Stringmap());
  EXPECT_CALL(*device_adaptor_,
              EmitBoolChanged(flimflam::kScanningProperty, false));
  EXPECT_CALL(*device_adaptor_,
              EmitStringmapsChanged(flimflam::kFoundNetworksProperty,
                                    SizeIs(0)));
  vector<DBusPropertiesMap> results;
  scan_callback_.Run(results, Error(Error::kOperationFailed));
  EXPECT_FALSE(capability_->scanning_);
}

// Validates expected behavior of OnListBearersReply function
TEST_F(CellularCapabilityUniversalMainTest, OnListBearersReply) {
  // Check that bearer_path_ is set correctly when an active bearer
  // is returned.
  const size_t kPathCount = 3;
  DBus::Path active_paths[kPathCount], inactive_paths[kPathCount];
  for (size_t i = 0; i < kPathCount; ++i) {
    active_paths[i] =
        DBus::Path(base::StringPrintf("%s/%zu", kActiveBearerPathPrefix, i));
    inactive_paths[i] =
        DBus::Path(base::StringPrintf("%s/%zu", kInactiveBearerPathPrefix, i));
  }

  std::vector<DBus::Path> paths;
  paths.push_back(inactive_paths[0]);
  paths.push_back(inactive_paths[1]);
  paths.push_back(active_paths[2]);
  paths.push_back(inactive_paths[1]);
  paths.push_back(inactive_paths[2]);

  Error error;
  capability_->OnListBearersReply(paths, error);
  EXPECT_STREQ(capability_->bearer_path_.c_str(), active_paths[2].c_str());

  paths.clear();

  // Check that bearer_path_ is empty if no active bearers are returned.
  paths.push_back(inactive_paths[0]);
  paths.push_back(inactive_paths[1]);
  paths.push_back(inactive_paths[2]);
  paths.push_back(inactive_paths[1]);

  capability_->OnListBearersReply(paths, error);
  EXPECT_TRUE(capability_->bearer_path_.empty());

  // Check that returning multiple bearers causes death.
  paths.push_back(active_paths[0]);
  paths.push_back(inactive_paths[1]);
  paths.push_back(inactive_paths[2]);
  paths.push_back(active_paths[1]);
  paths.push_back(inactive_paths[1]);

  EXPECT_DEATH(capability_->OnListBearersReply(paths, error),
      "Found more than one active bearer.");
}

// Validates expected behavior of Connect function
TEST_F(CellularCapabilityUniversalMainTest, Connect) {
  mm1::MockModemSimpleProxy *modem_simple_proxy = modem_simple_proxy_.get();
  SetSimpleProxy();
  Error error;
  DBusPropertiesMap properties;
  capability_->apn_try_list_.clear();
  ResultCallback callback =
      Bind(&CellularCapabilityUniversalTest::TestCallback, Unretained(this));
  DBus::Path bearer("/foo");

  // Test connect failures
  EXPECT_CALL(*modem_simple_proxy, Connect(_, _, _, _))
      .WillRepeatedly(SaveArg<2>(&connect_callback_));
  capability_->Connect(properties, &error, callback);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_CALL(*this, TestCallback(IsFailure()));
  EXPECT_CALL(*service_, ClearLastGoodApn());
  connect_callback_.Run(bearer, Error(Error::kOperationFailed));
  Mock::VerifyAndClearExpectations(this);

  // Test connect success
  capability_->Connect(properties, &error, callback);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  connect_callback_.Run(bearer, Error(Error::kSuccess));
  Mock::VerifyAndClearExpectations(this);

  // Test connect failures without a service.  Make sure that shill
  // does not crash if the connect failed and there is no
  // CellularService object.  This can happen if the modem is enabled
  // and then quickly disabled.
  cellular_->service_ = NULL;
  EXPECT_FALSE(capability_->cellular()->service());
  capability_->Connect(properties, &error, callback);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_CALL(*this, TestCallback(IsFailure()));
  connect_callback_.Run(bearer, Error(Error::kOperationFailed));
}

// Validates Connect iterates over APNs
TEST_F(CellularCapabilityUniversalMainTest, ConnectApns) {
  mm1::MockModemSimpleProxy *modem_simple_proxy = modem_simple_proxy_.get();
  SetSimpleProxy();
  Error error;
  DBusPropertiesMap properties;
  capability_->apn_try_list_.clear();
  ResultCallback callback =
      Bind(&CellularCapabilityUniversalTest::TestCallback, Unretained(this));
  DBus::Path bearer("/bearer0");

  const char apn_name_foo[] = "foo";
  const char apn_name_bar[] = "bar";
  EXPECT_CALL(*modem_simple_proxy, Connect(HasApn(apn_name_foo), _, _, _))
      .WillOnce(SaveArg<2>(&connect_callback_));
  Stringmap apn1;
  apn1[flimflam::kApnProperty] = apn_name_foo;
  capability_->apn_try_list_.push_back(apn1);
  Stringmap apn2;
  apn2[flimflam::kApnProperty] = apn_name_bar;
  capability_->apn_try_list_.push_back(apn2);
  capability_->FillConnectPropertyMap(&properties);
  capability_->Connect(properties, &error, callback);
  EXPECT_TRUE(error.IsSuccess());
  Mock::VerifyAndClearExpectations(modem_simple_proxy);

  EXPECT_CALL(*modem_simple_proxy, Connect(HasApn(apn_name_bar), _, _, _))
      .WillOnce(SaveArg<2>(&connect_callback_));
  EXPECT_CALL(*service_, ClearLastGoodApn());
  connect_callback_.Run(bearer, Error(Error::kInvalidApn));

  EXPECT_CALL(*service_, SetLastGoodApn(apn2));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  connect_callback_.Run(bearer, Error(Error::kSuccess));
}

// Validates GetTypeString and AccessTechnologyToTechnologyFamily
TEST_F(CellularCapabilityUniversalMainTest, GetTypeString) {
  const int gsm_technologies[] = {
    MM_MODEM_ACCESS_TECHNOLOGY_LTE,
    MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS,
    MM_MODEM_ACCESS_TECHNOLOGY_HSPA,
    MM_MODEM_ACCESS_TECHNOLOGY_HSUPA,
    MM_MODEM_ACCESS_TECHNOLOGY_HSDPA,
    MM_MODEM_ACCESS_TECHNOLOGY_UMTS,
    MM_MODEM_ACCESS_TECHNOLOGY_EDGE,
    MM_MODEM_ACCESS_TECHNOLOGY_GPRS,
    MM_MODEM_ACCESS_TECHNOLOGY_GSM_COMPACT,
    MM_MODEM_ACCESS_TECHNOLOGY_GSM,
    MM_MODEM_ACCESS_TECHNOLOGY_LTE | MM_MODEM_ACCESS_TECHNOLOGY_EVDO0,
    MM_MODEM_ACCESS_TECHNOLOGY_GSM | MM_MODEM_ACCESS_TECHNOLOGY_EVDO0,
    MM_MODEM_ACCESS_TECHNOLOGY_LTE | MM_MODEM_ACCESS_TECHNOLOGY_EVDOA,
    MM_MODEM_ACCESS_TECHNOLOGY_GSM | MM_MODEM_ACCESS_TECHNOLOGY_EVDOA,
    MM_MODEM_ACCESS_TECHNOLOGY_LTE | MM_MODEM_ACCESS_TECHNOLOGY_EVDOB,
    MM_MODEM_ACCESS_TECHNOLOGY_GSM | MM_MODEM_ACCESS_TECHNOLOGY_EVDOB,
    MM_MODEM_ACCESS_TECHNOLOGY_GSM | MM_MODEM_ACCESS_TECHNOLOGY_1XRTT,
  };
  for (size_t i = 0; i < arraysize(gsm_technologies); ++i) {
    capability_->access_technologies_ = gsm_technologies[i];
    ASSERT_EQ(capability_->GetTypeString(), flimflam::kTechnologyFamilyGsm);
  }
  const int cdma_technologies[] = {
    MM_MODEM_ACCESS_TECHNOLOGY_EVDO0,
    MM_MODEM_ACCESS_TECHNOLOGY_EVDOA,
    MM_MODEM_ACCESS_TECHNOLOGY_EVDOA | MM_MODEM_ACCESS_TECHNOLOGY_EVDO0,
    MM_MODEM_ACCESS_TECHNOLOGY_EVDOB,
    MM_MODEM_ACCESS_TECHNOLOGY_EVDOB | MM_MODEM_ACCESS_TECHNOLOGY_EVDO0,
    MM_MODEM_ACCESS_TECHNOLOGY_1XRTT,
  };
  for (size_t i = 0; i < arraysize(cdma_technologies); ++i) {
    capability_->access_technologies_ = cdma_technologies[i];
    ASSERT_EQ(capability_->GetTypeString(), flimflam::kTechnologyFamilyCdma);
  }
  capability_->access_technologies_ = MM_MODEM_ACCESS_TECHNOLOGY_UNKNOWN;
  ASSERT_EQ(capability_->GetTypeString(), "");
}

TEST_F(CellularCapabilityUniversalMainTest, AllowRoaming) {
  EXPECT_FALSE(cellular_->allow_roaming_);
  EXPECT_FALSE(capability_->provider_requires_roaming_);
  EXPECT_FALSE(capability_->AllowRoaming());
  capability_->provider_requires_roaming_ = true;
  EXPECT_TRUE(capability_->AllowRoaming());
  capability_->provider_requires_roaming_ = false;
  cellular_->allow_roaming_ = true;
  EXPECT_TRUE(capability_->AllowRoaming());
}

TEST_F(CellularCapabilityUniversalMainTest, SetHomeProvider) {
  static const char kTestCarrier[] = "The Cellular Carrier";
  static const char kCountry[] = "us";
  static const char kCode[] = "310160";

  EXPECT_FALSE(capability_->home_provider_);
  EXPECT_FALSE(capability_->provider_requires_roaming_);

  // No mobile provider DB available.
  capability_->SetHomeProvider();
  EXPECT_TRUE(cellular_->home_provider().GetName().empty());
  EXPECT_TRUE(cellular_->home_provider().GetCountry().empty());
  EXPECT_TRUE(cellular_->home_provider().GetCode().empty());
  EXPECT_FALSE(capability_->provider_requires_roaming_);

  InitProviderDB();

  // IMSI and Operator Code not available.
  capability_->SetHomeProvider();
  EXPECT_TRUE(cellular_->home_provider().GetName().empty());
  EXPECT_TRUE(cellular_->home_provider().GetCountry().empty());
  EXPECT_TRUE(cellular_->home_provider().GetCode().empty());
  EXPECT_FALSE(capability_->provider_requires_roaming_);

  // Operator Code available.
  capability_->operator_id_ = "310240";
  capability_->SetHomeProvider();
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
  EXPECT_EQ(kCountry, cellular_->home_provider().GetCountry());
  EXPECT_EQ("310240", cellular_->home_provider().GetCode());
  EXPECT_EQ(4, capability_->apn_list_.size());
  ASSERT_TRUE(capability_->home_provider_);
  EXPECT_FALSE(capability_->provider_requires_roaming_);

  cellular_->home_provider_.SetName("");
  cellular_->home_provider_.SetCountry("");
  cellular_->home_provider_.SetCode("");

  // IMSI available
  capability_->imsi_ = "310240123456789";
  capability_->operator_id_.clear();
  capability_->SetHomeProvider();
  EXPECT_EQ("T-Mobile", cellular_->home_provider().GetName());
  EXPECT_EQ(kCountry, cellular_->home_provider().GetCountry());
  EXPECT_EQ(kCode, cellular_->home_provider().GetCode());
  EXPECT_EQ(4, capability_->apn_list_.size());
  ASSERT_TRUE(capability_->home_provider_);
  EXPECT_FALSE(capability_->provider_requires_roaming_);

  Cellular::Operator oper;
  cellular_->set_home_provider(oper);
  capability_->spn_ = kTestCarrier;
  capability_->SetHomeProvider();
  EXPECT_EQ(kTestCarrier, cellular_->home_provider().GetName());
  EXPECT_EQ(kCountry, cellular_->home_provider().GetCountry());
  EXPECT_EQ(kCode, cellular_->home_provider().GetCode());
  EXPECT_FALSE(capability_->provider_requires_roaming_);

  static const char kCubic[] = "Cubic";
  capability_->spn_ = kCubic;
  capability_->SetHomeProvider();
  EXPECT_EQ(kCubic, cellular_->home_provider().GetName());
  EXPECT_EQ("", cellular_->home_provider().GetCode());
  ASSERT_TRUE(capability_->home_provider_);
  EXPECT_TRUE(capability_->provider_requires_roaming_);

  static const char kCUBIC[] = "CUBIC";
  capability_->spn_ = kCUBIC;
  capability_->home_provider_ = NULL;
  capability_->SetHomeProvider();
  EXPECT_EQ(kCUBIC, cellular_->home_provider().GetName());
  EXPECT_EQ("", cellular_->home_provider().GetCode());
  ASSERT_TRUE(capability_->home_provider_);
  EXPECT_TRUE(capability_->provider_requires_roaming_);
}

TEST_F(CellularCapabilityUniversalMainTest, UpdateScanningProperty) {
  // Save pointers to proxies before they are lost by the call to InitProxies
  // mm1::MockModemProxy *modem_proxy = modem_proxy_.get();
  SetUp();
  //EXPECT_CALL(*modem_proxy, set_state_changed_callback(_));
  capability_->InitProxies();

  EXPECT_FALSE(capability_->scanning_or_searching_);
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);

  capability_->scanning_ = true;
  capability_->UpdateScanningProperty();
  EXPECT_TRUE(capability_->scanning_or_searching_);

  capability_->scanning_ = false;
  capability_->cellular()->modem_state_ = Cellular::kModemStateInitializing;
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);
  capability_->cellular()->modem_state_ = Cellular::kModemStateLocked;
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);
  capability_->cellular()->modem_state_ = Cellular::kModemStateDisabled;
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);
  capability_->cellular()->modem_state_ = Cellular::kModemStateEnabling;
  capability_->UpdateScanningProperty();
  EXPECT_TRUE(capability_->scanning_or_searching_);
  capability_->cellular()->modem_state_ = Cellular::kModemStateEnabled;
  capability_->UpdateScanningProperty();
  EXPECT_TRUE(capability_->scanning_or_searching_);
  capability_->cellular()->modem_state_ = Cellular::kModemStateSearching;
  capability_->UpdateScanningProperty();
  EXPECT_TRUE(capability_->scanning_or_searching_);
  capability_->cellular()->modem_state_ = Cellular::kModemStateRegistered;
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);
  capability_->cellular()->modem_state_ = Cellular::kModemStateConnecting;
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);
  capability_->cellular()->modem_state_ = Cellular::kModemStateConnected;
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);
  capability_->cellular()->modem_state_ = Cellular::kModemStateDisconnecting;
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);

  // Modem with an unactivated service in the 'enabled' or 'searching' state
  capability_->cellular()->modem_state_ = Cellular::kModemStateEnabled;
  capability_->mdn_ = "0000000000";
  CellularService::OLP olp;
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(), GetOLPByMCCMNC(_))
      .WillRepeatedly(Return(&olp));
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);

  capability_->cellular()->modem_state_ = Cellular::kModemStateSearching;
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);
}

TEST_F(CellularCapabilityUniversalTimerTest, UpdateScanningPropertyTimeout) {
  SetUp();
  capability_->InitProxies();

  EXPECT_FALSE(capability_->scanning_or_searching_);
  EXPECT_TRUE(
      capability_->scanning_or_searching_timeout_callback_.IsCancelled());
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(capability_->scanning_or_searching_);
  EXPECT_TRUE(
      capability_->scanning_or_searching_timeout_callback_.IsCancelled());

  EXPECT_CALL(mock_dispatcher_,
              PostDelayedTask(
                  _,
                  CellularCapabilityUniversal::
                      kDefaultScanningOrSearchingTimeoutMilliseconds));

  capability_->scanning_ = true;
  capability_->UpdateScanningProperty();
  EXPECT_FALSE(
      capability_->scanning_or_searching_timeout_callback_.IsCancelled());
  EXPECT_TRUE(capability_->scanning_or_searching_);
  Mock::VerifyAndClearExpectations(&mock_dispatcher_);

  EXPECT_CALL(mock_dispatcher_,
              PostDelayedTask(
                  _,
                  CellularCapabilityUniversal::
                      kDefaultScanningOrSearchingTimeoutMilliseconds))
      .Times(0);

  capability_->scanning_ = false;
  capability_->UpdateScanningProperty();
  EXPECT_TRUE(
      capability_->scanning_or_searching_timeout_callback_.IsCancelled());
  EXPECT_FALSE(capability_->scanning_or_searching_);
  Mock::VerifyAndClearExpectations(&mock_dispatcher_);

  EXPECT_CALL(mock_dispatcher_,
              PostDelayedTask(
                  _,
                  CellularCapabilityUniversal::
                      kDefaultScanningOrSearchingTimeoutMilliseconds))
      .WillOnce(InvokeWithoutArgs(
          this,
          &CellularCapabilityUniversalTest::InvokeScanningOrSearchingTimeout));

  capability_->scanning_ = true;
  capability_->UpdateScanningProperty();
  // The callback has been scheduled
  EXPECT_FALSE(
      capability_->scanning_or_searching_timeout_callback_.IsCancelled());
  // Our mock invocation worked
  EXPECT_FALSE(capability_->scanning_or_searching_);
}

TEST_F(CellularCapabilityUniversalMainTest, UpdateStorageIdentifier) {
  CellularOperatorInfo::CellularOperator provider;

  SetService();

  const string prefix = "cellular_" + string(kMachineAddress) + "_";
  string default_identifier_pattern =
      prefix + string(CellularCapabilityUniversal::kGenericServiceNamePrefix);
  std::replace_if(default_identifier_pattern.begin(),
                  default_identifier_pattern.end(),
                  &Service::IllegalChar, '_');
  default_identifier_pattern += "*";

  // |capability_->operator_id_| is "".
  capability_->UpdateStorageIdentifier();
  EXPECT_TRUE(::MatchPattern(cellular_->service()->storage_identifier_,
                             default_identifier_pattern));

  // GetCellularOperatorByMCCMNC returns NULL.
  capability_->operator_id_ = "1";
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
      GetCellularOperatorByMCCMNC(capability_->operator_id_))
      .WillOnce(
          Return((const CellularOperatorInfo::CellularOperator *)NULL));

  capability_->UpdateStorageIdentifier();
  EXPECT_TRUE(::MatchPattern(cellular_->service()->storage_identifier_,
                             default_identifier_pattern));
  Mock::VerifyAndClearExpectations(modem_info_.mock_cellular_operator_info());

  // |capability_->imsi_| is not ""
  capability_->imsi_ = "TESTIMSI";
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
      GetCellularOperatorByMCCMNC(capability_->operator_id_))
      .WillOnce(
          Return((const CellularOperatorInfo::CellularOperator *)NULL));

  capability_->UpdateStorageIdentifier();
  EXPECT_EQ(prefix + "TESTIMSI", cellular_->service()->storage_identifier_);
  Mock::VerifyAndClearExpectations(modem_info_.mock_cellular_operator_info());

  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
      GetCellularOperatorByMCCMNC(capability_->operator_id_))
      .Times(2)
      .WillRepeatedly(Return(&provider));

  // |provider.identifier_| is "".
  capability_->UpdateStorageIdentifier();
  EXPECT_EQ(prefix + "TESTIMSI", cellular_->service()->storage_identifier_);

  // Success.
  provider.identifier_ = "testidentifier";
  capability_->UpdateStorageIdentifier();
  EXPECT_EQ(prefix + "testidentifier",
            cellular_->service()->storage_identifier_);
}

TEST_F(CellularCapabilityUniversalMainTest, UpdateOLP) {
  CellularService::OLP test_olp;
  test_olp.SetURL("http://testurl");
  test_olp.SetMethod("POST");
  test_olp.SetPostData("imei=${imei}&imsi=${imsi}&mdn=${mdn}&"
                       "min=${min}&iccid=${iccid}");

  capability_->imei_ = "1";
  capability_->imsi_ = "2";
  capability_->mdn_ = "3";
  capability_->min_ = "5";
  capability_->sim_identifier_ = "6";
  capability_->operator_id_ = "123456";

  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(),
      GetOLPByMCCMNC(capability_->operator_id_))
      .WillRepeatedly(Return(&test_olp));

  SetService();
  capability_->UpdateOLP();
  const CellularService::OLP &olp = cellular_->service()->olp();
  EXPECT_EQ("http://testurl", olp.GetURL());
  EXPECT_EQ("POST", olp.GetMethod());
  EXPECT_EQ("imei=1&imsi=2&mdn=3&min=5&iccid=6",
            olp.GetPostData());
}

TEST_F(CellularCapabilityUniversalMainTest, IsMdnValid) {
  capability_->mdn_.clear();
  EXPECT_FALSE(capability_->IsMdnValid());
  capability_->mdn_ = "0000000";
  EXPECT_FALSE(capability_->IsMdnValid());
  capability_->mdn_ = "0000001";
  EXPECT_TRUE(capability_->IsMdnValid());
  capability_->mdn_ = "1231223";
  EXPECT_TRUE(capability_->IsMdnValid());
}

TEST_F(CellularCapabilityUniversalTimerTest, CompleteActivation) {
  const char kIccid[] = "1234567";

  capability_->mdn_.clear();
  capability_->sim_identifier_.clear();

  EXPECT_CALL(*service_,
              SetActivationState(flimflam::kActivationStateActivating))
      .Times(0);
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(
                  PendingActivationStore::kIdentifierICCID, _, _))
      .Times(0);
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(_, _)).Times(0);
  Error error;
  capability_->CompleteActivation(&error);
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(service_);
  Mock::VerifyAndClearExpectations(&mock_dispatcher_);
  EXPECT_TRUE(
      capability_->activation_wait_for_registration_callback_.IsCancelled());

  capability_->sim_identifier_ = kIccid;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid,
                                 PendingActivationStore::kStatePending))
      .Times(1);
  EXPECT_CALL(*service_,
              SetActivationState(flimflam::kActivationStateActivating))
      .Times(1);
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(_, _)).Times(1);
  capability_->CompleteActivation(&error);
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  Mock::VerifyAndClearExpectations(service_);
  Mock::VerifyAndClearExpectations(&mock_dispatcher_);
  EXPECT_FALSE(
      capability_->activation_wait_for_registration_callback_.IsCancelled());

  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid,
                                 PendingActivationStore::kStatePending))
      .Times(0);
  EXPECT_CALL(*service_,
              SetActivationState(flimflam::kActivationStateActivating))
      .Times(0);
  EXPECT_CALL(mock_dispatcher_, PostDelayedTask(_, _)).Times(0);
  capability_->mdn_ = "1231231212";
  capability_->CompleteActivation(&error);
}

TEST_F(CellularCapabilityUniversalMainTest, UpdateServiceActivationState) {
  const char kIccid[] = "1234567";
  capability_->sim_identifier_.clear();
  capability_->mdn_ = "0000000000";
  CellularService::OLP olp;
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(), GetOLPByMCCMNC(_))
      .WillRepeatedly(Return(&olp));

  service_->SetAutoConnect(false);
  EXPECT_CALL(*service_,
              SetActivationState(flimflam::kActivationStateNotActivated))
      .Times(1);
  capability_->UpdateServiceActivationState();
  Mock::VerifyAndClearExpectations(service_);
  EXPECT_FALSE(service_->auto_connect());

  capability_->mdn_ = "1231231122";
  EXPECT_CALL(*service_,
              SetActivationState(flimflam::kActivationStateActivated))
      .Times(1);
  capability_->UpdateServiceActivationState();
  Mock::VerifyAndClearExpectations(service_);
  EXPECT_TRUE(service_->auto_connect());

  service_->SetAutoConnect(false);
  capability_->mdn_ = "0000000000";
  capability_->sim_identifier_ = kIccid;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid))
      .Times(2)
      .WillOnce(Return(PendingActivationStore::kStatePending))
      .WillOnce(Return(PendingActivationStore::kStatePendingTimeout));
  EXPECT_CALL(*service_,
              SetActivationState(flimflam::kActivationStateActivating))
      .Times(1);
  capability_->UpdateServiceActivationState();
  Mock::VerifyAndClearExpectations(service_);
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  EXPECT_FALSE(service_->auto_connect());

  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid))
      .Times(2)
      .WillRepeatedly(Return(PendingActivationStore::kStateActivated));
  EXPECT_CALL(*service_,
              SetActivationState(flimflam::kActivationStateActivated))
      .Times(1);
  capability_->UpdateServiceActivationState();
  Mock::VerifyAndClearExpectations(service_);
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
  EXPECT_TRUE(service_->auto_connect());
}

TEST_F(CellularCapabilityUniversalMainTest, ActivationWaitForRegisterTimeout) {
  const char kIccid[] = "1234567";

  mm1::MockModemProxy *modem_proxy = modem_proxy_.get();
  capability_->InitProxies();
  EXPECT_CALL(*modem_proxy, Reset(_, _, _)).Times(0);
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(_, _, _))
    .Times(0);

  // No ICCID, no MDN
  capability_->sim_identifier_.clear();
  capability_->mdn_.clear();
  capability_->reset_done_ = false;
  capability_->OnActivationWaitForRegisterTimeout();

  // State is not activated.
  capability_->sim_identifier_ = kIccid;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID, _))
    .WillOnce(Return(PendingActivationStore::kStateActivated))
    .WillRepeatedly(Return(PendingActivationStore::kStatePending));
  capability_->OnActivationWaitForRegisterTimeout();

  // Valid MDN.
  capability_->mdn_ = "0000000001";
  capability_->OnActivationWaitForRegisterTimeout();

  // Invalid MDN, reset done.
  capability_->mdn_ = "0000000000";
  capability_->reset_done_ = true;
  capability_->OnActivationWaitForRegisterTimeout();

  Mock::VerifyAndClearExpectations(modem_proxy);
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());

  // Reset not done.
  capability_->reset_done_ = false;
  EXPECT_CALL(*modem_proxy, Reset(_,_,_)).Times(1);
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID, _))
    .WillOnce(Return(PendingActivationStore::kStatePending));
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid,
                                 PendingActivationStore::kStatePendingTimeout))
    .Times(1);
  capability_->OnActivationWaitForRegisterTimeout();
}

TEST_F(CellularCapabilityUniversalMainTest, UpdatePendingActivationState) {
  const char kIccid[] = "1234567";

  mm1::MockModemProxy *modem_proxy = modem_proxy_.get();
  capability_->InitProxies();
  capability_->registration_state_ =
      MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING;

  // No MDN, no ICCID.
  capability_->mdn_ = "0000000";
  capability_->sim_identifier_.clear();
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID, _))
      .Times(0);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());

  // ICCID known.
  capability_->sim_identifier_ = kIccid;

  // After the modem has reset.
  capability_->reset_done_ = true;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid))
      .Times(1).WillOnce(Return(PendingActivationStore::kStatePending));
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid,
                                 PendingActivationStore::kStateActivated))
      .Times(1);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());

  // Before reset, not registered.
  capability_->reset_done_ = false;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid))
      .Times(2).WillRepeatedly(Return(PendingActivationStore::kStatePending));
  EXPECT_CALL(*service_,
              SetActivationState(flimflam::kActivationStateActivating))
     .Times(2);
  EXPECT_CALL(*modem_proxy, Reset(_, _, _)).Times(0);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(modem_proxy);

  // Before reset, registered.
  capability_->registration_state_ =
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME;
  EXPECT_CALL(*modem_proxy, Reset(_, _, _)).Times(1);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());

  // Not registered.
  capability_->registration_state_ =
      MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid))
      .Times(2).WillRepeatedly(Return(PendingActivationStore::kStateActivated));
  EXPECT_CALL(*service_, AutoConnect()).Times(0);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(service_);

  // Service, registered.
  capability_->registration_state_ =
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME;
  EXPECT_CALL(*service_, AutoConnect()).Times(1);
  capability_->UpdatePendingActivationState();

  cellular_->service_->activation_state_ =
      flimflam::kActivationStateNotActivated;

  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());

  // Device is connected.
  cellular_->state_ = Cellular::kStateConnected;
  EXPECT_CALL(*service_,
              SetActivationState(flimflam::kActivationStateActivated))
      .Times(3);
  capability_->UpdatePendingActivationState();

  // Device is linked.
  cellular_->state_ = Cellular::kStateLinked;
  capability_->UpdatePendingActivationState();

  // Got valid MDN.
  cellular_->state_ = Cellular::kStateRegistered;
  capability_->mdn_ = "1231223";
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              RemoveEntry(PendingActivationStore::kIdentifierICCID, kIccid))
      .Times(1);
  capability_->UpdatePendingActivationState();

  Mock::VerifyAndClearExpectations(service_);
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());

  // Timed out, not registered.
  capability_->mdn_.clear();
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid))
    .Times(1)
    .WillOnce(Return(PendingActivationStore::kStatePendingTimeout));
  capability_->registration_state_ =
      MM_MODEM_3GPP_REGISTRATION_STATE_SEARCHING;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(_, _, _))
    .Times(0);
  EXPECT_CALL(*service_, SetActivationState(_)).Times(0);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(service_);
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());

  // Timed out, registered.
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid))
    .Times(1)
    .WillOnce(Return(PendingActivationStore::kStatePendingTimeout));
  capability_->registration_state_ =
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              SetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid,
                                 PendingActivationStore::kStateActivated))
    .Times(1);
  EXPECT_CALL(*service_,
              SetActivationState(flimflam::kActivationStateActivated))
    .Times(1);
  capability_->UpdatePendingActivationState();
  Mock::VerifyAndClearExpectations(service_);
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
}

TEST_F(CellularCapabilityUniversalMainTest, UpdateOperatorInfo) {
  static const char kOperatorName[] = "Swisscom";
  InitProviderDB();
  capability_->serving_operator_.SetCode("22801");
  SetService();
  capability_->UpdateOperatorInfo();
  EXPECT_EQ(kOperatorName, capability_->serving_operator_.GetName());
  EXPECT_EQ("ch", capability_->serving_operator_.GetCountry());
  EXPECT_EQ(kOperatorName, cellular_->service()->serving_operator().GetName());

  static const char kTestOperator[] = "Testcom";
  capability_->serving_operator_.SetName(kTestOperator);
  capability_->serving_operator_.SetCountry("");
  capability_->UpdateOperatorInfo();
  EXPECT_EQ(kTestOperator, capability_->serving_operator_.GetName());
  EXPECT_EQ("ch", capability_->serving_operator_.GetCountry());
  EXPECT_EQ(kTestOperator, cellular_->service()->serving_operator().GetName());
}

TEST_F(CellularCapabilityUniversalMainTest, UpdateOperatorInfoViaOperatorId) {
  static const char kOperatorName[] = "Swisscom";
  static const char kOperatorId[] = "22801";
  InitProviderDB();
  capability_->serving_operator_.SetCode("");
  SetService();
  capability_->UpdateOperatorInfo();
  EXPECT_EQ("", capability_->serving_operator_.GetName());
  EXPECT_EQ("", capability_->serving_operator_.GetCountry());
  EXPECT_EQ("", cellular_->service()->serving_operator().GetName());

  capability_->operator_id_ = kOperatorId;

  capability_->UpdateOperatorInfo();
  EXPECT_EQ(kOperatorId, capability_->serving_operator_.GetCode());
  EXPECT_EQ(kOperatorName, capability_->serving_operator_.GetName());
  EXPECT_EQ("ch", capability_->serving_operator_.GetCountry());
  EXPECT_EQ(kOperatorName, cellular_->service()->serving_operator().GetName());
}

TEST_F(CellularCapabilityUniversalMainTest, CreateFriendlyServiceName) {
  CellularCapabilityUniversal::friendly_service_name_id_ = 0;
  EXPECT_EQ("Mobile Network 0", capability_->CreateFriendlyServiceName());
  EXPECT_EQ("Mobile Network 1", capability_->CreateFriendlyServiceName());

  capability_->operator_id_ = "0123";
  EXPECT_EQ("cellular_0123", capability_->CreateFriendlyServiceName());
  EXPECT_EQ("0123", capability_->serving_operator_.GetCode());

  capability_->serving_operator_.SetCode("1234");
  EXPECT_EQ("cellular_1234", capability_->CreateFriendlyServiceName());

  static const char kHomeProvider[] = "The GSM Home Provider";
  cellular_->home_provider_.SetName(kHomeProvider);
  EXPECT_EQ("cellular_1234", capability_->CreateFriendlyServiceName());
  capability_->registration_state_ = MM_MODEM_3GPP_REGISTRATION_STATE_HOME;
  EXPECT_EQ(kHomeProvider, capability_->CreateFriendlyServiceName());

  static const char kTestOperator[] = "A GSM Operator";
  capability_->serving_operator_.SetName(kTestOperator);
  EXPECT_EQ(kTestOperator, capability_->CreateFriendlyServiceName());

  capability_->registration_state_ = MM_MODEM_3GPP_REGISTRATION_STATE_ROAMING;
  EXPECT_EQ(StringPrintf("%s | %s", kHomeProvider, kTestOperator),
            capability_->CreateFriendlyServiceName());
}

TEST_F(CellularCapabilityUniversalMainTest, IsServiceActivationRequired) {
  capability_->mdn_ = "0000000000";
  EXPECT_FALSE(capability_->IsServiceActivationRequired());

  CellularService::OLP olp;
  EXPECT_CALL(*modem_info_.mock_cellular_operator_info(), GetOLPByMCCMNC(_))
      .WillOnce(Return((const CellularService::OLP *)NULL))
      .WillRepeatedly(Return(&olp));
  EXPECT_FALSE(capability_->IsServiceActivationRequired());

  capability_->mdn_ = "";
  EXPECT_FALSE(capability_->IsServiceActivationRequired());
  capability_->mdn_ = "1234567890";
  EXPECT_FALSE(capability_->IsServiceActivationRequired());
  capability_->mdn_ = "0000000000";
  EXPECT_TRUE(capability_->IsServiceActivationRequired());

  const char kIccid[] = "1234567890";
  capability_->sim_identifier_ = kIccid;
  EXPECT_CALL(*modem_info_.mock_pending_activation_store(),
              GetActivationState(PendingActivationStore::kIdentifierICCID,
                                 kIccid))
      .WillOnce(Return(PendingActivationStore::kStateActivated))
      .WillOnce(Return(PendingActivationStore::kStatePending))
      .WillOnce(Return(PendingActivationStore::kStatePendingTimeout))
      .WillOnce(Return(PendingActivationStore::kStateUnknown));
  EXPECT_FALSE(capability_->IsServiceActivationRequired());
  EXPECT_FALSE(capability_->IsServiceActivationRequired());
  EXPECT_FALSE(capability_->IsServiceActivationRequired());
  EXPECT_TRUE(capability_->IsServiceActivationRequired());
  Mock::VerifyAndClearExpectations(modem_info_.mock_pending_activation_store());
}

TEST_F(CellularCapabilityUniversalMainTest, OnModemCurrentCapabilitiesChanged) {
  EXPECT_FALSE(capability_->scanning_supported_);
  capability_->OnModemCurrentCapabilitiesChanged(MM_MODEM_CAPABILITY_LTE);
  EXPECT_FALSE(capability_->scanning_supported_);
  capability_->OnModemCurrentCapabilitiesChanged(MM_MODEM_CAPABILITY_CDMA_EVDO);
  EXPECT_FALSE(capability_->scanning_supported_);
  capability_->OnModemCurrentCapabilitiesChanged(MM_MODEM_CAPABILITY_GSM_UMTS);
  EXPECT_TRUE(capability_->scanning_supported_);
  capability_->OnModemCurrentCapabilitiesChanged(
      MM_MODEM_CAPABILITY_GSM_UMTS | MM_MODEM_CAPABILITY_CDMA_EVDO);
  EXPECT_TRUE(capability_->scanning_supported_);
}

TEST_F(CellularCapabilityUniversalMainTest, GetNetworkTechnologyStringOnE362) {
  capability_->model_id_.clear();
  capability_->access_technologies_ = 0;
  EXPECT_TRUE(capability_->GetNetworkTechnologyString().empty());

  capability_->model_id_ = CellularCapabilityUniversal::kE362ModelId;
  EXPECT_EQ(flimflam::kNetworkTechnologyLte,
            capability_->GetNetworkTechnologyString());

  capability_->access_technologies_ = MM_MODEM_ACCESS_TECHNOLOGY_GPRS;
  EXPECT_EQ(flimflam::kNetworkTechnologyLte,
            capability_->GetNetworkTechnologyString());

  capability_->model_id_.clear();
  EXPECT_EQ(flimflam::kNetworkTechnologyGprs,
            capability_->GetNetworkTechnologyString());
}

TEST_F(CellularCapabilityUniversalMainTest, ShouldDetectOutOfCredit) {
  capability_->model_id_.clear();
  EXPECT_FALSE(capability_->ShouldDetectOutOfCredit());
  capability_->model_id_ = CellularCapabilityUniversal::kE362ModelId;
  EXPECT_TRUE(capability_->ShouldDetectOutOfCredit());
}

TEST_F(CellularCapabilityUniversalMainTest, SimLockStatusToProperty) {
  Error error;
  KeyValueStore store = capability_->SimLockStatusToProperty(&error);
  EXPECT_FALSE(store.GetBool(flimflam::kSIMLockEnabledProperty));
  EXPECT_TRUE(store.GetString(flimflam::kSIMLockTypeProperty).empty());
  EXPECT_EQ(0, store.GetUint(flimflam::kSIMLockRetriesLeftProperty));

  capability_->sim_lock_status_.enabled = true;
  capability_->sim_lock_status_.retries_left = 3;
  capability_->sim_lock_status_.lock_type = MM_MODEM_LOCK_SIM_PIN;
  store = capability_->SimLockStatusToProperty(&error);
  EXPECT_TRUE(store.GetBool(flimflam::kSIMLockEnabledProperty));
  EXPECT_EQ("sim-pin", store.GetString(flimflam::kSIMLockTypeProperty));
  EXPECT_EQ(3, store.GetUint(flimflam::kSIMLockRetriesLeftProperty));

  capability_->sim_lock_status_.lock_type = MM_MODEM_LOCK_SIM_PUK;
  store = capability_->SimLockStatusToProperty(&error);
  EXPECT_EQ("sim-puk", store.GetString(flimflam::kSIMLockTypeProperty));

  capability_->sim_lock_status_.lock_type = MM_MODEM_LOCK_SIM_PIN2;
  store = capability_->SimLockStatusToProperty(&error);
  EXPECT_TRUE(store.GetString(flimflam::kSIMLockTypeProperty).empty());

  capability_->sim_lock_status_.lock_type = MM_MODEM_LOCK_SIM_PUK2;
  store = capability_->SimLockStatusToProperty(&error);
  EXPECT_TRUE(store.GetString(flimflam::kSIMLockTypeProperty).empty());
}

TEST_F(CellularCapabilityUniversalMainTest, OnLockRetriesChanged) {
  CellularCapabilityUniversal::LockRetryData data;
  const uint32 kDefaultRetries = 999;

  capability_->OnLockRetriesChanged(data);
  EXPECT_EQ(kDefaultRetries, capability_->sim_lock_status_.retries_left);

  data[MM_MODEM_LOCK_SIM_PIN] = 3;
  data[MM_MODEM_LOCK_SIM_PUK] = 10;
  capability_->OnLockRetriesChanged(data);
  EXPECT_EQ(3, capability_->sim_lock_status_.retries_left);

  capability_->sim_lock_status_.lock_type = MM_MODEM_LOCK_SIM_PUK;
  capability_->OnLockRetriesChanged(data);
  EXPECT_EQ(10, capability_->sim_lock_status_.retries_left);

  capability_->sim_lock_status_.lock_type = MM_MODEM_LOCK_SIM_PIN;
  capability_->OnLockRetriesChanged(data);
  EXPECT_EQ(3, capability_->sim_lock_status_.retries_left);

  data.clear();
  capability_->OnLockRetriesChanged(data);
  EXPECT_EQ(kDefaultRetries, capability_->sim_lock_status_.retries_left);
}

TEST_F(CellularCapabilityUniversalMainTest, OnLockTypeChanged) {
  EXPECT_EQ(MM_MODEM_LOCK_UNKNOWN, capability_->sim_lock_status_.lock_type);

  capability_->OnLockTypeChanged(MM_MODEM_LOCK_NONE);
  EXPECT_EQ(MM_MODEM_LOCK_NONE, capability_->sim_lock_status_.lock_type);
  EXPECT_FALSE(capability_->sim_lock_status_.enabled);

  capability_->OnLockTypeChanged(MM_MODEM_LOCK_SIM_PIN);
  EXPECT_EQ(MM_MODEM_LOCK_SIM_PIN, capability_->sim_lock_status_.lock_type);
  EXPECT_TRUE(capability_->sim_lock_status_.enabled);

  capability_->sim_lock_status_.enabled = false;
  capability_->OnLockTypeChanged(MM_MODEM_LOCK_SIM_PUK);
  EXPECT_EQ(MM_MODEM_LOCK_SIM_PUK, capability_->sim_lock_status_.lock_type);
  EXPECT_TRUE(capability_->sim_lock_status_.enabled);
}

TEST_F(CellularCapabilityUniversalMainTest, OnSimLockPropertiesChanged) {
  EXPECT_EQ(MM_MODEM_LOCK_UNKNOWN, capability_->sim_lock_status_.lock_type);
  EXPECT_EQ(0, capability_->sim_lock_status_.retries_left);

  DBusPropertiesMap changed;
  vector<string> invalidated;

  capability_->OnModemPropertiesChanged(changed, invalidated);
  EXPECT_EQ(MM_MODEM_LOCK_UNKNOWN, capability_->sim_lock_status_.lock_type);
  EXPECT_EQ(0, capability_->sim_lock_status_.retries_left);

  ::DBus::Variant variant;
  ::DBus::MessageIter writer = variant.writer();

  // Unlock retries changed, but the SIM wasn't locked.
  CellularCapabilityUniversal::LockRetryData retry_data;
  retry_data[MM_MODEM_LOCK_SIM_PIN] = 3;
  writer << retry_data;
  changed[MM_MODEM_PROPERTY_UNLOCKRETRIES] = variant;

  capability_->OnModemPropertiesChanged(changed, invalidated);
  EXPECT_EQ(MM_MODEM_LOCK_UNKNOWN, capability_->sim_lock_status_.lock_type);
  EXPECT_EQ(3, capability_->sim_lock_status_.retries_left);

  // Unlock retries changed and the SIM got locked.
  variant.clear();
  writer = variant.writer();
  writer << static_cast<uint32>(MM_MODEM_LOCK_SIM_PIN);
  changed[MM_MODEM_PROPERTY_UNLOCKREQUIRED] = variant;
  capability_->OnModemPropertiesChanged(changed, invalidated);
  EXPECT_EQ(MM_MODEM_LOCK_SIM_PIN, capability_->sim_lock_status_.lock_type);
  EXPECT_EQ(3, capability_->sim_lock_status_.retries_left);

  // Only unlock retries changed.
  changed.erase(MM_MODEM_PROPERTY_UNLOCKREQUIRED);
  retry_data[MM_MODEM_LOCK_SIM_PIN] = 2;
  variant.clear();
  writer = variant.writer();
  writer << retry_data;
  changed[MM_MODEM_PROPERTY_UNLOCKRETRIES] = variant;
  capability_->OnModemPropertiesChanged(changed, invalidated);
  EXPECT_EQ(MM_MODEM_LOCK_SIM_PIN, capability_->sim_lock_status_.lock_type);
  EXPECT_EQ(2, capability_->sim_lock_status_.retries_left);

  // Unlock retries changed with a value that doesn't match the current
  // lock type. Default to whatever count is available.
  retry_data.clear();
  retry_data[MM_MODEM_LOCK_SIM_PIN2] = 2;
  variant.clear();
  writer = variant.writer();
  writer << retry_data;
  changed[MM_MODEM_PROPERTY_UNLOCKRETRIES] = variant;
  capability_->OnModemPropertiesChanged(changed, invalidated);
  EXPECT_EQ(MM_MODEM_LOCK_SIM_PIN, capability_->sim_lock_status_.lock_type);
  EXPECT_EQ(2, capability_->sim_lock_status_.retries_left);
}

}  // namespace shill
