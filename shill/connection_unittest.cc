// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <linux/rtnetlink.h>

#include <string>
#include <vector>

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/connection.h"
#include "shill/ipconfig.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_device_info.h"
#include "shill/mock_resolver.h"
#include "shill/mock_routing_table.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/routing_table_entry.h"

using std::string;
using std::vector;
using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {
const char kTestDeviceName0[] = "netdev0";
const int kTestDeviceInterfaceIndex0 = 123;
const char kTestDeviceName1[] = "netdev1";
const int kTestDeviceInterfaceIndex1 = 321;
const char kIPAddress0[] = "192.168.1.1";
const char kGatewayAddress0[] = "192.168.1.254";
const char kGatewayAddress1[] = "192.168.2.254";
const char kBroadcastAddress0[] = "192.168.1.255";
const char kNameServer0[] = "8.8.8.8";
const char kNameServer1[] = "8.8.9.9";
const int32 kPrefix0 = 24;
const int32 kPrefix1 = 31;
const char kSearchDomain0[] = "chromium.org";
const char kSearchDomain1[] = "google.com";
}  // namespace {}

class ConnectionTest : public Test {
 public:
  ConnectionTest()
      : device_info_(new StrictMock<MockDeviceInfo>(
            &control_,
            static_cast<EventDispatcher*>(NULL),
            static_cast<Metrics*>(NULL),
            static_cast<Manager*>(NULL))),
        connection_(new Connection(
            kTestDeviceInterfaceIndex0,
            kTestDeviceName0,
            Technology::kUnknown,
            device_info_.get())),
        ipconfig_(new IPConfig(&control_, kTestDeviceName0)),
        local_address_(IPAddress::kFamilyIPv4),
        broadcast_address_(IPAddress::kFamilyIPv4),
        gateway_address_(IPAddress::kFamilyIPv4),
        default_address_(IPAddress::kFamilyIPv4) {}

  virtual void SetUp() {
    ReplaceSingletons(connection_);
    properties_.address = kIPAddress0;
    properties_.subnet_prefix = kPrefix0;
    properties_.gateway = kGatewayAddress0;
    properties_.broadcast_address = kBroadcastAddress0;
    properties_.dns_servers.push_back(kNameServer0);
    properties_.dns_servers.push_back(kNameServer1);
    properties_.domain_search.push_back(kSearchDomain0);
    properties_.domain_search.push_back(kSearchDomain1);
    properties_.address_family = IPAddress::kFamilyIPv4;
    UpdateProperties();
    EXPECT_TRUE(local_address_.SetAddressFromString(kIPAddress0));
    EXPECT_TRUE(broadcast_address_.SetAddressFromString(kBroadcastAddress0));
    EXPECT_TRUE(gateway_address_.SetAddressFromString(kGatewayAddress0));
  }

  virtual void TearDown() {
    EXPECT_CALL(*device_info_, FlushAddresses(kTestDeviceInterfaceIndex0));
    EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceInterfaceIndex0));
    EXPECT_CALL(routing_table_, FlushRoutesWithTag(kTestDeviceInterfaceIndex0));
    connection_ = NULL;
  }

  void ReplaceSingletons(ConnectionRefPtr connection) {
    connection->resolver_ = &resolver_;
    connection->routing_table_ = &routing_table_;
    connection->rtnl_handler_ = &rtnl_handler_;
  }

  void UpdateProperties() {
    ipconfig_->UpdateProperties(properties_, true);
  }

  bool PinHostRoute(ConnectionRefPtr connection,
                    const IPConfig::Properties &properties) {
    return connection->PinHostRoute(properties);
  }

 protected:
  scoped_ptr<StrictMock<MockDeviceInfo> > device_info_;
  ConnectionRefPtr connection_;
  MockControl control_;
  IPConfigRefPtr ipconfig_;
  IPConfig::Properties properties_;
  IPAddress local_address_;
  IPAddress broadcast_address_;
  IPAddress gateway_address_;
  IPAddress default_address_;
  StrictMock<MockResolver> resolver_;
  StrictMock<MockRoutingTable> routing_table_;
  StrictMock<MockRTNLHandler> rtnl_handler_;
};

TEST_F(ConnectionTest, InitState) {
  EXPECT_EQ(kTestDeviceInterfaceIndex0, connection_->interface_index_);
  EXPECT_EQ(kTestDeviceName0, connection_->interface_name_);
  EXPECT_FALSE(connection_->is_default());
  EXPECT_FALSE(connection_->routing_request_count_);
}

MATCHER_P2(IsIPAddress, address, prefix, "") {
  IPAddress match_address(address);
  match_address.set_prefix(prefix);
  return match_address.Equals(arg);
}

TEST_F(ConnectionTest, AddConfig) {
  EXPECT_CALL(rtnl_handler_,
              AddInterfaceAddress(kTestDeviceInterfaceIndex0,
                                  IsIPAddress(local_address_, kPrefix0),
                                  IsIPAddress(broadcast_address_, 0),
                                  IsIPAddress(default_address_, 0)));
  EXPECT_CALL(routing_table_,
              SetDefaultRoute(kTestDeviceInterfaceIndex0,
                              IsIPAddress(gateway_address_, 0),
                              Connection::kNonDefaultMetricBase +
                              kTestDeviceInterfaceIndex0));
  EXPECT_CALL(routing_table_,
              ConfigureRoutes(kTestDeviceInterfaceIndex0,
                              ipconfig_,
                              Connection::kDefaultMetric));
  connection_->UpdateFromIPConfig(ipconfig_);

  EXPECT_CALL(routing_table_, SetDefaultMetric(kTestDeviceInterfaceIndex0,
                                               Connection::kDefaultMetric));
  EXPECT_CALL(resolver_, SetDNSFromLists(
      ipconfig_->properties().dns_servers,
      ipconfig_->properties().domain_search));

  scoped_refptr<MockDevice> device(new StrictMock<MockDevice>(
      &control_,
      reinterpret_cast<EventDispatcher *>(NULL),
      reinterpret_cast<Metrics *>(NULL),
      reinterpret_cast<Manager *>(NULL),
      kTestDeviceName0,
      string(),
      kTestDeviceInterfaceIndex0));
  EXPECT_CALL(*device_info_, GetDevice(kTestDeviceInterfaceIndex0))
      .WillOnce(Return(device));
  EXPECT_CALL(*device.get(), RequestPortalDetection())
      .WillOnce(Return(true));
  connection_->SetIsDefault(true);
  EXPECT_TRUE(connection_->is_default());

  EXPECT_CALL(routing_table_,
              SetDefaultMetric(kTestDeviceInterfaceIndex0,
                               Connection::kNonDefaultMetricBase +
                               kTestDeviceInterfaceIndex0));
  connection_->SetIsDefault(false);
  EXPECT_FALSE(connection_->is_default());
}

TEST_F(ConnectionTest, AddConfigWithPeer) {
  const string kPeerAddress("192.168.1.222");
  IPAddress peer_address(IPAddress::kFamilyIPv4);
  EXPECT_TRUE(peer_address.SetAddressFromString(kPeerAddress));
  properties_.peer_address = kPeerAddress;
  properties_.gateway = string();
  UpdateProperties();
  EXPECT_CALL(rtnl_handler_,
              AddInterfaceAddress(kTestDeviceInterfaceIndex0,
                                  IsIPAddress(local_address_, kPrefix0),
                                  IsIPAddress(broadcast_address_, 0),
                                  IsIPAddress(peer_address, 0)));
  EXPECT_CALL(routing_table_, SetDefaultRoute(_, _, _)).Times(0);
  EXPECT_CALL(routing_table_,
              ConfigureRoutes(kTestDeviceInterfaceIndex0,
                              ipconfig_,
                              Connection::kDefaultMetric));
  connection_->UpdateFromIPConfig(ipconfig_);
}

TEST_F(ConnectionTest, AddConfigWithBrokenNetmask) {
  // Assign a prefix that makes the gateway unreachable.
  properties_.subnet_prefix = kPrefix1;
  UpdateProperties();

  // Connection should override with a prefix which will allow the
  // gateway to be reachable.
  EXPECT_CALL(rtnl_handler_,
              AddInterfaceAddress(kTestDeviceInterfaceIndex0,
                                  IsIPAddress(local_address_, kPrefix0),
                                  IsIPAddress(broadcast_address_, 0),
                                  IsIPAddress(default_address_, 0)));
  EXPECT_CALL(routing_table_,
              SetDefaultRoute(kTestDeviceInterfaceIndex0,
                              IsIPAddress(gateway_address_, 0),
                              Connection::kNonDefaultMetricBase +
                              kTestDeviceInterfaceIndex0));
  EXPECT_CALL(routing_table_,
              ConfigureRoutes(kTestDeviceInterfaceIndex0,
                              ipconfig_,
                              Connection::kDefaultMetric));
  connection_->UpdateFromIPConfig(ipconfig_);

  // Assign a gateway address that violates the minimum plausible prefix
  // the Connection can assign.
  properties_.gateway = kGatewayAddress1;
  UpdateProperties();

  // Connection cannot override this prefix, so it will revert to the
  // configured prefix, expecting the default route to fail.
  EXPECT_CALL(rtnl_handler_,
              AddInterfaceAddress(kTestDeviceInterfaceIndex0,
                                  IsIPAddress(local_address_, kPrefix1),
                                  IsIPAddress(broadcast_address_, 0),
                                  IsIPAddress(default_address_, 0)));
  EXPECT_CALL(routing_table_,
              SetDefaultRoute(kTestDeviceInterfaceIndex0, _, _));
  EXPECT_CALL(routing_table_,
              ConfigureRoutes(kTestDeviceInterfaceIndex0, _, _));
  connection_->UpdateFromIPConfig(ipconfig_);
}

TEST_F(ConnectionTest, AddConfigReverse) {
  EXPECT_CALL(routing_table_, SetDefaultMetric(kTestDeviceInterfaceIndex0,
                                               Connection::kDefaultMetric));
  vector<string> empty_list;
  EXPECT_CALL(resolver_, SetDNSFromLists(empty_list, empty_list));
  scoped_refptr<MockDevice> device(new StrictMock<MockDevice>(
      &control_,
      reinterpret_cast<EventDispatcher *>(NULL),
      reinterpret_cast<Metrics *>(NULL),
      reinterpret_cast<Manager *>(NULL),
      kTestDeviceName0,
      string(),
      kTestDeviceInterfaceIndex0));
  EXPECT_CALL(*device_info_, GetDevice(kTestDeviceInterfaceIndex0))
      .WillOnce(Return(device));
  EXPECT_CALL(*device.get(), RequestPortalDetection())
      .WillOnce(Return(true));
  connection_->SetIsDefault(true);

  EXPECT_CALL(rtnl_handler_,
              AddInterfaceAddress(kTestDeviceInterfaceIndex0,
                                  IsIPAddress(local_address_, kPrefix0),
                                  IsIPAddress(broadcast_address_, 0),
                                  IsIPAddress(default_address_, 0)));
  EXPECT_CALL(routing_table_, SetDefaultRoute(kTestDeviceInterfaceIndex0,
                                              IsIPAddress(gateway_address_, 0),
                                              Connection::kDefaultMetric));
  EXPECT_CALL(routing_table_,
              ConfigureRoutes(kTestDeviceInterfaceIndex0,
                              ipconfig_,
                              Connection::kDefaultMetric));
  EXPECT_CALL(resolver_, SetDNSFromIPConfig(ipconfig_));

  connection_->UpdateFromIPConfig(ipconfig_);
}

TEST_F(ConnectionTest, RouteRequest) {
  ConnectionRefPtr connection(new Connection(kTestDeviceInterfaceIndex0,
                                             kTestDeviceName0,
                                             Technology::kUnknown,
                                             device_info_.get()));
  ReplaceSingletons(connection);
  scoped_refptr<MockDevice> device(new StrictMock<MockDevice>(
      &control_,
      reinterpret_cast<EventDispatcher *>(NULL),
      reinterpret_cast<Metrics *>(NULL),
      reinterpret_cast<Manager *>(NULL),
      kTestDeviceName0,
      string(),
      kTestDeviceInterfaceIndex0));
  EXPECT_CALL(*device_info_, GetDevice(kTestDeviceInterfaceIndex0))
      .WillRepeatedly(Return(device));
  EXPECT_CALL(*device.get(), DisableReversePathFilter()).Times(1);
  connection->RequestRouting();
  connection->RequestRouting();

  // The first release should only decrement the reference counter.
  connection->ReleaseRouting();

  // Another release will re-enable reverse-path filter.
  EXPECT_CALL(*device.get(), EnableReversePathFilter());
  EXPECT_CALL(routing_table_, FlushCache());
  connection->ReleaseRouting();

  // The destructor will remove the routes and addresses.
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceInterfaceIndex0));
  EXPECT_CALL(routing_table_, FlushRoutesWithTag(kTestDeviceInterfaceIndex0));
  EXPECT_CALL(*device_info_.get(),
              FlushAddresses(kTestDeviceInterfaceIndex0));
}

TEST_F(ConnectionTest, Destructor) {
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceInterfaceIndex1));
  EXPECT_CALL(routing_table_, FlushRoutesWithTag(kTestDeviceInterfaceIndex1));
  EXPECT_CALL(*device_info_, FlushAddresses(kTestDeviceInterfaceIndex1));
  {
    ConnectionRefPtr connection(new Connection(kTestDeviceInterfaceIndex1,
                                               kTestDeviceName1,
                                               Technology::kUnknown,
                                               device_info_.get()));
    connection->resolver_ = &resolver_;
    connection->routing_table_ = &routing_table_;
    connection->rtnl_handler_ = &rtnl_handler_;
  }
}

TEST_F(ConnectionTest, RequestHostRoute) {
  ConnectionRefPtr connection(new Connection(kTestDeviceInterfaceIndex0,
                                             kTestDeviceName0,
                                             Technology::kUnknown,
                                             device_info_.get()));
  ReplaceSingletons(connection);
  IPAddress address(IPAddress::kFamilyIPv4);
  ASSERT_TRUE(address.SetAddressFromString(kIPAddress0));
  size_t prefix_len = address.GetLength() * 8;
  EXPECT_CALL(routing_table_, RequestRouteToHost(
      IsIPAddress(address, prefix_len), -1, kTestDeviceInterfaceIndex0))
      .WillOnce(Return(true));
  EXPECT_TRUE(connection->RequestHostRoute(address));

  // The destructor will remove the routes and addresses.
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceInterfaceIndex0));
  EXPECT_CALL(routing_table_, FlushRoutesWithTag(kTestDeviceInterfaceIndex0));
  EXPECT_CALL(*device_info_.get(),
              FlushAddresses(kTestDeviceInterfaceIndex0));
}

TEST_F(ConnectionTest, PinHostRoute) {
  static const char kGateway[] = "10.242.2.13";
  static const char kNetwork[] = "10.242.2.1";

  ConnectionRefPtr connection(new Connection(kTestDeviceInterfaceIndex0,
                                             kTestDeviceName0,
                                             Technology::kUnknown,
                                             device_info_.get()));
  ReplaceSingletons(connection);

  IPConfig::Properties props;
  props.address_family = IPAddress::kFamilyIPv4;
  EXPECT_FALSE(PinHostRoute(connection, props));

  props.gateway = kGateway;
  EXPECT_FALSE(PinHostRoute(connection, props));

  props.gateway.clear();
  props.trusted_ip = "xxx";
  EXPECT_FALSE(PinHostRoute(connection, props));

  props.gateway = kGateway;
  EXPECT_FALSE(PinHostRoute(connection, props));

  props.trusted_ip = kNetwork;
  IPAddress address(IPAddress::kFamilyIPv4);
  ASSERT_TRUE(address.SetAddressFromString(kNetwork));
  size_t prefix_len = address.GetLength() * 8;
  EXPECT_CALL(routing_table_, RequestRouteToHost(
      IsIPAddress(address, prefix_len), -1, kTestDeviceInterfaceIndex0))
      .WillOnce(Return(false));
  EXPECT_FALSE(PinHostRoute(connection, props));

  EXPECT_CALL(routing_table_, RequestRouteToHost(
      IsIPAddress(address, prefix_len), -1, kTestDeviceInterfaceIndex0))
      .WillOnce(Return(true));
  EXPECT_TRUE(PinHostRoute(connection, props));

  // The destructor will remove the routes and addresses.
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceInterfaceIndex0));
  EXPECT_CALL(routing_table_, FlushRoutesWithTag(kTestDeviceInterfaceIndex0));
  EXPECT_CALL(*device_info_.get(),
              FlushAddresses(kTestDeviceInterfaceIndex0));
}

}  // namespace shill
