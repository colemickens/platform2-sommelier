// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_ARC_SERVICE_H_
#define ARC_NETWORK_ARC_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include <base/memory/weak_ptr.h>
#include <shill/net/rtnl_handler.h>
#include <shill/net/rtnl_listener.h>

#include "arc/network/datapath.h"
#include "arc/network/guest_service.h"

namespace arc_networkd {

class ArcService : public GuestService {
 public:
  // |dev_mgr| cannot be null.
  ArcService(DeviceManagerBase* dev_mgr,
             bool is_legacy,
             std::unique_ptr<Datapath> datapath = nullptr);
  ~ArcService() = default;

  void OnStart() override;
  void OnStop() override;

  void OnDeviceAdded(Device* device) override;
  void OnDeviceRemoved(Device* device) override;
  void OnDefaultInterfaceChanged(const std::string& ifname) override;

  // Handles RT netlink messages in the container net namespace and if it
  // determines the link status has changed, toggles the device services
  // accordingly.
  void LinkMsgHandler(const shill::RTNLMessage& msg);

  void SetupIPv6(Device* device);
  void TeardownIPv6(Device* device);

  // Do not use. Only for testing.
  void SetPIDForTestingOnly();

 private:
  void StartDevice(Device* device);
  void StopDevice(Device* device);

  bool ShouldProcessDevice(const Device& device) const;

  std::unique_ptr<MinijailedProcessRunner> runner_;
  std::unique_ptr<Datapath> datapath_;

  pid_t pid_;
  std::unique_ptr<shill::RTNLHandler> rtnl_handler_;
  std::unique_ptr<shill::RTNLListener> link_listener_;
  std::set<std::string> devices_;

  base::WeakPtrFactory<ArcService> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ArcService);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_ARC_SERVICE_H_
