// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/arc_vm.h"

#include <utility>

#include <arc/network/guest_events.h>
#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>
#include <base/sys_info.h>
#include <base/time/time.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/concierge/tap_device_builder.h"
#include "vm_tools/concierge/vm_util.h"

using std::string;

namespace vm_tools {
namespace concierge {
namespace {

// Name of the control socket used for controlling crosvm.
constexpr char kCrosvmSocket[] = "arcvm.sock";

// Path to the wayland socket.
constexpr char kWaylandSocket[] = "/run/chrome/wayland-0";

// How long to wait before timing out on child process exits.
constexpr base::TimeDelta kChildExitTimeout = base::TimeDelta::FromSeconds(10);

// Offset in a subnet of the gateway/host.
constexpr size_t kHostAddressOffset = 0;

// Offset in a subnet of the client/guest.
constexpr size_t kGuestAddressOffset = 1;

// The CPU cgroup where all the ARCVM's crosvm processes should belong to.
constexpr char kArcvmCpuCgroup[] = "/sys/fs/cgroup/cpu/vms/arc";

}  // namespace

ArcVm::ArcVm(arc_networkd::MacAddress mac_addr,
             std::unique_ptr<arc_networkd::Subnet> subnet,
             uint32_t vsock_cid,
             std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
             base::FilePath runtime_dir,
             ArcVmFeatures features)
    : mac_addr_(std::move(mac_addr)),
      subnet_(std::move(subnet)),
      vsock_cid_(vsock_cid),
      seneschal_server_proxy_(std::move(seneschal_server_proxy)),
      features_(features) {
  CHECK(subnet_);
  CHECK(base::DirectoryExists(runtime_dir));

  // Take ownership of the runtime directory.
  CHECK(runtime_dir_.Set(runtime_dir));
}

ArcVm::~ArcVm() {
  Shutdown();
}

std::unique_ptr<ArcVm> ArcVm::Create(
    base::FilePath kernel,
    base::FilePath rootfs,
    std::vector<ArcVm::Disk> disks,
    arc_networkd::MacAddress mac_addr,
    std::unique_ptr<arc_networkd::Subnet> subnet,
    uint32_t vsock_cid,
    std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
    base::FilePath runtime_dir,
    ArcVmFeatures features,
    std::vector<string> params) {
  auto vm = base::WrapUnique(new ArcVm(
      std::move(mac_addr), std::move(subnet), vsock_cid,
      std::move(seneschal_server_proxy), std::move(runtime_dir), features));

  if (!vm->Start(std::move(kernel), std::move(rootfs), std::move(disks),
                 std::move(params))) {
    vm.reset();
  }

  return vm;
}

std::string ArcVm::GetVmSocketPath() const {
  return runtime_dir_.GetPath().Append(kCrosvmSocket).value();
}

bool ArcVm::Start(base::FilePath kernel,
                  base::FilePath rootfs,
                  std::vector<ArcVm::Disk> disks,
                  std::vector<string> params) {
  // Set up the tap device.
  base::ScopedFD tap_fd =
      BuildTapDevice(mac_addr_, GatewayAddress(), Netmask(), true /*vnet_hdr*/);
  if (!tap_fd.is_valid()) {
    LOG(ERROR) << "Unable to build and configure TAP device";
    return false;
  }

  // Build up the process arguments.
  // clang-format off
  std::vector<string> args = {
    kCrosvmBin,       "run",
    "--cpus",         std::to_string(base::SysInfo::NumberOfProcessors()),
    "--mem",          GetVmMemoryMiB(),
    "--disk",         rootfs.value(),
    "--tap-fd",       std::to_string(tap_fd.get()),
    "--cid",          std::to_string(vsock_cid_),
    "--socket",       GetVmSocketPath(),
    "--wayland-sock", kWaylandSocket,
    "--wayland-dmabuf",
    "--serial",       "type=syslog,num=1",
    "--syslog-tag",   base::StringPrintf("ARCVM(%u)", vsock_cid_),
    "--cras-audio",
    "--cras-capture",
    "--params",       base::JoinString(params, " "),
  };
  // clang-format on

  if (features_.gpu)
    args.emplace_back("--gpu");

  // Add any extra disks.
  for (const auto& disk : disks) {
    if (disk.writable) {
      args.emplace_back("--rwdisk");
    } else {
      args.emplace_back("--disk");
    }

    args.emplace_back(disk.path.value());
  }

  // Finally list the path to the kernel.
  args.emplace_back(kernel.value());

  // Put everything into the brillo::ProcessImpl.
  for (string& arg : args) {
    process_.AddArg(std::move(arg));
  }

  // Change the process group before exec so that crosvm sending SIGKILL to the
  // whole process group doesn't kill us as well. The function also changes the
  // cpu cgroup for ARCVM's crosvm processes.
  process_.SetPreExecCallback(base::Bind(
      &SetUpCrosvmProcess, base::FilePath(kArcvmCpuCgroup).Append("tasks")));

  if (!process_.Start()) {
    LOG(ERROR) << "Failed to start VM process";
    return false;
  }

  // Notify arc-networkd that ARCVM is up.
  if (!arc_networkd::NotifyArcVmStart(vsock_cid_)) {
    LOG(WARNING) << "Unable to notify networking services";
  }

  // TODO(yusukes|cmtm): Create a stub for talking to Android init inside the
  // VM.
  return true;
}

bool ArcVm::Shutdown() {
  // Notify arc-networkd that ARCVM is down.
  if (!arc_networkd::NotifyArcVmStop()) {
    LOG(WARNING) << "Unable to notify networking services";
  }

  // Do a sanity check here to make sure the process is still around.  It may
  // have crashed and we don't want to be waiting around for an RPC response
  // that's never going to come.  kill with a signal value of 0 is explicitly
  // documented as a way to check for the existence of a process.
  if (!CheckProcessExists(process_.pid())) {
    // The process is already gone.
    process_.Release();
    return true;
  }

  // TODO(yusukes|cmtm): Send SIGTERM to Android init so it gracefully shuts
  // down the OS.

  // Try to shut it down via the crosvm socket.
  RunCrosvmCommand("stop", GetVmSocketPath());

  // We can't actually trust the exit codes that crosvm gives us so just see if
  // it exited.
  if (WaitForChild(process_.pid(), kChildExitTimeout)) {
    process_.Release();
    return true;
  }

  LOG(WARNING) << "Failed to stop VM " << vsock_cid_ << " via crosvm socket";

  // Kill the process with SIGTERM.
  if (process_.Kill(SIGTERM, kChildExitTimeout.InSeconds())) {
    return true;
  }

  LOG(WARNING) << "Failed to kill VM " << vsock_cid_ << " with SIGTERM";

  // Kill it with fire.
  if (process_.Kill(SIGKILL, kChildExitTimeout.InSeconds())) {
    return true;
  }

  LOG(ERROR) << "Failed to kill VM " << vsock_cid_ << " with SIGKILL";
  return false;
}

bool ArcVm::AttachUsbDevice(uint8_t bus,
                            uint8_t addr,
                            uint16_t vid,
                            uint16_t pid,
                            int fd,
                            UsbControlResponse* response) {
  return vm_tools::concierge::AttachUsbDevice(GetVmSocketPath(), bus, addr, vid,
                                              pid, fd, response);
}

bool ArcVm::DetachUsbDevice(uint8_t port, UsbControlResponse* response) {
  return vm_tools::concierge::DetachUsbDevice(GetVmSocketPath(), port,
                                              response);
}

bool ArcVm::ListUsbDevice(std::vector<UsbDevice>* devices) {
  return vm_tools::concierge::ListUsbDevice(GetVmSocketPath(), devices);
}

void ArcVm::HandleSuspendImminent() {
  RunCrosvmCommand("suspend", GetVmSocketPath());
}

void ArcVm::HandleSuspendDone() {
  RunCrosvmCommand("resume", GetVmSocketPath());
}

// static
bool ArcVm::SetVmCpuRestriction(CpuRestrictionState cpu_restriction_state) {
  int cpu_shares = 1024;
  switch (cpu_restriction_state) {
    case CPU_RESTRICTION_FOREGROUND:
      break;
    case CPU_RESTRICTION_BACKGROUND:
      cpu_shares = 64;
      break;
    default:
      NOTREACHED();
  }
  return UpdateCpuShares(base::FilePath(kArcvmCpuCgroup), cpu_shares);
}

uint32_t ArcVm::GatewayAddress() const {
  return subnet_->AddressAtOffset(kHostAddressOffset);
}

uint32_t ArcVm::IPv4Address() const {
  return subnet_->AddressAtOffset(kGuestAddressOffset);
}

uint32_t ArcVm::Netmask() const {
  return subnet_->Netmask();
}

VmInterface::Info ArcVm::GetInfo() {
  VmInterface::Info info = {
      .ipv4_address = IPv4Address(),
      .pid = pid(),
      .cid = cid(),
      .seneschal_server_handle = seneschal_server_handle(),
      .status = VmInterface::Status::RUNNING,
  };

  return info;
}

bool ArcVm::GetVmEnterpriseReportingInfo(
    GetVmEnterpriseReportingInfoResponse* response) {
  response->set_success(false);
  response->set_failure_reason("Not implemented");
  return false;
}

}  // namespace concierge
}  // namespace vm_tools
