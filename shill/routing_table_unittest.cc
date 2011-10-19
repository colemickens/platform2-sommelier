// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/socket.h>
#include <linux/rtnetlink.h>

#include <base/logging.h>
#include <base/stl_util-inl.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/byte_string.h"
#include "shill/mock_control.h"
#include "shill/mock_sockets.h"
#include "shill/routing_table.h"
#include "shill/routing_table_entry.h"
#include "shill/rtnl_handler.h"
#include "shill/rtnl_message.h"

using testing::_;
using testing::Return;
using testing::Test;

namespace shill {

class TestEventDispatcher : public EventDispatcher {
 public:
  virtual IOHandler *CreateInputHandler(
      int /*fd*/,
      Callback1<InputData*>::Type */*callback*/) {
    return NULL;
  }
};

class RoutingTableTest : public Test {
 public:
  RoutingTableTest() : routing_table_(RoutingTable::GetInstance()) {}

  base::hash_map<int, std::vector<RoutingTableEntry> > *GetRoutingTables() {
    return &routing_table_->tables_;
  }

  void SendRouteMsg(RTNLMessage::Mode mode,
                    uint32 interface_index,
                    const RoutingTableEntry &entry);

  virtual void TearDown() {
    RTNLHandler::GetInstance()->Stop();
  }

 protected:
  static const int kTestSocket;
  static const uint32 kTestDeviceIndex0;
  static const uint32 kTestDeviceIndex1;
  static const char kTestDeviceName0[];
  static const char kTestNetAddress0[];
  static const char kTestNetAddress1[];

  void StartRTNLHandler();
  void StopRTNLHandler();

  MockSockets sockets_;
  RoutingTable *routing_table_;
  TestEventDispatcher dispatcher_;
};

const int RoutingTableTest::kTestSocket = 123;
const uint32 RoutingTableTest::kTestDeviceIndex0 = 12345;
const uint32 RoutingTableTest::kTestDeviceIndex1 = 67890;
const char RoutingTableTest::kTestDeviceName0[] = "test-device0";
const char RoutingTableTest::kTestNetAddress0[] = "192.168.1.1";
const char RoutingTableTest::kTestNetAddress1[] = "192.168.1.2";


MATCHER_P4(IsRoutingPacket, mode, index, entry, flags, "") {
  // NB: Matchers don't get passed multiple arguments, so we can
  //     get the address of a Send(), its length, but not both.
  //     We have to punt and assume the length is correct -- which
  //     should already be tested in rtnl_message_unittest.
  struct nlmsghdr hdr;
  memcpy(&hdr, arg, sizeof(hdr));

  RTNLMessage msg;
  if (!msg.Decode(ByteString(reinterpret_cast<const unsigned char *>(arg),
                             hdr.nlmsg_len))) {
    return false;
  }

  const RTNLMessage::RouteStatus &status = msg.route_status();

  uint32 oif;
  uint32 priority;

  return
    msg.type() == RTNLMessage::kTypeRoute &&
    msg.family() == entry.gateway.family() &&
    msg.flags() == (NLM_F_REQUEST | flags) &&
    status.table == RT_TABLE_MAIN &&
    status.protocol == RTPROT_BOOT &&
    status.scope == entry.scope &&
    status.type == RTN_UNICAST &&
    msg.HasAttribute(RTA_DST) &&
    IPAddress(msg.family(),
              msg.GetAttribute(RTA_DST)).Equals(entry.dst) &&
    !msg.HasAttribute(RTA_SRC) &&
    msg.HasAttribute(RTA_GATEWAY) &&
    IPAddress(msg.family(),
              msg.GetAttribute(RTA_GATEWAY)).Equals(entry.gateway) &&
    msg.GetAttribute(RTA_OIF).ConvertToCPUUInt32(&oif) &&
    oif == index &&
    msg.GetAttribute(RTA_PRIORITY).ConvertToCPUUInt32(&priority) &&
    priority == entry.metric;
}

void RoutingTableTest::SendRouteMsg(RTNLMessage::Mode mode,
                                    uint32 interface_index,
                                    const RoutingTableEntry &entry) {
  RTNLMessage msg(
      RTNLMessage::kTypeRoute,
      mode,
      0,
      0,
      0,
      0,
      entry.dst.family());

  msg.set_route_status(RTNLMessage::RouteStatus(
      entry.dst.prefix(),
      entry.src.prefix(),
      RT_TABLE_MAIN,
      RTPROT_BOOT,
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

  ByteString msgdata = msg.Encode();
  EXPECT_NE(0, msgdata.GetLength());

  InputData data(msgdata.GetData(), msgdata.GetLength());
  RTNLHandler::GetInstance()->ParseRTNL(&data);
}

void RoutingTableTest::StartRTNLHandler() {
  EXPECT_CALL(sockets_, Socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE))
      .WillOnce(Return(kTestSocket));
  EXPECT_CALL(sockets_, Bind(kTestSocket, _, sizeof(sockaddr_nl)))
      .WillOnce(Return(0));
  RTNLHandler::GetInstance()->Start(&dispatcher_, &sockets_);
}

void RoutingTableTest::StopRTNLHandler() {
  EXPECT_CALL(sockets_, Close(_)).WillOnce(Return(0));
  RTNLHandler::GetInstance()->Stop();
}

TEST_F(RoutingTableTest, RouteAddDelete) {
  EXPECT_CALL(sockets_, Send(kTestSocket, _, _, 0));
  StartRTNLHandler();
  routing_table_->Start();

  // Expect the tables to be empty by default
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
  // Add a single entry
  SendRouteMsg(RTNLMessage::kModeAdd,
               kTestDeviceIndex0,
               entry0);

  base::hash_map<int, std::vector<RoutingTableEntry> > *tables =
    GetRoutingTables();

  // Should have a single table, which should in turn have a single entry
  EXPECT_EQ(1, tables->size());
  EXPECT_TRUE(ContainsKey(*tables, kTestDeviceIndex0));
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex0].size());

  RoutingTableEntry test_entry = (*tables)[kTestDeviceIndex0][0];
  EXPECT_TRUE(entry0.Equals(test_entry));

  // Add a second entry for a different interface
  SendRouteMsg(RTNLMessage::kModeAdd,
               kTestDeviceIndex1,
               entry0);

  // Should have two tables, which should have a single entry each
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

  // Add a second gateway route to the second interface
  SendRouteMsg(RTNLMessage::kModeAdd,
               kTestDeviceIndex1,
               entry1);

  // Should have two tables, one of which has a single entry, the other has two
  EXPECT_EQ(2, tables->size());
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex0].size());
  EXPECT_EQ(2, (*tables)[kTestDeviceIndex1].size());

  test_entry = (*tables)[kTestDeviceIndex1][1];
  EXPECT_TRUE(entry1.Equals(test_entry));

  // Remove the first gateway route from the second interface
  SendRouteMsg(RTNLMessage::kModeDelete,
               kTestDeviceIndex1,
               entry0);

  // We should be back to having one route per table
  EXPECT_EQ(2, tables->size());
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex0].size());
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex1].size());

  test_entry = (*tables)[kTestDeviceIndex1][0];
  EXPECT_TRUE(entry1.Equals(test_entry));

  // Send a duplicate of the second gatway route message, changing the metric
  RoutingTableEntry entry2(entry1);
  entry2.metric++;
  SendRouteMsg(RTNLMessage::kModeAdd,
               kTestDeviceIndex1,
               entry2);

  // Routing table size shouldn't change, but the new metric should match
  EXPECT_EQ(1, (*tables)[kTestDeviceIndex1].size());
  test_entry = (*tables)[kTestDeviceIndex1][0];
  EXPECT_TRUE(entry2.Equals(test_entry));

  // Find a matching entry
  EXPECT_TRUE(routing_table_->GetDefaultRoute(kTestDeviceIndex1,
                                              IPAddress::kFamilyIPv4,
                                              &test_entry));
  EXPECT_TRUE(entry2.Equals(test_entry));

  // Test that a search for a non-matching family fails
  EXPECT_FALSE(routing_table_->GetDefaultRoute(kTestDeviceIndex1,
                                               IPAddress::kFamilyIPv6,
                                               &test_entry));

  // Remove last entry from an existing interface and test that we now fail
  SendRouteMsg(RTNLMessage::kModeDelete,
               kTestDeviceIndex1,
               entry2);

  EXPECT_FALSE(routing_table_->GetDefaultRoute(kTestDeviceIndex1,
                                               IPAddress::kFamilyIPv4,
                                               &test_entry));

  // Add a route from an IPConfig entry
  MockControl control;
  IPConfigRefPtr ipconfig(new IPConfig(&control, kTestDeviceName0));
  IPConfig::Properties properties;
  properties.address_family = IPAddress::kFamilyIPv4;
  properties.gateway = kTestNetAddress0;
  properties.address = kTestNetAddress1;
  ipconfig->UpdateProperties(properties, true);

  EXPECT_CALL(sockets_,
              Send(kTestSocket,
                   IsRoutingPacket(RTNLMessage::kModeAdd,
                                   kTestDeviceIndex1,
                                   entry0,
                                   NLM_F_CREATE | NLM_F_EXCL),
                   _,
                   0));
  EXPECT_TRUE(routing_table_->SetDefaultRoute(kTestDeviceIndex1,
                                              ipconfig,
                                              metric));

  // The table entry should look much like entry0, except with from_rtnl = false
  RoutingTableEntry entry3(entry0);
  entry3.from_rtnl = false;
  EXPECT_TRUE(routing_table_->GetDefaultRoute(kTestDeviceIndex1,
                                              IPAddress::kFamilyIPv4,
                                              &test_entry));
  EXPECT_TRUE(entry3.Equals(test_entry));

  // Setting the same route on the interface with a different metric should
  // push the route with different flags to indicate we are replacing it.
  RoutingTableEntry entry4(entry3);
  entry4.metric += 10;
  EXPECT_CALL(sockets_,
              Send(kTestSocket,
                   IsRoutingPacket(RTNLMessage::kModeAdd,
                                   kTestDeviceIndex1,
                                   entry4,
                                   NLM_F_CREATE | NLM_F_REPLACE),
                   _,
                   0));
  EXPECT_TRUE(routing_table_->SetDefaultRoute(kTestDeviceIndex1,
                                              ipconfig,
                                              entry4.metric));

  // Test that removing the table causes the route to disappear
  routing_table_->ResetTable(kTestDeviceIndex1);
  EXPECT_FALSE(ContainsKey(*tables, kTestDeviceIndex1));
  EXPECT_FALSE(routing_table_->GetDefaultRoute(kTestDeviceIndex1,
                                               IPAddress::kFamilyIPv4,
                                               &test_entry));
  EXPECT_EQ(1, GetRoutingTables()->size());

  // When we set the metric on an existing route, a new add message appears
  RoutingTableEntry entry5(entry4);
  entry5.metric += 10;
  EXPECT_CALL(sockets_,
              Send(kTestSocket,
                   IsRoutingPacket(RTNLMessage::kModeAdd,
                                   kTestDeviceIndex0,
                                   entry5,
                                   NLM_F_CREATE | NLM_F_REPLACE),
                   _,
                   0));
  routing_table_->SetDefaultMetric(kTestDeviceIndex0, entry5.metric);

  // Ask to flush table0.  We should see a delete message sent
  EXPECT_CALL(sockets_,
              Send(kTestSocket,
                   IsRoutingPacket(RTNLMessage::kModeDelete,
                                   kTestDeviceIndex0,
                                   entry0,
                                   0),
                   _,
                   0));
  routing_table_->FlushRoutes(kTestDeviceIndex0);

  // Test that the routing table size returns to zero
  EXPECT_EQ(1, GetRoutingTables()->size());
  routing_table_->ResetTable(kTestDeviceIndex0);
  EXPECT_EQ(0, GetRoutingTables()->size());

  routing_table_->Stop();
  StopRTNLHandler();
}

}  // namespace shill
