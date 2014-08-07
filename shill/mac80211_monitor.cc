// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mac80211_monitor.h"

#include <algorithm>

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>

#include "shill/logging.h"
#include "shill/metrics.h"

namespace shill {

using std::string;
using std::vector;

// static
// At 17-25 bytes per queue, this accommodates 80 queues.
// ath9k has 4 queues, and WP2 has 16 queues.
const size_t Mac80211Monitor::kMaxQueueStateSizeBytes = 2048;

Mac80211Monitor::Mac80211Monitor(
    const string &link_name, size_t queue_length_limit, Metrics *metrics)
    : link_name_(link_name),
      queue_length_limit_(queue_length_limit),
      metrics_(metrics) {
  CHECK(metrics_);
}

Mac80211Monitor::~Mac80211Monitor() {
}

uint32_t Mac80211Monitor::CheckAreQueuesStuck(
    const vector<QueueState> &queue_states) {
  size_t max_stuck_queue_len = 0;
  uint32_t stuck_flags = 0;
  for (const auto &queue_state : queue_states) {
    if (queue_state.queue_length < queue_length_limit_) {
      SLOG(WiFi, 5) << __func__
                    << " skipping queue of length " << queue_state.queue_length
                    << " (threshold is " << queue_length_limit_ << ")";
      continue;
    }
    if (!queue_state.stop_flags) {
      SLOG(WiFi, 5) << __func__
                    << " skipping queue of length " << queue_state.queue_length
                    << " (not stopped)";
      continue;
    }
    stuck_flags = stuck_flags | queue_state.stop_flags;
    max_stuck_queue_len =
        std::max(max_stuck_queue_len, queue_state.queue_length);
  }

  if (max_stuck_queue_len >= queue_length_limit_) {
    LOG(WARNING) << "max queue length is " << max_stuck_queue_len;
  }

  if (stuck_flags) {
    for (unsigned int i=0; i < kStopReasonMax; ++i) {
      auto stop_reason = static_cast<QueueStopReason>(i);
      if (stuck_flags & GetFlagForReason(stop_reason)) {
        metrics_->SendEnumToUMA(Metrics::kMetricWifiStoppedTxQueueReason,
                                stop_reason,
                                kStopReasonMax);
      }
    }

    metrics_->SendToUMA(Metrics::kMetricWifiStoppedTxQueueLength,
                        max_stuck_queue_len,
                        Metrics::kMetricWifiStoppedTxQueueLengthMin,
                        Metrics::kMetricWifiStoppedTxQueueLengthMax,
                        Metrics::kMetricWifiStoppedTxQueueLengthNumBuckets);
  }

  return stuck_flags;
}

// example input:
// 01: 0x00000000/0
// ...
// 04: 0x00000000/0
//
// static
vector<Mac80211Monitor::QueueState>
Mac80211Monitor::ParseQueueState(const string &state_string) {
  vector<string> queue_state_strings;
  vector <QueueState> queue_states;
  base::SplitString(state_string, '\n', &queue_state_strings);

  if (queue_state_strings.empty()) {
    return queue_states;
  }

  // Trailing newline generates empty string as last element.
  // Discard that empty string if present.
  if (queue_state_strings.back().empty()) {
    queue_state_strings.pop_back();
  }

  for (const auto &queue_state : queue_state_strings) {
    // Example |queue_state|: "00: 0x00000000/10".
    vector<string> queuenum_and_state;
    base::SplitString(queue_state, ':', &queuenum_and_state);
    if (queuenum_and_state.size() != 2) {
      LOG(WARNING) << __func__ << ": parse error on " << queue_state;
      continue;
    }

    // Example |queuenum_and_state|: {"00", "0x00000000/10"}.
    vector<string> stopflags_and_length;
    base::SplitString(queuenum_and_state[1], '/', &stopflags_and_length);
    if (stopflags_and_length.size() != 2) {
      LOG(WARNING) << __func__ << ": parse error on " << queue_state;
      continue;
    }

    // Example |stopflags_and_length|: {"0x00000000", "10"}.
    size_t queue_number;
    uint32_t stop_flags;
    size_t queue_length;
    if (!base::StringToSizeT(queuenum_and_state[0], &queue_number)) {
      LOG(WARNING) << __func__ << ": parse error on " << queue_state;
      continue;
    }
    if (!base::HexStringToUInt(stopflags_and_length[0], &stop_flags)) {
      LOG(WARNING) << __func__ << ": parse error on " << queue_state;
      continue;
    }
    if (!base::StringToSizeT(stopflags_and_length[1], &queue_length)) {
      LOG(WARNING) << __func__ << ": parse error on " << queue_state;
      continue;
    }
    queue_states.emplace_back(queue_number, stop_flags, queue_length);
  }

  return queue_states;
}

// static
Mac80211Monitor::QueueStopFlag Mac80211Monitor::GetFlagForReason(
    QueueStopReason reason) {
  switch (reason) {
    case kStopReasonDriver:
      return kStopFlagDriver;
    case kStopReasonPowerSave:
      return kStopFlagPowerSave;
    case kStopReasonChannelSwitch:
      return kStopFlagChannelSwitch;
    case kStopReasonAggregation:
      return kStopFlagAggregation;
    case kStopReasonSuspend:
      return kStopFlagSuspend;
    case kStopReasonBufferAdd:
      return kStopFlagBufferAdd;
    case kStopReasonChannelTypeChange:
      return kStopFlagChannelTypeChange;
  }

  // The switch statement above is exhaustive (-Wswitch will complain
  // if it is not). But |reason| might be outside the defined range for
  // the enum, so we need this to keep the compiler happy.
  return kStopFlagInvalid;
}

}  // namespace shill
