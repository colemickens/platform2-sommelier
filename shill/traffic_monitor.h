// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_TRAFFIC_MONITOR_H_
#define SHILL_TRAFFIC_MONITOR_H_

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <gtest/gtest_prod.h>

#include "shill/refptr_types.h"

namespace shill {

class EventDispatcher;

// TrafficMonitor detects certain abnormal scenarios on a network interface
// notifies an observer of various scenarios via callbacks.
class TrafficMonitor {
 public:
  typedef base::Closure NoIncomingTrafficCallback;

  TrafficMonitor(const DeviceRefPtr &device,
                 EventDispatcher *dispatcher);
  virtual ~TrafficMonitor();

  // Starts traffic monitoring on the selected device.
  virtual void Start();

  // Stops traffic monitoring on the selected device.
  virtual void Stop();

  void set_no_incoming_traffic_callback(
      const NoIncomingTrafficCallback &callback) {
    no_incoming_traffic_callback_ = callback;
  }

 private:
  friend class TrafficMonitorTest;
  FRIEND_TEST(TrafficMonitorTest, StartAndStop);
  FRIEND_TEST(TrafficMonitorTest, SampleTraffic);

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

  // Receive byte count obtained in the last sample when SampleTraffic()
  // was invoked.
  uint64 last_receive_byte_count_;
  // Transmit byte count obtained in the last sample when SampleTraffic()
  // was invoked.
  uint64 last_transmit_byte_count_;
  // Number of samples where both the receive and transmit byte counts
  // remain unchanged.
  int no_traffic_count_;
  // Number of samples where the transmit byte count changes but the
  // receive byte count remains unchanged.
  int no_incoming_traffic_count_;
  // Callback to invoke when TrafficMonitor detects that there is
  // outgoing traffic but no incoming traffic.
  NoIncomingTrafficCallback no_incoming_traffic_callback_;
  // Set to true after |no_incoming_traffic_callback_| is invoked
  // and reset to false after |no_incoming_traffic_count_| is reset to 0.
  bool no_incoming_traffic_callback_invoked_;

  DISALLOW_COPY_AND_ASSIGN(TrafficMonitor);
};

}  // namespace shill

#endif  // SHILL_TRAFFIC_MONITOR_H_
