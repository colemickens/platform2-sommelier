// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ipconfig.h"

#include <base/callback_old.h>
#include <gtest/gtest.h>

#include "shill/mock_control.h"

using testing::Test;

namespace shill {

namespace {
const char kDeviceName[] = "testdevice";
}  // namespace {}

class IPConfigTest : public Test {
 public:
  IPConfigTest() : ipconfig_(new IPConfig(&control_, kDeviceName)) {}

 protected:
  MockControl control_;
  IPConfigRefPtr ipconfig_;
};

TEST_F(IPConfigTest, DeviceName) {
  EXPECT_EQ(kDeviceName, ipconfig_->device_name());
}

TEST_F(IPConfigTest, RequestIP) {
  EXPECT_FALSE(ipconfig_->RequestIP());
}

TEST_F(IPConfigTest, RenewIP) {
  EXPECT_FALSE(ipconfig_->RenewIP());
}

TEST_F(IPConfigTest, ReleaseIP) {
  EXPECT_FALSE(ipconfig_->ReleaseIP());
}

TEST_F(IPConfigTest, UpdateProperties) {
  IPConfig::Properties properties;
  properties.address = "1.2.3.4";
  properties.subnet_cidr = 24;
  properties.broadcast_address = "11.22.33.44";
  properties.gateway = "5.6.7.8";
  properties.dns_servers.push_back("10.20.30.40");
  properties.dns_servers.push_back("20.30.40.50");
  properties.domain_name = "foo.org";
  properties.domain_search.push_back("zoo.org");
  properties.domain_search.push_back("zoo.com");
  properties.mtu = 700;
  ipconfig_->UpdateProperties(properties, true);
  EXPECT_EQ("1.2.3.4", ipconfig_->properties().address);
  EXPECT_EQ(24, ipconfig_->properties().subnet_cidr);
  EXPECT_EQ("11.22.33.44", ipconfig_->properties().broadcast_address);
  EXPECT_EQ("5.6.7.8", ipconfig_->properties().gateway);
  ASSERT_EQ(2, ipconfig_->properties().dns_servers.size());
  EXPECT_EQ("10.20.30.40", ipconfig_->properties().dns_servers[0]);
  EXPECT_EQ("20.30.40.50", ipconfig_->properties().dns_servers[1]);
  ASSERT_EQ(2, ipconfig_->properties().domain_search.size());
  EXPECT_EQ("zoo.org", ipconfig_->properties().domain_search[0]);
  EXPECT_EQ("zoo.com", ipconfig_->properties().domain_search[1]);
  EXPECT_EQ("foo.org", ipconfig_->properties().domain_name);
  EXPECT_EQ(700, ipconfig_->properties().mtu);
}

namespace {

class UpdateCallbackTest {
 public:
  UpdateCallbackTest(const IPConfigRefPtr &ipconfig, bool success)
      : ipconfig_(ipconfig),
        success_(success),
        called_(false) {}

  void Callback(const IPConfigRefPtr &ipconfig, bool success) {
    called_ = true;
    EXPECT_EQ(ipconfig_.get(), ipconfig.get());
    EXPECT_EQ(success_, success);
  }

  bool called() const { return called_; }

 private:
  IPConfigRefPtr ipconfig_;
  bool success_;
  bool called_;
};

}  // namespace {}

TEST_F(IPConfigTest, UpdateCallback) {
  for (int success = 0; success < 2; success++) {
    UpdateCallbackTest callback_test(ipconfig_, success);
    ASSERT_FALSE(callback_test.called());
    ipconfig_->RegisterUpdateCallback(
        NewCallback(&callback_test, &UpdateCallbackTest::Callback));
    ipconfig_->UpdateProperties(IPConfig::Properties(), success);
    EXPECT_TRUE(callback_test.called());
  }
}

}  // namespace shill
