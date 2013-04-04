// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_TRAFFIC_MONITOR_H_
#define SHILL_TRAFFIC_MONITOR_H_

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <map>

#include "shill/refptr_types.h"
#include "shill/socket_info.h"

namespace shill {

class EventDispatcher;
class SocketInfoReader;

// TrafficMonitor detects certain abnormal scenarios on a network interface
// and notifies an observer of various scenarios via callbacks.
class TrafficMonitor {
 public:
  typedef base::Closure TcpOutTrafficNotRoutedCallback;

  TrafficMonitor(const DeviceRefPtr &device, EventDispatcher *dispatcher);
  virtual ~TrafficMonitor();

  // Starts traffic monitoring on the selected device.
  virtual void Start();

  // Stops traffic monitoring on the selected device.
  virtual void Stop();

  // Sets the callback to invoke, if the traffic monitor detects that too many
  // packets are failing to get transmitted over a TCP connection.
  void set_tcp_out_traffic_not_routed_callback(
      const TcpOutTrafficNotRoutedCallback &callback) {
    outgoing_tcp_packets_not_routed_callback_ = callback;
  }

 private:
  friend class TrafficMonitorTest;
  FRIEND_TEST(TrafficMonitorTest,
      BuildIPPortToTxQueueLengthInvalidConnectionState);
  FRIEND_TEST(TrafficMonitorTest, BuildIPPortToTxQueueLengthInvalidDevice);
  FRIEND_TEST(TrafficMonitorTest, BuildIPPortToTxQueueLengthInvalidTimerState);
  FRIEND_TEST(TrafficMonitorTest, BuildIPPortToTxQueueLengthMultipleEntries);
  FRIEND_TEST(TrafficMonitorTest, BuildIPPortToTxQueueLengthValid);
  FRIEND_TEST(TrafficMonitorTest, BuildIPPortToTxQueueLengthZero);
  FRIEND_TEST(TrafficMonitorTest,
      SampleTrafficStuckTxQueueIncreasingQueueLength);
  FRIEND_TEST(TrafficMonitorTest, SampleTrafficStuckTxQueueSameQueueLength);
  FRIEND_TEST(TrafficMonitorTest,
      SampleTrafficStuckTxQueueVariousQueueLengths);
  FRIEND_TEST(TrafficMonitorTest, SampleTrafficUnstuckTxQueueNoConnection);
  FRIEND_TEST(TrafficMonitorTest, SampleTrafficUnstuckTxQueueStateChanged);
  FRIEND_TEST(TrafficMonitorTest, SampleTrafficUnstuckTxQueueZeroQueueLength);
  FRIEND_TEST(TrafficMonitorTest, StartAndStop);

  typedef std::map<std::string, uint64> IPPortToTxQueueLengthMap;

  // The minimum number of samples that indicate an abnormal scenario
  // required to trigger the callback.
  static const int kMinimumFailedSamplesToTrigger;
  // The frequency at which to sample the TCP connections.
  static const int64 kSamplingIntervalMilliseconds;

  // Builds map of IP address/port to tx queue lengths from socket info vector.
  // Skips sockets not on device, tx queue length is 0, connection state is not
  // established or does not have a pending retransmit timer.
  void BuildIPPortToTxQueueLength(
      const std::vector<SocketInfo> &socket_infos,
      IPPortToTxQueueLengthMap *tx_queue_length);

  // Samples traffic (e.g. receive and transmit byte counts) on the
  // selected device and invokes appropriate callbacks when certain
  // abnormal scenarios are detected.
  void SampleTraffic();

  // The device on which to perform traffic monitoring.
  DeviceRefPtr device_;

  // Dispatcher on which to create delayed tasks.
  EventDispatcher *dispatcher_;

  // Callback to invoke when TrafficMonitor needs to sample traffic
  // of the network interface.
  base::CancelableClosure sample_traffic_callback_;

  // Callback to invoke when we detect that the send queue has been increasing
  // on an ESTABLISHED TCP connection through the network interface.
  TcpOutTrafficNotRoutedCallback outgoing_tcp_packets_not_routed_callback_;

  // Reads and parses socket information from the system.
  scoped_ptr<SocketInfoReader> socket_info_reader_;

  // Number of consecutive failure cases sampled.
  int accummulated_failure_samples_;

  // Map of tx queue lengths from previous sampling pass.
  IPPortToTxQueueLengthMap old_tx_queue_lengths_;

  DISALLOW_COPY_AND_ASSIGN(TrafficMonitor);
};

}  // namespace shill

#endif  // SHILL_TRAFFIC_MONITOR_H_
