// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>
#include <linux/rtnetlink.h>

#include <vector>

#include <base/logging.h>
#include <base/stl_util.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/byte_string.h"
#include "shill/mock_control.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/routing_table.h"
#include "shill/routing_table_entry.h"
#include "shill/rtnl_handler.h"
#include "shill/rtnl_message.h"

using base::Callback;
using base::hash_map;
using std::queue;
using std::vector;
using testing::_;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace shill {

class TestEventDispatcher : public EventDispatcher {
 public:
  virtual IOHandler *CreateInputHandler(
      int /*fd*/,
      const Callback<void(InputData*)> &/*callback*/) {
    return NULL;
  }
};

class RoutingTableTest : public Test {
 public:
  RoutingTableTest() : routing_table_(new RoutingTable()) {}

  virtual void SetUp() {
    routing_table_->rtnl_handler_ = &rtnl_handler_;
    ON_CALL(rtnl_handler_, SendMessage(_)).WillByDefault(Return(true));
  }

  virtual void TearDown() {
    RTNLHandler::GetInstance()->Stop();
  }

  hash_map<int, vector<RoutingTableEntry> > *GetRoutingTables() {
    return &routing_table_->tables_;
  }

  queue<RoutingTable::Query> *GetQueries() {
    return &routing_table_->route_queries_;
  }

  void SendRouteEntry(RTNLMessage::Mode mode,
                      uint32 interface_index,
                      const RoutingTableEntry &entry);

  void SendRouteEntryWithSeqAndProto(RTNLMessage::Mode mode,
                                     uint32 interface_index,
                                     const RoutingTableEntry &entry,
                                     uint32 seq,
                                     unsigned char proto);

  void SendRouteMessage(const RTNLMessage &msg);

  bool SetSequenceForMessage(RTNLMessage *message) {
    message->set_seq(RoutingTableTest::kTestRequestSeq);
    return true;
  }

 protected:
  static const uint32 kTestDeviceIndex0;
  static const uint32 kTestDeviceIndex1;
  static const char kTestDeviceName0[];
  static const char kTestDeviceNetAddress4[];
  static const char kTestForeignNetAddress4[];
  static const char kTestForeignNetGateway4[];
  static const char kTestForeignNetAddress6[];
  static const char kTestForeignNetGateway6[];
  static const char kTestGatewayAddress4[];
  static const char kTestNetAddress0[];
  static const char kTestNetAddress1[];
  static const char kTestRemoteAddress4[];
  static const char kTestRemoteNetmask4[];
  static const char kTestRemoteNetwork4[];
  static const int kTestRemotePrefix4;
  static const uint32 kTestRequestSeq;
  static const int kTestRouteTag;

  RoutingTable *routing_table_;
  TestEventDispatcher dispatcher_;
  StrictMock<MockRTNLHandler> rtnl_handler_;
};

const uint32 RoutingTableTest::kTestDeviceIndex0 = 12345;
const uint32 RoutingTableTest::kTestDeviceIndex1 = 67890;
const char RoutingTableTest::kTestDeviceName0[] = "test-device0";
const char RoutingTableTest::kTestDeviceNetAddress4[] = "192.168.2.0/24";
const char RoutingTableTest::kTestForeignNetAddress4[] = "192.168.2.2";
const char RoutingTableTest::kTestForeignNetGateway4[] = "192.168.2.1";
const char RoutingTableTest::kTestForeignNetAddress6[] = "2000::/3";
const char RoutingTableTest::kTestForeignNetGateway6[] = "fe80:::::1";
const char RoutingTableTest::kTestGatewayAddress4[] = "192.168.2.254";
const char RoutingTableTest::kTestNetAddress0[] = "192.168.1.1";
const char RoutingTableTest::kTestNetAddress1[] = "192.168.1.2";
const char RoutingTableTest::kTestRemoteAddress4[] = "192.168.2.254";
const char RoutingTableTest::kTestRemoteNetmask4[] = "255.255.255.0";
const char RoutingTableTest::kTestRemoteNetwork4[] = "192.168.100.0";
const int RoutingTableTest::kTestRemotePrefix4 = 24;
const uint32 RoutingTableTest::kTestRequestSeq = 456;
const int RoutingTableTest::kTestRouteTag = 789;

MATCHER_P4(IsRoutingPacket, mode, index, entry, flags, "") {
  const RTNLMessage::RouteStatus &status = arg->route_status();

  uint32 oif;
  uint32 priority;

  return
      arg->type() == RTNLMessage::kTypeRoute &&
      arg->family() == entry.gateway.family() &&
      arg->flags() == (NLM_F_REQUEST | flags) &&
      status.table == RT_TABLE_MAIN &&
      status.protocol == RTPROT_BOOT &&
      status.scope == entry.scope &&
      status.type == RTN_UNICAST &&
      arg->HasAttribute(RTA_DST) &&
      IPAddress(arg->family(),
                arg->GetAttribute(RTA_DST),
                status.dst_prefix).Equals(entry.dst) &&
      !arg->HasAttribute(RTA_SRC) &&
      arg->HasAttribute(RTA_GATEWAY) &&
      IPAddress(arg->family(),
                arg->GetAttribute(RTA_GATEWAY)).Equals(entry.gateway) &&
      arg->GetAttribute(RTA_OIF).ConvertToCPUUInt32(&oif) &&
      oif == index &&
      arg->GetAttribute(RTA_PRIORITY).ConvertToCPUUInt32(&priority) &&
      priority == entry.metric;
}

void RoutingTableTest::SendRouteEntry(RTNLMessage::Mode mode,
                                      uint32 interface_index,
                                      const RoutingTableEntry &entry) {
  SendRouteEntryWithSeqAndProto(mode, interface_index, entry, 0, RTPROT_BOOT);
}

void RoutingTableTest::SendRouteEntryWithSeqAndProto(
    RTNLMessage::Mode mode,
    uint32 interface_index,
    const RoutingTableEntry &entry,
    uint32 seq,
    unsigned char proto) {
  RTNLMessage msg(
      RTNLMessage::kTypeRoute,
      mode,
      0,
      seq,
      0,
      0,
      entry.dst.family());

  msg.set_route_status(RTNLMessage::RouteStatus(
      entry.dst.prefix(),
      entry.src.prefix(),
      RT_TABLE_MAIN,
      proto,
      entry.scope,
      RTN_UNICAST,
      0));

  msg.SetAttribute(RTA_DST, entry.dst.address());
  if (!entry.src.IsDefault()) {
    msg.SetAttribute(RTA_SRC, entry.src.address());
  }
  if (!entry.gateway.IsDefault()) {
    msg.SetAttribute(RTA_GATEWAY, entry.gateway.address());
  }
  msg.SetAttribute(RTA_PRIORITY, ByteString::CreateFromCPUUInt32(entry.metric));
  msg.SetAttribute(RTA_OIF, ByteString::CreateFromCPUUInt32(interface_index));

  routing_table_->RouteMsgHandler(msg);
}

void RoutingTableTest::SendRouteMessage(const RTNLMessage &msg) {
  routing_table_->RouteMsgHandler(msg);
}

TEST_F(RoutingTableTest, Start) {
  EXPECT_CALL(rtnl_handler_, RequestDump(RTNLHandler::kRequestRoute));
  routing_table_->Start();
}

TEST_F(RoutingTableTest, RouteAddDelete) {
  // Expect the tables to be empty by default.
  EXPECT_EQ(0, GetRoutingTables()->size());

  IPAddress default_address(IPAddress::kFamilyIPv4);
  default_address.SetAddressToDefault();

  IPAddress gateway_address0(IPAddress::kFamilyIPv4);
  gateway_address0.SetAddressFromString(kTestNetAddress0);

  int metric = 10;

  RoutingTableEntry entry0(default_address,
                           default_address,
                           gateway_address0,
                           metric,
                           RT_SCOPE_UNIVERSE,
                           true);
  // Add a single entry.
  SendRouteEntry(RTNLMessage::kModeAdd,
                 kTestDeviceIndex0,
                 entry0);

  hash_map<int, vector<RoutingTableEntry> > *tables =
    GetRoutingTables();

  // We should have a single table, which should in turn have a single entry.
  EXPECT_EQ(1, tables->size());
  EXPECT_TRUE(ContainsKey(*tables, kTestDeviceIndex0));
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex0].size());

  RoutingTableEntry test_entry = (*tables)[kTestDeviceIndex0][0];
  EXPECT_TRUE(entry0.Equals(test_entry));

  // Add a second entry for a different interface.
  SendRouteEntry(RTNLMessage::kModeAdd,
                 kTestDeviceIndex1,
                 entry0);

  // We should have two tables, which should have a single entry each.
  EXPECT_EQ(2, tables->size());
  EXPECT_TRUE(ContainsKey(*tables, kTestDeviceIndex1));
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex0].size());
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex1].size());

  test_entry = (*tables)[kTestDeviceIndex1][0];
  EXPECT_TRUE(entry0.Equals(test_entry));

  IPAddress gateway_address1(IPAddress::kFamilyIPv4);
  gateway_address1.SetAddressFromString(kTestNetAddress1);

  RoutingTableEntry entry1(default_address,
                           default_address,
                           gateway_address1,
                           metric,
                           RT_SCOPE_UNIVERSE,
                           true);

  // Add a second gateway route to the second interface.
  SendRouteEntry(RTNLMessage::kModeAdd,
                 kTestDeviceIndex1,
                 entry1);

  // We should have two tables, one of which has a single entry, the other has
  // two.
  EXPECT_EQ(2, tables->size());
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex0].size());
  EXPECT_EQ(2, (*tables)[kTestDeviceIndex1].size());

  test_entry = (*tables)[kTestDeviceIndex1][1];
  EXPECT_TRUE(entry1.Equals(test_entry));

  // Remove the first gateway route from the second interface.
  SendRouteEntry(RTNLMessage::kModeDelete,
                 kTestDeviceIndex1,
                 entry0);

  // We should be back to having one route per table.
  EXPECT_EQ(2, tables->size());
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex0].size());
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex1].size());

  test_entry = (*tables)[kTestDeviceIndex1][0];
  EXPECT_TRUE(entry1.Equals(test_entry));

  // Send a duplicate of the second gateway route message, changing the metric.
  RoutingTableEntry entry2(entry1);
  entry2.metric++;
  SendRouteEntry(RTNLMessage::kModeAdd,
                 kTestDeviceIndex1,
                 entry2);

  // Routing table size shouldn't change, but the new metric should match.
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex1].size());
  test_entry = (*tables)[kTestDeviceIndex1][0];
  EXPECT_TRUE(entry2.Equals(test_entry));

  // Find a matching entry.
  EXPECT_TRUE(routing_table_->GetDefaultRoute(kTestDeviceIndex1,
                                              IPAddress::kFamilyIPv4,
                                              &test_entry));
  EXPECT_TRUE(entry2.Equals(test_entry));

  // Test that a search for a non-matching family fails.
  EXPECT_FALSE(routing_table_->GetDefaultRoute(kTestDeviceIndex1,
                                               IPAddress::kFamilyIPv6,
                                               &test_entry));

  // Remove last entry from an existing interface and test that we now fail.
  SendRouteEntry(RTNLMessage::kModeDelete,
                 kTestDeviceIndex1,
                 entry2);

  EXPECT_FALSE(routing_table_->GetDefaultRoute(kTestDeviceIndex1,
                                               IPAddress::kFamilyIPv4,
                                               &test_entry));

  // Add a route to a gatway address.
  IPAddress gateway_address(IPAddress::kFamilyIPv4);
  EXPECT_TRUE(gateway_address.SetAddressFromString(kTestNetAddress0));

  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingPacket(RTNLMessage::kModeAdd,
                                          kTestDeviceIndex1,
                                          entry0,
                                          NLM_F_CREATE | NLM_F_EXCL)));
  EXPECT_TRUE(routing_table_->SetDefaultRoute(kTestDeviceIndex1,
                                              gateway_address,
                                              metric));

  // The table entry should look much like entry0, except with
  // from_rtnl = false.
  RoutingTableEntry entry3(entry0);
  entry3.from_rtnl = false;
  EXPECT_TRUE(routing_table_->GetDefaultRoute(kTestDeviceIndex1,
                                              IPAddress::kFamilyIPv4,
                                              &test_entry));
  EXPECT_TRUE(entry3.Equals(test_entry));

  // Setting the same route on the interface with a different metric should
  // push the route with different flags to indicate we are replacing it,
  // then it should delete the old entry.
  RoutingTableEntry entry4(entry3);
  entry4.metric += 10;
  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingPacket(RTNLMessage::kModeAdd,
                                          kTestDeviceIndex1,
                                          entry4,
                                          NLM_F_CREATE | NLM_F_REPLACE)));
  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingPacket(RTNLMessage::kModeDelete,
                                   kTestDeviceIndex1,
                                   entry3,
                                   0)));
  EXPECT_TRUE(routing_table_->SetDefaultRoute(kTestDeviceIndex1,
                                              gateway_address,
                                              entry4.metric));

  // Test that removing the table causes the route to disappear.
  routing_table_->ResetTable(kTestDeviceIndex1);
  EXPECT_FALSE(ContainsKey(*tables, kTestDeviceIndex1));
  EXPECT_FALSE(routing_table_->GetDefaultRoute(kTestDeviceIndex1,
                                               IPAddress::kFamilyIPv4,
                                               &test_entry));
  EXPECT_EQ(1, GetRoutingTables()->size());

  // When we set the metric on an existing route, a new add and delete
  // operation should occur.
  RoutingTableEntry entry5(entry4);
  entry5.metric += 10;
  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingPacket(RTNLMessage::kModeAdd,
                                          kTestDeviceIndex0,
                                          entry5,
                                          NLM_F_CREATE | NLM_F_REPLACE)));
  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingPacket(RTNLMessage::kModeDelete,
                                          kTestDeviceIndex0,
                                          entry0,
                                          0)));
  routing_table_->SetDefaultMetric(kTestDeviceIndex0, entry5.metric);
  // Furthermore, the routing table should reflect the change in the metric
  // for the default route for the interface.
  RoutingTableEntry default_route;
  EXPECT_TRUE(routing_table_->GetDefaultRoute(kTestDeviceIndex0,
                                              IPAddress::kFamilyIPv4,
                                              &default_route));
  EXPECT_EQ(entry5.metric, default_route.metric);

  // Ask to flush table0.  We should see a delete message sent.
  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingPacket(RTNLMessage::kModeDelete,
                                          kTestDeviceIndex0,
                                          entry5,
                                          0)));
  routing_table_->FlushRoutes(kTestDeviceIndex0);
  EXPECT_EQ(0, (*tables)[kTestDeviceIndex0].size());

  // Test that the routing table size returns to zero.
  SendRouteEntry(RTNLMessage::kModeAdd,
                 kTestDeviceIndex0,
                 entry5);
  EXPECT_EQ(1, GetRoutingTables()->size());
  routing_table_->ResetTable(kTestDeviceIndex0);
  EXPECT_EQ(0, GetRoutingTables()->size());

  routing_table_->Stop();
}

TEST_F(RoutingTableTest, ConfigureRoutes) {
  MockControl control;
  IPConfigRefPtr ipconfig(new IPConfig(&control, kTestDeviceName0));
  IPConfig::Properties properties;
  properties.address_family = IPAddress::kFamilyIPv4;
  vector<IPConfig::Route> &routes = properties.routes;
  ipconfig->UpdateProperties(properties, true);

  const int kMetric = 10;
  EXPECT_TRUE(routing_table_->ConfigureRoutes(kTestDeviceIndex0,
                                             ipconfig,
                                             kMetric));

  IPConfig::Route route;
  route.host = kTestRemoteNetwork4;
  route.netmask = kTestRemoteNetmask4;
  route.gateway = kTestGatewayAddress4;
  routes.push_back(route);
  ipconfig->UpdateProperties(properties, true);

  IPAddress destination_address(IPAddress::kFamilyIPv4);
  IPAddress source_address(IPAddress::kFamilyIPv4);
  IPAddress gateway_address(IPAddress::kFamilyIPv4);
  ASSERT_TRUE(destination_address.SetAddressFromString(kTestRemoteNetwork4));
  destination_address.set_prefix(kTestRemotePrefix4);
  ASSERT_TRUE(gateway_address.SetAddressFromString(kTestGatewayAddress4));

  RoutingTableEntry entry(destination_address,
                          source_address,
                          gateway_address,
                          kMetric,
                          RT_SCOPE_UNIVERSE,
                          false);

  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingPacket(RTNLMessage::kModeAdd,
                                          kTestDeviceIndex0,
                                          entry,
                                          NLM_F_CREATE | NLM_F_EXCL)));
  EXPECT_TRUE(routing_table_->ConfigureRoutes(kTestDeviceIndex0,
                                              ipconfig,
                                              kMetric));

  routes.clear();
  route.gateway = "xxx";  // Invalid gateway entry -- should be skipped
  routes.push_back(route);
  route.host = "xxx";  // Invalid host entry -- should be skipped
  route.gateway = kTestGatewayAddress4;
  routes.push_back(route);
  route.host = kTestRemoteNetwork4;
  routes.push_back(route);
  ipconfig->UpdateProperties(properties, true);

  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingPacket(RTNLMessage::kModeAdd,
                                          kTestDeviceIndex0,
                                          entry,
                                          NLM_F_CREATE | NLM_F_EXCL)))
      .Times(1);
  EXPECT_FALSE(routing_table_->ConfigureRoutes(kTestDeviceIndex0,
                                               ipconfig,
                                               kMetric));
}

MATCHER_P2(IsRoutingQuery, destination, index, "") {
  const RTNLMessage::RouteStatus &status = arg->route_status();

  uint32 oif;

  return
      arg->type() == RTNLMessage::kTypeRoute &&
      arg->family() == destination.family() &&
      arg->flags() == NLM_F_REQUEST &&
      status.table == 0 &&
      status.protocol == 0 &&
      status.scope == 0 &&
      status.type == 0 &&
      arg->HasAttribute(RTA_DST) &&
      IPAddress(arg->family(),
                arg->GetAttribute(RTA_DST),
                status.dst_prefix).Equals(destination) &&
      !arg->HasAttribute(RTA_SRC) &&
      !arg->HasAttribute(RTA_GATEWAY) &&
      arg->GetAttribute(RTA_OIF).ConvertToCPUUInt32(&oif) &&
      oif == index &&
      !arg->HasAttribute(RTA_PRIORITY);

  return false;
}

TEST_F(RoutingTableTest, RequestHostRoute) {
  IPAddress destination_address(IPAddress::kFamilyIPv4);
  destination_address.SetAddressFromString(kTestRemoteAddress4);
  destination_address.set_prefix(24);

  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingQuery(destination_address,
                                         kTestDeviceIndex0)))
      .WillOnce(Invoke(this, &RoutingTableTest::SetSequenceForMessage));
  EXPECT_TRUE(routing_table_->RequestRouteToHost(destination_address,
                                                 kTestDeviceIndex0,
                                                 kTestRouteTag));

  IPAddress gateway_address(IPAddress::kFamilyIPv4);
  gateway_address.SetAddressFromString(kTestGatewayAddress4);

  IPAddress local_address(IPAddress::kFamilyIPv4);
  local_address.SetAddressFromString(kTestDeviceNetAddress4);

  const int kMetric = 10;
  RoutingTableEntry entry(destination_address,
                          local_address,
                          gateway_address,
                          kMetric,
                          RT_SCOPE_UNIVERSE,
                          true);

  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingPacket(RTNLMessage::kModeAdd,
                                          kTestDeviceIndex0,
                                          entry,
                                          NLM_F_CREATE | NLM_F_EXCL)));
  SendRouteEntryWithSeqAndProto(RTNLMessage::kModeAdd,
                                kTestDeviceIndex0,
                                entry,
                                kTestRequestSeq,
                                RTPROT_UNSPEC);

  hash_map<int, vector<RoutingTableEntry> > *tables =
    GetRoutingTables();

  // We should have a single table, which should in turn have a single entry.
  EXPECT_EQ(1, tables->size());
  EXPECT_TRUE(ContainsKey(*tables, kTestDeviceIndex0));
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex0].size());

  // This entry's tag should match the tag we requested.
  EXPECT_EQ(kTestRouteTag, (*tables)[kTestDeviceIndex0][0].tag);

  // Ask to flush routes with our tag.  We should see a delete message sent.
  EXPECT_CALL(rtnl_handler_,
              SendMessage(IsRoutingPacket(RTNLMessage::kModeDelete,
                                          kTestDeviceIndex0,
                                          entry,
                                          0)));

  routing_table_->FlushRoutesWithTag(kTestRouteTag);

  // After flushing routes for this tag, we should end up with no routes.
  EXPECT_EQ(0, (*tables)[kTestDeviceIndex0].size());
}

TEST_F(RoutingTableTest, RequestHostRouteBadSequence) {
  IPAddress destination_address(IPAddress::kFamilyIPv4);
  destination_address.SetAddressFromString(kTestRemoteAddress4);
  EXPECT_CALL(rtnl_handler_, SendMessage(_))
      .WillOnce(Invoke(this, &RoutingTableTest::SetSequenceForMessage));
  EXPECT_TRUE(routing_table_->RequestRouteToHost(destination_address,
                                                 kTestDeviceIndex0,
                                                 kTestRouteTag));
  EXPECT_FALSE(GetQueries()->empty());

  RoutingTableEntry entry(destination_address,
                          destination_address,
                          destination_address,
                          0,
                          RT_SCOPE_UNIVERSE,
                          true);

  // Try a sequence arriving before the one RoutingTable is looking for.
  // This should be a no-op.
  SendRouteEntryWithSeqAndProto(RTNLMessage::kModeAdd,
                                kTestDeviceIndex0,
                                entry,
                                kTestRequestSeq-1,
                                RTPROT_UNSPEC);
  EXPECT_FALSE(GetQueries()->empty());

  // Try a sequence arriving after the one RoutingTable is looking for.
  // This should cause the request to be purged.
  SendRouteEntryWithSeqAndProto(RTNLMessage::kModeAdd,
                                kTestDeviceIndex0,
                                entry,
                                kTestRequestSeq+1,
                                RTPROT_UNSPEC);
  EXPECT_TRUE(GetQueries()->empty());
}

}  // namespace shill
