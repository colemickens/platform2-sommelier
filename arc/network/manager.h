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
#include <chromeos/dbus/service_constants.h>
#include <patchpanel/proto_bindings/patchpanel-service.pb.h>

#include "arc/network/address_manager.h"
#include "arc/network/arc_service.h"
#include "arc/network/device_manager.h"
#include "arc/network/helper_process.h"
#include "arc/network/shill_client.h"
#include "arc/network/socket.h"

namespace arc_networkd {

// Main class that runs the mainloop and responds to LAN interface changes.
class Manager final : public brillo::DBusDaemon {
 public:
  Manager(std::unique_ptr<HelperProcess> adb_proxy,
          std::unique_ptr<HelperProcess> mcast_proxy,
          std::unique_ptr<HelperProcess> nd_proxy);
  ~Manager() = default;

 protected:
  int OnInit() override;

 private:
  void InitialSetup();

  bool StartArc(pid_t pid);
  void StopArc();
  bool StartArcVm(int cid);
  void StopArcVm();

  // Callback from ProcessReaper to notify Manager that one of the
  // subprocesses died.
  void OnSubprocessExited(pid_t pid, const siginfo_t& info);

  // Callback from Daemon to notify that a signal was received and
  // the daemon should clean up in preparation to exit.
  void OnShutdown(int* exit_code) override;

  // Handles DBus notification indicating ARC++ is booting up.
  std::unique_ptr<dbus::Response> OnArcStartup(dbus::MethodCall* method_call);

  // Handles DBus notification indicating ARC++ is spinning down.
  std::unique_ptr<dbus::Response> OnArcShutdown(dbus::MethodCall* method_call);

  // Handles DBus notification indicating ARCVM is booting up.
  std::unique_ptr<dbus::Response> OnArcVmStartup(dbus::MethodCall* method_call);

  // Handles DBus notification indicating ARCVM is spinning down.
  std::unique_ptr<dbus::Response> OnArcVmShutdown(
      dbus::MethodCall* method_call);

  // Dispatch |msg| to child processes.
  void SendGuestMessage(const GuestMessage& msg);

  // TODO(garrick): Remove this workaround ASAP.
  bool OnSignal(const struct signalfd_siginfo& info);

  friend std::ostream& operator<<(std::ostream& stream, const Manager& manager);

  // Guest services.
  std::unique_ptr<ArcService> arc_svc_;

  // DBus service.
  dbus::ExportedObject* dbus_svc_path_;  // Owned by |bus_|.

  // Other services.
  brillo::ProcessReaper process_reaper_;
  std::unique_ptr<HelperProcess> adb_proxy_;
  std::unique_ptr<HelperProcess> mcast_proxy_;
  std::unique_ptr<HelperProcess> nd_proxy_;

  AddressManager addr_mgr_;
  std::unique_ptr<DeviceManager> device_mgr_;

  std::unique_ptr<MinijailedProcessRunner> runner_;
  std::unique_ptr<Datapath> datapath_;

  base::WeakPtrFactory<Manager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MANAGER_H_
