// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WAKE_ON_WIFI_H_
#define SHILL_WAKE_ON_WIFI_H_

#include <base/basictypes.h>

namespace shill {

class ByteString;
class Error;
class IPAddress;
class SetWakeOnPacketConnMessage;

namespace WakeOnWifi {

// Creates a mask which specifies which bytes in pattern of length |pattern_len|
// to match against. Bits |offset| to |pattern_len| - 1 are set, which bits 0 to
// bits 0 to |offset| - 1 are unset. This mask is saved in |mask|.
void SetMask(ByteString *mask, uint32_t pattern_len, uint32_t offset);
// Creates a pattern and mask for an NL80211 message that programs the NIC to
// wake on packets originating from IP address |ip_addr|. The pattern and mask
// are saved in |pattern| and |mask| respectively.
bool CreateIPAddressPatternAndMask(const IPAddress &ip_addr,
                                   ByteString *pattern, ByteString *mask);
void CreateIPV4PatternAndMask(const IPAddress &ip_addr, ByteString *pattern,
                              ByteString *mask);
void CreateIPV6PatternAndMask(const IPAddress &ip_addr, ByteString *pattern,
                              ByteString *mask);
// Creates and sets an attribute in an NL80211 message |msg| which indicates the
// index of the wiphy device to program.
bool ConfigureWiphyIndex(SetWakeOnPacketConnMessage *msg, int32_t index);
// Creates and sets attributes in an NL80211 message |msg| so that the message
// will deprogram all wake-on-packet rules from the NIC with wiphy index
// |wiphy_index|.
// NOTE: Assumes that |msg| has not been altered since construction.
bool ConfigureRemoveAllWakeOnPacketMsg(SetWakeOnPacketConnMessage *msg,
                                       uint32_t wiphy_index, Error *error);
// Creates and sets attributes in an NL80211 message |msg| so that the message
// will program the NIC with wiphy index |wiphy_index| to wake on packets
// originating from IP address |ip_addr|.
// NOTE: Assumes that |msg| has not been altered since construction.
bool ConfigureAddWakeOnPacketMsg(SetWakeOnPacketConnMessage *msg,
                                 const IPAddress &ip_addr, uint32_t wiphy_index,
                                 Error *error);

}  // namespace WakeOnWifi

}  // namespace shill

#endif  // SHILL_WAKE_ON_WIFI_H_
