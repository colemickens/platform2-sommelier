// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_GUEST_SERVICE_H_
#define ARC_NETWORK_GUEST_SERVICE_H_

#include <base/bind.h>
#include <base/macros.h>

#include "arc/network/device_manager.h"

namespace arc_networkd {

class GuestService {
 public:
  using MessageHandler = base::Callback<void(const GuestMessage&)>;

  virtual ~GuestService() = default;

  void RegisterMessageHandler(const MessageHandler& handler);

 protected:
  // |dev_mgr| cannot be null and must outlive this object.
  GuestService(GuestMessage::GuestType guest, DeviceManager* dev_mgr);

  virtual void OnStart();
  virtual void OnStop();

  void DispatchMessage(const GuestMessage& msg);

  const GuestMessage::GuestType guest_;
  DeviceManager* dev_mgr_;

 private:
  MessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(GuestService);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_GUEST_SERVICE_H_
