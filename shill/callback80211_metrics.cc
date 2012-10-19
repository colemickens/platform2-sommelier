// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/callback80211_metrics.h"

#include <string>

#include <base/memory/weak_ptr.h>

#include "shill/config80211.h"
#include "shill/ieee80211.h"
#include "shill/link_monitor.h"
#include "shill/logging.h"
#include "shill/metrics.h"
#include "shill/scope_logger.h"
#include "shill/user_bound_nlmessage.h"

using base::Bind;
using std::string;

namespace shill {

Callback80211Metrics::Callback80211Metrics(Config80211 *config80211,
                                           Metrics *metrics)
    : Callback80211Object(config80211), metrics_(metrics),
      weak_ptr_factory_(this) {
}

void Callback80211Metrics::Config80211MessageCallback(
    const UserBoundNlMessage &message) {
  SLOG(WiFi, 3) << "Received " << message.GetMessageTypeString()
                << " (" << + message.GetMessageType() << ")";
  if (metrics_ && message.GetMessageType() == DeauthenticateMessage::kCommand) {
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

bool Callback80211Metrics::InstallAsBroadcastCallback() {
  if (config80211_) {
    callback_ = Bind(&Callback80211Metrics::Config80211MessageCallback,
                     weak_ptr_factory_.GetWeakPtr());
    return config80211_->AddBroadcastCallback(callback_);
  }
  return false;
}

}  // namespace shill.
