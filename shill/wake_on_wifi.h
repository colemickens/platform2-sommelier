// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WAKE_ON_WIFI_H_
#define SHILL_WAKE_ON_WIFI_H_

#include <linux/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <set>
#include <utility>
#include <vector>

#include "shill/refptr_types.h"

namespace shill {

class ByteString;
class Error;
class IPAddress;
class IPAddressStore;
class GetWakeOnPacketConnMessage;
class Nl80211Message;
class SetWakeOnPacketConnMessage;

namespace WakeOnWiFi {

// Types of triggers that can cause the NIC to wake the WiFi device.
enum WakeOnWiFiTrigger { kIPAddress, kDisconnect };

// Used for comparison of ByteString pairs in a set.
bool ByteStringPairIsLessThan(const std::pair<ByteString, ByteString> &lhs,
                              const std::pair<ByteString, ByteString> &rhs);
// Creates a mask which specifies which bytes in pattern of length |pattern_len|
// to match against. Bits |offset| to |pattern_len| - 1 are set, which bits 0 to
// bits 0 to |offset| - 1 are unset. This mask is saved in |mask|.
void SetMask(ByteString *mask, uint32_t pattern_len, uint32_t offset);
// Creates a pattern and mask for a NL80211 message that programs the NIC to
// wake on packets originating from IP address |ip_addr|. The pattern and mask
// are saved in |pattern| and |mask| respectively. Returns true iff the pattern
// and mask are successfully created and written to |pattern| and |mask|
// respectively.
bool CreateIPAddressPatternAndMask(const IPAddress &ip_addr,
                                   ByteString *pattern, ByteString *mask);
void CreateIPV4PatternAndMask(const IPAddress &ip_addr, ByteString *pattern,
                              ByteString *mask);
void CreateIPV6PatternAndMask(const IPAddress &ip_addr, ByteString *pattern,
                              ByteString *mask);
// Creates and sets an attribute in a NL80211 message |msg| which indicates the
// index of the wiphy device to program. Returns true iff |msg| is successfully
// configured.
bool ConfigureWiphyIndex(Nl80211Message *msg, int32_t index);
// Creates and sets attributes in an SetWakeOnPacketConnMessage |msg| so that
// the message will disable wake-on-packet functionality of the NIC with wiphy
// index |wiphy_index|. Returns true iff |msg| is successfully configured.
// NOTE: Assumes that |msg| has not been altered since construction.
bool ConfigureDisableWakeOnWiFiMessage(SetWakeOnPacketConnMessage *msg,
                                       uint32_t wiphy_index, Error *error);
// Creates and sets attributes in a SetWakeOnPacketConnMessage |msg|
// so that the message will program the NIC with wiphy index |wiphy_index| with
// wake on wireless triggers in |trigs|. If |trigs| contains the
// kIPAddress trigger, the NIC is programmed to wake on packets from the
// IP addresses in |addrs|. Returns true iff |msg| is successfully configured.
// NOTE: Assumes that |msg| has not been altered since construction.
bool ConfigureSetWakeOnWiFiSettingsMessage(
    SetWakeOnPacketConnMessage *msg,
    const std::set<WakeOnWiFiTrigger> &trigs,
    const IPAddressStore &addrs, uint32_t wiphy_index, Error *error);
// Helper function to ConfigureSetWakeOnWiFiSettingsMessage that creates a
// single nested attribute inside the attribute list referenced by |patterns|
// representing a wake-on-packet pattern matching rule with index |patnum|.
// Returns true iff the attribute is successfully created and set.
// NOTE: |patterns| is assumed to reference the nested attribute list
// NL80211_WOWLAN_TRIG_PKT_PATTERN.
// NOTE: |patnum| should be unique across multiple calls to this function to
// prevent the formation of a erroneous nl80211 message or the overwriting of
// pattern matching rules.
bool CreateSinglePattern(const IPAddress &ip_addr, AttributeListRefPtr patterns,
                         uint8_t patnum, Error *error);
// Creates and sets attributes in an GetWakeOnPacketConnMessage msg| so that
// the message will request for wake-on-packet settings information from the NIC
// with wiphy index |wiphy_index|. Returns true iff |msg| is successfully
// configured.
// NOTE: Assumes that |msg| has not been altered since construction.
bool ConfigureGetWakeOnWiFiSettingsMessage(GetWakeOnPacketConnMessage *msg,
                                       uint32_t wiphy_index, Error *error);
// Given a NL80211_CMD_GET_WOWLAN response or NL80211_CMD_SET_WOWLAN request
// |msg|, returns true iff the wake-on-wifi trigger settings in |msg| match
// those in |trigs|. Checks source IP addresses in |msg| match those in |addrs|
// if the kIPAddress trigger is in |trigs|.
bool WakeOnWiFiSettingsMatch(const Nl80211Message &msg,
                             const std::set<WakeOnWiFiTrigger> &trigs,
                             const IPAddressStore &addrs);

}  // namespace WakeOnWiFi

}  // namespace shill

#endif  // SHILL_WAKE_ON_WIFI_H_
