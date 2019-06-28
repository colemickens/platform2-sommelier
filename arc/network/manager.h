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
#include <base/message_loop/message_loop.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/process_reaper.h>

#include "arc/network/address_manager.h"
#include "arc/network/arc_ip_config.h"
#include "arc/network/device_manager.h"
#include "arc/network/helper_process.h"
#include "arc/network/shill_client.h"
#include "arc/network/socket.h"

namespace arc_networkd {

// Main class that runs the mainloop and responds to LAN interface changes.
class Manager final : public brillo::DBusDaemon,
                      public base::MessageLoopForIO::Watcher {
 public:
  Manager(std::unique_ptr<HelperProcess> ip_helper,
          std::unique_ptr<HelperProcess> adb_proxy,
          bool enable_multinet);
  ~Manager();

 protected:
  int OnInit() override;

  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override {}

 private:
  void InitialSetup();

  // Callback from ProcessReaper to notify Manager that one of the
  // subprocesses died.
  void OnSubprocessExited(pid_t pid, const siginfo_t& info);

  // Callback from Daemon to notify that a signal was received and
  // the daemon should clean up in preparation to exit.
  void OnShutdown(int* exit_code) override;

  // Processes notification messages received from guests.
  void OnGuestNotification(const std::string& notification);

  // Relays guest messages to the helper processes.
  void SendGuestMessage(const GuestMessage& msg);

  // Relays device messages to the IpHelper process.
  void SendDeviceMessage(const DeviceMessage& msg);

  bool OnSignal(const struct signalfd_siginfo& info);

  friend std::ostream& operator<<(std::ostream& stream, const Manager& manager);

  std::unique_ptr<HelperProcess> ip_helper_;
  std::unique_ptr<HelperProcess> adb_proxy_;

  AddressManager addr_mgr_;
  brillo::ProcessReaper process_reaper_;
  std::unique_ptr<DeviceManager> device_mgr_;

  bool enable_multinet_;
  int arc_pid_;

  Socket gsock_;
  base::MessageLoopForIO::FileDescriptorWatcher gsock_watcher_;

  base::WeakPtrFactory<Manager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MANAGER_H_
