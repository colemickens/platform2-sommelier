// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/manager.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/stringprintf.h>
#include <brillo/minijail/minijail.h>

#include "arc/network/ipc.pb.h"

namespace {

const uint64_t kManagerCapMask = CAP_TO_MASK(CAP_NET_RAW);
const char kUnprivilegedUser[] = "arc-networkd";

}  // namespace

namespace arc_networkd {

Manager::Manager(std::unique_ptr<HelperProcess> ip_helper, bool enable_multinet)
    : addr_mgr_({
          AddressManager::Guest::ARC,
          AddressManager::Guest::ARC_NET,
      }),
      enable_multinet_(enable_multinet) {
  ip_helper_ = std::move(ip_helper);
}

int Manager::OnInit() {
  // Run with minimal privileges.
  brillo::Minijail* m = brillo::Minijail::GetInstance();
  struct minijail* jail = m->New();

  // Most of these return void, but DropRoot() can fail if the user/group
  // does not exist.
  CHECK(m->DropRoot(jail, kUnprivilegedUser, kUnprivilegedUser))
      << "Could not drop root privileges";
  m->UseCapabilities(jail, kManagerCapMask);
  m->Enter(jail);
  m->Destroy(jail);

  // Handle subprocess lifecycle.

  process_reaper_.Register(this);

  CHECK(process_reaper_.WatchForChild(
      FROM_HERE, ip_helper_->pid(),
      base::Bind(&Manager::OnSubprocessExited, weak_factory_.GetWeakPtr(),
                 ip_helper_->pid())))
      << "Failed to watch child IpHelper process";

  // Handle container lifecycle.
  RegisterHandler(
      SIGUSR1, base::Bind(&Manager::OnContainerStart, base::Unretained(this)));
  RegisterHandler(
      SIGUSR2, base::Bind(&Manager::OnContainerStop, base::Unretained(this)));

  // Run after Daemon::OnInit().
  base::MessageLoopForIO::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Manager::InitialSetup, weak_factory_.GetWeakPtr()));

  return DBusDaemon::OnInit();
}

void Manager::InitialSetup() {
  shill_client_ = std::make_unique<ShillClient>(std::move(bus_));
  device_mgr_ = std::make_unique<DeviceManager>(
      &addr_mgr_, base::Bind(&Manager::SendMessage, weak_factory_.GetWeakPtr()),
      enable_multinet_ ? kAndroidDevice : kAndroidLegacyDevice);

  if (enable_multinet_) {
    shill_client_->RegisterDevicesChangedHandler(
        base::Bind(&Manager::OnDevicesChanged, weak_factory_.GetWeakPtr()));
    shill_client_->ScanDevices(
        base::Bind(&Manager::OnDevicesChanged, weak_factory_.GetWeakPtr()));
  }
}

bool Manager::OnContainerStart(const struct signalfd_siginfo& info) {
  if (info.ssi_code == SI_USER) {
    if (!enable_multinet_)
      shill_client_->RegisterDefaultInterfaceChangedHandler(base::Bind(
          &Manager::OnDefaultInterfaceChanged, weak_factory_.GetWeakPtr()));
  }

  // Stay registered.
  return false;
}

bool Manager::OnContainerStop(const struct signalfd_siginfo& info) {
  if (info.ssi_code == SI_USER) {
    if (!enable_multinet_) {
      shill_client_->UnregisterDefaultInterfaceChangedHandler();
      device_mgr_->EnableLegacyDevice("" /* disable */);
    }
  }

  // Stay registered.
  return false;
}

void Manager::OnDefaultInterfaceChanged(const std::string& ifname) {
  LOG(INFO) << "Default interface changed to " << ifname;
  device_mgr_->EnableLegacyDevice(ifname);
}

void Manager::OnDevicesChanged(const std::set<std::string>& devices) {
  device_mgr_->Reset(devices);
}

void Manager::OnShutdown(int* exit_code) {
  device_mgr_.reset();
}

void Manager::OnSubprocessExited(pid_t pid, const siginfo_t& info) {
  LOG(ERROR) << "Subprocess " << pid << " exited unexpectedly";
  Quit();
}

void Manager::SendMessage(const IpHelperMessage& msg) {
  ip_helper_->SendMessage(msg);
}

}  // namespace arc_networkd
