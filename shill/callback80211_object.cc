// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/callback80211_object.h"

#include <string>

#include <base/memory/weak_ptr.h>

#include "shill/config80211.h"
#include "shill/link_monitor.h"
#include "shill/logging.h"
#include "shill/scope_logger.h"
#include "shill/user_bound_nlmessage.h"

using base::Bind;
using base::LazyInstance;
using std::string;

namespace shill {

namespace {
LazyInstance<Callback80211Object> g_callback80211 = LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
const char Callback80211Object::kMetricLinkDisconnectCount[] =
    "Network.Shill.DisconnectCount";

Callback80211Object::Callback80211Object() :
    config80211_(NULL),
    weak_ptr_factory_(this) {
}

Callback80211Object::~Callback80211Object() {
  DeinstallAsCallback();
}

void Callback80211Object::Config80211MessageCallback(
    const UserBoundNlMessage &msg) {
  SLOG(WiFi, 2) << "Received " << msg.GetMessageTypeString()
                << " (" << + msg.GetMessageType() << ")";
  scoped_ptr<UserBoundNlMessage::AttributeNameIterator> i;

  for (i.reset(msg.GetAttributeNameIterator()); !i->AtEnd(); i->Advance()) {
    string value = "<unknown>";
    msg.GetAttributeString(i->GetName(), &value);
    SLOG(WiFi, 2) << "   Attr:" << msg.StringFromAttributeName(i->GetName())
                  << "=" << value
                  << " Type:" << msg.GetAttributeTypeString(i->GetName());
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
