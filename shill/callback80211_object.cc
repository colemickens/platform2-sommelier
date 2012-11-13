// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/callback80211_object.h"

#include <string>

#include <base/memory/weak_ptr.h>
#include <base/stringprintf.h>

#include "shill/config80211.h"
#include "shill/ieee80211.h"
#include "shill/link_monitor.h"
#include "shill/logging.h"
#include "shill/scope_logger.h"
#include "shill/user_bound_nlmessage.h"

using base::Bind;
using base::StringAppendF;
using std::string;

namespace shill {

Callback80211Object::Callback80211Object()
    : weak_ptr_factory_(this),
      callback_(Bind(&Callback80211Object::ReceiveConfig80211Message,
                     weak_ptr_factory_.GetWeakPtr())) {
}

Callback80211Object::~Callback80211Object() {
  DeinstallAsCallback();
}

void Callback80211Object::Config80211MessageCallback(
    const UserBoundNlMessage &message) {
  // Show the simplified version of the message.
  string output("@");
  StringAppendF(&output, "%s", message.ToString().c_str());
  SLOG(WiFi, 2) << output;

  // Show the more complicated version of the message.
  SLOG(WiFi, 3) << "Received " << message.message_type_string()
                << " (" << + message.message_type() << ")";

  scoped_ptr<UserBoundNlMessage::AttributeNameIterator> i;
  for (i.reset(message.GetAttributeNameIterator()); !i->AtEnd(); i->Advance()) {
    string value = "<unknown>";
    message.GetAttributeString(i->GetName(), &value);
    SLOG(WiFi, 3) << "   Attr:" << message.StringFromAttributeName(i->GetName())
                  << "=" << value
                  << " Type:" << message.GetAttributeTypeString(i->GetName());
  }
}

bool Callback80211Object::InstallAsBroadcastCallback() {
  return Config80211::GetInstance()->AddBroadcastCallback(callback_);
}

bool Callback80211Object::DeinstallAsCallback() {
  return Config80211::GetInstance()->RemoveBroadcastCallback(callback_);
}

void Callback80211Object::ReceiveConfig80211Message(
    const UserBoundNlMessage &msg) {
  Config80211MessageCallback(msg);
}

}  // namespace shill.
