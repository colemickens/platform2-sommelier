// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/callback80211_object.h"

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
using base::LazyInstance;
using std::string;

namespace shill {

namespace {
LazyInstance<Callback80211Object> g_callback80211 = LAZY_INSTANCE_INITIALIZER;
}  // namespace

Callback80211Object::Callback80211Object()
  : config80211_(NULL), metrics_(NULL), weak_ptr_factory_(this) {
}

Callback80211Object::~Callback80211Object() {
  DeinstallAsCallback();
}

void Callback80211Object::Config80211MessageCallback(
    const UserBoundNlMessage &message) {
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

  // Now, print out the message.

  SLOG(WiFi, 2) << "Received " << message.GetMessageTypeString()
                << " (" << + message.GetMessageType() << ")";
  scoped_ptr<UserBoundNlMessage::AttributeNameIterator> i;

  for (i.reset(message.GetAttributeNameIterator()); !i->AtEnd(); i->Advance()) {
    string value = "<unknown>";
    message.GetAttributeString(i->GetName(), &value);
    SLOG(WiFi, 2) << "   Attr:" << message.StringFromAttributeName(i->GetName())
                  << "=" << value
                  << " Type:" << message.GetAttributeTypeString(i->GetName());
  }
}

bool Callback80211Object::InstallAsCallback() {
  if (config80211_) {
    Config80211::Callback callback =
        Bind(&Callback80211Object::Config80211MessageCallback,
             weak_ptr_factory_.GetWeakPtr());
    config80211_->SetDefaultCallback(callback);
    return true;
  }
  return false;
}

bool Callback80211Object::DeinstallAsCallback() {
  if (config80211_) {
    config80211_->UnsetDefaultCallback();
    return true;
  }
  return false;
}

Callback80211Object *Callback80211Object::GetInstance() {
  return g_callback80211.Pointer();
}

}  // namespace shill.
