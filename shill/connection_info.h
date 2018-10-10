// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CONNECTION_INFO_H_
#define SHILL_CONNECTION_INFO_H_

#include <base/macros.h>

#include "shill/net/ip_address.h"

namespace shill {

class ConnectionInfo {
 public:
  ConnectionInfo();
  ConnectionInfo(int protocol,
                 int64_t time_to_expire_seconds,
                 bool is_unreplied,
                 IPAddress original_source_ip_address,
                 uint16_t original_source_port,
                 IPAddress original_destination_ip_address,
                 uint16_t original_destination_port,
                 IPAddress reply_source_ip_address,
                 uint16_t reply_source_port,
                 IPAddress reply_destination_ip_address,
                 uint16_t reply_destination_port);
  ConnectionInfo(const ConnectionInfo& info);
  ~ConnectionInfo();

  ConnectionInfo& operator=(const ConnectionInfo& info);

  int protocol;
  int64_t time_to_expire_seconds;
  bool is_unreplied;

  IPAddress original_source_ip_address;
  uint16_t original_source_port;
  IPAddress original_destination_ip_address;
  uint16_t original_destination_port;

  IPAddress reply_source_ip_address;
  uint16_t reply_source_port;
  IPAddress reply_destination_ip_address;
  uint16_t reply_destination_port;
};

}  // namespace shill

#endif  // SHILL_CONNECTION_INFO_H_
