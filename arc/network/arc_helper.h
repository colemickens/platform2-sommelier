// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_ARC_HELPER_H_
#define ARC_NETWORK_ARC_HELPER_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <shill/net/rtnl_handler.h>
#include <shill/net/rtnl_listener.h>

#include "arc/network/arc_ip_config.h"
#include "arc/network/device.h"
#include "arc/network/ipc.pb.h"

namespace arc_networkd {

class ArcHelper {
 public:
  ~ArcHelper() = default;
  static std::unique_ptr<ArcHelper> New();

  void Start(pid_t pid);
  void Stop(pid_t pid);

  void HandleCommand(const DeviceMessage& cmd);

 protected:
  ArcHelper() = default;

 private:
  void AddDevice(const std::string& ifname, const DeviceConfig& config);
  void RemoveDevice(const std::string& ifname);

  void LinkMsgHandler(const shill::RTNLMessage& msg);

  // ARC++ container PID.
  pid_t pid_ = 0;
  std::unique_ptr<shill::RTNLHandler> rtnl_handler_;
  std::unique_ptr<shill::RTNLListener> link_listener_;

  // IP configurations for the devices representing both physical host
  // interfaces (e.g. eth0) as well a pseudo devices (e.g. Android)
  // that can be remapped between host interfaces. Keyed by device interface.
  std::map<std::string, std::unique_ptr<ArcIpConfig>> arc_ip_configs_;
  // Remapping of |arc_ip_configs_| (which owns the pointers) keyed by
  // the container interface name.
  std::map<std::string, ArcIpConfig*> configs_by_arc_ifname_;

  base::WeakPtrFactory<ArcHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcHelper);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_ARC_HELPER_H_
