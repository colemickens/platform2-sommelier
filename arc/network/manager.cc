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

namespace {

const char kUnprivilegedUser[] = "arc-networkd";
const uint64_t kManagerCapMask = CAP_TO_MASK(CAP_NET_RAW);

}  // namespace

namespace arc_networkd {

Manager::Manager(std::unique_ptr<HelperProcess> ip_helper) {
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
  shill_client_.reset(new ShillClient(bus_));
  shill_client_->RegisterDevicesChangedHandler(
      base::Bind(&Manager::OnDevicesChanged, weak_factory_.GetWeakPtr()));

  // Legacy device.
  AddDevice(kAndroidDevice);
  shill_client_->ScanDevices(
      base::Bind(&Manager::OnDevicesChanged, weak_factory_.GetWeakPtr()));
}

void Manager::AddDevice(const std::string& name) {
  auto device = Device::ForInterface(name);
  if (!device)
    return;
  LOG(INFO) << "Adding device " << name;
  device->RegisterMessageSink(
      base::Bind(&Manager::SendMessage, weak_factory_.GetWeakPtr()));
  device->Announce();
  devices_.emplace(name, std::move(device));
}

bool Manager::OnContainerStart(const struct signalfd_siginfo& info) {
  if (info.ssi_code == SI_USER) {
    shill_client_->RegisterDefaultInterfaceChangedHandler(base::Bind(
        &Manager::OnDefaultInterfaceChanged, weak_factory_.GetWeakPtr()));
  }

  // Stay registered.
  return false;
}

bool Manager::OnContainerStop(const struct signalfd_siginfo& info) {
  if (info.ssi_code == SI_USER) {
    shill_client_->UnregisterDefaultInterfaceChangedHandler();
  }

  // Stay registered.
  return false;
}

void Manager::OnDefaultInterfaceChanged(const std::string& ifname) {
  LOG(INFO) << "Default interface changed to " << ifname;
  // This is only applicable to the legacy 'android' interface.
  const auto it = devices_.find(kAndroidDevice);
  if (it != devices_.end()) {
    it->second->Disable();
    if (!ifname.empty())
      it->second->Enable(ifname);
  }
}

void Manager::OnDevicesChanged(const std::set<std::string>& devices) {
  for (auto it = devices_.begin(); it != devices_.end();) {
    const std::string& name = it->first;
    if (name != kAndroidDevice && devices.find(name) == devices.end()) {
      LOG(INFO) << "Removing device " << name;
      it = devices_.erase(it);
    } else {
      ++it;
    }
  }
  for (const std::string& name : devices) {
    if (devices_.find(name) == devices_.end())
      AddDevice(name);
  }
}

void Manager::OnShutdown(int* exit_code) {
  for (auto& dev : devices_) {
    dev.second->Disable();
  }
}

void Manager::OnSubprocessExited(pid_t pid, const siginfo_t& info) {
  LOG(ERROR) << "Subprocess " << pid << " exited unexpectedly";
  Quit();
}

void Manager::SendMessage(const IpHelperMessage& msg) {
  ip_helper_->SendMessage(msg);
}

}  // namespace arc_networkd
