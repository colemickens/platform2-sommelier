// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/traffic_monitor.h"

#include <base/bind.h>
#include <base/strings/stringprintf.h>
#include <netinet/in.h>

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
const uint16 TrafficMonitor::kDnsPort = 53;
const int64 TrafficMonitor::kDnsTimedOutThresholdSeconds = 15;
const int TrafficMonitor::kMinimumFailedSamplesToTrigger = 2;
const int64 TrafficMonitor::kSamplingIntervalMilliseconds = 5000;

TrafficMonitor::TrafficMonitor(const DeviceRefPtr &device,
                               EventDispatcher *dispatcher)
    : device_(device),
      dispatcher_(dispatcher),
      socket_info_reader_(new SocketInfoReader),
      accummulated_congested_tx_queues_samples_(0),
      connection_info_reader_(new ConnectionInfoReader),
      accummulated_dns_failures_samples_(0) {
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
  ResetCongestedTxQueuesStats();
  ResetDnsFailingStats();
}

void TrafficMonitor::ResetCongestedTxQueuesStats() {
  accummulated_congested_tx_queues_samples_ = 0;
}

void TrafficMonitor::ResetCongestedTxQueuesStatsWithLogging() {
  SLOG(Link, 2) << __func__ << ": Tx-queues decongested";
  ResetCongestedTxQueuesStats();
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

bool TrafficMonitor::IsCongestedTxQueues() {
  SLOG(Link, 4) << __func__;
  vector<SocketInfo> socket_infos;
  if (!socket_info_reader_->LoadTcpSocketInfo(&socket_infos) ||
      socket_infos.empty()) {
    SLOG(Link, 3) << __func__ << ": Empty socket info";
    ResetCongestedTxQueuesStatsWithLogging();
    return false;
  }
  bool congested_tx_queues = true;
  IPPortToTxQueueLengthMap curr_tx_queue_lengths;
  BuildIPPortToTxQueueLength(socket_infos, &curr_tx_queue_lengths);
  if (curr_tx_queue_lengths.empty()) {
    SLOG(Link, 3) << __func__ << ": No interesting socket info";
    ResetCongestedTxQueuesStatsWithLogging();
  } else {
    IPPortToTxQueueLengthMap::iterator old_tx_queue_it;
    for (old_tx_queue_it = old_tx_queue_lengths_.begin();
         old_tx_queue_it != old_tx_queue_lengths_.end();
         ++old_tx_queue_it) {
      IPPortToTxQueueLengthMap::iterator curr_tx_queue_it =
          curr_tx_queue_lengths.find(old_tx_queue_it->first);
      if (curr_tx_queue_it == curr_tx_queue_lengths.end() ||
          curr_tx_queue_it->second < old_tx_queue_it->second) {
        congested_tx_queues = false;
        // TODO(armansito): If we had a false positive earlier, we may
        // want to correct it here by invoking a "connection back to normal
        // callback", so that the OutOfCredits property can be set to
        // false.
        break;
      }
    }
    if (congested_tx_queues) {
      ++accummulated_congested_tx_queues_samples_;
      SLOG(Link, 2) << __func__
                    << ": Congested tx-queues detected ("
                    << accummulated_congested_tx_queues_samples_ << ")";
    }
  }
  old_tx_queue_lengths_ = curr_tx_queue_lengths;

  return congested_tx_queues;
}

void TrafficMonitor::ResetDnsFailingStats() {
  accummulated_dns_failures_samples_ = 0;
}

void TrafficMonitor::ResetDnsFailingStatsWithLogging() {
  SLOG(Link, 2) << __func__ << ": DNS queries restored";
  ResetDnsFailingStats();
}

bool TrafficMonitor::IsDnsFailing() {
  SLOG(Link, 4) << __func__;
  vector<ConnectionInfo> connection_infos;
  if (!connection_info_reader_->LoadConnectionInfo(&connection_infos) ||
      connection_infos.empty()) {
    SLOG(Link, 3) << __func__ << ": Empty connection info";
  } else {
    // The time-to-expire counter is used to determine when a DNS request
    // has timed out.  This counter is the number of seconds remaining until
    // the entry is removed from the system IP connection tracker.  The
    // default time is 30 seconds.  This is too long of a wait.  Instead, we
    // want to time out at |kDnsTimedOutThresholdSeconds|.  Unfortunately,
    // we cannot simply look for entries less than
    // |kDnsTimedOutThresholdSeconds| because we will count the entry
    // multiple times once its time-to-expire is less than
    // |kDnsTimedOutThresholdSeconds|.  To ensure that we only count an
    // entry once, we look for entries in this time window between
    // |kDnsTimedOutThresholdSeconds| and |kDnsTimedOutLowerThresholdSeconds|.
    const int64 kDnsTimedOutLowerThresholdSeconds =
        kDnsTimedOutThresholdSeconds - kSamplingIntervalMilliseconds / 1000;
    string device_ip_address = device_->ipconfig()->properties().address;
    vector<ConnectionInfo>::const_iterator it;
    for (it = connection_infos.begin(); it != connection_infos.end(); ++it) {
      if (it->protocol() != IPPROTO_UDP ||
          it->time_to_expire_seconds() > kDnsTimedOutThresholdSeconds ||
          it->time_to_expire_seconds() <= kDnsTimedOutLowerThresholdSeconds ||
          !it->is_unreplied() ||
          it->original_source_ip_address().ToString() != device_ip_address ||
          it->original_destination_port() != kDnsPort)
        continue;

      ++accummulated_dns_failures_samples_;
      SLOG(Link, 2) << __func__
                    << ": DNS failures detected ("
                    << accummulated_dns_failures_samples_ << ")";
      return true;
    }
  }
  ResetDnsFailingStatsWithLogging();
  return false;
}

void TrafficMonitor::SampleTraffic() {
  SLOG(Link, 3) << __func__;

  if (IsCongestedTxQueues() &&
      accummulated_congested_tx_queues_samples_ ==
          kMinimumFailedSamplesToTrigger) {
    LOG(WARNING) << "Congested tx queues detected, out-of-credits?";
    outgoing_tcp_packets_not_routed_callback_.Run();
  } else if (IsDnsFailing() &&
             accummulated_dns_failures_samples_ ==
                 kMinimumFailedSamplesToTrigger) {
    LOG(WARNING) << "DNS queries failing, out-of-credits?";
    outgoing_tcp_packets_not_routed_callback_.Run();
  }

  dispatcher_->PostDelayedTask(sample_traffic_callback_.callback(),
                               kSamplingIntervalMilliseconds);
}

}  // namespace shill
