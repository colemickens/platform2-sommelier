// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_service.h"

#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/nice_mock_control.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_wimax.h"

using std::string;
using testing::_;
using testing::NiceMock;
using testing::Return;
using wimax_manager::kEAPAnonymousIdentity;
using wimax_manager::kEAPUserIdentity;
using wimax_manager::kEAPUserPassword;

namespace shill {

namespace {

const char kTestLinkName[] = "wm0";
const char kTestAddress[] = "0123456789AB";
const int kTestInterfaceIndex = 5;
const char kTestPath[] = "/org/chromium/WiMaxManager/Device/wm7";

}  // namespace

class WiMaxServiceTest : public testing::Test {
 public:
  WiMaxServiceTest()
      : manager_(&control_, NULL, NULL, NULL),
        wimax_(new MockWiMax(&control_, NULL, &metrics_, &manager_,
                             kTestLinkName, kTestAddress, kTestInterfaceIndex,
                             kTestPath)),
        service_(new WiMaxService(&control_, NULL, &metrics_, &manager_,
                                  wimax_)) {}

  virtual ~WiMaxServiceTest() {}

 protected:
  NiceMockControl control_;
  MockManager manager_;
  MockMetrics metrics_;
  scoped_refptr<MockWiMax> wimax_;
  WiMaxServiceRefPtr service_;
};

TEST_F(WiMaxServiceTest, GetConnectParameters) {
  {
    DBusPropertiesMap parameters;
    service_->GetConnectParameters(&parameters);

    EXPECT_TRUE(ContainsKey(parameters, kEAPAnonymousIdentity));
    EXPECT_STREQ("", parameters[kEAPAnonymousIdentity].reader().get_string());

    EXPECT_TRUE(ContainsKey(parameters, kEAPUserIdentity));
    EXPECT_STREQ("", parameters[kEAPUserIdentity].reader().get_string());

    EXPECT_TRUE(ContainsKey(parameters, kEAPUserPassword));
    EXPECT_STREQ("", parameters[kEAPUserPassword].reader().get_string());
  }
  {
    Service::EapCredentials eap;
    eap.anonymous_identity = "TestAnonymousIdentity";
    eap.identity = "TestUserIdentity";
    eap.password = "TestPassword";
    service_->set_eap(eap);

    DBusPropertiesMap parameters;
    service_->GetConnectParameters(&parameters);

    EXPECT_TRUE(ContainsKey(parameters, kEAPAnonymousIdentity));
    EXPECT_EQ(eap.anonymous_identity,
              parameters[kEAPAnonymousIdentity].reader().get_string());

    EXPECT_TRUE(ContainsKey(parameters, kEAPUserIdentity));
    EXPECT_EQ(eap.identity, parameters[kEAPUserIdentity].reader().get_string());

    EXPECT_TRUE(ContainsKey(parameters, kEAPUserPassword));
    EXPECT_EQ(eap.password, parameters[kEAPUserPassword].reader().get_string());
  }
}

TEST_F(WiMaxServiceTest, TechnologyIs) {
  EXPECT_TRUE(service_->TechnologyIs(Technology::kWiMax));
  EXPECT_FALSE(service_->TechnologyIs(Technology::kEthernet));
}

TEST_F(WiMaxServiceTest, GetDeviceRpcId) {
  Error error;
  EXPECT_EQ(DeviceMockAdaptor::kRpcId, service_->GetDeviceRpcId(&error));
  EXPECT_TRUE(error.IsSuccess());
}

}  // namespace shill
