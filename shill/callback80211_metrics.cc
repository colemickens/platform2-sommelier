// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/callback80211_metrics.h"

#include "shill/ieee80211.h"
#include "shill/logging.h"
#include "shill/metrics.h"
#include "shill/netlink_manager.h"
#include "shill/nl80211_message.h"

namespace shill {

Callback80211Metrics::Callback80211Metrics(
    const NetlinkManager &netlink_manager, Metrics *metrics)
    : metrics_(metrics) {}

void Callback80211Metrics::CollectDisconnectStatistics(
    const NetlinkMessage &netlink_message) {
  // We only handle deauthenticate messages, which are nl80211 messages.
  if (netlink_message.message_type() != Nl80211Message::GetMessageType()) {
    return;
  }
  const Nl80211Message &message =
      * reinterpret_cast<const Nl80211Message *>(&netlink_message);

  if (metrics_ &&
      message.command() == DeauthenticateMessage::kCommand) {
    SLOG(WiFi, 3) << "Handling Deauthenticate Message";
    Metrics::WiFiDisconnectByWhom by_whom =
        message.const_attributes()->IsFlagAttributeTrue(
            NL80211_ATTR_DISCONNECTED_BY_AP) ?
                    Metrics::kDisconnectedByAp : Metrics::kDisconnectedNotByAp;
    uint16_t reason = static_cast<uint16_t>(
        IEEE_80211::kReasonCodeInvalid);
    ByteString rawdata;
    if (message.const_attributes()->GetRawAttributeValue(NL80211_ATTR_FRAME,
                                                          &rawdata)) {
      Nl80211Frame frame(rawdata);
      reason = frame.reason();
    }
    IEEE_80211::WiFiReasonCode reason_enum =
        static_cast<IEEE_80211::WiFiReasonCode>(reason);
    metrics_->Notify80211Disconnect(by_whom, reason_enum);
  }
}

}  // namespace shill.
