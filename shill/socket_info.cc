// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/socket_info.h"

namespace shill {

SocketInfo::SocketInfo(ConnectionState connection_state,
                       const IPAddress& local_ip_address,
                       uint16_t local_port,
                       const IPAddress& remote_ip_address,
                       uint16_t remote_port,
                       uint64_t transmit_queue_value,
                       uint64_t receive_queue_value,
                       TimerState timer_state)
    : connection_state_(connection_state),
      local_ip_address_(local_ip_address),
      local_port_(local_port),
      remote_ip_address_(remote_ip_address),
      remote_port_(remote_port),
      transmit_queue_value_(transmit_queue_value),
      receive_queue_value_(receive_queue_value),
      timer_state_(timer_state) {
}

SocketInfo::SocketInfo()
    : connection_state_(kConnectionStateUnknown),
      local_ip_address_(IPAddress::kFamilyUnknown),
      local_port_(0),
      remote_ip_address_(IPAddress::kFamilyUnknown),
      remote_port_(0),
      transmit_queue_value_(0),
      receive_queue_value_(0),
      timer_state_(kTimerStateUnknown) {
}

SocketInfo::SocketInfo(const SocketInfo& socket_info)
    : connection_state_(socket_info.connection_state_),
      local_ip_address_(socket_info.local_ip_address_),
      local_port_(socket_info.local_port_),
      remote_ip_address_(socket_info.remote_ip_address_),
      remote_port_(socket_info.remote_port_),
      transmit_queue_value_(socket_info.transmit_queue_value_),
      receive_queue_value_(socket_info.receive_queue_value_),
      timer_state_(socket_info.timer_state_) {
}

SocketInfo::~SocketInfo() {}

SocketInfo& SocketInfo::operator=(const SocketInfo& socket_info) {
  connection_state_ = socket_info.connection_state_;
  local_ip_address_ = socket_info.local_ip_address_;
  local_port_ = socket_info.local_port_;
  remote_ip_address_ = socket_info.remote_ip_address_;
  remote_port_ = socket_info.remote_port_;
  transmit_queue_value_ = socket_info.transmit_queue_value_;
  receive_queue_value_ = socket_info.receive_queue_value_;
  timer_state_ = socket_info.timer_state_;

  return *this;
}

bool SocketInfo::IsSameSocketAs(const SocketInfo& socket_info) const {
  return (local_ip_address_.Equals(socket_info.local_ip_address_) &&
          local_port_ == socket_info.local_port_ &&
          remote_ip_address_.Equals(socket_info.remote_ip_address_) &&
          remote_port_ == socket_info.remote_port_);
}

}  // namespace shill
