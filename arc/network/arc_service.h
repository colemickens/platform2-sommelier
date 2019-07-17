// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_ARC_SERVICE_H_
#define ARC_NETWORK_ARC_SERVICE_H_

#include "arc/network/guest_service.h"

namespace arc_networkd {

class ArcService : public GuestService {
 public:
  // |dev_mgr| cannot be null.
  ArcService(DeviceManager* dev_mgr, bool is_legacy);
  ~ArcService() = default;

  void OnStart() override;
  void OnStop() override;

 protected:
  void OnDeviceAdded(Device* device) override;
  void OnDeviceRemoved(Device* device) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcService);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_ARC_SERVICE_H_
