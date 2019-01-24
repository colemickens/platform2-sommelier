// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/plugin_vm.h"

#include <signal.h>

#include <utility>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>

#include "vm_tools/concierge/tap_device_builder.h"

using std::string;

namespace vm_tools {
namespace concierge {
namespace {

// Path to the crosvm binary.
constexpr char kCrosvmBin[] = "/usr/bin/crosvm";

// Path to the plugin binary.
constexpr char kPluginBin[] = "/usr/bin/pvm";

// Name of the control socket used for controlling crosvm.
constexpr char kCrosvmSocket[] = "crosvm.sock";

// How long to wait before timing out on child process exits.
constexpr base::TimeDelta kChildExitTimeout = base::TimeDelta::FromSeconds(10);

// Sets the pgid of the current process to its pid.  This is needed because
// crosvm assumes that only it and its children are in the same process group
// and indiscriminately sends a SIGKILL if it needs to shut them down.
bool SetPgid() {
  if (setpgid(0, 0) != 0) {
    PLOG(ERROR) << "Failed to change process group id";
    return false;
  }

  return true;
}

}  // namespace

// static
std::unique_ptr<PluginVm> PluginVm::Create(
    uint32_t cpus,
    string params,
    arc_networkd::MacAddress mac_addr,
    std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr,
    uint32_t ipv4_netmask,
    uint32_t ipv4_gateway,
    base::FilePath stateful_dir,
    base::FilePath runtime_dir,
    base::FilePath cicerone_token_dir) {
  auto vm = base::WrapUnique(
      new PluginVm(std::move(mac_addr), std::move(ipv4_addr), ipv4_netmask,
                   ipv4_gateway, std::move(runtime_dir)));
  if (!vm->Start(cpus, std::move(params), std::move(stateful_dir),
                 std::move(cicerone_token_dir))) {
    vm.reset();
  }

  return vm;
}

PluginVm::~PluginVm() {
  Shutdown();
}

bool PluginVm::Shutdown() {
  // Do a sanity check here to make sure the process is still around.  It may
  // have crashed and we don't want to be waiting around for an RPC response
  // that's never going to come.  kill with a signal value of 0 is explicitly
  // documented as a way to check for the existence of a process.
  if (process_.pid() == 0 || (kill(process_.pid(), 0) < 0 && errno == ESRCH)) {
    // The process is already gone.
    process_.Release();
    return true;
  }

  // Kill the process with SIGTERM.
  if (process_.Kill(SIGTERM, kChildExitTimeout.InSeconds())) {
    return true;
  }

  LOG(WARNING) << "Failed to kill plugin VM with SIGTERM";

  // Kill it with fire.
  if (process_.Kill(SIGKILL, kChildExitTimeout.InSeconds())) {
    return true;
  }

  LOG(ERROR) << "Failed to kill plugin VM with SIGKILL";
  return false;
}

VmInterface::Info PluginVm::GetInfo() {
  VmInterface::Info info = {
      .ipv4_address = ipv4_addr_->Address(),
      .pid = process_.pid(),
      .cid = 0,
      .seneschal_server_handle = 0,
      .status = VmInterface::Status::RUNNING,
  };

  return info;
}

bool PluginVm::AttachUsbDevice(uint8_t bus,
                               uint8_t addr,
                               uint16_t vid,
                               uint16_t pid,
                               int fd,
                               UsbControlResponse* response) {
  return false;
}

bool PluginVm::DetachUsbDevice(uint8_t port, UsbControlResponse* response) {
  return false;
}

bool PluginVm::ListUsbDevice(std::vector<UsbDevice>* device) {
  return false;
}

PluginVm::PluginVm(arc_networkd::MacAddress mac_addr,
                   std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr,
                   uint32_t ipv4_netmask,
                   uint32_t ipv4_gateway,
                   base::FilePath runtime_dir)
    : mac_addr_(std::move(mac_addr)),
      ipv4_addr_(std::move(ipv4_addr)),
      netmask_(ipv4_netmask),
      gateway_(ipv4_gateway) {
  CHECK(ipv4_addr_);
  CHECK(base::DirectoryExists(runtime_dir));

  // Take ownership of the runtime directory.
  CHECK(runtime_dir_.Set(runtime_dir));
}

bool PluginVm::Start(uint32_t cpus,
                     string params,
                     base::FilePath stateful_dir,
                     base::FilePath cicerone_token_dir) {
  // Set up the tap device.
  base::ScopedFD tap_fd = BuildTapDevice(mac_addr_, gateway_, netmask_);
  if (!tap_fd.is_valid()) {
    LOG(ERROR) << "Unable to build and configure TAP device";
    return false;
  }

  // Build up the process arguments.
  // clang-format off
  std::vector<string> args = {
    kCrosvmBin,       "run",
    "--cpus",         std::to_string(cpus),
    "--mem",          "2048",  // TODO(b:80150167): memory allocation policy
    "--tap-fd",       std::to_string(tap_fd.get()),
    "--socket",       runtime_dir_.GetPath().Append(kCrosvmSocket).value(),
    "--params",       std::move(params),
    "--plugin",       kPluginBin,
    "--plugin-mount", base::StringPrintf("%s:/pvm:true",
                                         stateful_dir.value().c_str()),
    // This is the directory where the cicerone host socket lives. The plugin VM
    // also creates the guest socket for cicerone in this same directory using
    // the following <token>.sock as the name.
    "--plugin-mount",
    base::StringPrintf("/run/vm_cicerone/client:/cicerone_socket:true"),
    // This is the directory where the identity token for the plugin VM is
    // stored.
    "--plugin-mount", base::StringPrintf("%s:/cicerone_token:false",
                                         cicerone_token_dir.value().c_str()),
  };
  // clang-format on

  // Put everything into the brillo::ProcessImpl.
  for (string& arg : args) {
    process_.AddArg(std::move(arg));
  }

  // Change the process group before exec so that crosvm sending SIGKILL to the
  // whole process group doesn't kill us as well.
  process_.SetPreExecCallback(base::Bind(&SetPgid));

  if (!process_.Start()) {
    LOG(ERROR) << "Failed to start VM process";
    return false;
  }

  return true;
}

}  // namespace concierge
}  // namespace vm_tools
