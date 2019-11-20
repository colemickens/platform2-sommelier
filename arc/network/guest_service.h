// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_GUEST_SERVICE_H_
#define ARC_NETWORK_GUEST_SERVICE_H_

#include <string>

#include <base/bind.h>
#include <base/macros.h>

#include "arc/network/device_manager.h"

namespace arc_networkd {

class GuestService {
 public:
  using MessageHandler = base::Callback<void(const GuestMessage&)>;

  virtual ~GuestService() = default;

 protected:
  // |dev_mgr| cannot be null and must outlive this object.
  GuestService(GuestMessage::GuestType guest, DeviceManagerBase* dev_mgr);

  virtual bool Start(int32_t id);
  virtual void Stop(int32_t id);

  virtual void OnDeviceAdded(Device* device) {}
  virtual void OnDeviceRemoved(Device* device) {}
  virtual void OnDefaultInterfaceChanged(const std::string& ifname) {}

  const GuestMessage::GuestType guest_;
  DeviceManagerBase* dev_mgr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestService);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_GUEST_SERVICE_H_
