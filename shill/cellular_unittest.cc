// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular.h"

#include <chromeos/dbus/service_constants.h>

#include "shill/mock_modem_proxy.h"
#include "shill/mock_modem_simple_proxy.h"
#include "shill/property_store_unittest.h"
#include "shill/proxy_factory.h"

using std::string;
using testing::Return;

namespace shill {

class CellularTest : public PropertyStoreTest {
 public:
  CellularTest()
      : proxy_(new MockModemProxy()),
        simple_proxy_(new MockModemSimpleProxy()),
        proxy_factory_(this),
        device_(new Cellular(&control_interface_,
                             NULL,
                             &manager_,
                             "usb0",
                             3,
                             Cellular::kTypeGSM,
                             kDBusOwner,
                             kDBusPath)) {}
  virtual ~CellularTest() {}

  virtual void SetUp() {
    ProxyFactory::set_factory(&proxy_factory_);
  }

  virtual void TearDown() {
    ProxyFactory::set_factory(NULL);
    device_->Stop();
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    TestProxyFactory(CellularTest *test) : test_(test) {}

    virtual ModemProxyInterface *CreateModemProxy(const string &path,
                                                  const string &service) {
      return test_->proxy_.release();
    }

    virtual ModemSimpleProxyInterface *CreateModemSimpleProxy(
        const string &path,
        const string &service) {
      return test_->simple_proxy_.release();
    }

   private:
    CellularTest *test_;
  };

  static const char kDBusOwner[];
  static const char kDBusPath[];

  scoped_ptr<MockModemProxy> proxy_;
  scoped_ptr<MockModemSimpleProxy> simple_proxy_;
  TestProxyFactory proxy_factory_;
  CellularRefPtr device_;
};

const char CellularTest::kDBusOwner[] = ":1.19";
const char CellularTest::kDBusPath[] = "/org/chromium/ModemManager/Gobi/0";

TEST_F(CellularTest, Contains) {
  EXPECT_TRUE(device_->store()->Contains(flimflam::kNameProperty));
  EXPECT_FALSE(device_->store()->Contains(""));
}

TEST_F(CellularTest, Dispatch) {
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

TEST_F(CellularTest, GetTypeString) {
  EXPECT_EQ("CellularTypeGSM", device_->GetTypeString());
  device_->type_ = Cellular::kTypeCDMA;
  EXPECT_EQ("CellularTypeCDMA", device_->GetTypeString());
  device_->type_ = static_cast<Cellular::Type>(1234);
  EXPECT_EQ("CellularTypeUnknown-1234", device_->GetTypeString());
}

TEST_F(CellularTest, GetStateString) {
  EXPECT_EQ("CellularStateDisabled", device_->GetStateString());
  device_->state_ = Cellular::kStateEnabled;
  EXPECT_EQ("CellularStateEnabled", device_->GetStateString());
  device_->state_ = Cellular::kStateRegistered;
  EXPECT_EQ("CellularStateRegistered", device_->GetStateString());
  device_->state_ = Cellular::kStateConnected;
  EXPECT_EQ("CellularStateConnected", device_->GetStateString());
  device_->state_ = static_cast<Cellular::State>(2345);
  EXPECT_EQ("CellularStateUnknown-2345", device_->GetStateString());
}

TEST_F(CellularTest, Start) {
  EXPECT_CALL(*proxy_, Enable(true)).Times(1);
  EXPECT_CALL(*simple_proxy_, GetStatus())
      .WillOnce(Return(DBusPropertiesMap()));
  EXPECT_CALL(*proxy_, GetInfo()).WillOnce(Return(ModemProxyInterface::Info()));
  device_->Start();
  EXPECT_EQ(Cellular::kStateEnabled, device_->state_);
}

TEST_F(CellularTest, GetModemStatus) {
  static const char kCarrier[] = "The Cellular Carrier";
  DBusPropertiesMap props;
  props["carrier"].writer().append_string(kCarrier);
  props["unknown-property"].writer().append_string("irrelevant-value");
  EXPECT_CALL(*simple_proxy_, GetStatus()).WillOnce(Return(props));
  device_->simple_proxy_.reset(simple_proxy_.release());
  device_->state_ = Cellular::kStateEnabled;
  device_->GetModemStatus();
  EXPECT_EQ(kCarrier, device_->carrier_);
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

}  // namespace shill
