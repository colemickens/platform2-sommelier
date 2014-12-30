// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi/wake_on_wifi.h"

#include <errno.h>
#include <linux/nl80211.h>
#include <stdio.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/cancelable_callback.h>
#include <chromeos/dbus/service_constants.h>
#include <components/timers/alarm_timer.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/ip_address_store.h"
#include "shill/logging.h"
#include "shill/metrics.h"
#include "shill/net/netlink_manager.h"
#include "shill/net/nl80211_message.h"
#include "shill/property_accessor.h"
#include "shill/wifi/wifi.h"

using base::Bind;
using base::Closure;
using std::pair;
using std::set;
using std::string;
using std::vector;
using timers::AlarmTimer;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kWiFi;
static std::string ObjectID(WakeOnWiFi *w) { return "(wake_on_wifi)"; }
}

const char WakeOnWiFi::kWakeOnIPAddressPatternsNotSupported[] =
    "Wake on IP address patterns not supported by this WiFi device";
const char WakeOnWiFi::kWakeOnPacketDisabled[] =
    "Wake on Packet feature disabled, so do nothing";
const char WakeOnWiFi::kWakeOnWiFiDisabled[] = "Wake on WiFi is disabled";
const uint32_t WakeOnWiFi::kDefaultWiphyIndex = 999;
const int WakeOnWiFi::kVerifyWakeOnWiFiSettingsDelayMilliseconds = 300;
const int WakeOnWiFi::kMaxSetWakeOnPacketRetries = 2;
const int WakeOnWiFi::kMetricsReportingFrequencySeconds = 600;
const uint32_t WakeOnWiFi::kDefaultWakeToScanFrequencySeconds = 900;
const uint32_t WakeOnWiFi::kImmediateDHCPLeaseRenewalThresholdSeconds = 60;
// If a connection is not established during dark resume, give up and prepare
// the system to wake on SSID 1 second before suspending again.
// TODO(samueltan): link this to
// Manager::kTerminationActionsTimeoutMilliseconds rather than hard-coding
// this value.
int64_t WakeOnWiFi::DarkResumeActionsTimeoutMilliseconds = 8500;

WakeOnWiFi::WakeOnWiFi(NetlinkManager *netlink_manager,
                       EventDispatcher *dispatcher, Metrics *metrics)
    : dispatcher_(dispatcher),
      netlink_manager_(netlink_manager),
      metrics_(metrics),
      report_metrics_callback_(
          Bind(&WakeOnWiFi::ReportMetrics, base::Unretained(this))),
      num_set_wake_on_packet_retries_(0),
      wake_on_wifi_max_patterns_(0),
      wiphy_index_(kDefaultWiphyIndex),
      wiphy_index_received_(false),
#if defined(DISABLE_WAKE_ON_WIFI)
      wake_on_wifi_features_enabled_(kWakeOnWiFiFeaturesEnabledNotSupported),
#else
      // Wake on WiFi features temporarily disabled at run-time for boards that
      // support wake on WiFi.
      // TODO(samueltan): re-enable once pending issues have been resolved.
      wake_on_wifi_features_enabled_(kWakeOnWiFiFeaturesEnabledNone),
#endif  // DISABLE_WAKE_ON_WIFI
      dhcp_lease_renewal_timer_(true, false),
      wake_to_scan_timer_(true, false),
      in_dark_resume_(false),
      wake_to_scan_frequency_(kDefaultWakeToScanFrequencySeconds),
      weak_ptr_factory_(this) {
}

WakeOnWiFi::~WakeOnWiFi() {}

void WakeOnWiFi::InitPropertyStore(PropertyStore *store) {
  store->RegisterDerivedString(
      kWakeOnWiFiFeaturesEnabledProperty,
      StringAccessor(new CustomAccessor<WakeOnWiFi, string>(
          this, &WakeOnWiFi::GetWakeOnWiFiFeaturesEnabled,
          &WakeOnWiFi::SetWakeOnWiFiFeaturesEnabled)));
  store->RegisterUint32(kWakeToScanFrequencyProperty, &wake_to_scan_frequency_);
}

void WakeOnWiFi::StartMetricsTimer() {
#if !defined(DISABLE_WAKE_ON_WIFI)
  dispatcher_->PostDelayedTask(report_metrics_callback_.callback(),
                               kMetricsReportingFrequencySeconds * 1000);
#endif  // DISABLE_WAKE_ON_WIFI
}

string WakeOnWiFi::GetWakeOnWiFiFeaturesEnabled(Error *error) {
  return wake_on_wifi_features_enabled_;
}

bool WakeOnWiFi::SetWakeOnWiFiFeaturesEnabled(const std::string &enabled,
                                              Error *error) {
#if defined(DISABLE_WAKE_ON_WIFI)
  Error::PopulateAndLog(error, Error::kNotSupported,
                        "Wake on WiFi is not supported");
  return false;
#else
  if (wake_on_wifi_features_enabled_ == enabled) {
    return false;
  }
  if (enabled != kWakeOnWiFiFeaturesEnabledPacket &&
      enabled != kWakeOnWiFiFeaturesEnabledSSID &&
      enabled != kWakeOnWiFiFeaturesEnabledPacketSSID &&
      enabled != kWakeOnWiFiFeaturesEnabledNone) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Invalid Wake on WiFi feature");
    return false;
  }
  wake_on_wifi_features_enabled_ = enabled;
  return true;
#endif  // DISABLE_WAKE_ON_WIFI
}

void WakeOnWiFi::RunAndResetSuspendActionsDoneCallback(const Error &error) {
  if (!suspend_actions_done_callback_.is_null()) {
    suspend_actions_done_callback_.Run(error);
    suspend_actions_done_callback_.Reset();
  }
}

bool WakeOnWiFi::ByteStringPairIsLessThan(
    const std::pair<ByteString, ByteString> &lhs,
    const std::pair<ByteString, ByteString> &rhs) {
  // Treat the first value of the pair as the key.
  return ByteString::IsLessThan(lhs.first, rhs.first);
}

// static
void WakeOnWiFi::SetMask(ByteString *mask, uint32_t pattern_len,
                         uint32_t offset) {
  // Round up number of bytes required for the mask.
  int result_mask_len = (pattern_len + 8 - 1) / 8;
  vector<unsigned char> result_mask(result_mask_len, 0);
  // Set mask bits from offset to (pattern_len - 1)
  int mask_index;
  for (uint32_t curr_mask_bit = offset; curr_mask_bit < pattern_len;
       ++curr_mask_bit) {
    mask_index = curr_mask_bit / 8;
    result_mask[mask_index] |= 1 << (curr_mask_bit % 8);
  }
  mask->Clear();
  mask->Append(ByteString(result_mask));
}

// static
bool WakeOnWiFi::CreateIPAddressPatternAndMask(const IPAddress &ip_addr,
                                               ByteString *pattern,
                                               ByteString *mask) {
  if (ip_addr.family() == IPAddress::kFamilyIPv4) {
    WakeOnWiFi::CreateIPV4PatternAndMask(ip_addr, pattern, mask);
    return true;
  } else if (ip_addr.family() == IPAddress::kFamilyIPv6) {
    WakeOnWiFi::CreateIPV6PatternAndMask(ip_addr, pattern, mask);
    return true;
  } else {
    LOG(ERROR) << "Unrecognized IP Address type.";
    return false;
  }
}

// static
void WakeOnWiFi::CreateIPV4PatternAndMask(const IPAddress &ip_addr,
                                          ByteString *pattern,
                                          ByteString *mask) {
  struct {
    struct ethhdr eth_hdr;
    struct iphdr ipv4_hdr;
  } __attribute__((__packed__)) pattern_bytes;
  memset(&pattern_bytes, 0, sizeof(pattern_bytes));
  CHECK_EQ(sizeof(pattern_bytes.ipv4_hdr.saddr), ip_addr.GetLength());
  memcpy(&pattern_bytes.ipv4_hdr.saddr, ip_addr.GetConstData(),
         ip_addr.GetLength());
  int src_ip_offset =
      reinterpret_cast<unsigned char *>(&pattern_bytes.ipv4_hdr.saddr) -
      reinterpret_cast<unsigned char *>(&pattern_bytes);
  int pattern_len = src_ip_offset + ip_addr.GetLength();
  pattern->Clear();
  pattern->Append(ByteString(
      reinterpret_cast<const unsigned char *>(&pattern_bytes), pattern_len));
  WakeOnWiFi::SetMask(mask, pattern_len, src_ip_offset);
}

// static
void WakeOnWiFi::CreateIPV6PatternAndMask(const IPAddress &ip_addr,
                                          ByteString *pattern,
                                          ByteString *mask) {
  struct {
    struct ethhdr eth_hdr;
    struct ip6_hdr ipv6_hdr;
  } __attribute__((__packed__)) pattern_bytes;
  memset(&pattern_bytes, 0, sizeof(pattern_bytes));
  CHECK_EQ(sizeof(pattern_bytes.ipv6_hdr.ip6_src), ip_addr.GetLength());
  memcpy(&pattern_bytes.ipv6_hdr.ip6_src, ip_addr.GetConstData(),
         ip_addr.GetLength());
  int src_ip_offset =
      reinterpret_cast<unsigned char *>(&pattern_bytes.ipv6_hdr.ip6_src) -
      reinterpret_cast<unsigned char *>(&pattern_bytes);
  int pattern_len = src_ip_offset + ip_addr.GetLength();
  pattern->Clear();
  pattern->Append(ByteString(
      reinterpret_cast<const unsigned char *>(&pattern_bytes), pattern_len));
  WakeOnWiFi::SetMask(mask, pattern_len, src_ip_offset);
}

// static
bool WakeOnWiFi::ConfigureWiphyIndex(Nl80211Message *msg, int32_t index) {
  if (!msg->attributes()->CreateU32Attribute(NL80211_ATTR_WIPHY,
                                             "WIPHY index")) {
    return false;
  }
  if (!msg->attributes()->SetU32AttributeValue(NL80211_ATTR_WIPHY, index)) {
    return false;
  }
  return true;
}

// static
bool WakeOnWiFi::ConfigureDisableWakeOnWiFiMessage(
    SetWakeOnPacketConnMessage *msg, uint32_t wiphy_index, Error *error) {
  if (!ConfigureWiphyIndex(msg, wiphy_index)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Failed to configure Wiphy index.");
    return false;
  }
  return true;
}

// static
bool WakeOnWiFi::ConfigureSetWakeOnWiFiSettingsMessage(
    SetWakeOnPacketConnMessage *msg, const set<WakeOnWiFiTrigger> &trigs,
    const IPAddressStore &addrs, uint32_t wiphy_index, Error *error) {
  if (trigs.empty()) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "No triggers to configure.");
    return false;
  }
  if (trigs.find(kPattern) != trigs.end() && addrs.Empty()) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "No IP addresses to configure.");
    return false;
  }
  if (!ConfigureWiphyIndex(msg, wiphy_index)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Failed to configure Wiphy index.");
    return false;
  }
  if (!msg->attributes()->CreateNestedAttribute(NL80211_ATTR_WOWLAN_TRIGGERS,
                                                "WoWLAN Triggers")) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not create nested attribute "
                          "NL80211_ATTR_WOWLAN_TRIGGERS for "
                          "SetWakeOnPacketConnMessage.");
    return false;
  }
  if (!msg->attributes()->SetNestedAttributeHasAValue(
          NL80211_ATTR_WOWLAN_TRIGGERS)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not set nested attribute "
                          "NL80211_ATTR_WOWLAN_TRIGGERS for "
                          "SetWakeOnPacketConnMessage.");
    return false;
  }

  AttributeListRefPtr triggers;
  if (!msg->attributes()->GetNestedAttributeList(NL80211_ATTR_WOWLAN_TRIGGERS,
                                                 &triggers)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not get nested attribute list "
                          "NL80211_ATTR_WOWLAN_TRIGGERS for "
                          "SetWakeOnPacketConnMessage.");
    return false;
  }
  // Add triggers.
  for (WakeOnWiFiTrigger t : trigs) {
    switch (t) {
      case kDisconnect: {
        if (!triggers->CreateFlagAttribute(NL80211_WOWLAN_TRIG_DISCONNECT,
                                           "Wake on Disconnect")) {
          LOG(ERROR) << __func__ << "Could not create flag attribute "
                                    "NL80211_WOWLAN_TRIG_DISCONNECT";
          return false;
        }
        if (!triggers->SetFlagAttributeValue(NL80211_WOWLAN_TRIG_DISCONNECT,
                                             true)) {
          LOG(ERROR) << __func__ << "Could not set flag attribute "
                                    "NL80211_WOWLAN_TRIG_DISCONNECT";
          return false;
        }
        break;
      }
      case kPattern: {
        if (!triggers->CreateNestedAttribute(NL80211_WOWLAN_TRIG_PKT_PATTERN,
                                             "Pattern trigger")) {
          Error::PopulateAndLog(error, Error::kOperationFailed,
                                "Could not create nested attribute "
                                "NL80211_WOWLAN_TRIG_PKT_PATTERN for "
                                "SetWakeOnPacketConnMessage.");
          return false;
        }
        if (!triggers->SetNestedAttributeHasAValue(
                NL80211_WOWLAN_TRIG_PKT_PATTERN)) {
          Error::PopulateAndLog(error, Error::kOperationFailed,
                                "Could not set nested attribute "
                                "NL80211_WOWLAN_TRIG_PKT_PATTERN for "
                                "SetWakeOnPacketConnMessage.");
          return false;
        }
        AttributeListRefPtr patterns;
        if (!triggers->GetNestedAttributeList(NL80211_WOWLAN_TRIG_PKT_PATTERN,
                                              &patterns)) {
          Error::PopulateAndLog(error, Error::kOperationFailed,
                                "Could not get nested attribute list "
                                "NL80211_WOWLAN_TRIG_PKT_PATTERN for "
                                "SetWakeOnPacketConnMessage.");
          return false;
        }
        uint8_t patnum = 1;
        for (const IPAddress &addr : addrs.GetIPAddresses()) {
          if (!CreateSinglePattern(addr, patterns, patnum++, error)) {
            return false;
          }
        }
        break;
      }
      case kSSID: {
        // TODO(samueltan): construct wake on SSID trigger when available.
        break;
      }
      default: {
        LOG(ERROR) << __func__ << ": Unrecognized trigger";
        return false;
      }
    }
  }
  return true;
}

// static
bool WakeOnWiFi::CreateSinglePattern(const IPAddress &ip_addr,
                                     AttributeListRefPtr patterns,
                                     uint8_t patnum, Error *error) {
  ByteString pattern;
  ByteString mask;
  WakeOnWiFi::CreateIPAddressPatternAndMask(ip_addr, &pattern, &mask);
  if (!patterns->CreateNestedAttribute(patnum, "Pattern info")) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not create nested attribute "
                          "patnum for SetWakeOnPacketConnMessage.");
    return false;
  }
  if (!patterns->SetNestedAttributeHasAValue(patnum)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not set nested attribute "
                          "patnum for SetWakeOnPacketConnMessage.");
    return false;
  }

  AttributeListRefPtr pattern_info;
  if (!patterns->GetNestedAttributeList(patnum, &pattern_info)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not get nested attribute list "
                          "patnum for SetWakeOnPacketConnMessage.");
    return false;
  }
  // Add mask.
  if (!pattern_info->CreateRawAttribute(NL80211_PKTPAT_MASK, "Mask")) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not add attribute NL80211_PKTPAT_MASK to "
                          "pattern_info.");
    return false;
  }
  if (!pattern_info->SetRawAttributeValue(NL80211_PKTPAT_MASK, mask)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not set attribute NL80211_PKTPAT_MASK in "
                          "pattern_info.");
    return false;
  }

  // Add pattern.
  if (!pattern_info->CreateRawAttribute(NL80211_PKTPAT_PATTERN, "Pattern")) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not add attribute NL80211_PKTPAT_PATTERN to "
                          "pattern_info.");
    return false;
  }
  if (!pattern_info->SetRawAttributeValue(NL80211_PKTPAT_PATTERN, pattern)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not set attribute NL80211_PKTPAT_PATTERN in "
                          "pattern_info.");
    return false;
  }

  // Add offset.
  if (!pattern_info->CreateU32Attribute(NL80211_PKTPAT_OFFSET, "Offset")) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not add attribute NL80211_PKTPAT_OFFSET to "
                          "pattern_info.");
    return false;
  }
  if (!pattern_info->SetU32AttributeValue(NL80211_PKTPAT_OFFSET, 0)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not set attribute NL80211_PKTPAT_OFFSET in "
                          "pattern_info.");
    return false;
  }
  return true;
}

// static
bool WakeOnWiFi::ConfigureGetWakeOnWiFiSettingsMessage(
    GetWakeOnPacketConnMessage *msg, uint32_t wiphy_index, Error *error) {
  if (!ConfigureWiphyIndex(msg, wiphy_index)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Failed to configure Wiphy index.");
    return false;
  }
  return true;
}

// static
bool WakeOnWiFi::WakeOnWiFiSettingsMatch(const Nl80211Message &msg,
                                         const set<WakeOnWiFiTrigger> &trigs,
                                         const IPAddressStore &addrs) {
  if (msg.command() != NL80211_CMD_GET_WOWLAN &&
      msg.command() != NL80211_CMD_SET_WOWLAN) {
    LOG(ERROR) << "Invalid message command";
    return false;
  }
  AttributeListConstRefPtr triggers;
  if (!msg.const_attributes()->ConstGetNestedAttributeList(
          NL80211_ATTR_WOWLAN_TRIGGERS, &triggers)) {
    // No triggers in the returned message, which is valid iff we expect there
    // to be no triggers programmed into the NIC.
    return trigs.empty();
  }
  // If the disconnect trigger is found and set, but we did not expect this
  // trigger, we have a mismatch.
  bool wake_on_disconnect = false;
  triggers->GetFlagAttributeValue(NL80211_WOWLAN_TRIG_DISCONNECT,
                                  &wake_on_disconnect);
  if (trigs.find(kDisconnect) == trigs.end() && wake_on_disconnect) {
    return false;
  }
  // Check each trigger.
  for (WakeOnWiFiTrigger t : trigs) {
    switch (t) {
      case kDisconnect: {
        if (!wake_on_disconnect) {
          return false;
        }
        break;
      }
      case kPattern: {
        // Create pattern and masks that we expect to find in |msg|.
        set<pair<ByteString, ByteString>,
            bool (*)(const pair<ByteString, ByteString> &,
                     const pair<ByteString, ByteString> &)>
            expected_patt_mask_pairs(ByteStringPairIsLessThan);
        ByteString temp_pattern;
        ByteString temp_mask;
        for (const IPAddress &addr : addrs.GetIPAddresses()) {
          temp_pattern.Clear();
          temp_mask.Clear();
          CreateIPAddressPatternAndMask(addr, &temp_pattern, &temp_mask);
          expected_patt_mask_pairs.emplace(temp_pattern, temp_mask);
        }
        // Check these expected pattern and masks against those actually
        // contained in |msg|.
        AttributeListConstRefPtr patterns;
        if (!triggers->ConstGetNestedAttributeList(
                NL80211_WOWLAN_TRIG_PKT_PATTERN, &patterns)) {
          LOG(ERROR) << "Could not get nested attribute list "
                        "NL80211_WOWLAN_TRIG_PKT_PATTERN.";
          return false;
        }
        bool mismatch_found = false;
        size_t num_mismatch = expected_patt_mask_pairs.size();
        int pattern_index;
        AttributeIdIterator pattern_iter(*patterns);
        AttributeListConstRefPtr pattern_info;
        ByteString returned_mask;
        ByteString returned_pattern;
        while (!pattern_iter.AtEnd()) {
          returned_mask.Clear();
          returned_pattern.Clear();
          pattern_index = pattern_iter.GetId();
          if (!patterns->ConstGetNestedAttributeList(pattern_index,
                                                     &pattern_info)) {
            LOG(ERROR) << "Could not get nested attribute list index "
                       << pattern_index << " in patterns.";
            return false;
          }
          if (!pattern_info->GetRawAttributeValue(NL80211_PKTPAT_MASK,
                                                  &returned_mask)) {
            LOG(ERROR) << "Could not get attribute NL80211_PKTPAT_MASK in "
                          "pattern_info.";
            return false;
          }
          if (!pattern_info->GetRawAttributeValue(NL80211_PKTPAT_PATTERN,
                                                  &returned_pattern)) {
            LOG(ERROR) << "Could not get attribute NL80211_PKTPAT_PATTERN in "
                          "pattern_info.";
            return false;
          }
          if (expected_patt_mask_pairs.find(pair<ByteString, ByteString>(
                  returned_pattern, returned_mask)) ==
              expected_patt_mask_pairs.end()) {
            mismatch_found = true;
            break;
          } else {
            --num_mismatch;
          }
          pattern_iter.Advance();
        }
        if (mismatch_found || num_mismatch) {
          return false;
        }
        break;
      }
      case kSSID: {
        // TODO(samueltan): parse wake on SSID trigger when available.
        break;
      }
      default: {
        LOG(ERROR) << __func__ << ": Unrecognized trigger";
        return false;
      }
    }
  }
  return true;
}

void WakeOnWiFi::AddWakeOnPacketConnection(const string &ip_endpoint,
                                           Error *error) {
#if !defined(DISABLE_WAKE_ON_WIFI)
  if (wake_on_wifi_triggers_supported_.find(kPattern) ==
      wake_on_wifi_triggers_supported_.end()) {
    Error::PopulateAndLog(error, Error::kNotSupported,
                          kWakeOnIPAddressPatternsNotSupported);
    return;
  }
  IPAddress ip_addr(ip_endpoint);
  if (!ip_addr.IsValid()) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Invalid ip_address " + ip_endpoint);
    return;
  }
  if (wake_on_wifi_triggers_.size() >= wake_on_wifi_max_patterns_) {
    Error::PopulateAndLog(
        error, Error::kOperationFailed,
        "Max number of IP address patterns already registered");
    return;
  }
  wake_on_packet_connections_.AddUnique(ip_addr);
#else
  Error::PopulateAndLog(error, Error::kNotSupported, kWakeOnWiFiDisabled);
#endif  // DISABLE_WAKE_ON_WIFI
}

void WakeOnWiFi::RemoveWakeOnPacketConnection(const string &ip_endpoint,
                                              Error *error) {
#if !defined(DISABLE_WAKE_ON_WIFI)
  if (wake_on_wifi_triggers_supported_.find(kPattern) ==
      wake_on_wifi_triggers_supported_.end()) {
    Error::PopulateAndLog(error, Error::kNotSupported,
                          kWakeOnIPAddressPatternsNotSupported);
    return;
  }
  IPAddress ip_addr(ip_endpoint);
  if (!ip_addr.IsValid()) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Invalid ip_address " + ip_endpoint);
    return;
  }
  if (!wake_on_packet_connections_.Contains(ip_addr)) {
    Error::PopulateAndLog(error, Error::kNotFound,
                          "No such IP address match registered to wake device");
    return;
  }
  wake_on_packet_connections_.Remove(ip_addr);
#else
  Error::PopulateAndLog(error, Error::kNotSupported, kWakeOnWiFiDisabled);
#endif  // DISABLE_WAKE_ON_WIFI
}

void WakeOnWiFi::RemoveAllWakeOnPacketConnections(Error *error) {
#if !defined(DISABLE_WAKE_ON_WIFI)
  if (wake_on_wifi_triggers_supported_.find(kPattern) ==
      wake_on_wifi_triggers_supported_.end()) {
    Error::PopulateAndLog(error, Error::kNotSupported,
                          kWakeOnIPAddressPatternsNotSupported);
    return;
  }
  wake_on_packet_connections_.Clear();
#else
  Error::PopulateAndLog(error, Error::kNotSupported, kWakeOnWiFiDisabled);
#endif  // DISABLE_WAKE_ON_WIFI
}

void WakeOnWiFi::OnWakeOnWiFiSettingsErrorResponse(
    NetlinkManager::AuxilliaryMessageType type,
    const NetlinkMessage *raw_message) {
  Error error(Error::kOperationFailed);
  switch (type) {
    case NetlinkManager::kErrorFromKernel:
      if (!raw_message) {
        error.Populate(Error::kOperationFailed, "Unknown error from kernel");
        break;
      }
      if (raw_message->message_type() == ErrorAckMessage::GetMessageType()) {
        const ErrorAckMessage *error_ack_message =
            dynamic_cast<const ErrorAckMessage *>(raw_message);
        if (error_ack_message->error() == EOPNOTSUPP) {
          error.Populate(Error::kNotSupported);
        }
      }
      break;

    case NetlinkManager::kUnexpectedResponseType:
      error.Populate(Error::kNotRegistered,
                     "Message not handled by regular message handler:");
      break;

    case NetlinkManager::kTimeoutWaitingForResponse:
      // CMD_SET_WOWLAN messages do not receive responses, so this error type
      // is received when NetlinkManager times out the message handler. Return
      // immediately rather than run the done callback since this event does
      // not signify the completion of suspend actions.
      return;
      break;

    default:
      error.Populate(
          Error::kOperationFailed,
          "Unexpected auxilliary message type: " + std::to_string(type));
      break;
  }
  RunAndResetSuspendActionsDoneCallback(error);
}

// static
void WakeOnWiFi::OnSetWakeOnPacketConnectionResponse(
    const Nl80211Message &nl80211_message) {
  // NOP because kernel does not send a response to NL80211_CMD_SET_WOWLAN
  // requests.
}

void WakeOnWiFi::RequestWakeOnPacketSettings() {
  SLOG(this, 3) << __func__;
  Error e;
  GetWakeOnPacketConnMessage get_wowlan_msg;
  if (!ConfigureGetWakeOnWiFiSettingsMessage(&get_wowlan_msg, wiphy_index_,
                                             &e)) {
    LOG(ERROR) << e.message();
    return;
  }
  netlink_manager_->SendNl80211Message(
      &get_wowlan_msg, Bind(&WakeOnWiFi::VerifyWakeOnWiFiSettings,
                            weak_ptr_factory_.GetWeakPtr()),
      Bind(&NetlinkManager::OnAckDoNothing),
      Bind(&NetlinkManager::OnNetlinkMessageError));
}

void WakeOnWiFi::VerifyWakeOnWiFiSettings(
    const Nl80211Message &nl80211_message) {
  SLOG(this, 3) << __func__;
  if (WakeOnWiFiSettingsMatch(nl80211_message, wake_on_wifi_triggers_,
                              wake_on_packet_connections_)) {
    SLOG(this, 2) << __func__ << ": "
                  << "Wake-on-packet settings successfully verified";
    metrics_->NotifyVerifyWakeOnWiFiSettingsResult(
        Metrics::kVerifyWakeOnWiFiSettingsResultSuccess);
    RunAndResetSuspendActionsDoneCallback(Error(Error::kSuccess));
  } else {
    LOG(ERROR) << __func__ << " failed: discrepancy between wake-on-packet "
                              "settings on NIC and those in local data "
                              "structure detected";
    metrics_->NotifyVerifyWakeOnWiFiSettingsResult(
        Metrics::kVerifyWakeOnWiFiSettingsResultFailure);
    RetrySetWakeOnPacketConnections();
  }
}

void WakeOnWiFi::ApplyWakeOnWiFiSettings() {
  SLOG(this, 3) << __func__;
  if (!wiphy_index_received_) {
    LOG(ERROR) << "Interface index not yet received";
    return;
  }
  if (wake_on_wifi_triggers_.empty()) {
    LOG(INFO) << "No triggers to be programmed, so disable wake on WiFi";
    DisableWakeOnWiFi();
    return;
  }
  Error error;
  SetWakeOnPacketConnMessage set_wowlan_msg;
  if (!ConfigureSetWakeOnWiFiSettingsMessage(
          &set_wowlan_msg, wake_on_wifi_triggers_, wake_on_packet_connections_,
          wiphy_index_, &error)) {
    LOG(ERROR) << error.message();
    return;
  }
  if (!netlink_manager_->SendNl80211Message(
          &set_wowlan_msg,
          Bind(&WakeOnWiFi::OnSetWakeOnPacketConnectionResponse),
          Bind(&NetlinkManager::OnAckDoNothing),
          Bind(&WakeOnWiFi::OnWakeOnWiFiSettingsErrorResponse,
               weak_ptr_factory_.GetWeakPtr()))) {
    RunAndResetSuspendActionsDoneCallback(Error(Error::kOperationFailed));
    return;
  }

  verify_wake_on_packet_settings_callback_.Reset(
      Bind(&WakeOnWiFi::RequestWakeOnPacketSettings,
           weak_ptr_factory_.GetWeakPtr()));
  dispatcher_->PostDelayedTask(
      verify_wake_on_packet_settings_callback_.callback(),
      kVerifyWakeOnWiFiSettingsDelayMilliseconds);
}

void WakeOnWiFi::DisableWakeOnWiFi() {
  SLOG(this, 3) << __func__;
  Error error;
  SetWakeOnPacketConnMessage disable_wowlan_msg;
  if (!ConfigureDisableWakeOnWiFiMessage(&disable_wowlan_msg, wiphy_index_,
                                         &error)) {
    LOG(ERROR) << error.message();
    return;
  }
  if (!netlink_manager_->SendNl80211Message(
          &disable_wowlan_msg,
          Bind(&WakeOnWiFi::OnSetWakeOnPacketConnectionResponse),
          Bind(&NetlinkManager::OnAckDoNothing),
          Bind(&WakeOnWiFi::OnWakeOnWiFiSettingsErrorResponse,
               weak_ptr_factory_.GetWeakPtr()))) {
    RunAndResetSuspendActionsDoneCallback(Error(Error::kOperationFailed));
    return;
  }

  verify_wake_on_packet_settings_callback_.Reset(
      Bind(&WakeOnWiFi::RequestWakeOnPacketSettings,
           weak_ptr_factory_.GetWeakPtr()));
  dispatcher_->PostDelayedTask(
      verify_wake_on_packet_settings_callback_.callback(),
      kVerifyWakeOnWiFiSettingsDelayMilliseconds);
}

void WakeOnWiFi::RetrySetWakeOnPacketConnections() {
  SLOG(this, 3) << __func__;
  if (num_set_wake_on_packet_retries_ < kMaxSetWakeOnPacketRetries) {
    ApplyWakeOnWiFiSettings();
    ++num_set_wake_on_packet_retries_;
  } else {
    SLOG(this, 3) << __func__ << ": max retry attempts reached";
    num_set_wake_on_packet_retries_ = 0;
    RunAndResetSuspendActionsDoneCallback(Error(Error::kOperationFailed));
  }
}

bool WakeOnWiFi::WakeOnPacketEnabledAndSupported() {
  if (wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledNone ||
      wake_on_wifi_features_enabled_ ==
          kWakeOnWiFiFeaturesEnabledNotSupported ||
      wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledSSID) {
    return false;
  }
  if (wake_on_wifi_triggers_supported_.find(kPattern) ==
      wake_on_wifi_triggers_supported_.end()) {
    return false;
  }
  return true;
}

bool WakeOnWiFi::WakeOnSSIDEnabledAndSupported() {
  if (wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledNone ||
      wake_on_wifi_features_enabled_ ==
          kWakeOnWiFiFeaturesEnabledNotSupported ||
      wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledPacket) {
    return false;
  }
  if (wake_on_wifi_triggers_supported_.find(kDisconnect) ==
          wake_on_wifi_triggers_supported_.end() ||
      wake_on_wifi_triggers_supported_.find(kSSID) ==
          wake_on_wifi_triggers_supported_.end()) {
    return false;
  }
  return true;
}

void WakeOnWiFi::ReportMetrics() {
  Metrics::WakeOnWiFiFeaturesEnabledState reported_state;
  if (wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledNone) {
    reported_state = Metrics::kWakeOnWiFiFeaturesEnabledStateNone;
  } else if (wake_on_wifi_features_enabled_ ==
             kWakeOnWiFiFeaturesEnabledPacket) {
    reported_state = Metrics::kWakeOnWiFiFeaturesEnabledStatePacket;
  } else if (wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledSSID) {
    reported_state = Metrics::kWakeOnWiFiFeaturesEnabledStateSSID;
  } else if (wake_on_wifi_features_enabled_ ==
             kWakeOnWiFiFeaturesEnabledPacketSSID) {
    reported_state = Metrics::kWakeOnWiFiFeaturesEnabledStatePacketSSID;
  } else {
    LOG(ERROR) << __func__ << ": "
               << "Invalid wake on WiFi features state";
    return;
  }
  metrics_->NotifyWakeOnWiFiFeaturesEnabledState(reported_state);
  StartMetricsTimer();
}

void WakeOnWiFi::ParseWakeOnWiFiCapabilities(
    const Nl80211Message &nl80211_message) {
  // Verify NL80211_CMD_NEW_WIPHY.
  if (nl80211_message.command() != NewWiphyMessage::kCommand) {
    LOG(ERROR) << "Received unexpected command:" << nl80211_message.command();
    return;
  }
  AttributeListConstRefPtr triggers_supported;
  if (nl80211_message.const_attributes()->ConstGetNestedAttributeList(
          NL80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED, &triggers_supported)) {
    bool disconnect_supported = false;
    if (triggers_supported->GetFlagAttributeValue(
            NL80211_WOWLAN_TRIG_DISCONNECT, &disconnect_supported)) {
      if (disconnect_supported) {
        wake_on_wifi_triggers_supported_.insert(WakeOnWiFi::kDisconnect);
        SLOG(this, 7) << "Waking on disconnect supported by this WiFi device";
      }
    }
    ByteString data;
    if (triggers_supported->GetRawAttributeValue(
            NL80211_WOWLAN_TRIG_PKT_PATTERN, &data)) {
      struct nl80211_pattern_support *patt_support =
          reinterpret_cast<struct nl80211_pattern_support *>(data.GetData());
      // Determine the IPV4 and IPV6 pattern lengths we will use by
      // constructing dummy patterns and getting their lengths.
      ByteString dummy_pattern;
      ByteString dummy_mask;
      WakeOnWiFi::CreateIPV4PatternAndMask(IPAddress("192.168.0.20"),
                                           &dummy_pattern, &dummy_mask);
      size_t ipv4_pattern_len = dummy_pattern.GetLength();
      WakeOnWiFi::CreateIPV6PatternAndMask(
          IPAddress("FEDC:BA98:7654:3210:FEDC:BA98:7654:3210"), &dummy_pattern,
          &dummy_mask);
      size_t ipv6_pattern_len = dummy_pattern.GetLength();
      // Check if the pattern matching capabilities of this WiFi device will
      // allow IPV4 and IPV6 patterns to be used.
      if (patt_support->min_pattern_len <=
              std::min(ipv4_pattern_len, ipv6_pattern_len) &&
          patt_support->max_pattern_len >=
              std::max(ipv4_pattern_len, ipv6_pattern_len)) {
        wake_on_wifi_triggers_supported_.insert(WakeOnWiFi::kPattern);
        wake_on_wifi_max_patterns_ = patt_support->max_patterns;
        SLOG(this, 7) << "Waking on up to " << wake_on_wifi_max_patterns_
                      << " registered patterns of "
                      << patt_support->min_pattern_len << "-"
                      << patt_support->max_pattern_len
                      << " bytes supported by this WiFi device";
      }
    }
    // TODO(samueltan): remove this placeholder when wake on SSID capability
    // can be parsed from NL80211 message.
    wake_on_wifi_triggers_supported_.insert(WakeOnWiFi::kSSID);
  }
}

void WakeOnWiFi::ParseWiphyIndex(const Nl80211Message &nl80211_message) {
  // Verify NL80211_CMD_NEW_WIPHY.
  if (nl80211_message.command() != NewWiphyMessage::kCommand) {
    LOG(ERROR) << "Received unexpected command:" << nl80211_message.command();
    return;
  }
  if (!nl80211_message.const_attributes()->GetU32AttributeValue(
          NL80211_ATTR_WIPHY, &wiphy_index_)) {
    LOG(ERROR) << "NL80211_CMD_NEW_WIPHY had no NL80211_ATTR_WIPHY";
    return;
  }
  wiphy_index_received_ = true;
}

void WakeOnWiFi::OnBeforeSuspend(
    bool is_connected,
    bool have_service_configured_for_autoconnect,
    const ResultCallback &done_callback,
    const Closure &renew_dhcp_lease_callback,
    const Closure &remove_supplicant_networks_callback,
    bool have_dhcp_lease,
    uint32_t time_to_next_lease_renewal) {
  LOG(INFO) << __func__ << ": "
            << (is_connected ? "connected" : "not connected");
#if defined(DISABLE_WAKE_ON_WIFI)
  // Wake on WiFi disabled, so immediately report success.
  done_callback.Run(Error(Error::kSuccess));
#else
  suspend_actions_done_callback_ = done_callback;
  if (have_dhcp_lease && is_connected &&
      time_to_next_lease_renewal < kImmediateDHCPLeaseRenewalThresholdSeconds) {
    // Renew DHCP lease immediately if we have one that is expiring soon.
    renew_dhcp_lease_callback.Run();
    dispatcher_->PostTask(
        Bind(&WakeOnWiFi::BeforeSuspendActions, weak_ptr_factory_.GetWeakPtr(),
             is_connected, have_service_configured_for_autoconnect, false,
             time_to_next_lease_renewal, remove_supplicant_networks_callback));
  } else {
    dispatcher_->PostTask(Bind(
        &WakeOnWiFi::BeforeSuspendActions, weak_ptr_factory_.GetWeakPtr(),
        is_connected, have_service_configured_for_autoconnect, have_dhcp_lease,
        time_to_next_lease_renewal, remove_supplicant_networks_callback));
  }
#endif  // DISABLE_WAKE_ON_WIFI
}

void WakeOnWiFi::OnAfterResume() {
  LOG(INFO) << __func__;
#if !defined(DISABLE_WAKE_ON_WIFI)
  wake_to_scan_timer_.Stop();
  dhcp_lease_renewal_timer_.Stop();
  if (WakeOnPacketEnabledAndSupported() || WakeOnSSIDEnabledAndSupported()) {
    // Unconditionally disable wake on WiFi on resume if these features
    // were enabled before the last suspend.
    DisableWakeOnWiFi();
  }
#endif  // DISABLE_WAKE_ON_WIFI
}

void WakeOnWiFi::OnDarkResume(
    bool is_connected,
    bool have_service_configured_for_autoconnect,
    const ResultCallback &done_callback,
    const Closure &renew_dhcp_lease_callback,
    const Closure &initiate_scan_callback,
    const Closure &remove_supplicant_networks_callback) {
  LOG(INFO) << __func__ << ": "
            << (is_connected ? "connected" : "not connected");
#if defined(DISABLE_WAKE_ON_WIFI)
  done_callback.Run(Error(Error::kSuccess));
#else
  in_dark_resume_ = true;
  suspend_actions_done_callback_ = done_callback;
  // Assume that we are disconnected if we time out. Consequently, we do not
  // need to start a DHCP lease renewal timer.
  dark_resume_actions_timeout_callback_.Reset(
      Bind(&WakeOnWiFi::BeforeSuspendActions, weak_ptr_factory_.GetWeakPtr(),
           false, have_service_configured_for_autoconnect, false, 0,
           remove_supplicant_networks_callback));
  dispatcher_->PostDelayedTask(dark_resume_actions_timeout_callback_.callback(),
                               DarkResumeActionsTimeoutMilliseconds);

  if (is_connected) {
    renew_dhcp_lease_callback.Run();
  } else {
    remove_supplicant_networks_callback.Run();
    metrics_->NotifyDarkResumeInitiateScan();
    initiate_scan_callback.Run();
  }
#endif  // DISABLE_WAKE_ON_WIFI
}

void WakeOnWiFi::BeforeSuspendActions(
    bool is_connected,
    bool have_service_configured_for_autoconnect,
    bool start_lease_renewal_timer,
    uint32_t time_to_next_lease_renewal,
    const Closure &remove_supplicant_networks_callback) {
  SLOG(this, 3) << __func__ << ": "
                << (is_connected ? "connected" : "not connected");
  // Note: No conditional compilation because all entry points to this functions
  // are already conditionally compiled based on DISABLE_WAKE_ON_WIFI.

  // Create copy so callback can be run despite calling Cancel().
  Closure supplicant_callback_copy(remove_supplicant_networks_callback);
  dark_resume_actions_timeout_callback_.Cancel();

  // Add relevant triggers to be programmed into the NIC.
  wake_on_wifi_triggers_.clear();
  if (!wake_on_packet_connections_.Empty() &&
      WakeOnPacketEnabledAndSupported() && is_connected) {
    SLOG(this, 3) << "Enabling wake on pattern";
    wake_on_wifi_triggers_.insert(kPattern);
  }
  if (WakeOnSSIDEnabledAndSupported()) {
    if (is_connected) {
      SLOG(this, 3) << "Enabling wake on disconnect";
      wake_on_wifi_triggers_.insert(kDisconnect);
      wake_on_wifi_triggers_.erase(kSSID);
      wake_to_scan_timer_.Stop();
      if (start_lease_renewal_timer) {
        // Timer callback is NO-OP since dark resume logic will initiate DHCP
        // lease renewal.
        dhcp_lease_renewal_timer_.Start(
            FROM_HERE, base::TimeDelta::FromSeconds(time_to_next_lease_renewal),
            Bind(&WakeOnWiFi::OnTimerWakeDoNothing, base::Unretained(this)));
      }
    } else {
      SLOG(this, 3) << "Enabling wake on SSID";
      // Force a disconnect in case supplicant is currently in the process of
      // connecting, and remove all networks so scans triggered in dark resume
      // are passive.
      supplicant_callback_copy.Run();
      wake_on_wifi_triggers_.erase(kDisconnect);
      wake_on_wifi_triggers_.insert(kSSID);
      dhcp_lease_renewal_timer_.Stop();
      if (have_service_configured_for_autoconnect) {
        // Only makes sense to wake to scan in dark resume if there is at least
        // one WiFi service that we can auto-connect to after the scan.
        // Timer callback is NO-OP since dark resume logic will initiate scan.
        wake_to_scan_timer_.Start(
            FROM_HERE, base::TimeDelta::FromSeconds(wake_to_scan_frequency_),
            Bind(&WakeOnWiFi::OnTimerWakeDoNothing, base::Unretained(this)));
      }
    }
  }

  if (!in_dark_resume_ && wake_on_wifi_triggers_.empty()) {
    // No need program NIC on normal resume in this case since wake on WiFi
    // would already have been disabled on the last (non-dark) resume.
    LOG(INFO) << "No need to disable wake on WiFi on NIC in regular "
                 "suspend";
    RunAndResetSuspendActionsDoneCallback(Error(Error::kSuccess));
    return;
  }

  in_dark_resume_ = false;
  ApplyWakeOnWiFiSettings();
}

void WakeOnWiFi::OnDHCPLeaseObtained(bool start_lease_renewal_timer,
                                     uint32_t time_to_next_lease_renewal) {
  SLOG(this, 3) << __func__;
  if (in_dark_resume_) {
#if defined(DISABLE_WAKE_ON_WIFI)
    SLOG(this, 2) << "Wake on WiFi not supported, so do nothing";
#else
    // If we obtain a DHCP lease, we are connected, so the callback to have
    // supplicant remove networks will not be invoked in
    // WakeOnWiFi::BeforeSuspendActions. Likewise, we will not use the value of
    // |have_service_configured_for_autoconnect| argument, so pass an arbitrary
    // value.
    BeforeSuspendActions(true, true, start_lease_renewal_timer,
                         time_to_next_lease_renewal, base::Closure());
#endif  // DISABLE_WAKE_ON_WIFI
  } else {
    SLOG(this, 2) << "Not in dark resume, so do nothing";
  }
}

void WakeOnWiFi::ReportConnectedToServiceAfterWake(bool is_connected) {
#if defined(DISABLE_WAKE_ON_WIFI)
  metrics_->NotifyConnectedToServiceAfterWake(
      is_connected
          ? Metrics::kWiFiConnetionStatusAfterWakeOnWiFiDisabledWakeConnected
          : Metrics::
                kWiFiConnetionStatusAfterWakeOnWiFiDisabledWakeNotConnected);
#else
  if (WakeOnSSIDEnabledAndSupported()) {
    // Only logged if wake on WiFi is supported and wake on SSID was enabled to
    // maintain connectivity while suspended.
    metrics_->NotifyConnectedToServiceAfterWake(
        is_connected
            ? Metrics::kWiFiConnetionStatusAfterWakeOnWiFiEnabledWakeConnected
            : Metrics::
                  kWiFiConnetionStatusAfterWakeOnWiFiEnabledWakeNotConnected);
  } else {
    metrics_->NotifyConnectedToServiceAfterWake(
        is_connected
            ? Metrics::kWiFiConnetionStatusAfterWakeOnWiFiDisabledWakeConnected
            : Metrics::
                  kWiFiConnetionStatusAfterWakeOnWiFiDisabledWakeNotConnected);
  }
#endif  // DISABLE_WAKE_ON_WIFI
}

}  // namespace shill
