// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MANAGER_H_
#define ARC_NETWORK_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include <base/memory/weak_ptr.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/process_reaper.h>

#include "arc/network/arc_ip_config.h"
#include "arc/network/device.h"
#include "arc/network/helper_process.h"
#include "arc/network/ipc.pb.h"
#include "arc/network/shill_client.h"

namespace arc_networkd {

// Main class that runs the mainloop and responds to LAN interface changes.
class Manager final : public brillo::DBusDaemon {
 public:
  explicit Manager(std::unique_ptr<HelperProcess> ip_helper);

 protected:
  int OnInit() override;

 private:
  // Callback from ShillClient, invoked whenever the default network
  // interface changes or goes away.
  void OnDefaultInterfaceChanged(const std::string& ifname);

  // Establishes new ShillClient and scans for host devices.
  void InitialSetup();

  // Callback for ScanDevices, invoked when a device is found.
  void AddDevice(const std::string& name);

  // Callback from ShillClient, invoked whenever the device list changes.
  // |devices| will contain all devices currently connected to shill
  // (e.g. "eth0", "wlan0", etc).
  void OnDevicesChanged(const std::set<std::string>& devices);

  // Callback from ProcessReaper to notify Manager that one of the
  // subprocesses died.
  void OnSubprocessExited(pid_t pid, const siginfo_t& info);

  // Callback from Daemon to notify that a signal was received and
  // the daemon should clean up in preparation to exit.
  void OnShutdown(int* exit_code) override;

  // Callbacks from Daemon to notify that a signal was received
  // indicating the container is either booting up or going down.
  bool OnContainerStart(const struct signalfd_siginfo& info);
  bool OnContainerStop(const struct signalfd_siginfo& info);

  // Utility method for passing messages to the helper process.
  void SendMessage(const IpHelperMessage& msg);

  friend std::ostream& operator<<(std::ostream& stream, const Manager& manager);

  // Persistent objects.
  brillo::ProcessReaper process_reaper_;
  std::unique_ptr<ShillClient> shill_client_;
  std::unique_ptr<HelperProcess> ip_helper_;

  // Connected devices keyed by the interface name.
  // The legacy device is mapped to the "android" interface name.
  std::map<std::string, std::unique_ptr<Device>> devices_;

  base::WeakPtrFactory<Manager> weak_factory_{this};
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MANAGER_H_
