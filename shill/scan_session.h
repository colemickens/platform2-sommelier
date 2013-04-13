// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SCAN_SESSION_H_
#define SHILL_SCAN_SESSION_H_

#include <vector>

#include <base/basictypes.h>

#include "shill/wifi_provider.h"

namespace shill {

// Contains the state of a progressive wifi scan (for example, a list of the
// requested frequencies and an indication of which of those still need to be
// scanned).  A wifi scan using ScanSession can transpire across multiple
// requests, each one encompassing a different set of frequencies.
//
// Use this as follows (note, this is shown as synchronous code for clarity
// but it really should be implemented as asynchronous code):
//
// ScanSession scan_session(frequencies_seen_ever, all_scan_frequencies_);
// while (scan_session.HasMoreFrequencies()) {
//   scan_session.InitiateScan(scan_session.GetScanFrequencies(
//      kScanFraction, kMinScanFrequencies, kMaxScanFrequencies));
//  // Wait for scan results.
// }
//
// The sequence for a single scan is:
//
//   +-------------+                                                +--------+
//   | ScanSession |                                                | Kernel |
//   +---+---------+                                                +-----+--+
//       |--- NL80211_CMD_TRIGGER_SCAN ---------------------------------->|
//       |<-- NL80211_CMD_TRIGGER_SCAN (broadcast) -----------------------|
//       |<-- NL80211_CMD_NEW_SCAN_RESULTS (broadcast) -------------------|
//       |--- NL80211_CMD_GET_SCAN -------------------------------------->|
//       |<-- NL80211_CMD_NEW_SCAN_RESULTS (reply, unicast, NLM_F_MULTI) -|
//       |<-- NL80211_CMD_NEW_SCAN_RESULTS (reply, unicast, NLM_F_MULTI) -|
//       |                               ...                              |
//       |<-- NL80211_CMD_NEW_SCAN_RESULTS (reply, unicast, NLM_F_MULTI) -|
//       |                                                                |
//
// ScanSession::OnNewScanBroadcast handles the broadcast
// NL80211_CMD_NEW_SCAN_RESULTS by issuing a NL80211_CMD_GET_SCAN and
// installing OnNewScanUnicast to handle the unicast
// NL80211_CMD_NEW_SCAN_RESULTS.

class ScanSession {
 public:
  // The frequency lists provide the frequencies that are returned by
  // |GetScanFrequencies|.  Frequencies are taken, first, from the connected
  // list (in order of the number of connections per frequency -- high before
  // low) and then from the unconnected list (in the order provided).
  ScanSession(const WiFiProvider::FrequencyCountList &connected_frequency_list,
              const std::vector<uint16_t> &unconnected_frequency_list);

  virtual ~ScanSession();

  // Returns true if |ScanSession| contains unscanned frequencies.
  bool HasMoreFrequencies() const;

  // Scanning WiFi frequencies for access points takes a long time (on the
  // order of 100ms per frequency and the kernel doesn't return the result until
  // the answers are ready for all the frequencies in the batch).  Given this,
  // scanning all frequencies in one batch takes a very long time.
  // |GetScanFrequencies| is intended to be called multiple times in order to
  // get a number of small batches of frequencies to scan.  Frequencies most
  // likely to yield a successful connection (based on previous connections)
  // are returned first followed by less-likely frequencies followed, finally,
  // by frequencies to which this machine hasn't connected before.
  //
  // |GetScanFrequencies| gets the next set of WiFi scan frequencies.  Returns
  // at least |min_frequencies| (unless fewer frequencies remain from previous
  // calls) and no more than |max_frequencies|.  Inside these constraints,
  // |GetScanFrequencies| tries to return at least the number of frequencies
  // required to reach the connection fraction |scan_fraction| out of the total
  // number of previous connections.  For example, the first call requesting
  // 33.3% will return the minimum number frequencies that add up to _at least_
  // the 33.3rd percentile of frequencies to which we've successfully connected
  // in the past.  The next call of 33.3% returns the minimum number of
  // frequencies required so that the total of the frequencies returned are _at
  // least_ the 66.6th percentile of the frequencies to which we've successfully
  // connected.
  //
  // For example, say we've connected to 3 frequencies before:
  //  freq a,count=10; freq b,count=5; freq c,count=5.
  //
  //  GetScanFrequencies(.50,2,10) // Returns a & b (|a| reaches %ile but |b| is
  //                               // required to meet the minimum).
  //  GetScanFrequencies(.51,2,10) // Returns c & 9 frequencies from the list
  //                               // of frequencies to which we've never
  //                               // connected.
  virtual std::vector<uint16_t> GetScanFrequencies(float scan_fraction,
                                                   size_t min_frequencies,
                                                   size_t max_frequencies);

 private:
  friend class ScanSessionTest;

  // Assists with sorting the |connected_frequency_list| passed to the
  // constructor.
  static bool CompareFrequencyCount(const WiFiProvider::FrequencyCount &first,
                                    const WiFiProvider::FrequencyCount &second);

  // List of frequencies, sorted by the number of successful connections for
  // each frequency.
  WiFiProvider::FrequencyCountList frequency_list_;
  size_t total_connections_;
  size_t total_connects_provided_;
  float total_fraction_wanted_;

  DISALLOW_COPY_AND_ASSIGN(ScanSession);
};

}  // namespace shill.

#endif  // SHILL_SCAN_SESSION_H_
