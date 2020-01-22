// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_CROSTINI_SERVICE_H_
#define ARC_NETWORK_CROSTINI_SERVICE_H_

#include <map>
#include <memory>
#include <string>

#include <base/memory/weak_ptr.h>

#include "arc/network/datapath.h"
#include "arc/network/device.h"
#include "arc/network/device_manager.h"

namespace arc_networkd {

// Crostini networking service handling address allocation and TAP device
// management for Termina VMs.
class CrostiniService {
 public:
  // |dev_mgr| and |datapath| cannot be null.
  CrostiniService(DeviceManagerBase* dev_mgr, Datapath* datapath);
  ~CrostiniService() = default;

  bool Start(int32_t cid);
  void Stop(int32_t cid);

  const Device* const TAP(int32_t cid) const;

 private:
  bool AddTAP(int32_t cid);
  void OnDefaultInterfaceChanged(const std::string& ifname);

  DeviceManagerBase* dev_mgr_;
  Datapath* datapath_;
  // Mapping of VM CIDs to TAP devices
  std::map<int32_t, std::unique_ptr<Device>> taps_;

  base::WeakPtrFactory<CrostiniService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CrostiniService);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_CROSTINI_SERVICE_H_
