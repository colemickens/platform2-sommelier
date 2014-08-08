// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mac80211_monitor.h"

#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>

#include "shill/logging.h"

namespace shill {

using std::string;
using std::vector;

Mac80211Monitor::Mac80211Monitor() {
}

Mac80211Monitor::~Mac80211Monitor() {
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

}  // namespace shill
