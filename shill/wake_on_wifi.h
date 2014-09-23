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

#include <base/cancelable_callback.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>

#include "shill/callbacks.h"
#include "shill/ip_address.h"
#include "shill/ip_address_store.h"
#include "shill/refptr_types.h"

namespace shill {

class ByteString;
class Error;
class EventDispatcher;
class GetWakeOnPacketConnMessage;
class NetlinkManager;
class Nl80211Message;
class SetWakeOnPacketConnMessage;
class WiFi;

class WakeOnWiFi {
 public:
  WakeOnWiFi(NetlinkManager *netlink_manager, EventDispatcher *dispatcher);
  ~WakeOnWiFi();

  // Types of triggers that can cause the NIC to wake the WiFi device.
  enum WakeOnWiFiTrigger { kIPAddress, kDisconnect };

  // Enable the NIC to wake on packets received from |ip_endpoint|.
  // Note: The actual programming of the NIC only happens before the system
  // suspends, in |OnBeforeSuspend|.
  void AddWakeOnPacketConnection(const IPAddress &ip_endpoint, Error *error);
  // Remove rule to wake on packets received from |ip_endpoint| from the NIC.
  // Note: The actual programming of the NIC only happens before the system
  // suspends, in |OnBeforeSuspend|.
  void RemoveWakeOnPacketConnection(const IPAddress &ip_endpoint, Error *error);
  // Remove all rules to wake on incoming packets from the NIC.
  // Note: The actual programming of the NIC only happens before the system
  // suspends, in |OnBeforeSuspend|.
  void RemoveAllWakeOnPacketConnections(Error *error);
  // Given a NL80211_CMD_NEW_WIPHY message |nl80211_message|, parses the
  // wake on wifi capabilities of the NIC and set relevant members of this
  // WakeOnWiFi object to reflect the supported capbilities.
  void ParseWakeOnWiFiCapabilities(const Nl80211Message &nl80211_message);
  // Given a NL80211_CMD_NEW_WIPHY message |nl80211_message|, parses the
  // wiphy index of the NIC and sets |wiphy_index_| with the parsed index.
  void ParseWiphyIndex(const Nl80211Message &nl80211_message);
  // Performs pre-suspend actions relevant to wake on wireless functionality.
  void OnBeforeSuspend(const ResultCallback &callback);
  // Performs post-resume actions relevant to wake on wireless functionality.
  void OnAfterResume();

 private:
  friend class WakeOnWiFiTest;  // access to several members for tests
  friend class WiFiObjectTest;  // netlink_manager_
  FRIEND_TEST(WakeOnWiFiTest, ParseWiphyIndex_Success);  // kDefaultWiphyIndex
  // Tests that need kMaxSetWakeOnPacketRetries
  FRIEND_TEST(WakeOnWiFiTest,
              RetrySetWakeOnPacketConnections_LessThanMaxRetries);
  FRIEND_TEST(WakeOnWiFiTest,
              RetrySetWakeOnPacketConnections_MaxAttemptsWithCallbackSet);
  FRIEND_TEST(WakeOnWiFiTest,
              RetrySetWakeOnPacketConnections_MaxAttemptsCallbackUnset);

  static const uint32_t kDefaultWiphyIndex;
  static const int kVerifyWakeOnWiFiSettingsDelaySeconds;
  static const int kMaxSetWakeOnPacketRetries;

  // Used for comparison of ByteString pairs in a set.
  static bool ByteStringPairIsLessThan(
      const std::pair<ByteString, ByteString> &lhs,
      const std::pair<ByteString, ByteString> &rhs);
  // Creates a mask which specifies which bytes in pattern of length
  // |pattern_len| to match against. Bits |offset| to |pattern_len| - 1 are set,
  // which bits 0 to bits 0 to |offset| - 1 are unset. This mask is saved in
  // |mask|.
  static void SetMask(ByteString *mask, uint32_t pattern_len, uint32_t offset);
  // Creates a pattern and mask for a NL80211 message that programs the NIC to
  // wake on packets originating from IP address |ip_addr|. The pattern and mask
  // are saved in |pattern| and |mask| respectively. Returns true iff the
  // pattern and mask are successfully created and written to |pattern| and
  // |mask| respectively.
  static bool CreateIPAddressPatternAndMask(const IPAddress &ip_addr,
                                            ByteString *pattern,
                                            ByteString *mask);
  static void CreateIPV4PatternAndMask(const IPAddress &ip_addr,
                                       ByteString *pattern, ByteString *mask);
  static void CreateIPV6PatternAndMask(const IPAddress &ip_addr,
                                       ByteString *pattern, ByteString *mask);
  // Creates and sets an attribute in a NL80211 message |msg| which indicates
  // the index of the wiphy interface to program. Returns true iff |msg| is
  // successfully configured.
  static bool ConfigureWiphyIndex(Nl80211Message *msg, int32_t index);
  // Creates and sets attributes in an SetWakeOnPacketConnMessage |msg| so that
  // the message will disable wake-on-packet functionality of the NIC with wiphy
  // index |wiphy_index|. Returns true iff |msg| is successfully configured.
  // NOTE: Assumes that |msg| has not been altered since construction.
  static bool ConfigureDisableWakeOnWiFiMessage(SetWakeOnPacketConnMessage *msg,
                                                uint32_t wiphy_index,
                                                Error *error);
  // Creates and sets attributes in a SetWakeOnPacketConnMessage |msg|
  // so that the message will program the NIC with wiphy index |wiphy_index|
  // with wake on wireless triggers in |trigs|. If |trigs| contains the
  // kIPAddress trigger, the NIC is programmed to wake on packets from the
  // IP addresses in |addrs|. Returns true iff |msg| is successfully configured.
  // NOTE: Assumes that |msg| has not been altered since construction.
  static bool ConfigureSetWakeOnWiFiSettingsMessage(
      SetWakeOnPacketConnMessage *msg, const std::set<WakeOnWiFiTrigger> &trigs,
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
  static bool CreateSinglePattern(const IPAddress &ip_addr,
                                  AttributeListRefPtr patterns, uint8_t patnum,
                                  Error *error);
  // Creates and sets attributes in an GetWakeOnPacketConnMessage msg| so that
  // the message will request for wake-on-packet settings information from the
  // NIC with wiphy index |wiphy_index|. Returns true iff |msg| is successfully
  // configured.
  // NOTE: Assumes that |msg| has not been altered since construction.
  static bool ConfigureGetWakeOnWiFiSettingsMessage(
      GetWakeOnPacketConnMessage *msg, uint32_t wiphy_index, Error *error);
  // Given a NL80211_CMD_GET_WOWLAN response or NL80211_CMD_SET_WOWLAN request
  // |msg|, returns true iff the wake-on-wifi trigger settings in |msg| match
  // those in |trigs|. Checks that source IP addresses in |msg| match those in
  // |addrs| if the kIPAddress flag is in |trigs|.
  static bool WakeOnWiFiSettingsMatch(const Nl80211Message &msg,
                                      const std::set<WakeOnWiFiTrigger> &trigs,
                                      const IPAddressStore &addrs);
  // Message handler for NL80211_CMD_SET_WOWLAN responses.
  static void OnSetWakeOnPacketConnectionResponse(
      const Nl80211Message &nl80211_message);
  // Request wake on WiFi settings for this WiFi device.
  void RequestWakeOnPacketSettings();
  // Verify that the wake on WiFi settings programmed into the NIC match
  // those recorded locally for this device in |wake_on_packet_connections_|
  // and |wake_on_wifi_triggers_|.
  void VerifyWakeOnWiFiSettings(const Nl80211Message &nl80211_message);
  // Sends an NL80211 message to program the NIC with wake on WiFi settings
  // configured in |wake_on_packet_connections_| and |wake_on_wifi_triggers_|.
  // If |wake_on_wifi_triggers_| is empty, calls |DisableWakeOnWiFi|.
  void ApplyWakeOnWiFiSettings();
  // Helper function called by |ApplyWakeOnWiFiSettings| that sends an NL80211
  // message to program the NIC to disable wake on WiFi.
  void DisableWakeOnWiFi();
  // Calls |ApplyWakeOnWiFiSettings| and counts this call as
  // a retry. If |kMaxSetWakeOnPacketRetries| retries have already been
  // performed, resets counter and returns.
  void RetrySetWakeOnPacketConnections();

  // Pointers to objects owned by the WiFi object that created this object.
  EventDispatcher *dispatcher_;
  NetlinkManager *netlink_manager_;
  // Executes after the NIC's wake-on-packet settings are configured via
  // NL80211 messages to verify that the new configuration has taken effect.
  // Calls RequestWakeOnPacketSettings.
  base::CancelableClosure verify_wake_on_packet_settings_callback_;
  // Callback to be invoked after all suspend actions finish executing.
  ResultCallback suspend_actions_done_callback_;
  // Number of retry attempts to program the NIC's wake-on-packet settings.
  int num_set_wake_on_packet_retries_;
  // Keeps track of triggers that the NIC will be programmed to wake from
  // while suspended.
  std::set<WakeOnWiFi::WakeOnWiFiTrigger> wake_on_wifi_triggers_;
  // Keeps track of what wake on wifi triggers this WiFi device supports.
  std::set<WakeOnWiFi::WakeOnWiFiTrigger> wake_on_wifi_triggers_supported_;
  // Max number of patterns this WiFi device can be programmed to wake on
  // at one time.
  size_t wake_on_wifi_max_patterns_;
  // Keeps track of IP addresses whose packets this device will wake upon
  // receiving while the device is suspended.
  IPAddressStore wake_on_packet_connections_;
  uint32_t wiphy_index_;
  bool wiphy_index_received_;
  base::WeakPtrFactory<WakeOnWiFi> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WakeOnWiFi);
};

}  // namespace shill

#endif  // SHILL_WAKE_ON_WIFI_H_
