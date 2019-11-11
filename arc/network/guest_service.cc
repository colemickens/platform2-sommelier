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
      guest_, base::Bind(&GuestService::OnDeviceAdded, base::Unretained(this)));
  dev_mgr_->RegisterDeviceRemovedHandler(
      guest_,
      base::Bind(&GuestService::OnDeviceRemoved, base::Unretained(this)));
  dev_mgr_->RegisterDefaultInterfaceChangedHandler(
      guest_, base::Bind(&GuestService::OnDefaultInterfaceChanged,
                         base::Unretained(this)));
}

void GuestService::OnStart() {
  dev_mgr_->OnGuestStart(guest_);
}

void GuestService::OnStop() {
  dev_mgr_->OnGuestStop(guest_);
}

}  // namespace arc_networkd
