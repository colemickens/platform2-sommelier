// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/connection.h"
#include "shill/ip_address.h"
#include "shill/ipconfig.h"
#include "shill/mock_control.h"
#include "shill/mock_resolver.h"
#include "shill/mock_routing_table.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/routing_table_entry.h"

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
const char kNameServer0[] = "8.8.8.8";
const char kNameServer1[] = "8.8.9.9";
const char kSearchDomain0[] = "chromium.org";
const char kSearchDomain1[] = "google.com";
}  // namespace {}

class ConnectionTest : public Test {
 public:
  ConnectionTest()
      : connection_(
            new Connection(kTestDeviceInterfaceIndex0, kTestDeviceName0)),
        ipconfig_(new IPConfig(&control_, kTestDeviceName0)) {}

  virtual void SetUp() {
    connection_->resolver_ = &resolver_;
    connection_->routing_table_ = &routing_table_;
    connection_->rtnl_handler_ = &rtnl_handler_;

    IPConfig::Properties properties;
    properties.address = kIPAddress0;
    properties.gateway = kGatewayAddress0;
    properties.dns_servers.push_back(kNameServer0);
    properties.dns_servers.push_back(kNameServer1);
    properties.domain_search.push_back(kSearchDomain0);
    properties.domain_search.push_back(kSearchDomain1);
    properties.address_family = IPAddress::kAddressFamilyIPv4;
    ipconfig_->UpdateProperties(properties, true);
  }

 protected:
  ConnectionRefPtr connection_;
  MockControl control_;
  IPConfigRefPtr ipconfig_;
  StrictMock<MockResolver> resolver_;
  StrictMock<MockRoutingTable> routing_table_;
  StrictMock<MockRTNLHandler> rtnl_handler_;
};

TEST_F(ConnectionTest, InitState) {
  EXPECT_EQ(kTestDeviceInterfaceIndex0, connection_->interface_index_);
  EXPECT_EQ(kTestDeviceName0, connection_->interface_name_);
  EXPECT_FALSE(connection_->is_default());
}

TEST_F(ConnectionTest, AddConfig) {
  EXPECT_CALL(rtnl_handler_,
              AddInterfaceAddress(kTestDeviceInterfaceIndex0, _));
  EXPECT_CALL(routing_table_, SetDefaultRoute(kTestDeviceInterfaceIndex0,
                                             ipconfig_,
                                             Connection::kNonDefaultMetric));
  connection_->UpdateFromIPConfig(ipconfig_);

  EXPECT_CALL(routing_table_, SetDefaultMetric(kTestDeviceInterfaceIndex0,
                                               Connection::kDefaultMetric));
  EXPECT_CALL(resolver_, SetDNSFromLists(
      ipconfig_->properties().dns_servers,
      ipconfig_->properties().domain_search));

  connection_->SetDefault(true);
  EXPECT_TRUE(connection_->is_default());

  EXPECT_CALL(routing_table_, SetDefaultMetric(kTestDeviceInterfaceIndex0,
                                               Connection::kNonDefaultMetric));
  connection_->SetDefault(false);
  EXPECT_FALSE(connection_->is_default());
}

TEST_F(ConnectionTest, AddConfigReverse) {
  EXPECT_CALL(routing_table_, SetDefaultMetric(kTestDeviceInterfaceIndex0,
                                               Connection::kDefaultMetric));
  std::vector<std::string> empty_list;
  EXPECT_CALL(resolver_, SetDNSFromLists(empty_list, empty_list));
  connection_->SetDefault(true);

  EXPECT_CALL(rtnl_handler_,
              AddInterfaceAddress(kTestDeviceInterfaceIndex0, _));
  EXPECT_CALL(routing_table_, SetDefaultRoute(kTestDeviceInterfaceIndex0,
                                             ipconfig_,
                                             Connection::kDefaultMetric));
  EXPECT_CALL(resolver_, SetDNSFromIPConfig(ipconfig_));

  connection_->UpdateFromIPConfig(ipconfig_);
}

TEST_F(ConnectionTest, Destructor) {
  EXPECT_CALL(routing_table_, FlushRoutes(kTestDeviceInterfaceIndex1));
  {
    ConnectionRefPtr connection(new Connection(kTestDeviceInterfaceIndex1,
                                               kTestDeviceName1));
    connection->resolver_ = &resolver_;
    connection->routing_table_ = &routing_table_;
    connection->rtnl_handler_ = &rtnl_handler_;
  }
}

}  // namespace shill
