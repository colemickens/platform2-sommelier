// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/traffic_monitor.h"

#include <base/bind.h>

#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/event_dispatcher.h"

namespace shill {

namespace {

// The sampling interval should be greater than or equal to the interval
// at which DeviceInfo refreshes the receive and transmit byte counts.
const int64 kSamplingIntervalMilliseconds =
    DeviceInfo::kRequestLinkStatisticsIntervalMilliseconds;

// When there are |kNoTrafficThreshold| consecutive samples where both
// the receive and transmit byte counts remain unchanged, it resets
// the detection of "no incoming traffic" scenario.
const int kNoTrafficThreshold = 9;

// When there are |kNoIncomingTrafficThreshold| samples where the
// transmit byte count changes but the receive byte count remains
// unchanged, a "no incoming traffic" scenario is flagged.
const int kNoIncomingTrafficThreshold = 2;

}  // namespace

TrafficMonitor::TrafficMonitor(const DeviceRefPtr &device,
                               EventDispatcher *dispatcher)
    : device_(device),
      dispatcher_(dispatcher),
      last_receive_byte_count_(0),
      last_transmit_byte_count_(0),
      no_traffic_count_(0),
      no_incoming_traffic_count_(0),
      no_incoming_traffic_callback_invoked_(false) {
}

TrafficMonitor::~TrafficMonitor() {
  Stop();
}

void TrafficMonitor::Start() {
  Stop();

  last_receive_byte_count_ = device_->GetReceiveByteCount();
  last_transmit_byte_count_ = device_->GetTransmitByteCount();

  sample_traffic_callback_.Reset(base::Bind(&TrafficMonitor::SampleTraffic,
                                            base::Unretained(this)));
  dispatcher_->PostDelayedTask(sample_traffic_callback_.callback(),
                               kSamplingIntervalMilliseconds);
}

void TrafficMonitor::Stop() {
  sample_traffic_callback_.Cancel();
  last_receive_byte_count_ = 0;
  last_transmit_byte_count_ = 0;
  no_traffic_count_ = 0;
  no_incoming_traffic_count_ = 0;
  no_incoming_traffic_callback_invoked_ = false;
}

void TrafficMonitor::SampleTraffic() {
  uint64 current_receive_byte_count = device_->GetReceiveByteCount();
  uint64 current_transmit_byte_count = device_->GetTransmitByteCount();

  if (current_receive_byte_count != last_receive_byte_count_) {
    // There is incoming traffic. Reset the "no incoming traffic" detection.
    no_traffic_count_ = 0;
    no_incoming_traffic_count_ = 0;
    no_incoming_traffic_callback_invoked_ = false;
  } else if (current_transmit_byte_count != last_transmit_byte_count_) {
    // There is outgoing traffic but no incoming traffic.
    no_traffic_count_ = 0;
    no_incoming_traffic_count_++;
  } else if (++no_traffic_count_ == kNoTrafficThreshold) {
    // There is neither incoming nor outgoing traffic.
    //
    // If neither the receive nor transmit byte count has changed for
    // |kNoTrafficThreshold| consecutive sampling periods, reset the
    // "no incoming traffic" detection.
    no_traffic_count_ = 1;
    no_incoming_traffic_count_ = 0;
    no_incoming_traffic_callback_invoked_ = false;
  }

  // Only invoke the "no incoming traffic" callback once when we *start*
  // to see the "no incoming traffic" scenario.
  if (no_incoming_traffic_count_ == kNoIncomingTrafficThreshold) {
    if (!no_incoming_traffic_callback_invoked_ &&
        !no_incoming_traffic_callback_.is_null()) {
      no_incoming_traffic_callback_.Run();
      no_incoming_traffic_callback_invoked_ = true;
    }
  }

  last_receive_byte_count_ = current_receive_byte_count;
  last_transmit_byte_count_ = current_transmit_byte_count;

  dispatcher_->PostDelayedTask(sample_traffic_callback_.callback(),
                               kSamplingIntervalMilliseconds);
}

}  // namespace shill
