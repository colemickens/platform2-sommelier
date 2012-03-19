// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_driver.h"

#include <algorithm>

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>

#include "shill/error.h"
#include "shill/ipconfig.h"
#include "shill/mock_adaptors.h"
#include "shill/mock_connection.h"
#include "shill/mock_device_info.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_service.h"
#include "shill/mock_vpn.h"
#include "shill/mock_vpn_service.h"
#include "shill/nice_mock_control.h"
#include "shill/rpc_task.h"
#include "shill/vpn.h"
#include "shill/vpn_service.h"

using std::map;
using std::string;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace shill {

class OpenVPNDriverTest : public testing::Test,
                          public RPCTaskDelegate {
 public:
  OpenVPNDriverTest()
      : device_info_(&control_, NULL, NULL, NULL),
        manager_(&control_, &dispatcher_, &metrics_, &glib_),
        driver_(new OpenVPNDriver(&control_, &dispatcher_, &metrics_, &manager_,
                                  &device_info_, &glib_, args_)),
        service_(new MockVPNService(&control_, &dispatcher_, &metrics_,
                                    &manager_, driver_)),
        device_(new MockVPN(&control_, &dispatcher_, &metrics_, &manager_,
                            kInterfaceName, kInterfaceIndex)) {}

  virtual ~OpenVPNDriverTest() {}

  virtual void TearDown() {
    driver_->child_watch_tag_ = 0;
    driver_->pid_ = 0;
    driver_->device_ = NULL;
    driver_->service_ = NULL;
  }

 protected:
  static const char kOption[];
  static const char kProperty[];
  static const char kValue[];
  static const char kOption2[];
  static const char kProperty2[];
  static const char kValue2[];
  static const char kGateway1[];
  static const char kNetmask1[];
  static const char kNetwork1[];
  static const char kGateway2[];
  static const char kNetmask2[];
  static const char kNetwork2[];
  static const char kInterfaceName[];
  static const int kInterfaceIndex;

  void SetArgs() {
    driver_->args_ = args_;
  }

  // Used to assert that a flag appears in the options.
  void ExpectInFlags(const vector<string> &options, const string &flag,
                     const string &value);

  // Inherited from RPCTaskDelegate.
  virtual void Notify(const string &reason, const map<string, string> &dict);

  NiceMockControl control_;
  MockDeviceInfo device_info_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockGLib glib_;
  MockManager manager_;
  KeyValueStore args_;
  OpenVPNDriver *driver_;  // Owner by |service_|.
  scoped_refptr<MockVPNService> service_;
  scoped_refptr<MockVPN> device_;
};

const char OpenVPNDriverTest::kOption[] = "--openvpn-option";
const char OpenVPNDriverTest::kProperty[] = "OpenVPN.SomeProperty";
const char OpenVPNDriverTest::kValue[] = "some-property-value";
const char OpenVPNDriverTest::kOption2[] = "--openvpn-option2";
const char OpenVPNDriverTest::kProperty2[] = "OpenVPN.SomeProperty2";
const char OpenVPNDriverTest::kValue2[] = "some-property-value2";
const char OpenVPNDriverTest::kGateway1[] = "10.242.2.13";
const char OpenVPNDriverTest::kNetmask1[] = "255.255.255.255";
const char OpenVPNDriverTest::kNetwork1[] = "10.242.2.1";
const char OpenVPNDriverTest::kGateway2[] = "10.242.2.14";
const char OpenVPNDriverTest::kNetmask2[] = "255.255.0.0";
const char OpenVPNDriverTest::kNetwork2[] = "192.168.0.0";
const char OpenVPNDriverTest::kInterfaceName[] = "tun0";
const int OpenVPNDriverTest::kInterfaceIndex = 123;

void OpenVPNDriverTest::Notify(const string &/*reason*/,
                               const map<string, string> &/*dict*/) {}

void OpenVPNDriverTest::ExpectInFlags(const vector<string> &options,
                                      const string &flag,
                                      const string &value) {
  vector<string>::const_iterator it =
      std::find(options.begin(), options.end(), flag);

  EXPECT_TRUE(it != options.end());
  if (it != options.end())
    return;  // Don't crash below.
  it++;
  EXPECT_TRUE(it != options.end());
  if (it != options.end())
    return;  // Don't crash below.
  EXPECT_EQ(value, *it);
}


TEST_F(OpenVPNDriverTest, Connect) {
  EXPECT_CALL(*service_, SetState(Service::kStateConfiguring));
  const string interface = kInterfaceName;
  EXPECT_CALL(device_info_, CreateTunnelInterface(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(interface), Return(true)));
  Error error;
  driver_->Connect(service_, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kInterfaceName, driver_->tunnel_interface_);
}

TEST_F(OpenVPNDriverTest, ConnectTunnelFailure) {
  EXPECT_CALL(*service_, SetState(Service::kStateConfiguring));
  EXPECT_CALL(device_info_, CreateTunnelInterface(_))
      .WillOnce(Return(false));
  EXPECT_CALL(*service_, SetState(Service::kStateFailure));
  Error error;
  driver_->Connect(service_, &error);
  EXPECT_EQ(Error::kInternalError, error.type());
  EXPECT_TRUE(driver_->tunnel_interface_.empty());
}

TEST_F(OpenVPNDriverTest, Notify) {
  map<string, string> dict;
  driver_->device_ = device_;
  EXPECT_CALL(*device_, UpdateIPConfig(_));
  driver_->Notify("up", dict);
}

TEST_F(OpenVPNDriverTest, NotifyFail) {
  map<string, string> dict;
  driver_->device_ = device_;
  EXPECT_CALL(*device_, OnDisconnected());
  driver_->Notify("fail", dict);
}

TEST_F(OpenVPNDriverTest, GetRouteOptionEntry) {
  OpenVPNDriver::RouteOptions routes;
  EXPECT_EQ(NULL, OpenVPNDriver::GetRouteOptionEntry("foo", "bar", &routes));
  EXPECT_TRUE(routes.empty());
  EXPECT_EQ(NULL, OpenVPNDriver::GetRouteOptionEntry("foo", "foo", &routes));
  EXPECT_TRUE(routes.empty());
  EXPECT_EQ(NULL, OpenVPNDriver::GetRouteOptionEntry("foo", "fooZ", &routes));
  EXPECT_TRUE(routes.empty());
  IPConfig::Route *route =
      OpenVPNDriver::GetRouteOptionEntry("foo", "foo12", &routes);
  EXPECT_EQ(1, routes.size());
  EXPECT_EQ(route, &routes[12]);
  route = OpenVPNDriver::GetRouteOptionEntry("foo", "foo13", &routes);
  EXPECT_EQ(2, routes.size());
  EXPECT_EQ(route, &routes[13]);
}

TEST_F(OpenVPNDriverTest, ParseRouteOption) {
  OpenVPNDriver::RouteOptions routes;
  OpenVPNDriver::ParseRouteOption("foo", "bar", &routes);
  EXPECT_TRUE(routes.empty());
  OpenVPNDriver::ParseRouteOption("gateway_2", kGateway2, &routes);
  OpenVPNDriver::ParseRouteOption("netmask_2", kNetmask2, &routes);
  OpenVPNDriver::ParseRouteOption("network_2", kNetwork2, &routes);
  EXPECT_EQ(1, routes.size());
  OpenVPNDriver::ParseRouteOption("gateway_1", kGateway1, &routes);
  OpenVPNDriver::ParseRouteOption("netmask_1", kNetmask1, &routes);
  OpenVPNDriver::ParseRouteOption("network_1", kNetwork1, &routes);
  EXPECT_EQ(2, routes.size());
  EXPECT_EQ(kGateway1, routes[1].gateway);
  EXPECT_EQ(kNetmask1, routes[1].netmask);
  EXPECT_EQ(kNetwork1, routes[1].host);
  EXPECT_EQ(kGateway2, routes[2].gateway);
  EXPECT_EQ(kNetmask2, routes[2].netmask);
  EXPECT_EQ(kNetwork2, routes[2].host);
}

TEST_F(OpenVPNDriverTest, SetRoutes) {
  OpenVPNDriver::RouteOptions routes;
  routes[1].gateway = "1.2.3.4";
  routes[1].host= "1.2.3.4";
  routes[2].host = "2.3.4.5";
  routes[2].netmask = "255.0.0.0";
  routes[3].netmask = "255.0.0.0";
  routes[3].gateway = "1.2.3.5";
  routes[5].host = kNetwork2;
  routes[5].netmask = kNetmask2;
  routes[5].gateway = kGateway2;
  routes[4].host = kNetwork1;
  routes[4].netmask = kNetmask1;
  routes[4].gateway = kGateway1;
  IPConfig::Properties props;
  OpenVPNDriver::SetRoutes(routes, &props);
  ASSERT_EQ(2, props.routes.size());
  EXPECT_EQ(kGateway1, props.routes[0].gateway);
  EXPECT_EQ(kNetmask1, props.routes[0].netmask);
  EXPECT_EQ(kNetwork1, props.routes[0].host);
  EXPECT_EQ(kGateway2, props.routes[1].gateway);
  EXPECT_EQ(kNetmask2, props.routes[1].netmask);
  EXPECT_EQ(kNetwork2, props.routes[1].host);
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
  config["trusted_ip"] = "99.88.77.66";
  config["tun_mtu"] = "1000";
  config["foreign_option_2"] = "dhcp-option DNS 4.4.4.4";
  config["foreign_option_1"] = "dhcp-option DNS 1.1.1.1";
  config["foreign_option_3"] = "dhcp-option DNS 2.2.2.2";
  config["route_network_2"] = kNetwork2;
  config["route_network_1"] = kNetwork1;
  config["route_netmask_2"] = kNetmask2;
  config["route_netmask_1"] = kNetmask1;
  config["route_gateway_2"] = kGateway2;
  config["route_gateway_1"] = kGateway1;
  config["foo"] = "bar";
  IPConfig::Properties props;
  OpenVPNDriver::ParseIPConfiguration(config, &props);
  EXPECT_EQ(IPAddress::kFamilyIPv4, props.address_family);
  EXPECT_EQ("4.5.6.7", props.address);
  EXPECT_EQ("1.2.255.255", props.broadcast_address);
  EXPECT_EQ(24, props.subnet_prefix);
  EXPECT_EQ("33.44.55.66", props.peer_address);
  EXPECT_EQ("192.168.1.1", props.gateway);
  EXPECT_EQ("99.88.77.66", props.trusted_ip);
  EXPECT_EQ(1000, props.mtu);
  ASSERT_EQ(3, props.dns_servers.size());
  EXPECT_EQ("1.1.1.1", props.dns_servers[0]);
  EXPECT_EQ("4.4.4.4", props.dns_servers[1]);
  EXPECT_EQ("2.2.2.2", props.dns_servers[2]);
  ASSERT_EQ(2, props.routes.size());
  EXPECT_EQ(kGateway1, props.routes[0].gateway);
  EXPECT_EQ(kNetmask1, props.routes[0].netmask);
  EXPECT_EQ(kNetwork1, props.routes[0].host);
  EXPECT_EQ(kGateway2, props.routes[1].gateway);
  EXPECT_EQ(kNetmask2, props.routes[1].netmask);
  EXPECT_EQ(kNetwork2, props.routes[1].host);
}

TEST_F(OpenVPNDriverTest, InitOptionsNoHost) {
  Error error;
  vector<string> options;
  driver_->InitOptions(&options, &error);
  EXPECT_EQ(Error::kInvalidArguments, error.type());
  EXPECT_TRUE(options.empty());
}

TEST_F(OpenVPNDriverTest, InitOptions) {
  static const char kHost[] = "192.168.2.254";
  args_.SetString(flimflam::kProviderHostProperty, kHost);
  SetArgs();
  driver_->rpc_task_.reset(new RPCTask(&control_, this));
  Error error;
  vector<string> options;
  driver_->tunnel_interface_ = kInterfaceName;
  driver_->InitOptions(&options, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ("--client", options[0]);
  ExpectInFlags(options, "--remote", kHost);
  ExpectInFlags(options, "CONNMAN_PATH", RPCTaskMockAdaptor::kRpcId);
  ExpectInFlags(options, "--dev", kInterfaceName);
  EXPECT_EQ("openvpn", options.back());
  EXPECT_EQ(kInterfaceName, driver_->tunnel_interface_);
}

TEST_F(OpenVPNDriverTest, AppendValueOption) {
  vector<string> options;
  driver_->AppendValueOption("OpenVPN.UnknownProperty", kOption, &options);
  EXPECT_TRUE(options.empty());

  args_.SetString(kProperty, "");
  SetArgs();
  driver_->AppendValueOption(kProperty, kOption, &options);
  EXPECT_TRUE(options.empty());

  args_.SetString(kProperty, kValue);
  args_.SetString(kProperty2, kValue2);
  SetArgs();
  driver_->AppendValueOption(kProperty, kOption, &options);
  driver_->AppendValueOption(kProperty2, kOption2, &options);
  EXPECT_EQ(4, options.size());
  EXPECT_EQ(kOption, options[0]);
  EXPECT_EQ(kValue, options[1]);
  EXPECT_EQ(kOption2, options[2]);
  EXPECT_EQ(kValue2, options[3]);
}

TEST_F(OpenVPNDriverTest, AppendFlag) {
  vector<string> options;
  driver_->AppendValueOption("OpenVPN.UnknownProperty", kOption, &options);
  EXPECT_TRUE(options.empty());

  args_.SetString(kProperty, "");
  args_.SetString(kProperty2, kValue2);
  SetArgs();
  driver_->AppendFlag(kProperty, kOption, &options);
  driver_->AppendFlag(kProperty2, kOption2, &options);
  EXPECT_EQ(2, options.size());
  EXPECT_EQ(kOption, options[0]);
  EXPECT_EQ(kOption2, options[1]);
}

TEST_F(OpenVPNDriverTest, ClaimInterface) {
  driver_->tunnel_interface_ = kInterfaceName;
  EXPECT_FALSE(driver_->ClaimInterface(string(kInterfaceName) + "XXX",
                                       kInterfaceIndex));
  EXPECT_FALSE(driver_->device_);

  static const char kHost[] = "192.168.2.254";
  args_.SetString(flimflam::kProviderHostProperty, kHost);
  SetArgs();
  EXPECT_CALL(glib_, SpawnAsyncWithPipesCWD(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(glib_, ChildWatchAdd(_, _, _)).WillOnce(Return(1));
  EXPECT_TRUE(driver_->ClaimInterface(kInterfaceName, kInterfaceIndex));
  ASSERT_TRUE(driver_->device_);
  EXPECT_EQ(kInterfaceIndex, driver_->device_->interface_index());
}

TEST_F(OpenVPNDriverTest, Cleanup) {
  const unsigned int kTag = 123;
  const int kPID = 123456;
  driver_->child_watch_tag_ = kTag;
  driver_->pid_ = kPID;
  driver_->rpc_task_.reset(new RPCTask(&control_, this));
  driver_->tunnel_interface_ = kInterfaceName;
  driver_->device_ = device_;
  driver_->service_ = service_;
  EXPECT_CALL(glib_, SourceRemove(kTag));
  EXPECT_CALL(glib_, SpawnClosePID(kPID));
  EXPECT_CALL(*device_, Stop());
  EXPECT_CALL(device_info_, DeleteInterface(kInterfaceIndex));
  EXPECT_CALL(*service_, SetState(Service::kStateFailure));
  driver_->Cleanup();
  EXPECT_EQ(0, driver_->child_watch_tag_);
  EXPECT_EQ(0, driver_->pid_);
  EXPECT_FALSE(driver_->rpc_task_.get());
  EXPECT_TRUE(driver_->tunnel_interface_.empty());
  EXPECT_FALSE(driver_->device_);
  EXPECT_FALSE(driver_->service_);
}

TEST_F(OpenVPNDriverTest, SpawnOpenVPN) {
  EXPECT_FALSE(driver_->SpawnOpenVPN());

  static const char kHost[] = "192.168.2.254";
  args_.SetString(flimflam::kProviderHostProperty, kHost);
  SetArgs();
  driver_->tunnel_interface_ = "tun0";
  driver_->rpc_task_.reset(new RPCTask(&control_, this));

  const int kPID = 234678;
  EXPECT_CALL(glib_, SpawnAsyncWithPipesCWD(_, _, _, _, _, _, _, _, _, _))
      .WillOnce(Return(false))
      .WillOnce(DoAll(SetArgumentPointee<5>(kPID), Return(true)));
  const int kTag = 6;
  EXPECT_CALL(glib_, ChildWatchAdd(kPID, &driver_->OnOpenVPNDied, driver_))
      .WillOnce(Return(kTag));
  EXPECT_FALSE(driver_->SpawnOpenVPN());
  EXPECT_TRUE(driver_->SpawnOpenVPN());
  EXPECT_EQ(kPID, driver_->pid_);
  EXPECT_EQ(kTag, driver_->child_watch_tag_);
}

TEST_F(OpenVPNDriverTest, OnOpenVPNDied) {
  const int kPID = 99999;
  driver_->child_watch_tag_ = 333;
  driver_->pid_ = kPID;
  EXPECT_CALL(glib_, SpawnClosePID(kPID));
  OpenVPNDriver::OnOpenVPNDied(kPID, 2, driver_);
  EXPECT_EQ(0, driver_->child_watch_tag_);
  EXPECT_EQ(0, driver_->pid_);
}

TEST_F(OpenVPNDriverTest, Disconnect) {
  driver_->device_ = device_;
  driver_->service_ = service_;
  EXPECT_CALL(*device_, OnDisconnected());
  EXPECT_CALL(*device_, Stop());
  EXPECT_CALL(device_info_, DeleteInterface(kInterfaceIndex));
  EXPECT_CALL(*service_, SetState(Service::kStateIdle));
  driver_->Disconnect();
  EXPECT_FALSE(driver_->service_);
}

MATCHER_P(IsIPAddress, address, "") {
  IPAddress ip_address(IPAddress::kFamilyIPv4);
  EXPECT_TRUE(ip_address.SetAddressFromString(address));
  return ip_address.Equals(arg);
}

TEST_F(OpenVPNDriverTest, PinHostRoute) {
  IPConfig::Properties props;
  props.address_family = IPAddress::kFamilyIPv4;
  EXPECT_FALSE(driver_->PinHostRoute(props));

  props.gateway = kGateway1;
  EXPECT_FALSE(driver_->PinHostRoute(props));

  props.gateway.clear();
  props.trusted_ip = "xxx";
  EXPECT_FALSE(driver_->PinHostRoute(props));

  props.gateway = kGateway1;
  EXPECT_FALSE(driver_->PinHostRoute(props));

  props.trusted_ip = kNetwork1;
  EXPECT_CALL(manager_, GetDefaultService())
      .WillOnce(Return(reinterpret_cast<Service *>(NULL)));
  EXPECT_FALSE(driver_->PinHostRoute(props));

  scoped_refptr<MockService> mock_service(
      new NiceMock<MockService>(&control_,
                                &dispatcher_,
                                &metrics_,
                                &manager_));
  scoped_refptr<MockConnection> mock_connection(
      new StrictMock<MockConnection>(&device_info_));
  mock_service->set_mock_connection(mock_connection);
  EXPECT_CALL(manager_, GetDefaultService())
      .WillOnce(Return(mock_service));

  EXPECT_CALL(*mock_connection.get(), RequestHostRoute(IsIPAddress(kNetwork1)))
      .WillOnce(Return(false));
  EXPECT_FALSE(driver_->PinHostRoute(props));

  EXPECT_CALL(manager_, GetDefaultService())
      .WillOnce(Return(mock_service));
  EXPECT_CALL(*mock_connection.get(), RequestHostRoute(IsIPAddress(kNetwork1)))
      .WillOnce(Return(true));
  EXPECT_TRUE(driver_->PinHostRoute(props));
}

TEST_F(OpenVPNDriverTest, VerifyPaths) {
  // Ensure that the various path constants that the OpenVPN driver uses
  // actually exists in the build image.  Due to build dependencies, they
  // should already exist by the time we run unit tests.

  // The OpenVPNDriver path constants are absolute.  FilePath::Append
  // asserts that its argument is not an absolute path, so we need to
  // strip the leading separators.  There's nothing built into FilePath
  // to do so.
  string vpn_path(OpenVPNDriver::kOpenVPNPath);
  TrimString(vpn_path, FilePath::kSeparators, &vpn_path);
  EXPECT_TRUE(file_util::PathExists(FilePath(SYSROOT).Append(vpn_path)));

  string vpn_script(OpenVPNDriver::kOpenVPNScript);
  TrimString(vpn_script, FilePath::kSeparators, &vpn_script);
  EXPECT_TRUE(file_util::PathExists(FilePath(SYSROOT).Append(vpn_script)));
}

}  // namespace shill
