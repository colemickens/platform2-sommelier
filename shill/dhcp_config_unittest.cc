// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_config.h"
#include "shill/dhcp_provider.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_glib.h"

using std::string;
using std::vector;
using testing::_;
using testing::Return;
using testing::SetArgumentPointee;
using testing::Test;

namespace shill {

class DHCPConfigTest : public Test {
 public:
  DHCPConfigTest()
      : device_(new MockDevice(&control_interface_, NULL, NULL, "testname", 0)),
        config_(new DHCPConfig(DHCPProvider::GetInstance(), device_, &glib_)) {}

 protected:
  MockGLib glib_;
  MockControl control_interface_;
  scoped_refptr<MockDevice> device_;
  DHCPConfigRefPtr config_;
};

TEST_F(DHCPConfigTest, GetIPv4AddressString) {
  EXPECT_EQ("255.255.255.255", config_->GetIPv4AddressString(0xffffffff));
  EXPECT_EQ("0.0.0.0", config_->GetIPv4AddressString(0));
  EXPECT_EQ("1.2.3.4", config_->GetIPv4AddressString(0x04030201));
}

TEST_F(DHCPConfigTest, ParseConfiguration) {
  DHCPConfig::Configuration conf;
  conf[DHCPConfig::kConfigurationKeyIPAddress].writer().append_uint32(
      0x01020304);
  conf[DHCPConfig::kConfigurationKeySubnetCIDR].writer().append_byte(
      16);
  conf[DHCPConfig::kConfigurationKeyBroadcastAddress].writer().append_uint32(
      0x10203040);
  {
    vector<unsigned int> routers;
    routers.push_back(0x02040608);
    routers.push_back(0x03050709);
    DBus::MessageIter writer =
        conf[DHCPConfig::kConfigurationKeyRouters].writer();
    writer << routers;
  }
  {
    vector<unsigned int> dns;
    dns.push_back(0x09070503);
    dns.push_back(0x08060402);
    DBus::MessageIter writer = conf[DHCPConfig::kConfigurationKeyDNS].writer();
    writer << dns;
  }
  conf[DHCPConfig::kConfigurationKeyDomainName].writer().append_string(
      "domain-name");
  {
    vector<string> search;
    search.push_back("foo.com");
    search.push_back("bar.com");
    DBus::MessageIter writer =
        conf[DHCPConfig::kConfigurationKeyDomainSearch].writer();
    writer << search;
  }
  conf[DHCPConfig::kConfigurationKeyMTU].writer().append_uint16(600);
  conf["UnknownKey"] = DBus::Variant();

  IPConfig::Properties properties;
  ASSERT_TRUE(config_->ParseConfiguration(conf, &properties));
  EXPECT_EQ("4.3.2.1", properties.address);
  EXPECT_EQ(16, properties.subnet_cidr);
  EXPECT_EQ("64.48.32.16", properties.broadcast_address);
  EXPECT_EQ("8.6.4.2", properties.gateway);
  ASSERT_EQ(2, properties.dns_servers.size());
  EXPECT_EQ("3.5.7.9", properties.dns_servers[0]);
  EXPECT_EQ("2.4.6.8", properties.dns_servers[1]);
  EXPECT_EQ("domain-name", properties.domain_name);
  ASSERT_EQ(2, properties.domain_search.size());
  EXPECT_EQ("foo.com", properties.domain_search[0]);
  EXPECT_EQ("bar.com", properties.domain_search[1]);
  EXPECT_EQ(600, properties.mtu);
}

TEST_F(DHCPConfigTest, Start) {
  EXPECT_CALL(glib_, SpawnAsync(_, _, _, _, _, _, _, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(config_->Start());
  EXPECT_EQ(0, config_->pid_);

  const unsigned int kPID = 1234;
  EXPECT_CALL(glib_, SpawnAsync(_, _, _, _, _, _, _, _))
      .WillOnce(DoAll(SetArgumentPointee<6>(kPID), Return(true)));
  EXPECT_TRUE(config_->Start());
  EXPECT_EQ(kPID, config_->pid_);
  DHCPProvider *provider = DHCPProvider::GetInstance();
  ASSERT_TRUE(provider->configs_.find(kPID) != provider->configs_.end());
  EXPECT_EQ(config_.get(), provider->configs_.find(1234)->second.get());
}

}  // namespace shill
