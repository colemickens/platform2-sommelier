// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wake_on_wifi.h"

#include <errno.h>
#include <linux/nl80211.h>
#include <stdio.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/cancelable_callback.h>

#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/ip_address_store.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/net/netlink_manager.h"
#include "shill/net/nl80211_message.h"
#include "shill/property_accessor.h"
#include "shill/wifi.h"

using base::Bind;
using std::pair;
using std::set;
using std::string;
using std::vector;

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
const int WakeOnWiFi::kVerifyWakeOnWiFiSettingsDelaySeconds = 1;
const int WakeOnWiFi::kMaxSetWakeOnPacketRetries = 2;

WakeOnWiFi::WakeOnWiFi(NetlinkManager *netlink_manager,
                       EventDispatcher *dispatcher, Manager *manager)
    : dispatcher_(dispatcher),
      netlink_manager_(netlink_manager),
      manager_(manager),
      num_set_wake_on_packet_retries_(0),
      wake_on_wifi_max_patterns_(0),
      wiphy_index_(kDefaultWiphyIndex),
      wiphy_index_received_(false),
#if defined(DISABLE_WAKE_ON_WIFI)
      wake_on_wifi_features_enabled_(kWakeOnWiFiFeaturesEnabledNone),
#else
      wake_on_wifi_features_enabled_(kWakeOnWiFiFeaturesEnabledSSID),
#endif  // DISABLE_WAKE_ON_WIFI
      weak_ptr_factory_(this) {
}

WakeOnWiFi::~WakeOnWiFi() {}

void WakeOnWiFi::InitPropertyStore(PropertyStore *store) {
  store->RegisterDerivedString(
      kWakeOnWiFiFeaturesEnabledProperty,
      StringAccessor(new CustomAccessor<WakeOnWiFi, string>(
          this, &WakeOnWiFi::GetWakeOnWiFiFeaturesEnabled,
          &WakeOnWiFi::SetWakeOnWiFiFeaturesEnabled)));
}

string WakeOnWiFi::GetWakeOnWiFiFeaturesEnabled(Error *error) {
  return wake_on_wifi_features_enabled_;
}

bool WakeOnWiFi::SetWakeOnWiFiFeaturesEnabled(const std::string &enabled,
                                              Error *error) {
  if (wake_on_wifi_features_enabled_ == enabled) {
    return false;
  }
#if defined(DISABLE_WAKE_ON_WIFI)
  if (enabled != kWakeOnWiFiFeaturesEnabledNone) {
    Error::PopulateAndLog(error, Error::kNotSupported,
                          "Wake on WiFi is not supported");
    return false;
  }
#else
  if (enabled != kWakeOnWiFiFeaturesEnabledPacket &&
      enabled != kWakeOnWiFiFeaturesEnabledSSID &&
      enabled != kWakeOnWiFiFeaturesEnabledPacketSSID &&
      enabled != kWakeOnWiFiFeaturesEnabledNone) {
    Error::PopulateAndLog(error, Error::kInvalidArguments,
                          "Invalid Wake on WiFi feature");
    return false;
  }
#endif  // DISABLE_WAKE_ON_WIFI
  wake_on_wifi_features_enabled_ = enabled;
  return true;
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
  if (trigs.find(kIPAddress) != trigs.end() && addrs.Empty()) {
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
      case kIPAddress: {
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
      case kIPAddress: {
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
  if (!WakeOnPacketEnabled()) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          kWakeOnPacketDisabled);
    return;
  }
  if (wake_on_wifi_triggers_supported_.find(kIPAddress) ==
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
  if (!WakeOnPacketEnabled()) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          kWakeOnPacketDisabled);
    return;
  }
  if (wake_on_wifi_triggers_supported_.find(kIPAddress) ==
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
  if (!WakeOnPacketEnabled()) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          kWakeOnPacketDisabled);
    return;
  }
  if (wake_on_wifi_triggers_supported_.find(kIPAddress) ==
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
      error.Populate(Error::kOperationTimeout, "Timeout waiting for response");
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
  if (WakeOnWiFiSettingsMatch(nl80211_message, wake_on_wifi_triggers_,
                              wake_on_packet_connections_)) {
    SLOG(this, 2) << __func__ << ": "
                  << "Wake-on-packet settings successfully verified";
    RunAndResetSuspendActionsDoneCallback(Error(Error::kSuccess));
  } else {
    LOG(ERROR) << __func__ << " failed: discrepancy between wake-on-packet "
                              "settings on NIC and those in local data "
                              "structure detected";
    RetrySetWakeOnPacketConnections();
  }
}

void WakeOnWiFi::ApplyWakeOnWiFiSettings() {
  if (!wiphy_index_received_) {
    LOG(ERROR) << "Interface index not yet received";
    return;
  }
  if (wake_on_wifi_triggers_.empty()) {
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
      kVerifyWakeOnWiFiSettingsDelaySeconds * 1000);
}

void WakeOnWiFi::DisableWakeOnWiFi() {
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
      kVerifyWakeOnWiFiSettingsDelaySeconds * 1000);
}

void WakeOnWiFi::RetrySetWakeOnPacketConnections() {
  if (num_set_wake_on_packet_retries_ < kMaxSetWakeOnPacketRetries) {
    SLOG(this, 2) << __func__;
    ApplyWakeOnWiFiSettings();
    ++num_set_wake_on_packet_retries_;
  } else {
    SLOG(this, 2) << __func__ << ": max retry attempts reached";
    num_set_wake_on_packet_retries_ = 0;
    RunAndResetSuspendActionsDoneCallback(Error(Error::kOperationFailed));
  }
}

bool WakeOnWiFi::WakeOnPacketEnabled() {
  return (wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledPacket ||
          wake_on_wifi_features_enabled_ ==
              kWakeOnWiFiFeaturesEnabledPacketSSID);
}

bool WakeOnWiFi::WakeOnSSIDEnabled() {
  return (wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledSSID ||
          wake_on_wifi_features_enabled_ ==
              kWakeOnWiFiFeaturesEnabledPacketSSID);
}

bool WakeOnWiFi::WakeOnWiFiFeaturesDisabled() {
  return wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledNone;
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
        wake_on_wifi_triggers_supported_.insert(WakeOnWiFi::kIPAddress);
        wake_on_wifi_max_patterns_ = patt_support->max_patterns;
        SLOG(this, 7) << "Waking on up to " << wake_on_wifi_max_patterns_
                      << " registered patterns of "
                      << patt_support->min_pattern_len << "-"
                      << patt_support->max_pattern_len
                      << " bytes supported by this WiFi device";
      }
    }
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

void WakeOnWiFi::OnBeforeSuspend(const ResultCallback &callback) {
#if defined(DISABLE_WAKE_ON_WIFI)
  // Wake on WiFi disabled, so immediately report success.
  callback.Run(Error(Error::kSuccess));
#else
  if (wake_on_wifi_triggers_supported_.empty() ||
      WakeOnWiFiFeaturesDisabled()) {
    callback.Run(Error(Error::kSuccess));
    return;
  }

  if (wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledPacket) {
    wake_on_wifi_triggers_.insert(WakeOnWiFi::kIPAddress);
  } else if (wake_on_wifi_features_enabled_ == kWakeOnWiFiFeaturesEnabledSSID) {
    wake_on_wifi_triggers_.insert(WakeOnWiFi::kDisconnect);
    // TODO(samueltan): insert wake on SSID relevant triggers here once
    // they become available.
  } else {
    DCHECK(wake_on_wifi_features_enabled_ ==
           kWakeOnWiFiFeaturesEnabledPacketSSID);
    wake_on_wifi_triggers_.insert(WakeOnWiFi::kDisconnect);
    wake_on_wifi_triggers_.insert(WakeOnWiFi::kIPAddress);
    // TODO(samueltan): insert wake on SSID relevant triggers here once
    // they become available.
  }

  if (wake_on_wifi_triggers_.find(WakeOnWiFi::kIPAddress) !=
          wake_on_wifi_triggers_.end() &&
      wake_on_packet_connections_.Empty()) {
    // Do not program NIC to wake on IP address patterns if no wake on packet
    // connections have been registered.
    wake_on_wifi_triggers_.erase(WakeOnWiFi::kIPAddress);
    if (wake_on_wifi_triggers_.empty()) {
      // Optimization: report success and return immediately instead of
      // asynchronously calling WakeOnWiFi::ApplyWakeOnPacketSettings.
      callback.Run(Error(Error::kSuccess));
      return;
    }
  }

  suspend_actions_done_callback_ = callback;
  dispatcher_->PostTask(Bind(&WakeOnWiFi::ApplyWakeOnWiFiSettings,
                             weak_ptr_factory_.GetWeakPtr()));
#endif  // DISABLE_WAKE_ON_WIFI
}

void WakeOnWiFi::OnAfterResume() {
#if !defined(DISABLE_WAKE_ON_WIFI)
  // Unconditionally disable wake on WiFi on resume.
  if (!wake_on_wifi_triggers_supported_.empty() &&
      !WakeOnWiFiFeaturesDisabled()) {
    wake_on_wifi_triggers_.clear();
    ApplyWakeOnWiFiSettings();
  }
#endif  // DISABLE_WAKE_ON_WIFI
}

}  // namespace shill
