// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/manager.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <brillo/minijail/minijail.h>

#include "arc/network/guest_events.h"
#include "arc/network/ipc.pb.h"

namespace arc_networkd {

Manager::Manager(std::unique_ptr<HelperProcess> ip_helper,
                 std::unique_ptr<HelperProcess> adb_proxy,
                 bool enable_multinet)
    : ip_helper_(std::move(ip_helper)),
      adb_proxy_(std::move(adb_proxy)),
      addr_mgr_({
          AddressManager::Guest::ARC,
          AddressManager::Guest::ARC_NET,
      }),
      enable_multinet_(enable_multinet) {}

int Manager::OnInit() {
  prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);

  // Handle subprocess lifecycle.
  process_reaper_.Register(this);

  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, ip_helper_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 ip_helper_->pid())))
      << "Failed to watch ip-helper child process";

  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, adb_proxy_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 adb_proxy_->pid())))
      << "Failed to watch adb-proxy child process";

  // TODO(garrick): Remove this workaround ASAP.
  // Handle signals for ARC lifecycle.
  RegisterHandler(SIGUSR1,
                  base::Bind(&Manager::OnSignal, base::Unretained(this)));
  RegisterHandler(SIGUSR2,
                  base::Bind(&Manager::OnSignal, base::Unretained(this)));

  // Run after Daemon::OnInit().
  base::MessageLoopForIO::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Manager::InitialSetup, weak_factory_.GetWeakPtr()));

  return DBusDaemon::OnInit();
}

void Manager::InitialSetup() {
  device_mgr_ = std::make_unique<DeviceManager>(
      std::make_unique<ShillClient>(std::move(bus_)), &addr_mgr_,
      base::Bind(&Manager::SendDeviceMessage, weak_factory_.GetWeakPtr()),
      !enable_multinet_);

  arc_svc_ = std::make_unique<ArcService>(device_mgr_.get(), !enable_multinet_);
  arc_svc_->RegisterMessageHandler(
      base::Bind(&Manager::OnGuestMessage, weak_factory_.GetWeakPtr()));
}

// TODO(garrick): Remove this workaround ASAP.
bool Manager::OnSignal(const struct signalfd_siginfo& info) {
  // Only ARC++ scripts send signals so nothing to do for VM.
  (info.ssi_signo == SIGUSR1) ? arc_svc_->OnStart() : arc_svc_->OnStop();
  return false;
}

void Manager::OnShutdown(int* exit_code) {
  device_mgr_.reset();
}

void Manager::OnSubprocessExited(pid_t pid, const siginfo_t& info) {
  LOG(ERROR) << "Subprocess " << pid << " exited unexpectedly";
  Quit();
}

void Manager::OnGuestMessage(const GuestMessage& msg) {
  IpHelperMessage ipm;
  *ipm.mutable_guest_message() = msg;
  ip_helper_->SendMessage(ipm);
  adb_proxy_->SendMessage(ipm);
}

void Manager::SendDeviceMessage(const DeviceMessage& msg) {
  IpHelperMessage ipm;
  *ipm.mutable_device_message() = msg;
  ip_helper_->SendMessage(ipm);
}

}  // namespace arc_networkd
