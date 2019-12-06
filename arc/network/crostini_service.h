// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_CROSTINI_SERVICE_H_
#define ARC_NETWORK_CROSTINI_SERVICE_H_

#include <set>
#include <string>

#include <base/memory/weak_ptr.h>

#include "arc/network/datapath.h"
#include "arc/network/device.h"
#include "arc/network/device_manager.h"

namespace arc_networkd {

// Crostini networking service hanlding address allocation and TAP device
// management for Termina VMs.
class CrostiniService {
 public:
  class Context : public Device::Context {
   public:
    Context() = default;
    ~Context() = default;

    bool IsLinkUp() const override;

    int32_t CID() const;
    void SetCID(int32_t cid);
    const std::string& TAP() const;
    void SetTAP(const std::string& tap);

   private:
    int32_t cid_;
    std::string tap_;

    DISALLOW_COPY_AND_ASSIGN(Context);
  };

  // |dev_mgr| and |datapath| cannot be null.
  CrostiniService(DeviceManagerBase* dev_mgr, Datapath* datapath);
  ~CrostiniService();

  bool Start(int32_t cid);
  void Stop(int32_t cid);

  void OnDeviceAdded(Device* device);
  void OnDeviceRemoved(Device* device);

 private:
  DeviceManagerBase* dev_mgr_;
  Datapath* datapath_;
  std::set<int32_t> cids_;

  base::WeakPtrFactory<CrostiniService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CrostiniService);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_CROSTINI_SERVICE_H_
