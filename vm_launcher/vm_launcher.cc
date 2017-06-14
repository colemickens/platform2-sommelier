// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <map>
#include <memory>
#include <string>

#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <brillo/process.h>
#include <brillo/syslog_logging.h>

#include "vm_launcher/constants.h"
#include "vm_launcher/mac_address.h"
#include "vm_launcher/nfs_launcher.h"
#include "vm_launcher/subnet.h"

namespace {

std::string BuildKernelCommandLine(
    const std::map<std::string, std::string>& args) {
  std::string command_line;
  for (const auto& key_value : args) {
    if (!command_line.empty()) {
      command_line += " ";
    }
    command_line += key_value.first;
    command_line += "=";
    command_line += key_value.second;
  }
  return command_line;
}

}  // namespace

int main(int argc, char** argv) {
  DEFINE_bool(kvmtool,
              false,
              "Use the kvmtool hypervisor instead of the default crosvm");
  DEFINE_bool(runc, false, "Use the runc container runtime instead of run_oci");
  DEFINE_bool(nfs, false, "Launch NFS server before launching the VM");
  DEFINE_string(container, "", "Path of the container to start");
  brillo::FlagHelper::Init(argc, argv, "Launches a container in a VM");
  brillo::InitLog(brillo::kLogToSyslog);

  if (FLAGS_container.empty()) {
    LOG(ERROR) << "No container to start";
    return 1;
  }

  struct stat container_stat;
  int rc = stat(FLAGS_container.c_str(), &container_stat);
  if (rc) {
    PLOG(ERROR) << "Failed to stat container path";
    return 1;
  }

  // TODO(smbarber): Make an init script do this.
  rc = mkdir(vm_launcher::kVmRuntimeDirectory, 0700);
  if (rc && errno != EEXIST) {
    PLOG(ERROR) << "Failed to create vm runtime directory";
    return 1;
  }

  // TODO(smbarber): Work with crosvm one day.
  if (!FLAGS_kvmtool) {
    LOG(ERROR) << "Only kvmtool is supported as a VM hypervisor";
    return 1;
  }

  brillo::ProcessImpl vm_process;
  vm_process.AddArg(vm_launcher::kLkvmBin);
  vm_process.AddArg("run");
  vm_process.AddStringOption("-k", vm_launcher::kVmKernelPath);
  vm_process.AddStringOption("-d",
                             std::string(vm_launcher::kVmRootfsPath) + ",ro");

  if (S_ISDIR(container_stat.st_mode & S_IFMT)) {
    vm_process.AddStringOption("--9p", FLAGS_container + ",container_rootfs");
  } else {
    vm_process.AddStringOption("-d", FLAGS_container + ",ro");
  }

  auto mac_addr = vm_launcher::MacAddress::Create();
  if (!mac_addr) {
    LOG(ERROR) << "Could not allocate MAC address";
    return 1;
  }

  LOG(INFO) << "Allocated MAC address " << mac_addr->ToString();

  auto subnet = vm_launcher::Subnet::Create();
  if (!subnet) {
    LOG(ERROR) << "Could not allocate subnet";
    return 1;
  }

  LOG(INFO) << "Allocated subnet with"
            << " gateway: " << subnet->GetGatewayAddress()
            << " ip: " << subnet->GetIpAddress()
            << " netmask: " << subnet->GetNetmask();

  // Handle networking-specific args.
  vm_process.AddStringOption(
      "-n",
      base::StringPrintf("mode=tap,guest_mac=%s,host_ip=%s,guest_ip=%s",
                         mac_addr->ToString().c_str(),
                         subnet->GetGatewayAddress().c_str(),
                         subnet->GetIpAddress().c_str()));

  // Create kernel command line args.
  std::map<std::string, std::string> args = {
      {"container_runtime", FLAGS_runc ? "runc" : "kvmtool"},
      {"ip_addr", subnet->GetIpAddress()},
      {"netmask", subnet->GetNetmask()},
      {"gateway", subnet->GetGatewayAddress()}};

  vm_process.AddStringOption("-p", BuildKernelCommandLine(args));

  vm_process.AddArg("--rng");

  // kvmtool likes sticking sockets in HOME. Force it to use /run/vm instead.
  rc = setenv("HOME", vm_launcher::kVmRuntimeDirectory, true);
  if (rc < 0) {
    PLOG(ERROR) << "Could not set HOME before launching kvmtool";
    return 1;
  }
  vm_launcher::NfsLauncher nfs;

  if (FLAGS_nfs && !nfs.Launch()) {
    LOG(ERROR) << "Unable to launch NFS server";
    return 1;
  }

  rc = vm_process.Run();
  LOG(INFO) << "VM exit with status code " << rc;

  return 0;
}
