// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/plugin_vm.h"

#include <signal.h>

#include <utility>
#include <vector>

#include <base/files/file.h>
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

// Name of the plugin dispatcher runtime directory.
constexpr char kDispatcherRuntimeDir[] = "/run/pvm";

// Name of the plugin dispatcher socket.
constexpr char kDispatcherSocket[] = "vmplugin_dispatcher.socket";

// Path to the plugin binaries and other assets.
constexpr char kPluginBinDir[] = "/opt/pita/";

// Name of the plugin VM binary.
constexpr char kPluginBinName[] = "pvm";

constexpr gid_t kPluginGidMap[] = {
    7,    // lp
    600,  // cras
};

// Name of the runtime directory inside the jail.
constexpr char kRuntimeDir[] = "/run/pvm";

// Name of the stateful directory inside the jail.
constexpr char kStatefulDir[] = "/pvm";

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
    std::vector<string> params,
    arc_networkd::MacAddress mac_addr,
    std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr,
    uint32_t ipv4_netmask,
    uint32_t ipv4_gateway,
    base::FilePath stateful_dir,
    base::FilePath root_dir,
    base::FilePath runtime_dir,
    std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy) {
  auto vm = base::WrapUnique(
      new PluginVm(std::move(mac_addr), std::move(ipv4_addr), ipv4_netmask,
                   ipv4_gateway, std::move(seneschal_server_proxy),
                   std::move(root_dir), std::move(runtime_dir)));
  if (!vm->Start(cpus, std::move(params), std::move(stateful_dir))) {
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
      .seneschal_server_handle = seneschal_server_handle(),
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

bool PluginVm::WriteResolvConf(const base::FilePath& parent_dir,
                               const std::vector<string>& nameservers,
                               const std::vector<string>& search_domains) {
  // Create temporary directory on the same file system so that we
  // can atomically replace old resolv.conf with new one.
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(parent_dir)) {
    LOG(ERROR) << "Failed to create temporary directory under "
               << parent_dir.value();
    return false;
  }

  base::FilePath path = temp_dir.GetPath().Append("resolv.conf");
  base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to create temporary file " << path.value();
    return false;
  }

  for (auto& ns : nameservers) {
    string nameserver_line = base::StringPrintf("nameserver %s\n", ns.c_str());
    if (!file.WriteAtCurrentPos(nameserver_line.c_str(),
                                nameserver_line.length())) {
      LOG(ERROR) << "Failed to write nameserver to temporary file";
      return false;
    }
  }

  if (!search_domains.empty()) {
    string search_domains_line = base::StringPrintf(
        "search %s\n", base::JoinString(search_domains, " ").c_str());
    if (!file.WriteAtCurrentPos(search_domains_line.c_str(),
                                search_domains_line.length())) {
      LOG(ERROR) << "Failed to write search domains to temporary file";
      return false;
    }
  }

  constexpr char kResolvConfOptions[] =
      "options single-request timeout:1 attempts:5\n";
  if (!file.WriteAtCurrentPos(kResolvConfOptions, strlen(kResolvConfOptions))) {
    LOG(ERROR) << "Failed to write search resolver options to temporary file";
    return false;
  }

  // This should flush the buffers.
  file.Close();

  base::File::Error err;
  if (!ReplaceFile(path, parent_dir.Append("resolv.conf"), &err)) {
    LOG(ERROR) << "Failed to replace resolv.conf with new instance: "
               << base::File::ErrorToString(err);
    return false;
  }

  return true;
}

bool PluginVm::SetResolvConfig(const std::vector<string>& nameservers,
                               const std::vector<string>& search_domains) {
  return WriteResolvConf(root_dir_.GetPath().Append("etc"), nameservers,
                         search_domains);
}

PluginVm::PluginVm(arc_networkd::MacAddress mac_addr,
                   std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr,
                   uint32_t ipv4_netmask,
                   uint32_t ipv4_gateway,
                   std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
                   base::FilePath root_dir,
                   base::FilePath runtime_dir)
    : mac_addr_(std::move(mac_addr)),
      ipv4_addr_(std::move(ipv4_addr)),
      netmask_(ipv4_netmask),
      gateway_(ipv4_gateway),
      seneschal_server_proxy_(std::move(seneschal_server_proxy)) {
  CHECK(ipv4_addr_);
  CHECK(base::DirectoryExists(root_dir));
  CHECK(base::DirectoryExists(runtime_dir));

  // Take ownership of the root and runtime directories.
  CHECK(root_dir_.Set(root_dir));
  CHECK(runtime_dir_.Set(runtime_dir));
}

bool PluginVm::Start(uint32_t cpus,
                     std::vector<string> params,
                     base::FilePath stateful_dir) {
  // Set up the tap device.
  base::ScopedFD tap_fd =
      BuildTapDevice(mac_addr_, gateway_, netmask_, false /*vnet_hdr*/);
  if (!tap_fd.is_valid()) {
    LOG(ERROR) << "Unable to build and configure TAP device";
    return false;
  }

  // Build up the process arguments.
  // clang-format off
  std::vector<string> args = {
    kCrosvmBin,       "run",
    "--cpus",         std::to_string(cpus),
    "--tap-fd",       std::to_string(tap_fd.get()),
    "--plugin",       base::FilePath(kPluginBinDir)
                          .Append(kPluginBinName)
                          .value(),
  };
  // clang-format on

  std::vector<string> bind_mounts = {
      "/dev/log:/dev/log:true",
      // TODO(b:117218264) replace with CUPS proxy socket when ready.
      "/run/cups/cups.sock:/run/cups/cups.sock:true",
      // TODO(b:127478233) replace with CRAS proxy socket when ready.
      "/run/cras/.cras_socket:/run/cras/.cras_socket:true",
      base::StringPrintf("%s:%s:false", kPluginBinDir, kPluginBinDir),
      // This is directory where the VM image resides.
      base::StringPrintf("%s:%s:true", stateful_dir.value().c_str(),
                         kStatefulDir),
      // This is directory where control socket, 9p socket, and other axillary
      // runtime data lives.
      base::StringPrintf("%s:%s:true", runtime_dir_.GetPath().value().c_str(),
                         kRuntimeDir),
      // Plugin '/etc' directory.
      base::StringPrintf("%s:%s:true",
                         root_dir_.GetPath().Append("etc").value().c_str(),
                         "/etc"),
      // This is the directory where the cicerone host socket lives. The plugin
      // VM also creates the guest socket for cicerone in this same directory
      // using the following <token>.sock as the name. The token resides in
      // the VM runtime directory with name cicerone.token.
      base::StringPrintf("/run/vm_cicerone/client:%s:true",
                         base::FilePath(kRuntimeDir)
                             .Append("cicerone_socket")
                             .value()
                             .c_str()),
  };

  // When testing dispatcher socket might be missing, let's warn and continue.
  if (!PathExists(
          base::FilePath(kDispatcherRuntimeDir).Append(kDispatcherSocket))) {
    LOG(WARNING) << "Plugin dispatcher socket is missing";
  } else {
    bind_mounts.emplace_back(base::StringPrintf(
        "%s:%s:true",
        base::FilePath(kDispatcherRuntimeDir)
            .Append(kDispatcherSocket)
            .value()
            .c_str(),
        base::FilePath(kRuntimeDir).Append(kDispatcherSocket).value().c_str()));
  }

  // Put everything into the brillo::ProcessImpl.
  for (auto& arg : args) {
    process_.AddArg(std::move(arg));
  }
  for (auto gid : kPluginGidMap) {
    process_.AddArg("--plugin-gid-map");
    process_.AddArg(base::StringPrintf("%u:%u:1", gid, gid));
  }
  for (auto& mount : bind_mounts) {
    process_.AddArg("--plugin-mount");
    process_.AddArg(std::move(mount));
  }
  for (auto& param : params) {
    process_.AddArg("--params");
    process_.AddArg(std::move(param));
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
