// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIFI_WAKE_ON_WIFI_H_
#define SHILL_WIFI_WAKE_ON_WIFI_H_

#include <linux/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/cancelable_callback.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <components/timers/alarm_timer.h>

#include "shill/callbacks.h"
#include "shill/ip_address_store.h"
#include "shill/net/ip_address.h"
#include "shill/net/netlink_manager.h"
#include "shill/refptr_types.h"

namespace shill {

class ByteString;
class Error;
class EventDispatcher;
class GetWakeOnPacketConnMessage;
class Metrics;
class Nl80211Message;
class PropertyStore;
class SetWakeOnPacketConnMessage;
class WiFi;

class WakeOnWiFi {
 public:
  WakeOnWiFi(NetlinkManager *netlink_manager, EventDispatcher *dispatcher,
             Metrics *metrics);
  virtual ~WakeOnWiFi();

  // Registers |store| with properties related to wake on WiFi.
  void InitPropertyStore(PropertyStore *store);

  // Starts |metrics_timer_| so that wake on WiFi related metrics are
  // periodically collected.
  void StartMetricsTimer();

  // Types of triggers that can cause the NIC to wake the WiFi device.
  enum WakeOnWiFiTrigger { kPattern, kDisconnect, kSSID };

  // Enable the NIC to wake on packets received from |ip_endpoint|.
  // Note: The actual programming of the NIC only happens before the system
  // suspends, in |OnBeforeSuspend|.
  void AddWakeOnPacketConnection(const std::string &ip_endpoint, Error *error);
  // Remove rule to wake on packets received from |ip_endpoint| from the NIC.
  // Note: The actual programming of the NIC only happens before the system
  // suspends, in |OnBeforeSuspend|.
  void RemoveWakeOnPacketConnection(const std::string &ip_endpoint,
                                    Error *error);
  // Remove all rules to wake on incoming packets from the NIC.
  // Note: The actual programming of the NIC only happens before the system
  // suspends, in |OnBeforeSuspend|.
  void RemoveAllWakeOnPacketConnections(Error *error);
  // Given a NL80211_CMD_NEW_WIPHY message |nl80211_message|, parses the
  // wake on WiFi capabilities of the NIC and set relevant members of this
  // WakeOnWiFi object to reflect the supported capbilities.
  void ParseWakeOnWiFiCapabilities(const Nl80211Message &nl80211_message);
  // Given a NL80211_CMD_NEW_WIPHY message |nl80211_message|, parses the
  // wiphy index of the NIC and sets |wiphy_index_| with the parsed index.
  void ParseWiphyIndex(const Nl80211Message &nl80211_message);
  // Performs pre-suspend actions relevant to wake on wireless functionality.
  // Initiates DHCP lease renewal if there is a lease due to renewal soon,
  // then calls WakeOnWiFi::BeforeSuspendActions.
  //
  // Arguments:
  //  - |is_connected|: whether the WiFi device is connected.
  //  - |done_callback|: callback to invoke when suspend  actions have
  //    completed.
  //  - |renew_dhcp_lease_callback|: callback to invoke to initiate DHCP lease
  //    renewal.
  //  - |remove_supplicant_networks_callback|: callback to invoke
  //    to remove all networks from WPA supplicant.
  //  - |have_dhcp_lease|: whether or not there is a DHCP lease to renew.
  //  - |time_to_next_lease_renewal|: number of seconds until next DHCP lease
  //    renewal is due.
  virtual void OnBeforeSuspend(
      bool is_connected,
      const ResultCallback &done_callback,
      const base::Closure &renew_dhcp_lease_callback,
      const base::Closure &remove_supplicant_networks_callback,
      bool have_dhcp_lease,
      uint32_t time_to_next_lease_renewal);
  // Performs post-resume actions relevant to wake on wireless functionality.
  virtual void OnAfterResume();
  // Performs and post actions to be performed in dark resume.
  // When we wake up in dark resume and wake on WiFi is supported, we start a
  // timeout timer, and do one of two things depending on whether the WiFi
  // device is connected to a service:
  //   - If the WiFi device is connected before suspend, initiate DHCP lease
  //     renewal.
  //   - Otherwise, WiFi device is not connected, so initiate a scan.
  // There are two possible outcomes from these actions:
  //   - A DHCP lease is obtained, either because of connection to a network
  //     or successful lease renewal.
  //   - The timer expires, so dark resume actions time out.
  // In either case, WakeOnWiFi::BeforeSuspendActions is invoked asynchronously.
  // If WiFi device is connected (in the case where we got the DHCP lease),
  // we start the lease renewal timer, disable wake on SSID, and enable wake on
  // disconnect. Otherwise, we stop the lease renewal timer, enable wake on
  // SSID, disable wake on disconnect, and remove all networks from WPA
  // supplicant. This is the same logic applied before regular suspend, and
  // should ensure that the correct actions are taken both before regular
  // suspend and suspend in dark resume to maintain connectivity.
  //
  // Arguments:
  //  - |is_connected|: whether the WiFi device is connected.
  //  - |done_callback|: callback to invoke when dark resume actions have
  //    completed.
  //  - |renew_dhcp_lease_callback|: callback to invoke to initiate DHCP lease
  //    renewal.
  //  - |initate_scan_callback|: callback to invoke to initiate a scan.
  //  - |remove_supplicant_networks_callback|: callback to invoke
  //    to remove all networks from WPA supplicant.
  virtual void OnDarkResume(
      bool is_connected,
      const ResultCallback &done_callback,
      const base::Closure &renew_dhcp_lease_callback,
      const base::Closure &initiate_scan_callback,
      const base::Closure &remove_supplicant_networks_callback);
  // Wrapper around WakeOnWiFi::BeforeSuspendActions that checks if shill is
  // currently in dark resume before invoking the function.
  virtual void OnDHCPLeaseObtained(bool start_lease_renewal_timer,
                                   uint32_t time_to_next_lease_renewal);

 private:
  friend class WakeOnWiFiTest;  // access to several members for tests
  friend class WiFiObjectTest;  // netlink_manager_
  // Tests that need kWakeOnWiFiDisabled.
  FRIEND_TEST(WakeOnWiFiTestWithMockDispatcher,
              WakeOnWiFiDisabled_AddWakeOnPacketConnection_ReturnsError);
  FRIEND_TEST(WakeOnWiFiTestWithMockDispatcher,
              WakeOnWiFiDisabled_RemoveWakeOnPacketConnection_ReturnsError);
  FRIEND_TEST(WakeOnWiFiTestWithMockDispatcher,
              WakeOnWiFiDisabled_RemoveAllWakeOnPacketConnections_ReturnsError);
  FRIEND_TEST(WakeOnWiFiTestWithMockDispatcher,
              ParseWiphyIndex_Success);  // kDefaultWiphyIndex
  // Tests that need kMaxSetWakeOnPacketRetries.
  FRIEND_TEST(WakeOnWiFiTestWithMockDispatcher,
              RetrySetWakeOnPacketConnections_LessThanMaxRetries);
  FRIEND_TEST(WakeOnWiFiTestWithMockDispatcher,
              RetrySetWakeOnPacketConnections_MaxAttemptsWithCallbackSet);
  FRIEND_TEST(WakeOnWiFiTestWithMockDispatcher,
              RetrySetWakeOnPacketConnections_MaxAttemptsCallbackUnset);
  // Tests that need WakeOnWiFi::kDarkResumeActionsTimeoutMilliseconds
  FRIEND_TEST(WakeOnWiFiTestWithMockDispatcher,
              OnBeforeSuspend_DHCPLeaseRenewal);

  static const char kWakeOnIPAddressPatternsNotSupported[];
  static const char kWakeOnPacketDisabled[];
  static const char kWakeOnWiFiDisabled[];
  static const uint32_t kDefaultWiphyIndex;
  static const int kVerifyWakeOnWiFiSettingsDelayMilliseconds;
  static const int kMaxSetWakeOnPacketRetries;
  static const int kMetricsReportingFrequencySeconds;
  static const uint32_t kDefaultWakeToScanFrequencySeconds;
  static const uint32_t kImmediateDHCPLeaseRenewalThresholdSeconds;
  static int64_t DarkResumeActionsTimeoutMilliseconds;  // non-const for testing

  std::string GetWakeOnWiFiFeaturesEnabled(Error *error);
  bool SetWakeOnWiFiFeaturesEnabled(const std::string &enabled, Error *error);
  // Helper function to run and reset |suspend_actions_done_callback_|.
  void RunAndResetSuspendActionsDoneCallback(const Error &error);
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
  // kPattern trigger, the NIC is programmed to wake on packets from the
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
  // Handler for NL80211 message error responses from NIC wake on WiFi setting
  // programming attempts.
  void OnWakeOnWiFiSettingsErrorResponse(
      NetlinkManager::AuxilliaryMessageType type,
      const NetlinkMessage *raw_message);
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
  // Utility functions to check which wake on WiFi features are currently
  // enabled based on the descriptor |wake_on_wifi_features_enabled_| and
  // are supported by the NIC.
  bool WakeOnPacketEnabledAndSupported();
  bool WakeOnSSIDEnabledAndSupported();
  // Called by metrics_timer_ to reports metrics.
  void ReportMetrics();
  // Actions executed before normal suspend and dark resume suspend.
  //
  // Arguments:
  //  - |is_connected|: whether the WiFi device is connected.
  //  - |start_lease_renewal_timer|: whether or not to start the DHCP lease
  //    renewal timer.
  //  - |time_to_next_lease_renewal|: number of seconds until next DHCP lease
  //    renewal is due.
  //  - |remove_supplicant_networks_callback|: callback to invoke
  //    to remove all networks from WPA supplicant.
  void BeforeSuspendActions(
      bool is_connected,
      bool start_lease_renewal_timer,
      uint32_t time_to_next_lease_renewal,
      const base::Closure &remove_supplicant_networks_callback);

  // Needed for |dhcp_lease_renewal_timer_| and |wake_to_scan_timer_| since
  // passing a empty base::Closure() causes a run-time DCHECK error when
  // AlarmTimer::Start or AlarmTimer::Reset are called.
  void OnTimerWakeDoNothing() {}

  // Pointers to objects owned by the WiFi object that created this object.
  EventDispatcher *dispatcher_;
  NetlinkManager *netlink_manager_;
  Metrics *metrics_;
  // Executes after the NIC's wake-on-packet settings are configured via
  // NL80211 messages to verify that the new configuration has taken effect.
  // Calls RequestWakeOnPacketSettings.
  base::CancelableClosure verify_wake_on_packet_settings_callback_;
  // Callback to be invoked after all suspend actions finish executing both
  // before regular suspend and before suspend in dark resume.
  ResultCallback suspend_actions_done_callback_;
  // Callback to report wake on WiFi related metrics.
  base::CancelableClosure report_metrics_callback_;
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
  // Describes the wake on WiFi features that are currently enabled.
  std::string wake_on_wifi_features_enabled_;
  // Timer that wakes the system to renew DHCP leases.
  timers::AlarmTimer dhcp_lease_renewal_timer_;
  // Timer that wakes the system to scan for networks.
  timers::AlarmTimer wake_to_scan_timer_;
  // Executes when the dark resume actions timer expires. Calls
  // ScanTimerHandler.
  base::CancelableClosure dark_resume_actions_timeout_callback_;
  // Whether shill is currently in dark resume.
  bool in_dark_resume_;
  // Frequency (in seconds) that the system is woken during suspend to perform
  // scans.
  uint32_t wake_to_scan_frequency_;

  base::WeakPtrFactory<WakeOnWiFi> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WakeOnWiFi);
};

}  // namespace shill

#endif  // SHILL_WIFI_WAKE_ON_WIFI_H_
