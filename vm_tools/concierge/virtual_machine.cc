// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/virtual_machine.h"

#include <arpa/inet.h>
#include <linux/capability.h>
#include <signal.h>

#include <utility>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>
#include <base/sys_info.h>
#include <google/protobuf/repeated_field.h>
#include <grpc++/grpc++.h>

#include "vm_tools/common/constants.h"

using std::string;

namespace vm_tools {
namespace concierge {
namespace {

using LaunchProcessResult = VirtualMachine::LaunchProcessResult;
using ProcessExitBehavior = VirtualMachine::ProcessExitBehavior;
using ProcessStatus = VirtualMachine::ProcessStatus;
using Subnet = SubnetPool::Subnet;

// Path to the crosvm binary.
constexpr char kCrosvmBin[] = "/usr/bin/crosvm";

// Name of the control socket used for controlling crosvm.
constexpr char kCrosvmSocket[] = "crosvm.sock";

// Path to the wayland socket.
constexpr char kWaylandSocket[] = "/run/chrome/wayland-0";

// How long to wait before timing out on shutdown RPCs.
constexpr int64_t kShutdownTimeoutSeconds = 6;

// How long to wait before timing out on regular RPCs.
constexpr int64_t kDefaultTimeoutSeconds = 2;

// Calculates the amount of memory to give the virtual machine. Currently
// configured to provide 75% of system memory. This is deliberately over
// provisioned with the expectation that we will use the balloon driver to
// reduce the actual memory footprint.
string GetVmMemoryMiB() {
  int64_t vm_memory_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  vm_memory_mb /= 4;
  vm_memory_mb *= 3;

  return std::to_string(vm_memory_mb);
}

// Converts an EUI-48 mac address into string representation.
string MacAddressToString(const MacAddress& addr) {
  constexpr char kMacAddressFormat[] =
      "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx";
  return base::StringPrintf(kMacAddressFormat, addr[0], addr[1], addr[2],
                            addr[3], addr[4], addr[5]);
}

// Converts a string into an IPv4 address in network byte order.
bool StringToIPv4Address(const string& address, uint32_t* addr) {
  CHECK(addr);

  struct in_addr in = {};
  if (inet_pton(AF_INET, address.c_str(), &in) != 1) {
    return false;
  }
  *addr = in.s_addr;
  return true;
}

}  // namespace

VirtualMachine::VirtualMachine(MacAddress mac_addr,
                               std::unique_ptr<Subnet> subnet,
                               uint32_t vsock_cid,
                               base::FilePath runtime_dir)
    : mac_addr_(std::move(mac_addr)),
      subnet_(std::move(subnet)),
      vsock_cid_(vsock_cid) {
  CHECK(subnet_);
  CHECK(base::DirectoryExists(runtime_dir));

  // Take ownership of the runtime directory.
  CHECK(runtime_dir_.Set(runtime_dir));
}

VirtualMachine::~VirtualMachine() {
  Shutdown();
}

std::unique_ptr<VirtualMachine> VirtualMachine::Create(
    base::FilePath kernel,
    base::FilePath rootfs,
    std::vector<VirtualMachine::Disk> disks,
    MacAddress mac_addr,
    std::unique_ptr<Subnet> subnet,
    uint32_t vsock_cid,
    base::FilePath runtime_dir) {
  auto vm = base::WrapUnique(new VirtualMachine(std::move(mac_addr),
                                                std::move(subnet), vsock_cid,
                                                std::move(runtime_dir)));

  if (!vm->Start(std::move(kernel), std::move(rootfs), std::move(disks))) {
    vm.reset();
  }

  return vm;
}

bool VirtualMachine::Start(base::FilePath kernel,
                           base::FilePath rootfs,
                           std::vector<VirtualMachine::Disk> disks) {
  // Build up the process arguments.
  // clang-format off
  std::vector<string> args = {
      kCrosvmBin,       "run",
      "--cpus",         std::to_string(base::SysInfo::NumberOfProcessors()),
      "--mem",          GetVmMemoryMiB(),
      "--root",         rootfs.value(),
      "--mac",          MacAddressToString(mac_addr_),
      "--host_ip",      subnet_->GatewayAddress(),
      "--netmask",      subnet_->Netmask(),
      "--cid",          std::to_string(vsock_cid_),
      "--socket",       runtime_dir_.path().Append(kCrosvmSocket).value(),
      "--wayland-sock", kWaylandSocket,
  };
  // clang-format on

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

  if (!process_.Start()) {
    LOG(ERROR) << "Failed to start VM process";
    return false;
  }

  // Create a stub for talking to the maitre'd instance inside the VM.
  stub_ = std::make_unique<vm_tools::Maitred::Stub>(grpc::CreateChannel(
      base::StringPrintf("vsock:%u:%u", vsock_cid_, vm_tools::kMaitredPort),
      grpc::InsecureChannelCredentials()));

  return true;
}

bool VirtualMachine::Shutdown() {
  // Do a sanity check here to make sure the process is still around.  It may
  // have crashed and we don't want to be waiting around for an RPC response
  // that's never going to come.  kill with a signal value of 0 is explicitly
  // documented as a way to check for the existence of a process.
  if (process_.pid() == 0 || (kill(process_.pid(), 0) < 0 && errno == ESRCH)) {
    // The process is already gone.
    process_.Release();
    return true;
  }

  LOG(INFO) << "Shutting down VM " << vsock_cid_;

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kShutdownTimeoutSeconds, GPR_TIMESPAN)));

  vm_tools::EmptyMessage empty;
  grpc::Status status = stub_->Shutdown(&ctx, empty, &empty);

  if (status.ok()) {
    process_.Wait();
    return true;
  }

  LOG(WARNING) << "Shutdown RPC failed for VM " << vsock_cid_ << " with error "
               << "code " << status.error_code() << ": "
               << status.error_message();

  // Try to shut it down via the crosvm socket.
  brillo::ProcessImpl crosvm;
  crosvm.AddArg(kCrosvmBin);
  crosvm.AddArg("stop");
  crosvm.AddArg(runtime_dir_.path().Append(kCrosvmSocket).value());

  int code = crosvm.Run();
  if (code == 0) {
    process_.Wait();
    return true;
  }

  LOG(WARNING) << "Failed to stop VM " << vsock_cid_ << " via crosvm socket "
               << "(status code " << code << ")";

  // Kill the process with SIGTERM.
  if (process_.Kill(SIGTERM, kShutdownTimeoutSeconds)) {
    return true;
  }

  LOG(WARNING) << "Failed to kill VM " << vsock_cid_ << " with SIGTERM";

  // Kill it with fire.
  if (process_.Kill(SIGKILL, kShutdownTimeoutSeconds)) {
    return true;
  }

  LOG(ERROR) << "Failed to kill VM " << vsock_cid_ << " with SIGKILL";
  return false;
}

LaunchProcessResult VirtualMachine::LaunchProcess(std::vector<string> args,
                                                  bool respawn,
                                                  bool wait_for_exit) {
  CHECK(!args.empty());
  DCHECK(!(respawn && wait_for_exit));

  LOG(INFO) << "Launching " << args[0] << " inside VM " << vsock_cid_;

  vm_tools::LaunchProcessRequest request;
  vm_tools::LaunchProcessResponse response;

  google::protobuf::RepeatedPtrField<string> argv(args.begin(), args.end());
  request.mutable_argv()->Swap(&argv);
  request.set_respawn(respawn);
  request.set_wait_for_exit(wait_for_exit);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = stub_->LaunchProcess(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to launch " << args[0] << ": "
               << status.error_message();
    return {.status = ProcessStatus::FAILED};
  }

  LaunchProcessResult result;
  switch (response.status()) {
    case vm_tools::UNKNOWN:
      result.status = ProcessStatus::UNKNOWN;
      break;
    case vm_tools::EXITED:
      result.status = ProcessStatus::EXITED;
      result.code = response.code();
      break;
    case vm_tools::SIGNALED:
      result.status = ProcessStatus::SIGNALED;
      result.code = response.code();
      break;
    case vm_tools::FAILED:
      result.status = ProcessStatus::FAILED;
      break;
    default:
      result.status = ProcessStatus::UNKNOWN;
      break;
  }

  return result;
}

LaunchProcessResult VirtualMachine::StartProcess(
    std::vector<string> args, ProcessExitBehavior exit_behavior) {
  return LaunchProcess(std::move(args),
                       exit_behavior == ProcessExitBehavior::RESPAWN_ON_EXIT,
                       false);
}

LaunchProcessResult VirtualMachine::RunProcess(std::vector<string> args) {
  return LaunchProcess(std::move(args), false, true);
}

bool VirtualMachine::ConfigureNetwork() {
  LOG(INFO) << "Configuring network for VM " << vsock_cid_;

  vm_tools::NetworkConfigRequest request;
  vm_tools::EmptyMessage response;

  vm_tools::IPv4Config* config = request.mutable_ipv4_config();
  uint32_t addr;

  if (!StringToIPv4Address(subnet_->IPv4Address().c_str(), &addr)) {
    LOG(ERROR) << "Failed to parse guest IPv4 address";
    return false;
  }
  config->set_address(addr);

  if (!StringToIPv4Address(subnet_->Netmask().c_str(), &addr)) {
    LOG(ERROR) << "Failed to parse subnet netmask";
    return false;
  }
  config->set_netmask(addr);

  if (!StringToIPv4Address(subnet_->GatewayAddress().c_str(), &addr)) {
    LOG(ERROR) << "Failed to parse subnet gateway address";
    return false;
  }
  config->set_gateway(addr);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = stub_->ConfigureNetwork(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to configure network for VM " << vsock_cid_ << ": "
               << status.error_message();
    return false;
  }

  return true;
}

bool VirtualMachine::Mount(string source,
                           string target,
                           string fstype,
                           uint64_t mountflags,
                           string options) {
  LOG(INFO) << "Mounting " << source << " on " << target << " inside VM "
            << vsock_cid_;

  vm_tools::MountRequest request;
  vm_tools::MountResponse response;

  request.mutable_source()->swap(source);
  request.mutable_target()->swap(target);
  request.mutable_fstype()->swap(fstype);
  request.set_mountflags(mountflags);
  request.mutable_options()->swap(options);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = stub_->Mount(&ctx, request, &response);
  if (!status.ok() || response.error() != 0) {
    LOG(ERROR) << "Failed to mount " << request.source() << " on "
               << request.target() << " inside VM " << vsock_cid_ << ": "
               << (status.ok() ? strerror(response.error())
                               : status.error_message());
    return false;
  }

  return true;
}

void VirtualMachine::set_stub_for_testing(
    std::unique_ptr<vm_tools::Maitred::Stub> stub) {
  stub_ = std::move(stub);
}

std::unique_ptr<VirtualMachine> VirtualMachine::CreateForTesting(
    MacAddress mac_addr,
    std::unique_ptr<SubnetPool::Subnet> subnet,
    uint32_t vsock_cid,
    base::FilePath runtime_dir,
    std::unique_ptr<vm_tools::Maitred::Stub> stub) {
  auto vm = base::WrapUnique(new VirtualMachine(std::move(mac_addr),
                                                std::move(subnet), vsock_cid,
                                                std::move(runtime_dir)));

  vm->set_stub_for_testing(std::move(stub));

  return vm;
}

}  // namespace concierge
}  // namespace vm_tools
