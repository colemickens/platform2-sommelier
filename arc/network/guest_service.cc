// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/guest_service.h"

#include <base/bind.h>

namespace arc_networkd {

GuestService::GuestService(GuestMessage::GuestType guest,
                           DeviceManagerBase* dev_mgr)
    : guest_(guest), dev_mgr_(dev_mgr) {
  dev_mgr_->RegisterDeviceAddedHandler(
      base::Bind(&GuestService::OnDeviceAdded, base::Unretained(this)));
  dev_mgr_->RegisterDeviceRemovedHandler(
      base::Bind(&GuestService::OnDeviceRemoved, base::Unretained(this)));
  dev_mgr_->RegisterDefaultInterfaceChangedHandler(base::Bind(
      &GuestService::OnDefaultInterfaceChanged, base::Unretained(this)));
}

void GuestService::RegisterMessageHandler(const MessageHandler& handler) {
  handler_ = handler;
}

void GuestService::OnStart() {
  dev_mgr_->OnGuestStart(guest_);
}

void GuestService::OnStop() {
  dev_mgr_->OnGuestStop(guest_);
}

void GuestService::DispatchMessage(const GuestMessage& msg) {
  if (!handler_.is_null())
    handler_.Run(msg);
}

}  // namespace arc_networkd
