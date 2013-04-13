// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/scan_session.h"

#include <algorithm>
#include <set>
#include <vector>

#include <base/stl_util.h>

#include "shill/logging.h"

using std::set;
using std::vector;

namespace shill {

ScanSession::ScanSession(
    const WiFiProvider::FrequencyCountList &connected_frequency_list,
    const vector<uint16_t> &unconnected_frequency_list)
    : frequency_list_(connected_frequency_list),
      total_connections_(0),
      total_connects_provided_(0),
      total_fraction_wanted_(0.0) {
  SLOG(WiFi, 7) << "Frequency connections vector:";
  sort(frequency_list_.begin(), frequency_list_.end(),
       &ScanSession::CompareFrequencyCount);
  set<uint16_t> seen_frequencies;
  for (const auto freq_conn : frequency_list_) {
    SLOG(WiFi, 7) << "    freq[" << freq_conn.frequency << "] = "
                  << freq_conn.connection_count;
    seen_frequencies.insert(freq_conn.frequency);
    total_connections_ += freq_conn.connection_count;
  }

  for (const auto freq : unconnected_frequency_list) {
    if (!ContainsKey(seen_frequencies, freq)) {
      frequency_list_.push_back(WiFiProvider::FrequencyCount(freq, 0));
    }
  }
}

ScanSession::~ScanSession() {}

bool ScanSession::HasMoreFrequencies() const {
  return !frequency_list_.empty();
}

vector<uint16_t> ScanSession::GetScanFrequencies(float fraction_wanted,
                                                 size_t min_frequencies,
                                                 size_t max_frequencies) {
  DCHECK(fraction_wanted >= 0);
  total_fraction_wanted_ += fraction_wanted;
  float total_connects_wanted = total_fraction_wanted_ * total_connections_;

  vector<uint16_t> frequencies;
  WiFiProvider::FrequencyCountList::iterator freq_connect =
      frequency_list_.begin();
  SLOG(WiFi, 7) << "Scanning for frequencies:";
  while (freq_connect != frequency_list_.end()) {
    if (frequencies.size() >= min_frequencies) {
      if (total_connects_provided_ >= total_connects_wanted)
        break;
      if (frequencies.size() >= max_frequencies)
        break;
    }
    uint16_t frequency = freq_connect->frequency;
    size_t connection_count = freq_connect->connection_count;
    total_connects_provided_ += connection_count;
    frequencies.push_back(frequency);
    SLOG(WiFi, 7) << "    freq[" << frequency << "] = " << connection_count;

    freq_connect = frequency_list_.erase(freq_connect);
  }
  return frequencies;
}

// static
bool ScanSession::CompareFrequencyCount(
    const WiFiProvider::FrequencyCount &first,
    const WiFiProvider::FrequencyCount &second) {
  return first.connection_count > second.connection_count;
}

}  // namespace shill

