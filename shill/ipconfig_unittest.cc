// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/callback_old.h>

#include "shill/ipconfig.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"

using testing::Test;

namespace shill {

class IPConfigTest : public Test {
 public:
  IPConfigTest()
      : device_(new MockDevice(&control_interface_, NULL, NULL, "testname", 0)),
        ipconfig_(new IPConfig(device_)) {}

 protected:
  MockControl control_interface_;
  scoped_refptr<const MockDevice> device_;
  IPConfigRefPtr ipconfig_;
};

TEST_F(IPConfigTest, GetDeviceName) {
  EXPECT_EQ("testname", ipconfig_->GetDeviceName());
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
  ipconfig_->UpdateProperties(properties);
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

class UpdateCallbackTest {
 public:
  UpdateCallbackTest(IPConfigRefPtr ipconfig)
      : ipconfig_(ipconfig),
        called_(false) {}

  void Callback(IPConfigRefPtr ipconfig) {
    called_ = true;
    EXPECT_EQ(ipconfig.get(), ipconfig_.get());
  }

  bool called() const { return called_; }

 private:
  IPConfigRefPtr ipconfig_;
  bool called_;
};

TEST_F(IPConfigTest, UpdateCallback) {
  UpdateCallbackTest callback_test(ipconfig_);
  ASSERT_FALSE(callback_test.called());
  ipconfig_->RegisterUpdateCallback(
      NewCallback(&callback_test, &UpdateCallbackTest::Callback));
  ipconfig_->UpdateProperties(IPConfig::Properties());
  EXPECT_TRUE(callback_test.called());
}

}  // namespace shill
