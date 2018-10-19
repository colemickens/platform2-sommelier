// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SOCKET_INFO_H_
#define SHILL_SOCKET_INFO_H_

#include <stdint.h>

#include "shill/net/ip_address.h"

namespace shill {

struct SocketInfo {
  // These connection states (except kConnectionStateUnknown and
  // kConnectionStateMax) are equivalent to and should be kept in sync with
  // those defined in kernel/inlude/net/tcp_states.h
  enum ConnectionState {
    kConnectionStateUnknown = -1,
    kConnectionStateEstablished = 1,
    kConnectionStateSynSent,
    kConnectionStateSynRecv,
    kConnectionStateFinWait1,
    kConnectionStateFinWait2,
    kConnectionStateTimeWait,
    kConnectionStateClose,
    kConnectionStateCloseWait,
    kConnectionStateLastAck,
    kConnectionStateListen,
    kConnectionStateClosing,
    kConnectionStateMax,
  };

  // These timer states (except kTimerStateUnknown and kTimerStateMax) are
  // equivalent to and should be kept in sync with those specified in
  // kernel/Documentation/networking/proc_net_tcp.txt
  enum TimerState {
    kTimerStateUnknown = -1,
    kTimerStateNoTimerPending = 0,
    kTimerStateRetransmitTimerPending,
    kTimerStateAnotherTimerPending,
    kTimerStateInTimeWaitState,
    kTimerStateZeroWindowProbeTimerPending,
    kTimerStateMax,
  };

  SocketInfo();
  SocketInfo(ConnectionState connection_state,
             const IPAddress& local_ip_address,
             uint16_t local_port,
             const IPAddress& remote_ip_address,
             uint16_t remote_port,
             uint64_t transmit_queue_value,
             uint64_t receive_queue_value,
             TimerState timer_state);
  SocketInfo(const SocketInfo& socket_info);
  ~SocketInfo();

  SocketInfo& operator=(const SocketInfo& socket_info);

  ConnectionState connection_state;
  IPAddress local_ip_address;
  uint16_t local_port;
  IPAddress remote_ip_address;
  uint16_t remote_port;
  uint64_t transmit_queue_value;
  uint64_t receive_queue_value;
  TimerState timer_state;
};

}  // namespace shill

#endif  // SHILL_SOCKET_INFO_H_
