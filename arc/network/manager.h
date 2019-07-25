// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MANAGER_H_
#define ARC_NETWORK_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include <base/files/file_descriptor_watcher_posix.h>
#include <base/memory/weak_ptr.h>
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/process_reaper.h>

#include "arc/network/address_manager.h"
#include "arc/network/arc_service.h"
#include "arc/network/device_manager.h"
#include "arc/network/guest_events.h"
#include "arc/network/helper_process.h"
#include "arc/network/shill_client.h"
#include "arc/network/socket.h"

namespace arc_networkd {

// Main class that runs the mainloop and responds to LAN interface changes.
class Manager final : public brillo::DBusDaemon {
 public:
  Manager(std::unique_ptr<HelperProcess> adb_proxy, bool enable_multinet);
  ~Manager() = default;

 protected:
  int OnInit() override;

 private:
  void InitialSetup();

  // Callback from ProcessReaper to notify Manager that one of the
  // subprocesses died.
  void OnSubprocessExited(pid_t pid, const siginfo_t& info);

  // Callback from Daemon to notify that a signal was received and
  // the daemon should clean up in preparation to exit.
  void OnShutdown(int* exit_code) override;

  // Processes notification messages received from guests.
  void OnGuestMessage(const GuestMessage& msg);

  // TODO(garrick): Remove this workaround ASAP.
  bool OnSignal(const struct signalfd_siginfo& info);
  void OnFileCanReadWithoutBlocking();

  friend std::ostream& operator<<(std::ostream& stream, const Manager& manager);

  // Guest services.
  std::unique_ptr<ArcService> arc_svc_;

  // Other services.
  brillo::ProcessReaper process_reaper_;
  std::unique_ptr<HelperProcess> adb_proxy_;

  AddressManager addr_mgr_;
  std::unique_ptr<DeviceManager> device_mgr_;

  bool enable_multinet_;

  Socket gsock_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> gsock_watcher_;

  base::WeakPtrFactory<Manager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MANAGER_H_
