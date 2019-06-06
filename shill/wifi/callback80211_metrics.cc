// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi/callback80211_metrics.h"

#include <string>

#include "shill/logging.h"
#include "shill/metrics.h"
#include "shill/net/ieee80211.h"
#include "shill/net/netlink_manager.h"
#include "shill/net/nl80211_message.h"

namespace shill {

namespace log_scope {
static auto kModuleLogScope = ScopeLogger::kWiFi;
static std::string ObjectID(const Callback80211Metrics* c) {
  return "(callback80211metrics)";
}
}  // namespace log_scope

Callback80211Metrics::Callback80211Metrics(Metrics* metrics)
    : metrics_(metrics) {}

IEEE_80211::WiFiReasonCode Callback80211Metrics::WiFiReasonCodeFromUint16(
    uint16_t reason) const {
  IEEE_80211::WiFiReasonCode reason_enum = IEEE_80211::kReasonCodeInvalid;
  if (reason == IEEE_80211::kReasonCodeReserved0 ||
      reason == IEEE_80211::kReasonCodeReserved12 ||
      (reason >= IEEE_80211::kReasonCodeReservedBegin25 &&
       reason <= IEEE_80211::kReasonCodeReservedEnd31) ||
      (reason >= IEEE_80211::kReasonCodeReservedBegin40 &&
       reason <= IEEE_80211::kReasonCodeReservedEnd44) ||
      reason >= IEEE_80211::kReasonCodeMax) {
    SLOG(this, 1) << "Invalid reason code in disconnect message";
    reason_enum = IEEE_80211::kReasonCodeInvalid;
  } else {
    reason_enum = static_cast<IEEE_80211::WiFiReasonCode>(reason);
  }
  return reason_enum;
}

void Callback80211Metrics::CollectDisconnectStatistics(
    const NetlinkMessage& netlink_message) {
  if (!metrics_) {
    return;
  }
  // We only handle disconnect and deauthenticate messages, both of which are
  // nl80211 messages.
  if (netlink_message.message_type() != Nl80211Message::GetMessageType()) {
    return;
  }
  const Nl80211Message& message =
      * reinterpret_cast<const Nl80211Message*>(&netlink_message);

  // Station-instigated disconnects provide their information in the
  // deauthenticate message but AP-instigated disconnects provide it in the
  // disconnect message.
  uint16_t reason = IEEE_80211::kReasonCodeUnspecified;
  if (message.command() == DeauthenticateMessage::kCommand) {
    SLOG(this, 3) << "Handling Deauthenticate Message";
    message.Print(3, 3);
    // If there's no frame, this is probably an AP-caused disconnect and
    // there'll be a disconnect message to tell us about that.
    ByteString rawdata;
    if (!message.const_attributes()->GetRawAttributeValue(NL80211_ATTR_FRAME,
                                                          &rawdata)) {
      SLOG(this, 5) << "No frame in deauthenticate message, ignoring";
      return;
    }
    Nl80211Frame frame(rawdata);
    reason = frame.reason();
  } else if (message.command() == DisconnectMessage::kCommand) {
    SLOG(this, 3) << "Handling Disconnect Message";
    message.Print(3, 3);
    // If there's no reason code, this is probably a STA-caused disconnect and
    // there was be a disconnect message to tell us about that.
    if (!message.const_attributes()->GetU16AttributeValue(
            NL80211_ATTR_REASON_CODE, &reason)) {
      SLOG(this, 5) << "No reason code in disconnect message, ignoring";
      return;
    }
  } else {
    return;
  }

  IEEE_80211::WiFiReasonCode reason_enum = WiFiReasonCodeFromUint16(reason);

  Metrics::WiFiDisconnectByWhom by_whom =
      message.const_attributes()->IsFlagAttributeTrue(
          NL80211_ATTR_DISCONNECTED_BY_AP) ? Metrics::kDisconnectedByAp :
          Metrics::kDisconnectedNotByAp;
  SLOG(this, 1) << "Notify80211Disconnect by " << (by_whom ? "station" : "AP")
                << " because:" << static_cast<int>(reason_enum);
  metrics_->Notify80211Disconnect(by_whom, reason_enum);
}

}  // namespace shill.
