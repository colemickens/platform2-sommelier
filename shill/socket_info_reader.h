// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SOCKET_INFO_READER_H_
#define SHILL_SOCKET_INFO_READER_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <gtest/gtest_prod.h>

#include "shill/socket_info.h"

namespace base {

class FilePath;

}  // namespace base

namespace shill {

class SocketInfoReader {
 public:
  SocketInfoReader();
  virtual ~SocketInfoReader();

  virtual bool LoadTcpSocketInfo(std::vector<SocketInfo> *info_list);

 private:
  FRIEND_TEST(SocketInfoReaderTest, LoadSocketInfo);
  FRIEND_TEST(SocketInfoReaderTest, ParseConnectionState);
  FRIEND_TEST(SocketInfoReaderTest, ParseIPAddress);
  FRIEND_TEST(SocketInfoReaderTest, ParseIPAddressAndPort);
  FRIEND_TEST(SocketInfoReaderTest, ParsePort);
  FRIEND_TEST(SocketInfoReaderTest, ParseSocketInfo);
  FRIEND_TEST(SocketInfoReaderTest, ParseTimerState);
  FRIEND_TEST(SocketInfoReaderTest, ParseTransimitAndReceiveQueueValues);

  bool LoadSocketInfo(const base::FilePath &info_file_path,
                      std::vector<SocketInfo> *info_list);
  bool ParseSocketInfo(const std::string &input, SocketInfo *socket_info);
  bool ParseIPAddressAndPort(
      const std::string &input, IPAddress *ip_address, uint16 *port);
  bool ParseIPAddress(const std::string &input, IPAddress *ip_address);
  bool ParsePort(const std::string &input, uint16 *port);
  bool ParseTransimitAndReceiveQueueValues(
      const std::string &input,
      uint64 *transmit_queue_value, uint64 *receive_queue_value);
  bool ParseConnectionState(const std::string &input,
                            SocketInfo::ConnectionState *connection_state);
  bool ParseTimerState(const std::string &input,
                       SocketInfo::TimerState *timer_state);

  DISALLOW_COPY_AND_ASSIGN(SocketInfoReader);
};

}  // namespace shill

#endif  // SHILL_SOCKET_INFO_READER_H_
