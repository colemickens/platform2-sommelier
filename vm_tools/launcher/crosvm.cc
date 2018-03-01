// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/launcher/crosvm.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <chrono>  // NOLINT(build/c++11)
#include <memory>
#include <utility>
#include <vector>

#include <base/command_line.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/sys_info.h>
#include <brillo/process.h>
#include <grpc++/grpc++.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/launcher/constants.h"
#include "vm_tools/launcher/mac_address.h"
#include "vm_tools/launcher/nfs_export.h"
#include "vm_tools/launcher/subnet.h"
#include "vm_tools/launcher/vsock_cid.h"

#include "vm_guest.grpc.pb.h"  // NOLINT(build/include)

namespace vm_tools {
namespace launcher {

namespace {

constexpr char kCrosvmSocket[] = "crosvm.sock";
constexpr int kGrpcTimeoutSeconds = 1;
constexpr int kShutdownTimeoutSeconds = 2;
constexpr int kVmMaxMemoryMiB = 8192;

bool StringToIPv4Address(const std::string& address, uint32_t* addr) {
  struct in_addr in = {};
  if (inet_pton(AF_INET, address.c_str(), &in) != 1) {
    return false;
  }
  *addr = in.s_addr;
  return true;
}

}  // namespace

CrosVM::CrosVM(const std::string& name,
               const base::FilePath& vm_kernel,
               const base::FilePath& vm_rootfs,
               const base::FilePath& instance_runtime_dir,
               std::unique_ptr<MacAddress> mac_addr,
               std::shared_ptr<Subnet> subnet,
               std::unique_ptr<VsockCid> cid,
               std::unique_ptr<NfsExport> nfs_export,
               bool release_on_destruction)
    : name_(name),
      vm_kernel_(vm_kernel),
      vm_rootfs_(vm_rootfs),
      instance_runtime_dir_(instance_runtime_dir),
      mac_addr_(std::move(mac_addr)),
      subnet_(subnet),
      cid_(std::move(cid)),
      nfs_export_(std::move(nfs_export)),
      release_on_destruction_(release_on_destruction),
      vm_process_(std::make_unique<brillo::ProcessImpl>()) {}

CrosVM::~CrosVM() {
  if (!Teardown())
    LOG(ERROR) << "Failed to cleanly tear down CrosVM";
}

std::unique_ptr<CrosVM> CrosVM::Create(const std::string& name,
                                       const base::FilePath& vm_kernel,
                                       const base::FilePath& vm_rootfs,
                                       const base::FilePath& nfs_path) {
  if (!base::PathExists(vm_kernel)) {
    LOG(ERROR) << "VM kernel '" << vm_kernel.value() << "' does not exist";
    return nullptr;
  }
  if (!base::PathExists(vm_rootfs)) {
    LOG(ERROR) << "VM rootfs '" << vm_rootfs.value() << "' does not exist";
    return nullptr;
  }

  base::FilePath instance_dir =
      base::FilePath(kVmRuntimeDirectory).Append(name);
  if (base::PathExists(instance_dir)) {
    LOG(ERROR) << "VM name '" << name << "' is already in use";
    return nullptr;
  }

  int ret = mkdir(instance_dir.value().c_str(), 0770);
  if (ret) {
    PLOG(ERROR) << "Failed to make VM runtime directory";
    return nullptr;
  }

  auto mac_addr = MacAddress::Create(instance_dir);
  if (!mac_addr) {
    LOG(ERROR) << "Could not allocate MAC address";
    return nullptr;
  }

  LOG(INFO) << "Allocated MAC address " << mac_addr->ToString();

  auto subnet = Subnet::Create(instance_dir);
  if (!subnet) {
    LOG(ERROR) << "Could not allocate subnet";
    return nullptr;
  }

  LOG(INFO) << "Allocated subnet with"
            << " gateway: " << subnet->GetGatewayAddress()
            << " ip: " << subnet->GetIpAddress()
            << " netmask: " << subnet->GetNetmask();

  auto cid = VsockCid::Create(instance_dir);
  if (!cid) {
    LOG(ERROR) << "Could not allocate vsock cid";
    return nullptr;
  }

  LOG(INFO) << "Allocated vsock cid: " << cid->GetCid();

  std::unique_ptr<NfsExport> nfs_export = nullptr;
  if (!nfs_path.empty()) {
    nfs_export = NfsExport::Create(instance_dir, nfs_path, subnet);
    if (!nfs_export) {
      LOG(ERROR) << "Could not allocate NFS export id";
      return nullptr;
    }
    LOG(INFO) << "Allocated NFS export id: " << nfs_export->GetExportID();
  }

  return std::unique_ptr<CrosVM>(new CrosVM(
      name, vm_kernel, vm_rootfs, instance_dir, std::move(mac_addr),
      std::move(subnet), std::move(cid), std::move(nfs_export), true));
}

std::unique_ptr<CrosVM> CrosVM::Load(const std::string& name) {
  base::FilePath instance_dir =
      base::FilePath(kVmRuntimeDirectory).Append(name);
  if (!base::DirectoryExists(instance_dir)) {
    LOG(ERROR) << "VM '" << name << "' doesn't appear to be running";
    return nullptr;
  }

  auto mac_addr = MacAddress::Load(instance_dir);
  if (!mac_addr) {
    LOG(ERROR) << "Could not load MAC address";
    return nullptr;
  }

  auto subnet = Subnet::Load(instance_dir);
  if (!subnet) {
    LOG(ERROR) << "Could not load subnet";
    return nullptr;
  }

  auto cid = VsockCid::Load(instance_dir);
  if (!cid) {
    LOG(ERROR) << "Could not load vsock cid";
    return nullptr;
  }

  auto nfs_export = NfsExport::Load(instance_dir, subnet);
  if (!nfs_export) {
    LOG(WARNING)
        << "Could not allocate NFS export id. The VM may not have NFS enabled.";
  }

  base::FilePath emptyPath;
  auto crosvm = std::unique_ptr<CrosVM>(
      new CrosVM(name, emptyPath, emptyPath, instance_dir, std::move(mac_addr),
                 subnet, std::move(cid), std::move(nfs_export), false));

  if (!crosvm->RestoreProcessState())
    return nullptr;

  return crosvm;
}

bool CrosVM::GetNameForPid(pid_t target_pid, std::string* vm_name) {
  base::FileEnumerator file_enum(base::FilePath(kVmRuntimeDirectory),
                                 false,  // recursive
                                 base::FileEnumerator::DIRECTORIES);

  for (base::FilePath instance_dir = file_enum.Next(); !instance_dir.empty();
       instance_dir = file_enum.Next()) {
    base::FilePath pid_path = instance_dir.Append("pid");

    std::string pid_raw;
    if (!base::ReadFileToString(pid_path, &pid_raw)) {
      LOG(ERROR) << "Failed to read pid path: " << pid_path.value();
      return false;
    }

    pid_t pid;
    if (!base::StringToInt(pid_raw, &pid)) {
      LOG(ERROR) << "Failed to parse pid contents: " << pid_raw;
      return false;
    }

    if (pid == target_pid) {
      *vm_name = instance_dir.BaseName().value();
      return true;
    }
  }

  return false;
}

bool CrosVM::Start(bool ssh,
                   const base::FilePath& container_disk,
                   bool rw_container) {
  if (!BuildCrosVMCommandLine(container_disk, rw_container))
    return false;

  vm_process_->RedirectInput("/dev/null");
  vm_process_->RedirectOutput("/dev/null");
  if (!vm_process_->Start()) {
    LOG(ERROR) << "Failed to start VM process";
    return false;
  }

  if (!SaveProcessState())
    return false;

  if (!VMInit(ssh, !container_disk.empty(), rw_container))
    return false;

  // VM has started succesfully; don't tear it down when we exit now.
  SetReleaseOnDestruction(false);

  return true;
}

bool CrosVM::Run(bool ssh,
                 const base::FilePath& container_disk,
                 bool rw_container) {
  if (!BuildCrosVMCommandLine(container_disk, rw_container))
    return false;

  if (!vm_process_->Start()) {
    LOG(ERROR) << "Failed to start VM process";
    return false;
  }

  if (!SaveProcessState())
    return false;

  if (!VMInit(ssh, !container_disk.empty(), rw_container))
    return false;

  int rc = vm_process_->Wait();
  LOG(INFO) << "VM exit with status code " << rc;

  return Teardown();
}

bool CrosVM::Stop() {
  SetReleaseOnDestruction(true);
  return Teardown();
}

bool CrosVM::SaveProcessState() {
  base::FilePath pid_path = instance_runtime_dir_.Append("pid");
  std::string pid = base::StringPrintf("%d", vm_process_->pid());
  return base::WriteFile(pid_path, pid.c_str(), pid.length());
}

bool CrosVM::RestoreProcessState() {
  base::FilePath pid_path = instance_runtime_dir_.Append("pid");
  if (!vm_process_->ResetPidByFile(pid_path.value())) {
    LOG(ERROR) << "Failed to load VM process pid from " << pid_path.value();
    return false;
  }

  // If the VM process is no longer running, don't try to manage it.
  if (!brillo::Process::ProcessExists(vm_process_->pid()))
    return vm_process_->Release() != 0;

  // The VM process is still running, so set up the maitred stub.
  stub_ = std::make_unique<vm_tools::Maitred::Stub>(grpc::CreateChannel(
      base::StringPrintf("vsock:%u:%u", cid_->GetCid(), vm_tools::kMaitredPort),
      grpc::InsecureChannelCredentials()));

  return true;
}

bool CrosVM::BuildCrosVMCommandLine(const base::FilePath& container_disk,
                                    bool rw_container) {
  vm_process_->AddArg(kCrosvmBin);
  vm_process_->AddArg("run");

  // Give the VM the same number of CPUs as the host, and 75% of system memory
  // or 8 GiB, whichever is less. This is overprovisioned under the assumption
  // that virtio-balloon will reduce the real memory footprint.
  vm_process_->AddStringOption(
      "--cpus", std::to_string(base::SysInfo::NumberOfProcessors()));

  int64_t vm_memory_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  vm_memory_mb = (vm_memory_mb * 3) / 4;
  if (vm_memory_mb > kVmMaxMemoryMiB)
    vm_memory_mb = kVmMaxMemoryMiB;
  vm_process_->AddStringOption("--mem", std::to_string(vm_memory_mb));

  // Add rootfs disk and container disk.
  vm_process_->AddStringOption("--root", vm_rootfs_.value());
  if (!container_disk.empty()) {
    if (rw_container)
      vm_process_->AddStringOption("--rwdisk", container_disk.value());
    else
      vm_process_->AddStringOption("--disk", container_disk.value());
  }

  // Handle networking-specific args.
  vm_process_->AddStringOption("--mac", mac_addr_->ToString());
  vm_process_->AddStringOption("--host_ip", subnet_->GetGatewayAddress());
  vm_process_->AddStringOption("--netmask", subnet_->GetNetmask());

  vm_process_->AddStringOption("--cid",
                               base::StringPrintf("%u", cid_->GetCid()));

  base::FilePath socket_path = instance_runtime_dir_.Append(kCrosvmSocket);
  vm_process_->AddStringOption("--socket", socket_path.value());

  vm_process_->AddStringOption("--wayland-sock", "/run/chrome/wayland-0");

  vm_process_->AddArg(vm_kernel_.value());

  return true;
}

bool CrosVM::VMInit(bool ssh, bool run_container, bool rw_container) {
  stub_ = std::make_unique<vm_tools::Maitred::Stub>(grpc::CreateChannel(
      base::StringPrintf("vsock:%u:%u", cid_->GetCid(), vm_tools::kMaitredPort),
      grpc::InsecureChannelCredentials()));

  // TODO(smbarber): Remove this terrible sleep. crbug.com/765056
  sleep(2);
  if (!ConfigureNetwork())
    return false;

  // For ssh, we must first generate the host key, then we can start sshd.
  if (ssh) {
    if (!LaunchProcess(
            {"/usr/bin/ssh-keygen", "-f",
             "/run/sshd/ssh_host_ed25519_key", "-N", "", "-t", "ed25519"},
            false, true)) {
      LOG(ERROR) << "Failed to generate SSH host key for guest";
      return false;
    }

    if (!LaunchProcess(
            {"/usr/sbin/sshd", "-f", "/etc/ssh/termina_sshd_config"},
            true, false)) {
      LOG(ERROR) << "Failed to start sshd in guest";
      return false;
    }
  }

  if (run_container) {
    uint64_t mount_flags = 0;
    if (!rw_container)
      mount_flags = MS_RDONLY;

    if (!Mount("/dev/vdb", "/mnt/container_rootfs", "ext4", mount_flags, "")) {
      LOG(ERROR) << "Failed to mount container disk image";
      return false;
    }

    if (nfs_export_) {
      const std::string source =
          base::StringPrintf("%s:%s", subnet_->GetGatewayAddress().c_str(),
                             nfs_export_->GetExportPath().value().c_str());
      const std::string opts = base::StringPrintf(
          "nolock,vers=3,addr=%s", subnet_->GetGatewayAddress().c_str());
      if (!Mount(source, "/mnt/container_private", "nfs", 0, opts)) {
        LOG(ERROR) << "Failed to mount nfs share";
        return false;
      }
    }

    if (!LaunchProcess(
            {"run_oci", "run", "--cgroup_parent=chronos_containers",
             "--container_path=/mnt/container_rootfs", "termina_container"},
            false, false)) {
      LOG(ERROR) << "Failed to start container in guest";
      return false;
    }
  }
  return true;
}

void CrosVM::SetReleaseOnDestruction(bool release_on_destruction) {
  release_on_destruction_ = release_on_destruction;
}

bool CrosVM::LaunchProcess(const std::vector<std::string>& args,
                           bool respawn,
                           bool wait_for_exit) {
  vm_tools::LaunchProcessRequest request;
  vm_tools::LaunchProcessResponse response;
  google::protobuf::RepeatedPtrField<std::string> argv(args.begin(),
                                                       args.end());
  request.mutable_argv()->Swap(&argv);

  request.set_respawn(respawn);
  request.set_wait_for_exit(wait_for_exit);

  grpc::ClientContext ctx;
  ctx.set_deadline(std::chrono::system_clock::now() +
                   std::chrono::seconds(kGrpcTimeoutSeconds));

  grpc::Status status = stub_->LaunchProcess(&ctx, request, &response);

  if (!status.ok()) {
    LOG(ERROR) << "Failed to start " << args[0] << ": "
               << status.error_message();
    return false;
  }

  return true;
}

bool CrosVM::Mount(const std::string& source,
                   const std::string& target,
                   const std::string& fstype,
                   const uint64_t mountflags,
                   const std::string& options) {
  vm_tools::MountRequest request;
  vm_tools::MountResponse response;

  request.set_source(source);
  request.set_target(target);
  request.set_fstype(fstype);
  request.set_mountflags(mountflags);
  request.set_options(options);

  grpc::ClientContext ctx;
  ctx.set_deadline(std::chrono::system_clock::now() +
                   std::chrono::seconds(kGrpcTimeoutSeconds));

  grpc::Status status = stub_->Mount(&ctx, request, &response);

  if (!status.ok()) {
    LOG(ERROR) << "Failed to send mount RPC for " << target << ": "
               << status.error_message();
    return false;
  }

  if (response.error() != 0) {
    LOG(ERROR) << "Failed to mount " << target << ": "
               << strerror(response.error());
    return false;
  }

  return true;
}

bool CrosVM::ConfigureNetwork() {
  vm_tools::NetworkConfigRequest request;
  vm_tools::IPv4Config* config = request.mutable_ipv4_config();
  uint32_t addr;

  if (!StringToIPv4Address(subnet_->GetIpAddress().c_str(), &addr)) {
    LOG(ERROR) << "Failed to parse guest IPv4 address";
    return false;
  }
  config->set_address(addr);

  if (!StringToIPv4Address(subnet_->GetNetmask().c_str(), &addr)) {
    LOG(ERROR) << "Failed to parse subnet netmask";
    return false;
  }
  config->set_netmask(addr);

  if (!StringToIPv4Address(subnet_->GetGatewayAddress().c_str(), &addr)) {
    LOG(ERROR) << "Failed to parse subnet gateway address";
    return false;
  }
  config->set_gateway(addr);

  grpc::ClientContext ctx;
  ctx.set_deadline(std::chrono::system_clock::now() +
                   std::chrono::seconds(kGrpcTimeoutSeconds));

  vm_tools::EmptyMessage empty;

  grpc::Status status = stub_->ConfigureNetwork(&ctx, request, &empty);

  if (status.ok()) {
    LOG(INFO) << "Successfully configured network";
    return true;
  } else {
    LOG(ERROR) << "Failed to configure network: " << status.error_message();
    return false;
  }
}

bool CrosVM::Shutdown() {
  grpc::ClientContext ctx;
  ctx.set_deadline(std::chrono::system_clock::now() +
                   std::chrono::seconds(kShutdownTimeoutSeconds));

  vm_tools::EmptyMessage empty;

  grpc::Status status = stub_->Shutdown(&ctx, empty, &empty);

  // HACK: maitred currently shuts down before a response can be sent.
  // We assume here that an endpoint read failing means the shutdown succeeded.
  if (status.ok() || status.error_message() == "Endpoint read failed") {
    LOG(INFO) << "Successfully shut down VM";
    return true;
  } else {
    LOG(ERROR) << "Failed to shut down VM: " << status.error_message();
    return false;
  }
}

bool CrosVM::Teardown() {
  // Resources should be set to match CrosVM's release_on_destruction policy.
  if (mac_addr_)
    mac_addr_->SetReleaseOnDestruction(release_on_destruction_);
  if (subnet_)
    subnet_->SetReleaseOnDestruction(release_on_destruction_);
  if (cid_)
    cid_->SetReleaseOnDestruction(release_on_destruction_);
  if (nfs_export_)
    nfs_export_->SetReleaseOnDestruction(release_on_destruction_);

  if (release_on_destruction_) {
    // Check that the VM process is running before we attempt any shutdown.
    bool vm_dead = !brillo::Process::ProcessExists(vm_process_->pid());
    if (!vm_dead && Shutdown())
      vm_dead = true;

    if (!vm_dead && StopCrosvm())
      vm_dead = true;

    // Attempt SIGTERM first. If this succeeds, we can release the process
    // from management. Otherwise, we'll send a SIGKILL automatically
    // when the process is destructed.
    if (!vm_dead && vm_process_->Kill(SIGTERM, 5))
      vm_dead = true;

    // If shutdown was successful, release pid from management to avoid
    // an unnecessary SIGKILL.
    if (vm_dead)
      vm_process_->Release();

    if (!base::DeleteFile(instance_runtime_dir_, true)) {
      LOG(ERROR) << "Failed to remove runtime dir for '" << name_ << "'";
      return false;
    }
  } else {
    // Release the VM process from management, otherwise it will be SIGKILL'd.
    vm_process_->Release();
  }

  return true;
}

bool CrosVM::StopCrosvm() {
  brillo::ProcessImpl stop_process;
  stop_process.AddArg(kCrosvmBin);
  stop_process.AddArg("stop");
  base::FilePath socket_path = instance_runtime_dir_.Append(kCrosvmSocket);
  stop_process.AddArg(socket_path.value());

  LOG(INFO) << "Stopping crosvm via control socket";
  return stop_process.Run() == 0;
}

}  // namespace launcher
}  // namespace vm_tools
