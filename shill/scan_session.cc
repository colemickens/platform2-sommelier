// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/scan_session.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/memory/weak_ptr.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>

#include "shill/event_dispatcher.h"
#include "shill/logging.h"
#include "shill/netlink_manager.h"
#include "shill/nl80211_attribute.h"
#include "shill/nl80211_message.h"

using base::Bind;
using base::StringPrintf;
using std::set;
using std::string;
using std::vector;

namespace shill {

const float ScanSession::kAllFrequencies = 1.1;
const uint64_t ScanSession::kScanRetryDelayMilliseconds = 100;  // Arbitrary.
const size_t ScanSession::kScanRetryCount = 10;

ScanSession::ScanSession(
    NetlinkManager *netlink_manager,
    EventDispatcher *dispatcher,
    const WiFiProvider::FrequencyCountList &previous_frequencies,
    const set<uint16_t> &available_frequencies,
    uint32_t ifindex,
    const FractionList &fractions,
    size_t min_frequencies,
    size_t max_frequencies,
    OnScanFailed on_scan_failed)
    : weak_ptr_factory_(this),
      netlink_manager_(netlink_manager),
      dispatcher_(dispatcher),
      frequency_list_(previous_frequencies),
      total_connections_(0),
      total_connects_provided_(0),
      total_fraction_wanted_(0.0),
      wifi_interface_index_(ifindex),
      ssids_(ByteString::IsLessThan),
      fractions_(fractions),
      min_frequencies_(min_frequencies),
      max_frequencies_(max_frequencies),
      on_scan_failed_(on_scan_failed),
      scan_tries_left_(kScanRetryCount) {
  sort(frequency_list_.begin(), frequency_list_.end(),
       &ScanSession::CompareFrequencyCount);
  // Add to |frequency_list_| all the frequencies from |available_frequencies|
  // that aren't in |previous_frequencies|.
  set<uint16_t> seen_frequencies;
  for (const auto &freq_conn : frequency_list_) {
    seen_frequencies.insert(freq_conn.frequency);
    total_connections_ += freq_conn.connection_count;
  }
  for (const auto freq : available_frequencies) {
    if (!ContainsKey(seen_frequencies, freq)) {
      frequency_list_.push_back(WiFiProvider::FrequencyCount(freq, 0));
    }
  }

  SLOG(WiFi, 7) << "Frequency connections vector:";
  for (const auto &freq_conn : frequency_list_) {
    SLOG(WiFi, 7) << "    freq[" << freq_conn.frequency << "] = "
                  << freq_conn.connection_count;
  }
}

ScanSession::~ScanSession() {}

bool ScanSession::HasMoreFrequencies() const {
  return !frequency_list_.empty();
}

vector<uint16_t> ScanSession::GetScanFrequencies(float fraction_wanted,
                                                 size_t min_frequencies,
                                                 size_t max_frequencies) {
  DCHECK_GE(fraction_wanted, 0);
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

void ScanSession::InitiateScan() {
  float fraction_wanted = kAllFrequencies;
  if (!fractions_.empty()) {
    fraction_wanted = fractions_.front();
    fractions_.pop_front();
  }
  current_scan_frequencies_ = GetScanFrequencies(fraction_wanted,
                                                 min_frequencies_,
                                                 max_frequencies_);
  DoScan(current_scan_frequencies_);
}

void ScanSession::ReInitiateScan() {
  DoScan(current_scan_frequencies_);
}

void ScanSession::DoScan(const vector<uint16_t> &scan_frequencies) {
  if (scan_frequencies.empty()) {
    LOG(INFO) << "Not sending empty frequency list";
    return;
  }
  TriggerScanMessage trigger_scan;
  trigger_scan.attributes()->SetU32AttributeValue(NL80211_ATTR_IFINDEX,
                                                  wifi_interface_index_);
  AttributeListRefPtr frequency_list;
  if (!trigger_scan.attributes()->GetNestedAttributeList(
      NL80211_ATTR_SCAN_FREQUENCIES, &frequency_list) || !frequency_list) {
    LOG(FATAL) << "Couldn't get NL80211_ATTR_SCAN_FREQUENCIES.";
  }
  trigger_scan.attributes()->SetNestedAttributeHasAValue(
      NL80211_ATTR_SCAN_FREQUENCIES);

  SLOG(WiFi, 7) << "We have requested scan frequencies:";
  string attribute_name;
  int i = 0;
  for (const auto freq : scan_frequencies) {
    SLOG(WiFi, 7) << "  " << freq;
    attribute_name = StringPrintf("Frequency-%d", i);
    frequency_list->CreateU32Attribute(i, attribute_name.c_str());
    frequency_list->SetU32AttributeValue(i, freq);
    ++i;
  }

  if (!ssids_.empty()) {
    AttributeListRefPtr ssid_list;
    if (!trigger_scan.attributes()->GetNestedAttributeList(
        NL80211_ATTR_SCAN_SSIDS, &ssid_list) || !ssid_list) {
      LOG(FATAL) << "Couldn't get NL80211_ATTR_SCAN_SSIDS attribute.";
    }
    trigger_scan.attributes()->SetNestedAttributeHasAValue(
        NL80211_ATTR_SCAN_SSIDS);
    int i = 0;
    string attribute_name;
    for (const auto &ssid : ssids_) {
      attribute_name = StringPrintf("NL80211_ATTR_SSID_%d", i);
      ssid_list->CreateRawAttribute(i, attribute_name.c_str());
      ssid_list->SetRawAttributeValue(i, ssid);
      ++i;
    }
    // Add an empty one at the end so we ask for a broadcast in addition to
    // the specific SSIDs.
    attribute_name = StringPrintf("NL80211_ATTR_SSID_%d", i);
    ssid_list->CreateRawAttribute(i, attribute_name.c_str());
    ssid_list->SetRawAttributeValue(i, ByteString());
  }
  netlink_manager_->SendMessage(&trigger_scan,
                                Bind(&ScanSession::OnTriggerScanResponse,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void ScanSession::OnTriggerScanResponse(const NetlinkMessage &netlink_message) {
  if (netlink_message.message_type() != NLMSG_ERROR) {
    LOG(WARNING) << "Didn't expect _this_ message, here:";
    netlink_message.Print(0, 0);
    on_scan_failed_.Run();
    return;
  }

  const ErrorAckMessage *error_ack_message =
      dynamic_cast<const ErrorAckMessage *>(&netlink_message);
  if (error_ack_message->error()) {
    LOG(ERROR) << __func__ << ": Message failed: "
               << error_ack_message->ToString();
    if (error_ack_message->error() == EBUSY) {
      if (scan_tries_left_ == 0) {
        LOG(ERROR) << "Retried progressive scan " << kScanRetryCount
                   << " times and failed each time.  Giving up.";
        on_scan_failed_.Run();
        scan_tries_left_ = kScanRetryCount;
        return;
      }
      --scan_tries_left_;
      SLOG(WiFi, 3) << __func__ << " - trying again (" << scan_tries_left_
                    << " remaining after this)";
      dispatcher_->PostDelayedTask(Bind(&ScanSession::ReInitiateScan,
                                        weak_ptr_factory_.GetWeakPtr()),
                                   kScanRetryDelayMilliseconds);
      return;
    }
    on_scan_failed_.Run();
  } else {
    SLOG(WiFi, 6) << __func__ << ": Message ACKed";
  }
}

void ScanSession::AddSsid(const ByteString &ssid) {
  ssids_.insert(ssid);
}

// static
bool ScanSession::CompareFrequencyCount(
    const WiFiProvider::FrequencyCount &first,
    const WiFiProvider::FrequencyCount &second) {
  return first.connection_count > second.connection_count;
}

}  // namespace shill

