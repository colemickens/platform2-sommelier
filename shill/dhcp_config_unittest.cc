// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_config.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"

using std::string;
using std::vector;
using testing::Test;

namespace shill {

class DHCPConfigTest : public Test {
 public:
  DHCPConfigTest() :
      device_(new MockDevice(&control_interface_, NULL, NULL, "testname", 0)) {}

 protected:
  MockControl control_interface_;
  scoped_refptr<MockDevice> device_;
};

TEST_F(DHCPConfigTest, GetIPv4AddressString) {
  DHCPConfigRefPtr config(new DHCPConfig(NULL, *device_));
  EXPECT_EQ("255.255.255.255", config->GetIPv4AddressString(0xffffffff));
  EXPECT_EQ("0.0.0.0", config->GetIPv4AddressString(0));
  EXPECT_EQ("1.2.3.4", config->GetIPv4AddressString(0x04030201));
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
    DBus::Variant var;
    vector<unsigned int> routers;
    routers.push_back(0x02040608);
    routers.push_back(0x03050709);
    DBus::MessageIter writer = var.writer();
    writer << routers;
    conf[DHCPConfig::kConfigurationKeyRouters] = var;
  }
  {
    DBus::Variant var;
    vector<unsigned int> dns;
    dns.push_back(0x09070503);
    dns.push_back(0x08060402);
    DBus::MessageIter writer = var.writer();
    writer << dns;
    conf[DHCPConfig::kConfigurationKeyDNS] = var;
  }
  conf[DHCPConfig::kConfigurationKeyDomainName].writer().append_string(
      "domain-name");
  {
    DBus::Variant var;
    vector<string> search;
    search.push_back("foo.com");
    search.push_back("bar.com");
    DBus::MessageIter writer = var.writer();
    writer << search;
    conf[DHCPConfig::kConfigurationKeyDomainSearch] = var;
  }
  conf[DHCPConfig::kConfigurationKeyMTU].writer().append_uint16(600);
  conf["UnknownKey"] = DBus::Variant();

  DHCPConfigRefPtr config(new DHCPConfig(NULL, *device_));
  IPConfig::Properties properties;
  ASSERT_TRUE(config->ParseConfiguration(conf, &properties));
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

}  // namespace shill
