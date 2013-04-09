// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/traffic_monitor.h"

#include <base/bind.h>
#include <base/stringprintf.h>

#include "shill/device.h"
#include "shill/device_info.h"
#include "shill/event_dispatcher.h"
#include "shill/logging.h"
#include "shill/socket_info_reader.h"

using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

// static
const int TrafficMonitor::kMinimumFailedSamplesToTrigger = 2;
const int64 TrafficMonitor::kSamplingIntervalMilliseconds = 5000;

TrafficMonitor::TrafficMonitor(const DeviceRefPtr &device,
                               EventDispatcher *dispatcher)
    : device_(device),
      dispatcher_(dispatcher),
      socket_info_reader_(new SocketInfoReader),
      accummulated_failure_samples_(0) {
}

TrafficMonitor::~TrafficMonitor() {
  Stop();
}

void TrafficMonitor::Start() {
  SLOG(Link, 2) << __func__;
  Stop();

  sample_traffic_callback_.Reset(base::Bind(&TrafficMonitor::SampleTraffic,
                                            base::Unretained(this)));
  dispatcher_->PostDelayedTask(sample_traffic_callback_.callback(),
                               kSamplingIntervalMilliseconds);
}

void TrafficMonitor::Stop() {
  SLOG(Link, 2) << __func__;
  sample_traffic_callback_.Cancel();
  accummulated_failure_samples_ = 0;
}

void TrafficMonitor::BuildIPPortToTxQueueLength(
    const vector<SocketInfo> &socket_infos,
    IPPortToTxQueueLengthMap *tx_queue_lengths) {
  SLOG(Link, 3) << __func__;
  string device_ip_address = device_->ipconfig()->properties().address;
  vector<SocketInfo>::const_iterator it;
  for (it = socket_infos.begin(); it != socket_infos.end(); ++it) {
    SLOG(Link, 4) << "SocketInfo(IP=" << it->local_ip_address().ToString()
                  << ", TX=" << it->transmit_queue_value()
                  << ", State=" << it->connection_state()
                  << ", TimerState=" << it->timer_state();
    if (it->local_ip_address().ToString() != device_ip_address ||
        it->transmit_queue_value() == 0 ||
        it->connection_state() != SocketInfo::kConnectionStateEstablished ||
        (it->timer_state() != SocketInfo::kTimerStateRetransmitTimerPending &&
         it->timer_state() !=
            SocketInfo::kTimerStateZeroWindowProbeTimerPending)) {
      SLOG(Link, 4) << "Connection Filtered.";
      continue;
    }
    SLOG(Link, 3) << "Monitoring connection: TX=" << it->transmit_queue_value()
                  << " TimerState=" << it->timer_state();

    string local_ip_port =
        StringPrintf("%s:%d",
                     it->local_ip_address().ToString().c_str(),
                     it->local_port());
    (*tx_queue_lengths)[local_ip_port] = it->transmit_queue_value();
  }
}

void TrafficMonitor::SampleTraffic() {
  SLOG(Link, 2) << __func__;
  vector<SocketInfo> socket_infos;
  if (!socket_info_reader_->LoadTcpSocketInfo(&socket_infos) ||
      socket_infos.empty()) {
    SLOG(Link, 2) << __func__ << ": Empty socket info";
    accummulated_failure_samples_ = 0;
    return;
  }
  IPPortToTxQueueLengthMap curr_tx_queue_lengths;
  BuildIPPortToTxQueueLength(socket_infos, &curr_tx_queue_lengths);
  if (curr_tx_queue_lengths.empty()) {
    SLOG(Link, 2) << __func__ << ": No interesting socket info";
    accummulated_failure_samples_ = 0;
  } else {
    bool congested_tx_queues = true;
    IPPortToTxQueueLengthMap::iterator old_tx_queue_it;
    for (old_tx_queue_it = old_tx_queue_lengths_.begin();
         old_tx_queue_it != old_tx_queue_lengths_.end();
         ++old_tx_queue_it) {
      IPPortToTxQueueLengthMap::iterator curr_tx_queue_it =
          curr_tx_queue_lengths.find(old_tx_queue_it->first);
      if (curr_tx_queue_it == curr_tx_queue_lengths.end() ||
          curr_tx_queue_it->second < old_tx_queue_it->second) {
        congested_tx_queues = false;
        break;
      }
    }
    if (congested_tx_queues &&
        ++accummulated_failure_samples_ == kMinimumFailedSamplesToTrigger) {
      LOG(WARNING) << "Congested tx queues detected, out-of-credits?";
      outgoing_tcp_packets_not_routed_callback_.Run();
    }
  }
  old_tx_queue_lengths_ = curr_tx_queue_lengths;

  dispatcher_->PostDelayedTask(sample_traffic_callback_.callback(),
                               kSamplingIntervalMilliseconds);
}

}  // namespace shill
