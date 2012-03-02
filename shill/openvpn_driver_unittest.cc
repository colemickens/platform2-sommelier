// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_driver.h"

#include <algorithm>

#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/ipconfig.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_device_info.h"
#include "shill/nice_mock_control.h"
#include "shill/rpc_task.h"

using std::map;
using std::string;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;

namespace shill {

class OpenVPNDriverTest : public testing::Test,
                          public RPCTaskDelegate {
 public:
  OpenVPNDriverTest()
      : device_info_(&control_, NULL, NULL, NULL),
        driver_(&control_, &device_info_, args_) {}

  virtual ~OpenVPNDriverTest() {}

  // Inherited from RPCTaskDelegate.
  virtual void Notify(const string &reason, const map<string, string> &dict);

 protected:
  static const char kOption[];
  static const char kProperty[];
  static const char kValue[];
  static const char kOption2[];
  static const char kProperty2[];
  static const char kValue2[];

  void SetArgs() {
    driver_.args_ = args_;
  }

  // Used to assert that a flag appears in the options.
  void ExpectInFlags(const vector<string> &options, const string &flag,
                     const string &value);


  NiceMockControl control_;
  MockDeviceInfo device_info_;
  KeyValueStore args_;
  OpenVPNDriver driver_;
};

const char OpenVPNDriverTest::kOption[] = "--openvpn-option";
const char OpenVPNDriverTest::kProperty[] = "OpenVPN.SomeProperty";
const char OpenVPNDriverTest::kValue[] = "some-property-value";
const char OpenVPNDriverTest::kOption2[] = "--openvpn-option2";
const char OpenVPNDriverTest::kProperty2[] = "OpenVPN.SomeProperty2";
const char OpenVPNDriverTest::kValue2[] = "some-property-value2";

void OpenVPNDriverTest::Notify(const string &/*reason*/,
                               const map<string, string> &/*dict*/) {}

void OpenVPNDriverTest::ExpectInFlags(const vector<string> &options,
                                      const string &flag,
                                      const string &value) {
  vector<string>::const_iterator it =
      std::find(options.begin(), options.end(), flag);

  EXPECT_TRUE(it != options.end());
  if (it != options.end())
    return;                             // Don't crash below.
  it++;
  EXPECT_TRUE(it != options.end());
  if (it != options.end())
    return;                             // Don't crash below.
  EXPECT_EQ(value, *it);
}


TEST_F(OpenVPNDriverTest, Connect) {
  Error error;
  driver_.Connect(&error);
  EXPECT_EQ(Error::kNotSupported, error.type());
}

TEST_F(OpenVPNDriverTest, Notify) {
  map<string, string> dict;
  EXPECT_FALSE(driver_.Notify("down", dict));
  EXPECT_TRUE(driver_.Notify("up", dict));
}

TEST_F(OpenVPNDriverTest, ParseForeignOption) {
  IPConfig::Properties props;
  OpenVPNDriver::ParseForeignOption("", &props);
  OpenVPNDriver::ParseForeignOption("dhcp-option DOMAIN", &props);
  OpenVPNDriver::ParseForeignOption("dhcp-option DOMAIN zzz.com foo", &props);
  OpenVPNDriver::ParseForeignOption("dhcp-Option DOmAIN xyz.com", &props);
  ASSERT_EQ(1, props.domain_search.size());
  EXPECT_EQ("xyz.com", props.domain_search[0]);
  OpenVPNDriver::ParseForeignOption("dhcp-option DnS 1.2.3.4", &props);
  ASSERT_EQ(1, props.dns_servers.size());
  EXPECT_EQ("1.2.3.4", props.dns_servers[0]);
}

TEST_F(OpenVPNDriverTest, ParseForeignOptions) {
  // Basically test that std::map is a sorted container.
  map<int, string> options;
  options[5] = "dhcp-option DOMAIN five.com";
  options[2] = "dhcp-option DOMAIN two.com";
  options[8] = "dhcp-option DOMAIN eight.com";
  options[7] = "dhcp-option DOMAIN seven.com";
  options[4] = "dhcp-option DOMAIN four.com";
  IPConfig::Properties props;
  OpenVPNDriver::ParseForeignOptions(options, &props);
  ASSERT_EQ(5, props.domain_search.size());
  EXPECT_EQ("two.com", props.domain_search[0]);
  EXPECT_EQ("four.com", props.domain_search[1]);
  EXPECT_EQ("five.com", props.domain_search[2]);
  EXPECT_EQ("seven.com", props.domain_search[3]);
  EXPECT_EQ("eight.com", props.domain_search[4]);
}

TEST_F(OpenVPNDriverTest, ParseIPConfiguration) {
  map<string, string> config;
  config["ifconfig_loCal"] = "4.5.6.7";
  config["ifconfiG_broadcast"] = "1.2.255.255";
  config["ifconFig_netmAsk"] = "255.255.255.0";
  config["ifconfig_remotE"] = "33.44.55.66";
  config["route_vpN_gateway"] = "192.168.1.1";
  config["tun_mtu"] = "1000";
  config["foreign_option_2"] = "dhcp-option DNS 4.4.4.4";
  config["foreign_option_1"] = "dhcp-option DNS 1.1.1.1";
  config["foreign_option_3"] = "dhcp-option DNS 2.2.2.2";
  config["foo"] = "bar";
  IPConfig::Properties props;
  OpenVPNDriver::ParseIPConfiguration(config, &props);
  EXPECT_EQ(IPAddress::kFamilyIPv4, props.address_family);
  EXPECT_EQ("4.5.6.7", props.address);
  EXPECT_EQ("1.2.255.255", props.broadcast_address);
  EXPECT_EQ(24, props.subnet_cidr);
  EXPECT_EQ("33.44.55.66", props.peer_address);
  EXPECT_EQ("192.168.1.1", props.gateway);
  EXPECT_EQ(1000, props.mtu);
  ASSERT_EQ(3, props.dns_servers.size());
  EXPECT_EQ("1.1.1.1", props.dns_servers[0]);
  EXPECT_EQ("4.4.4.4", props.dns_servers[1]);
  EXPECT_EQ("2.2.2.2", props.dns_servers[2]);
}

TEST_F(OpenVPNDriverTest, InitOptionsNoHost) {
  Error error;
  vector<string> options;
  driver_.InitOptions(&options, &error);
  EXPECT_EQ(Error::kInvalidArguments, error.type());
  EXPECT_TRUE(options.empty());
}

TEST_F(OpenVPNDriverTest, InitOptions) {
  static const char kHost[] = "192.168.2.254";
  args_.SetString(flimflam::kProviderHostProperty, kHost);
  SetArgs();
  driver_.rpc_task_.reset(new RPCTask(&control_, this));
  Error error;
  vector<string> options;
  const string kInterfaceName("tun0");
  EXPECT_CALL(device_info_, CreateTunnelInterface(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(kInterfaceName), Return(true)));
  driver_.InitOptions(&options, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ("--client", options[0]);
  ExpectInFlags(options, "--remote", kHost);
  ExpectInFlags(options, "CONNMAN_PATH", RPCTaskMockAdaptor::kRpcId);
  ExpectInFlags(options, "--dev", kInterfaceName);
  EXPECT_EQ("openvpn", options.back());
  EXPECT_EQ(kInterfaceName, driver_.tunnel_interface_);
}

TEST_F(OpenVPNDriverTest, AppendValueOption) {
  vector<string> options;
  driver_.AppendValueOption("OpenVPN.UnknownProperty", kOption, &options);
  EXPECT_TRUE(options.empty());

  args_.SetString(kProperty, "");
  SetArgs();
  driver_.AppendValueOption(kProperty, kOption, &options);
  EXPECT_TRUE(options.empty());

  args_.SetString(kProperty, kValue);
  args_.SetString(kProperty2, kValue2);
  SetArgs();
  driver_.AppendValueOption(kProperty, kOption, &options);
  driver_.AppendValueOption(kProperty2, kOption2, &options);
  EXPECT_EQ(4, options.size());
  EXPECT_EQ(kOption, options[0]);
  EXPECT_EQ(kValue, options[1]);
  EXPECT_EQ(kOption2, options[2]);
  EXPECT_EQ(kValue2, options[3]);
}

TEST_F(OpenVPNDriverTest, AppendFlag) {
  vector<string> options;
  driver_.AppendValueOption("OpenVPN.UnknownProperty", kOption, &options);
  EXPECT_TRUE(options.empty());

  args_.SetString(kProperty, "");
  args_.SetString(kProperty2, kValue2);
  SetArgs();
  driver_.AppendFlag(kProperty, kOption, &options);
  driver_.AppendFlag(kProperty2, kOption2, &options);
  EXPECT_EQ(2, options.size());
  EXPECT_EQ(kOption, options[0]);
  EXPECT_EQ(kOption2, options[1]);
}

TEST_F(OpenVPNDriverTest, ClaimInterface) {
  const string kInterfaceName("tun0");
  driver_.tunnel_interface_ = kInterfaceName;
  const int kInterfaceIndex = 1122;
  EXPECT_FALSE(driver_.ClaimInterface(kInterfaceName + "XXX", kInterfaceIndex));
  EXPECT_EQ(-1, driver_.interface_index_);

  EXPECT_TRUE(driver_.ClaimInterface(kInterfaceName, kInterfaceIndex));
  EXPECT_EQ(kInterfaceIndex, driver_.interface_index_);
}

}  // namespace shill
