// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/callback80211_metrics.h"

#include "shill/config80211.h"
#include "shill/ieee80211.h"
#include "shill/link_monitor.h"
#include "shill/logging.h"
#include "shill/metrics.h"
#include "shill/user_bound_nlmessage.h"

namespace shill {

Callback80211Metrics::Callback80211Metrics(Metrics *metrics)
    : Callback80211Object(),
      metrics_(metrics) {
}

void Callback80211Metrics::Config80211MessageCallback(
    const UserBoundNlMessage &message) {
  SLOG(WiFi, 3) << "Received " << message.message_type_string()
                << " (" << + message.message_type() << ")";
  if (metrics_ &&
      message.message_type() == DeauthenticateMessage::kCommand) {
    Metrics::WiFiDisconnectByWhom by_whom =
        message.AttributeExists(NL80211_ATTR_DISCONNECTED_BY_AP) ?
                    Metrics::kDisconnectedByAp : Metrics::kDisconnectedNotByAp;
    uint16_t reason = static_cast<uint16_t>(
        IEEE_80211::kReasonCodeInvalid);
    void *rawdata = NULL;
    int frame_byte_count = 0;
    if (message.GetRawAttributeData(NL80211_ATTR_FRAME, &rawdata,
                                    &frame_byte_count)) {
      const uint8_t *frame_data = reinterpret_cast<const uint8_t *>(rawdata);
      Nl80211Frame frame(frame_data, frame_byte_count);
      reason = frame.reason();
    }
    IEEE_80211::WiFiReasonCode reason_enum =
        static_cast<IEEE_80211::WiFiReasonCode>(reason);
    metrics_->Notify80211Disconnect(by_whom, reason_enum);
  }
}

}  // namespace shill.
