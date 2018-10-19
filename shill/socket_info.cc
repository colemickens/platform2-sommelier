// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/socket_info.h"

namespace shill {

SocketInfo::SocketInfo()
    : connection_state(kConnectionStateUnknown),
      local_ip_address(IPAddress::kFamilyUnknown),
      local_port(0),
      remote_ip_address(IPAddress::kFamilyUnknown),
      remote_port(0),
      transmit_queue_value(0),
      receive_queue_value(0),
      timer_state(kTimerStateUnknown) {}

SocketInfo::SocketInfo(ConnectionState connection_state,
                       const IPAddress& local_ip_address,
                       uint16_t local_port,
                       const IPAddress& remote_ip_address,
                       uint16_t remote_port,
                       uint64_t transmit_queue_value,
                       uint64_t receive_queue_value,
                       TimerState timer_state)
    : connection_state(connection_state),
      local_ip_address(local_ip_address),
      local_port(local_port),
      remote_ip_address(remote_ip_address),
      remote_port(remote_port),
      transmit_queue_value(transmit_queue_value),
      receive_queue_value(receive_queue_value),
      timer_state(timer_state) {}

SocketInfo::SocketInfo(const SocketInfo& socket_info) = default;

SocketInfo::~SocketInfo() = default;

SocketInfo& SocketInfo::operator=(const SocketInfo& socket_info) = default;

}  // namespace shill
