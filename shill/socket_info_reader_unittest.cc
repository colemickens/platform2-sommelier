// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/socket_info_reader.h"

#include <base/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/stringprintf.h>
#include <gtest/gtest.h>

using base::FilePath;
using base::ScopedTempDir;
using std::string;
using std::vector;

// TODO(benchan): Test IPv6 addresses.

namespace shill {

namespace {

const char kIPAddress_0_0_0_0[] = "0.0.0.0";
const char kIPAddress_127_0_0_1[] = "127.0.0.1";
const char kIPAddress_192_168_1_10[] = "192.168.1.10";
const char kIPAddress_255_255_255_255[] = "255.255.255.255";
const char *kSocketInfoLines[] = {
    "  sl  local_address rem_address   st tx_queue rx_queue tr tm->when "
    "retrnsmt   uid  timeout inode                                      ",
    "   0: 0100007F:0019 00000000:0000 0A 0000000A:00000005 00:00000000 "
    "00000000     0        0 36948 1 0000000000000000 100 0 0 10 -1     ",
    "   1: 0A01A8C0:0050 0100007F:03FC 01 00000000:00000000 00:00000000 "
    "00000000 65534        0 2787034 1 0000000000000000 100 0 0 10 -1   ",
};

}  // namespace

class SocketInfoReaderTest : public testing::Test {
 protected:
  IPAddress StringToIPv4Address(const string &address_string) {
    IPAddress ip_address(IPAddress::kFamilyIPv4);
    EXPECT_TRUE(ip_address.SetAddressFromString(address_string));
    return ip_address;
  }

  void CreateSocketInfoFile(const char **lines, size_t num_lines,
                            const FilePath &dir_path, FilePath *file_path) {
    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(dir_path, file_path));
    for (size_t i = 0; i < num_lines; ++i) {
      string line = lines[i];
      line += '\n';
      ASSERT_EQ(line.size(),
                file_util::AppendToFile(*file_path, line.data(), line.size()));
    }
  }

  void ExpectSocketInfoEqual(const SocketInfo &info1, const SocketInfo &info2) {
    EXPECT_EQ(info1.connection_state(), info2.connection_state());
    EXPECT_TRUE(info1.local_ip_address().Equals(info2.local_ip_address()));
    EXPECT_EQ(info1.local_port(), info2.local_port());
    EXPECT_TRUE(info1.remote_ip_address().Equals(info2.remote_ip_address()));
    EXPECT_EQ(info1.remote_port(), info2.remote_port());
    EXPECT_EQ(info1.transmit_queue_value(), info2.transmit_queue_value());
    EXPECT_EQ(info1.receive_queue_value(), info2.receive_queue_value());
    EXPECT_EQ(info1.timer_state(), info2.timer_state());
  }

  SocketInfoReader reader_;
};

TEST_F(SocketInfoReaderTest, LoadSocketInfo) {
  FilePath file_path("/non-existent-file");
  vector<SocketInfo> info_list;

  EXPECT_FALSE(reader_.LoadSocketInfo(file_path, &info_list));
  EXPECT_TRUE(info_list.empty());

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CreateSocketInfoFile(kSocketInfoLines, 1, temp_dir.path(), &file_path);
  EXPECT_TRUE(reader_.LoadSocketInfo(file_path, &info_list));
  EXPECT_TRUE(info_list.empty());

  CreateSocketInfoFile(kSocketInfoLines, arraysize(kSocketInfoLines),
                       temp_dir.path(), &file_path);
  EXPECT_TRUE(reader_.LoadSocketInfo(file_path, &info_list));
  EXPECT_EQ(arraysize(kSocketInfoLines) - 1, info_list.size());
  ExpectSocketInfoEqual(SocketInfo(SocketInfo::kConnectionStateListen,
                                   StringToIPv4Address(kIPAddress_127_0_0_1),
                                   25,
                                   StringToIPv4Address(kIPAddress_0_0_0_0),
                                   0,
                                   10,
                                   5,
                                   SocketInfo::kTimerStateNoTimerPending),
                        info_list[0]);
  ExpectSocketInfoEqual(SocketInfo(SocketInfo::kConnectionStateEstablished,
                                   StringToIPv4Address(kIPAddress_192_168_1_10),
                                   80,
                                   StringToIPv4Address(kIPAddress_127_0_0_1),
                                   1020,
                                   0,
                                   0,
                                   SocketInfo::kTimerStateNoTimerPending),
                        info_list[1]);
}

TEST_F(SocketInfoReaderTest, ParseSocketInfo) {
  SocketInfo info;

  EXPECT_FALSE(reader_.ParseSocketInfo("", &info));
  EXPECT_FALSE(reader_.ParseSocketInfo(kSocketInfoLines[0], &info));

  EXPECT_TRUE(reader_.ParseSocketInfo(kSocketInfoLines[1], &info));
  ExpectSocketInfoEqual(SocketInfo(SocketInfo::kConnectionStateListen,
                                   StringToIPv4Address(kIPAddress_127_0_0_1),
                                   25,
                                   StringToIPv4Address(kIPAddress_0_0_0_0),
                                   0,
                                   10,
                                   5,
                                   SocketInfo::kTimerStateNoTimerPending),
                        info);
}

TEST_F(SocketInfoReaderTest, ParseIPAddressAndPort) {
  IPAddress ip_address(IPAddress::kFamilyUnknown);
  uint16 port = 0;

  EXPECT_FALSE(reader_.ParseIPAddressAndPort("", &ip_address, &port));
  EXPECT_FALSE(reader_.ParseIPAddressAndPort("00000000", &ip_address, &port));
  EXPECT_FALSE(reader_.ParseIPAddressAndPort("00000000:", &ip_address, &port));
  EXPECT_FALSE(reader_.ParseIPAddressAndPort(":0000", &ip_address, &port));
  EXPECT_FALSE(reader_.ParseIPAddressAndPort("0000000Y:0000",
                                             &ip_address, &port));
  EXPECT_FALSE(reader_.ParseIPAddressAndPort("00000000:000Y", &ip_address,
                                             &port));

  EXPECT_TRUE(reader_.ParseIPAddressAndPort("0a01A8c0:0050",
                                            &ip_address, &port));
  EXPECT_TRUE(ip_address.Equals(StringToIPv4Address(kIPAddress_192_168_1_10)));
  EXPECT_EQ(80, port);
}

TEST_F(SocketInfoReaderTest, ParseIPAddress) {
  IPAddress ip_address(IPAddress::kFamilyUnknown);

  EXPECT_FALSE(reader_.ParseIPAddress("", &ip_address));
  EXPECT_FALSE(reader_.ParseIPAddress("0", &ip_address));
  EXPECT_FALSE(reader_.ParseIPAddress("00", &ip_address));
  EXPECT_FALSE(reader_.ParseIPAddress("0000000Y", &ip_address));

  EXPECT_TRUE(reader_.ParseIPAddress("00000000", &ip_address));
  EXPECT_TRUE(ip_address.Equals(StringToIPv4Address(kIPAddress_0_0_0_0)));

  EXPECT_TRUE(reader_.ParseIPAddress("0100007F", &ip_address));
  EXPECT_TRUE(ip_address.Equals(StringToIPv4Address(kIPAddress_127_0_0_1)));

  EXPECT_TRUE(reader_.ParseIPAddress("0a01A8c0", &ip_address));
  EXPECT_TRUE(ip_address.Equals(StringToIPv4Address(kIPAddress_192_168_1_10)));

  EXPECT_TRUE(reader_.ParseIPAddress("ffffffff", &ip_address));
  EXPECT_TRUE(ip_address.Equals(
      StringToIPv4Address(kIPAddress_255_255_255_255)));
}

TEST_F(SocketInfoReaderTest, ParsePort) {
  uint16 port = 0;

  EXPECT_FALSE(reader_.ParsePort("", &port));
  EXPECT_FALSE(reader_.ParsePort("0", &port));
  EXPECT_FALSE(reader_.ParsePort("00", &port));
  EXPECT_FALSE(reader_.ParsePort("000", &port));
  EXPECT_FALSE(reader_.ParsePort("000Y", &port));

  EXPECT_TRUE(reader_.ParsePort("0000", &port));
  EXPECT_EQ(0, port);

  EXPECT_TRUE(reader_.ParsePort("0050", &port));
  EXPECT_EQ(80, port);

  EXPECT_TRUE(reader_.ParsePort("abCD", &port));
  EXPECT_EQ(43981, port);

  EXPECT_TRUE(reader_.ParsePort("ffff", &port));
  EXPECT_EQ(65535, port);
}

TEST_F(SocketInfoReaderTest, ParseTransimitAndReceiveQueueValues) {
  uint64 transmit_queue_value = 0, receive_queue_value = 0;

  EXPECT_FALSE(reader_.ParseTransimitAndReceiveQueueValues(
      "", &transmit_queue_value, &receive_queue_value));
  EXPECT_FALSE(reader_.ParseTransimitAndReceiveQueueValues(
      "00000000", &transmit_queue_value, &receive_queue_value));
  EXPECT_FALSE(reader_.ParseTransimitAndReceiveQueueValues(
      "00000000:", &transmit_queue_value, &receive_queue_value));
  EXPECT_FALSE(reader_.ParseTransimitAndReceiveQueueValues(
      ":00000000", &transmit_queue_value, &receive_queue_value));
  EXPECT_FALSE(reader_.ParseTransimitAndReceiveQueueValues(
      "0000000Y:00000000", &transmit_queue_value, &receive_queue_value));
  EXPECT_FALSE(reader_.ParseTransimitAndReceiveQueueValues(
      "00000000:0000000Y", &transmit_queue_value, &receive_queue_value));

  EXPECT_TRUE(reader_.ParseTransimitAndReceiveQueueValues(
      "00000001:FFFFFFFF", &transmit_queue_value, &receive_queue_value));
  EXPECT_EQ(1, transmit_queue_value);
  EXPECT_EQ(0xffffffff, receive_queue_value);
}

TEST_F(SocketInfoReaderTest, ParseConnectionState) {
  SocketInfo::ConnectionState connection_state =
      SocketInfo::kConnectionStateUnknown;

  EXPECT_FALSE(reader_.ParseConnectionState("", &connection_state));
  EXPECT_FALSE(reader_.ParseConnectionState("0", &connection_state));
  EXPECT_FALSE(reader_.ParseConnectionState("X", &connection_state));

  EXPECT_TRUE(reader_.ParseConnectionState("00", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateUnknown, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("01", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateEstablished, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("02", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateSynSent, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("03", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateSynRecv, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("04", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateFinWait1, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("05", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateFinWait2, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("06", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateTimeWait, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("07", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateClose, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("08", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateCloseWait, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("09", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateLastAck, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("0A", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateListen, connection_state);
  EXPECT_TRUE(reader_.ParseConnectionState("0B", &connection_state));
  EXPECT_EQ(SocketInfo::kConnectionStateClosing, connection_state);

  for (int i = SocketInfo::kConnectionStateMax; i < 256; ++i) {
    EXPECT_TRUE(reader_.ParseConnectionState(
        base::StringPrintf("%02X", i), &connection_state));
    EXPECT_EQ(SocketInfo::kConnectionStateUnknown, connection_state);
  }
}

TEST_F(SocketInfoReaderTest, ParseTimerState) {
  SocketInfo::TimerState timer_state = SocketInfo::kTimerStateUnknown;

  EXPECT_FALSE(reader_.ParseTimerState("", &timer_state));
  EXPECT_FALSE(reader_.ParseTimerState("0", &timer_state));
  EXPECT_FALSE(reader_.ParseTimerState("X", &timer_state));
  EXPECT_FALSE(reader_.ParseTimerState("00", &timer_state));

  EXPECT_TRUE(reader_.ParseTimerState("00:00000000", &timer_state));
  EXPECT_EQ(SocketInfo::kTimerStateNoTimerPending, timer_state);
  EXPECT_TRUE(reader_.ParseTimerState("01:00000000", &timer_state));
  EXPECT_EQ(SocketInfo::kTimerStateRetransmitTimerPending, timer_state);
  EXPECT_TRUE(reader_.ParseTimerState("02:00000000", &timer_state));
  EXPECT_EQ(SocketInfo::kTimerStateAnotherTimerPending, timer_state);
  EXPECT_TRUE(reader_.ParseTimerState("03:00000000", &timer_state));
  EXPECT_EQ(SocketInfo::kTimerStateInTimeWaitState, timer_state);
  EXPECT_TRUE(reader_.ParseTimerState("04:00000000", &timer_state));
  EXPECT_EQ(SocketInfo::kTimerStateZeroWindowProbeTimerPending, timer_state);

  for (int i = SocketInfo::kTimerStateMax; i < 256; ++i) {
    EXPECT_TRUE(reader_.ParseTimerState(
        base::StringPrintf("%02X:00000000", i), &timer_state));
    EXPECT_EQ(SocketInfo::kTimerStateUnknown, timer_state);
  }
}

}  // namespace shill
