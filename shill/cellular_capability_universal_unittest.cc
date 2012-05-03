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

}  // namespace shill
