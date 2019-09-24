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

  // ARC++ container PID.
  pid_t pid_ = 0;

  base::WeakPtrFactory<ArcHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcHelper);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_ARC_HELPER_H_
