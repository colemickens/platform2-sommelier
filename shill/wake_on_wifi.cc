// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wake_on_wifi.h"

#include <stdio.h>

#include <set>
#include <utility>
#include <vector>

#include "shill/byte_string.h"
#include "shill/error.h"
#include "shill/ip_address.h"
#include "shill/ip_address_store.h"
#include "shill/nl80211_message.h"

using std::pair;
using std::set;
using std::vector;

namespace shill {

namespace WakeOnWifi {

bool ByteStringPairIsLessThan(const std::pair<ByteString, ByteString> &lhs,
                    const std::pair<ByteString, ByteString> &rhs) {
  // Treat the first value of the pair as the key.
  return ByteString::IsLessThan(lhs.first, rhs.first);
}

void SetMask(ByteString *mask, uint32_t pattern_len, uint32_t offset) {
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

bool CreateIPAddressPatternAndMask(const IPAddress &ip_addr,
                                   ByteString *pattern, ByteString *mask) {
  if (ip_addr.family() == IPAddress::kFamilyIPv4) {
    CreateIPV4PatternAndMask(ip_addr, pattern, mask);
    return true;
  } else if (ip_addr.family() == IPAddress::kFamilyIPv6) {
    CreateIPV6PatternAndMask(ip_addr, pattern, mask);
    return true;
  } else {
    LOG(ERROR) << "Unrecognized IP Address type.";
    return false;
  }
}

void CreateIPV4PatternAndMask(const IPAddress &ip_addr, ByteString *pattern,
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
  WakeOnWifi::SetMask(mask, pattern_len, src_ip_offset);
}

void CreateIPV6PatternAndMask(const IPAddress &ip_addr, ByteString *pattern,
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
  WakeOnWifi::SetMask(mask, pattern_len, src_ip_offset);
}

bool ConfigureWiphyIndex(Nl80211Message *msg, int32_t index) {
  if (!msg->attributes()->CreateU32Attribute(NL80211_ATTR_WIPHY,
                                             "WIPHY index")) {
    return false;
  }
  if (!msg->attributes()->SetU32AttributeValue(NL80211_ATTR_WIPHY, index)) {
    return false;
  }
  return true;
}

bool ConfigureRemoveAllWakeOnPacketMsg(SetWakeOnPacketConnMessage *msg,
                                       uint32_t wiphy_index, Error *error) {
  if (!WakeOnWifi::ConfigureWiphyIndex(msg, wiphy_index)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Failed to configure Wiphy index.");
    return false;
  }
  return true;
}

bool ConfigureAddWakeOnPacketMsg(SetWakeOnPacketConnMessage *msg,
                                 const IPAddressStore &addrs,
                                 uint32_t wiphy_index, Error *error) {
  if (!WakeOnWifi::ConfigureWiphyIndex(msg, wiphy_index)) {
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
  if (!triggers->CreateNestedAttribute(NL80211_WOWLAN_TRIG_PKT_PATTERN,
                                       "Pattern trigger")) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Could not create nested attribute "
                          "NL80211_WOWLAN_TRIG_PKT_PATTERN for "
                          "SetWakeOnPacketConnMessage.");
    return false;
  }
  if (!triggers->SetNestedAttributeHasAValue(NL80211_WOWLAN_TRIG_PKT_PATTERN)) {
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

  return true;
}

bool CreateSinglePattern(const IPAddress &ip_addr, AttributeListRefPtr patterns,
                         uint8_t patnum, Error *error) {
  ByteString pattern;
  ByteString mask;
  WakeOnWifi::CreateIPAddressPatternAndMask(ip_addr, &pattern, &mask);
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

bool ConfigureGetWakeOnPacketMsg(GetWakeOnPacketConnMessage *msg,
                                 uint32_t wiphy_index, Error *error) {
  if (!WakeOnWifi::ConfigureWiphyIndex(msg, wiphy_index)) {
    Error::PopulateAndLog(error, Error::kOperationFailed,
                          "Failed to configure Wiphy index.");
    return false;
  }
  return true;
}

bool WakeOnPacketSettingsMatch(const Nl80211Message &msg,
                               const IPAddressStore &addrs) {
  if (msg.command() != NL80211_CMD_GET_WOWLAN &&
      msg.command() != NL80211_CMD_SET_WOWLAN) {
    LOG(ERROR) << "Invalid message command";
    return false;
  }
  set<pair<ByteString, ByteString>,
      bool (*)(const pair<ByteString, ByteString> &,
               const pair<ByteString, ByteString> &)>
      expected_patt_mask_pairs(ByteStringPairIsLessThan);
  ByteString temp_pattern;
  ByteString temp_mask;
  for (const IPAddress &addr : addrs.GetIPAddresses()) {
    temp_pattern.Clear();
    temp_mask.Clear();
    WakeOnWifi::CreateIPAddressPatternAndMask(addr, &temp_pattern, &temp_mask);
    expected_patt_mask_pairs.emplace(temp_pattern, temp_mask);
  }
  AttributeListConstRefPtr triggers;
  if (!msg.const_attributes()->ConstGetNestedAttributeList(
          NL80211_ATTR_WOWLAN_TRIGGERS, &triggers)) {
    // No triggers in the returned message, which is valid iff the NIC has no
    // wake-on-packet settings currently programmed into it.
    return (expected_patt_mask_pairs.size() == 0);
  }
  AttributeListConstRefPtr patterns;
  if (!triggers->ConstGetNestedAttributeList(NL80211_WOWLAN_TRIG_PKT_PATTERN,
                                             &patterns)) {
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
    if (!patterns->ConstGetNestedAttributeList(pattern_index, &pattern_info)) {
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
    if (expected_patt_mask_pairs.find(
            pair<ByteString, ByteString>(returned_pattern, returned_mask)) ==
        expected_patt_mask_pairs.end()) {
      mismatch_found = true;
      break;
    } else {
      --num_mismatch;
    }
    pattern_iter.Advance();
  }
  return (!mismatch_found && !num_mismatch);
}

}  // namespace WakeOnWifi

}  // namespace shill
