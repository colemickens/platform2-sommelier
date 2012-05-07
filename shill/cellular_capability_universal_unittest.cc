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
#include <mm/ModemManager-names.h>

#include "shill/cellular.h"
#include "shill/cellular_service.h"
#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/mock_adaptors.h"
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
        service_(new MockCellularService(&control_,
                                         &dispatcher_,
                                         &metrics_,
                                         &manager_,
                                         cellular_)),
        modem_3gpp_proxy_(new mm1::MockModemModem3gppProxy()),
        modem_cdma_proxy_(new mm1::MockModemModemCdmaProxy()),
        modem_proxy_(new mm1::MockModemProxy()),
        modem_simple_proxy_(new mm1::MockModemSimpleProxy()),
        sim_proxy_(new mm1::MockSimProxy()),
        properties_proxy_(new MockDBusPropertiesProxy()),
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
    cellular_->service_ = service_;
  }

  virtual void TearDown() {
    capability_->proxy_factory_ = NULL;
  }

  void InitProviderDB() {
    const char kTestMobileProviderDBPath[] = "provider_db_unittest.bfd";

    provider_db_ = mobile_provider_open_db(kTestMobileProviderDBPath);
    ASSERT_TRUE(provider_db_);
    cellular_->provider_db_ = provider_db_;
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

  void Set3gppProxy() {
    capability_->modem_3gpp_proxy_.reset(modem_3gpp_proxy_.release());
  }

  void SetSimpleProxy() {
    capability_->modem_simple_proxy_.reset(modem_simple_proxy_.release());
  }

  MOCK_METHOD1(TestCallback, void(const Error &error));

 protected:
  static const char kImei[];
  static const char kSimPath[];
  static const uint32 kAccessTechnologies;

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
    virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
        const std::string &/* path */,
        const std::string &/* service */) {
      return test_->properties_proxy_.release();
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
  MockCellularService *service_;  // owned by cellular_
  scoped_ptr<mm1::MockModemModem3gppProxy> modem_3gpp_proxy_;
  scoped_ptr<mm1::MockModemModemCdmaProxy> modem_cdma_proxy_;
  scoped_ptr<mm1::MockModemProxy> modem_proxy_;
  scoped_ptr<mm1::MockModemSimpleProxy> modem_simple_proxy_;
  scoped_ptr<mm1::MockSimProxy> sim_proxy_;
  scoped_ptr<MockDBusPropertiesProxy> properties_proxy_;
  TestProxyFactory proxy_factory_;
  CellularCapabilityUniversal *capability_;  // Owned by |cellular_|.
  NiceMock<DeviceMockAdaptor> *device_adaptor_;  // Owned by |cellular_|.
  mobile_provider_db *provider_db_;
  DBusPropertyMapsCallback scan_callback_;  // saved for testing scan operations
  DBusPathCallback connect_callback_;  // saved for testing connect operations
};

const char CellularCapabilityUniversalTest::kImei[] = "999911110000";
const char CellularCapabilityUniversalTest::kSimPath[] = "/foo/sim";
const uint32 CellularCapabilityUniversalTest::kAccessTechnologies =
    MM_MODEM_ACCESS_TECHNOLOGY_LTE |
    MM_MODEM_ACCESS_TECHNOLOGY_HSPA_PLUS;

TEST_F(CellularCapabilityUniversalTest, StartModem) {
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

  // After setup we lose pointers to the proxies, so it is hard to set
  // expectations.
  SetUp();

  Error error;
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  ResultCallback callback =
      Bind(&CellularCapabilityUniversalTest::TestCallback, Unretained(this));
  capability_->StartModem(&error, callback);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kImei, capability_->imei_);
  EXPECT_EQ(kAccessTechnologies, capability_->access_technologies_);
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

TEST_F(CellularCapabilityUniversalTest, PropertiesChanged) {
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
}

TEST_F(CellularCapabilityUniversalTest, SimPropertiesChanged) {
  // Set up mock modem properties
  DBusPropertiesMap modem_properties;
  modem_properties[MM_MODEM_PROPERTY_SIM].writer().append_path(kSimPath);

  // Set up mock modem sim properties
  const char kImsi[] = "310100000001";
  DBusPropertiesMap sim_properties;
  sim_properties[MM_SIM_PROPERTY_IMSI].writer().append_string(kImsi);

  EXPECT_CALL(*properties_proxy_, GetAll(MM_DBUS_INTERFACE_SIM))
      .WillOnce(Return(sim_properties));

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

  // Updating the SIM
  DBusPropertiesMap new_properties;
  const char kCountry[] = "us";
  const char kCode[] = "310160";
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
  EXPECT_EQ(kCode, cellular_->home_provider().GetCode());
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

// Validates that OnScanReply does not crash with a null callback.
TEST_F(CellularCapabilityUniversalTest, ScanWithNullCallback) {
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
TEST_F(CellularCapabilityUniversalTest, Scan) {
  Error error;

  EXPECT_CALL(*modem_3gpp_proxy_, Scan(_, _, CellularCapability::kTimeoutScan))
      .WillRepeatedly(SaveArg<1>(&scan_callback_));
  EXPECT_CALL(*device_adaptor_,
              EmitBoolChanged(flimflam::kScanningProperty, true));
  Set3gppProxy();
  capability_->Scan(&error, ResultCallback());
  EXPECT_TRUE(capability_->scanning_);

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

  // Simulate the completion of the scan with no networks in the results.
  EXPECT_CALL(*device_adaptor_,
              EmitBoolChanged(flimflam::kScanningProperty, true));
  capability_->Scan(&error, ResultCallback());
  EXPECT_TRUE(capability_->scanning_);
  EXPECT_CALL(*device_adaptor_,
              EmitBoolChanged(flimflam::kScanningProperty, false));
  EXPECT_CALL(*device_adaptor_,
              EmitStringmapsChanged(flimflam::kFoundNetworksProperty,
                                    SizeIs(0)));
  scan_callback_.Run(vector<DBusPropertiesMap>(), Error());
  EXPECT_FALSE(capability_->scanning_);
}

// Validates expected property updates when scan fails
TEST_F(CellularCapabilityUniversalTest, ScanFailure) {
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

// Validates expected behavior of Connect function
TEST_F(CellularCapabilityUniversalTest, Connect) {
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
      .WillOnce(SaveArg<2>(&connect_callback_));
  capability_->Connect(properties, &error, callback);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_CALL(*this, TestCallback(IsFailure()));
  EXPECT_CALL(*service_, ClearLastGoodApn());
  connect_callback_.Run(bearer, Error(Error::kOperationFailed));

  // Test connect success
  EXPECT_CALL(*modem_simple_proxy, Connect(_, _, _, _))
      .WillOnce(SaveArg<2>(&connect_callback_));
  capability_->Connect(properties, &error, callback);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  connect_callback_.Run(bearer, Error(Error::kSuccess));
}

// Validates Connect iterates over APNs
TEST_F(CellularCapabilityUniversalTest, ConnectApns) {
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

  EXPECT_CALL(*modem_simple_proxy, Connect(HasApn(apn_name_bar), _, _, _))
      .WillOnce(SaveArg<2>(&connect_callback_));
  EXPECT_CALL(*service_, ClearLastGoodApn());
  connect_callback_.Run(bearer, Error(Error::kInvalidApn));

  EXPECT_CALL(*service_, SetLastGoodApn(apn2));
  EXPECT_CALL(*this, TestCallback(IsSuccess()));
  connect_callback_.Run(bearer, Error(Error::kSuccess));
}

}  // namespace shill
