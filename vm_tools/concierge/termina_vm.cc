// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/termina_vm.h"

#include <arpa/inet.h>
#include <linux/capability.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <utility>

#include <base/bind.h>
#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/guid.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <base/sys_info.h>
#include <base/time/time.h>
#include <google/protobuf/repeated_field.h>
#include <grpcpp/grpcpp.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/concierge/tap_device_builder.h"
#include "vm_tools/concierge/vm_util.h"

using std::string;

namespace vm_tools {
namespace concierge {
namespace {

// Name of the control socket used for controlling crosvm.
constexpr char kCrosvmSocket[] = "crosvm.sock";

// Path to the logger(1) binary.
constexpr char kLoggerBin[] = "/usr/bin/logger";

// Path to the wayland socket.
constexpr char kWaylandSocket[] = "/run/chrome/wayland-0";

// How long to wait before timing out on shutdown RPCs.
constexpr int64_t kShutdownTimeoutSeconds = 30;

// How long to wait before timing out on StartTermina RPCs.
constexpr int64_t kStartTerminaTimeoutSeconds = 150;

// How long to wait before timing out on regular RPCs.
constexpr int64_t kDefaultTimeoutSeconds = 10;

// How long to wait before timing out on child process exits.
constexpr base::TimeDelta kChildExitTimeout = base::TimeDelta::FromSeconds(10);

// Offset in a subnet of the gateway/host.
constexpr size_t kHostAddressOffset = 0;

// Offset in a subnet of the client/guest.
constexpr size_t kGuestAddressOffset = 1;

// The CPU cgroup where all the Termina crosvm processes should belong to.
constexpr char kTerminaCpuCgroup[] = "/sys/fs/cgroup/cpu/vms/termina";

}  // namespace

TerminaVm::TerminaVm(
    arc_networkd::MacAddress mac_addr,
    std::unique_ptr<arc_networkd::Subnet> subnet,
    uint32_t vsock_cid,
    std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
    base::FilePath runtime_dir,
    std::string stateful_device,
    VmFeatures features)
    : mac_addr_(std::move(mac_addr)),
      subnet_(std::move(subnet)),
      vsock_cid_(vsock_cid),
      seneschal_server_proxy_(std::move(seneschal_server_proxy)),
      features_(features),
      stateful_device_(stateful_device) {
  CHECK(subnet_);
  CHECK(base::DirectoryExists(runtime_dir));

  // Take ownership of the runtime directory.
  CHECK(runtime_dir_.Set(runtime_dir));
}

TerminaVm::~TerminaVm() {
  Shutdown();
}

std::unique_ptr<TerminaVm> TerminaVm::Create(
    base::FilePath kernel,
    base::FilePath rootfs,
    std::vector<TerminaVm::Disk> disks,
    arc_networkd::MacAddress mac_addr,
    std::unique_ptr<arc_networkd::Subnet> subnet,
    uint32_t vsock_cid,
    std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
    base::FilePath runtime_dir,
    std::string stateful_device,
    VmFeatures features) {
  auto vm = base::WrapUnique(
      new TerminaVm(std::move(mac_addr), std::move(subnet), vsock_cid,
                    std::move(seneschal_server_proxy), std::move(runtime_dir),
                    std::move(stateful_device), features));

  if (!vm->Start(std::move(kernel), std::move(rootfs), std::move(disks))) {
    vm.reset();
  }

  return vm;
}

std::string TerminaVm::GetVmSocketPath() const {
  return runtime_dir_.GetPath().Append(kCrosvmSocket).value();
}

bool TerminaVm::Start(base::FilePath kernel,
                      base::FilePath rootfs,
                      std::vector<TerminaVm::Disk> disks) {
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
      "--root",         rootfs.value(),
      "--tap-fd",       std::to_string(tap_fd.get()),
      "--cid",          std::to_string(vsock_cid_),
      "--socket",       GetVmSocketPath(),
      "--wayland-sock", kWaylandSocket,
      "--cras-audio",
      "--params",      "snd_intel8x0.inside_vm=1 snd_intel8x0.ac97_clock=48000",
  };
  // clang-format on

  if (USE_CROSVM_WL_DMABUF)
    args.emplace_back("--wayland-dmabuf");

  if (features_.gpu)
    args.emplace_back("--gpu");

  if (features_.software_tpm)
    args.emplace_back("--software-tpm");

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
  // cpu cgroup for Termina crosvm processes.
  process_.SetPreExecCallback(base::Bind(
      &SetUpCrosvmProcess, base::FilePath(kTerminaCpuCgroup).Append("tasks")));

  // Redirect STDOUT to a pipe.
  process_.RedirectUsingPipe(STDOUT_FILENO, false /* is_input */);

  if (!process_.Start()) {
    LOG(ERROR) << "Failed to start VM process";
    return false;
  }

  // Setup kernel logger process.

  // Setup logger arguments.
  std::vector<string> logger_args = {
      kLoggerBin,
      // Host syslog deamon requires priority to be set.
      "-p",
      "auth.info",
      "--skip-empty",
      // Tag each to specify the VM number.
      "--tag",
      base::StringPrintf("VM(%u)", vsock_cid_),
  };

  for (string& arg : logger_args) {
    logger_process_.AddArg(std::move(arg));
  }

  // Bind crosvm's output pipe to the logger's input pipe.
  logger_process_.BindFd(process_.GetPipe(STDOUT_FILENO), STDIN_FILENO);

  // If the Logger file fails to start, just leave a warning.
  if (!logger_process_.Start()) {
    LOG(ERROR) << "Failed to start the logger process for VM " << vsock_cid_;
  }

  // Create a stub for talking to the maitre'd instance inside the VM.
  stub_ = std::make_unique<vm_tools::Maitred::Stub>(grpc::CreateChannel(
      base::StringPrintf("vsock:%u:%u", vsock_cid_, vm_tools::kMaitredPort),
      grpc::InsecureChannelCredentials()));

  return true;
}

bool TerminaVm::Shutdown() {
  // Do a sanity check here to make sure the process is still around.  It may
  // have crashed and we don't want to be waiting around for an RPC response
  // that's never going to come.  kill with a signal value of 0 is explicitly
  // documented as a way to check for the existence of a process.
  if (!CheckProcessExists(process_.pid())) {
    // The process is already gone.
    process_.Release();
    return true;
  }

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kShutdownTimeoutSeconds, GPR_TIMESPAN)));

  vm_tools::EmptyMessage empty;
  grpc::Status status = stub_->Shutdown(&ctx, empty, &empty);

  // brillo::ProcessImpl doesn't provide a timed wait function and while the
  // Shutdown RPC may have been successful we can't really trust crosvm to
  // actually exit.  This may result in an untimed wait() blocking indefinitely.
  // Instead, do a timed wait here and only return success if the process
  // _actually_ exited as reported by the kernel, which is really the only
  // thing we can trust here.
  if (status.ok() && WaitForChild(process_.pid(), kChildExitTimeout)) {
    process_.Release();
    return true;
  }

  LOG(WARNING) << "Shutdown RPC failed for VM " << vsock_cid_ << " with error "
               << "code " << status.error_code() << ": "
               << status.error_message();

  // Try to shut it down via the crosvm socket.
  RunCrosvmCommand("stop");

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

bool TerminaVm::ConfigureNetwork(const std::vector<string>& nameservers,
                                 const std::vector<string>& search_domains) {
  LOG(INFO) << "Configuring network for VM " << vsock_cid_;

  vm_tools::NetworkConfigRequest request;
  vm_tools::EmptyMessage response;

  vm_tools::IPv4Config* config = request.mutable_ipv4_config();
  config->set_address(IPv4Address());
  config->set_gateway(GatewayAddress());
  config->set_netmask(Netmask());

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

  // TODO(smbarber): check return value here once all VMs have SetResolvConfig.
  // Ignore the return value here for now. If the guest VM doesn't yet
  // implement the SetResolvConfig RPC, it's not a failure.
  SetResolvConfig(nameservers, search_domains);
  return true;
}

void TerminaVm::RunCrosvmCommand(string command) {
  vm_tools::concierge::RunCrosvmCommand(std::move(command), GetVmSocketPath());
}

bool TerminaVm::Mount(string source,
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

bool TerminaVm::StartTermina(std::string lxd_subnet,
                             std::string stateful_device,
                             std::string* out_error) {
  // We record the kernel version early to ensure that no container has
  // been started and the VM can still be trusted.
  RecordKernelVersionForEnterpriseReporting();

  vm_tools::StartTerminaRequest request;
  vm_tools::StartTerminaResponse response;

  request.set_tremplin_ipv4_address(GatewayAddress());
  request.mutable_lxd_ipv4_subnet()->swap(lxd_subnet);
  request.mutable_stateful_device()->swap(stateful_device);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kStartTerminaTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = stub_->StartTermina(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to start Termina: " << status.error_message();
    out_error->assign(status.error_message());
    return false;
  }

  return true;
}

void TerminaVm::RecordKernelVersionForEnterpriseReporting() {
  grpc::ClientContext ctx_get_kernel_version;
  ctx_get_kernel_version.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kStartTerminaTimeoutSeconds, GPR_TIMESPAN)));
  vm_tools::EmptyMessage empty;
  vm_tools::GetKernelVersionResponse grpc_response;
  grpc::Status get_kernel_version_status =
      stub_->GetKernelVersion(&ctx_get_kernel_version, empty, &grpc_response);
  if (!get_kernel_version_status.ok()) {
    LOG(WARNING) << "Failed to retrieve kernel version for VM " << vsock_cid_
                 << ": " << get_kernel_version_status.error_message();
  } else {
    kernel_version_ =
        grpc_response.kernel_release() + " " + grpc_response.kernel_version();
  }
}

bool TerminaVm::AttachUsbDevice(uint8_t bus,
                                uint8_t addr,
                                uint16_t vid,
                                uint16_t pid,
                                int fd,
                                UsbControlResponse* response) {
  return vm_tools::concierge::AttachUsbDevice(GetVmSocketPath(), bus, addr, vid,
                                              pid, fd, response);
}

bool TerminaVm::DetachUsbDevice(uint8_t port, UsbControlResponse* response) {
  return vm_tools::concierge::DetachUsbDevice(GetVmSocketPath(), port,
                                              response);
}

bool TerminaVm::ListUsbDevice(std::vector<UsbDevice>* device) {
  return vm_tools::concierge::ListUsbDevice(GetVmSocketPath(), device);
}

void TerminaVm::HandleSuspendImminent() {
  RunCrosvmCommand("suspend");
}

void TerminaVm::HandleSuspendDone() {
  RunCrosvmCommand("resume");
}

bool TerminaVm::Mount9P(uint32_t port, string target) {
  LOG(INFO) << "Mounting 9P file system from port " << port << " on " << target;

  vm_tools::Mount9PRequest request;
  vm_tools::MountResponse response;

  request.set_port(port);
  request.set_target(std::move(target));

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = stub_->Mount9P(&ctx, request, &response);
  if (!status.ok() || response.error() != 0) {
    LOG(ERROR) << "Failed to mount 9P server on " << request.target()
               << " inside VM " << vsock_cid_ << ": "
               << (status.ok() ? strerror(response.error())
                               : status.error_message());
    return false;
  }

  return true;
}

bool TerminaVm::SetResolvConfig(const std::vector<string>& nameservers,
                                const std::vector<string>& search_domains) {
  LOG(INFO) << "Setting resolv config for VM " << vsock_cid_;

  vm_tools::SetResolvConfigRequest request;
  vm_tools::EmptyMessage response;

  vm_tools::ResolvConfig* resolv_config = request.mutable_resolv_config();

  google::protobuf::RepeatedPtrField<string> request_nameservers(
      nameservers.begin(), nameservers.end());
  resolv_config->mutable_nameservers()->Swap(&request_nameservers);

  google::protobuf::RepeatedPtrField<string> request_search_domains(
      search_domains.begin(), search_domains.end());
  resolv_config->mutable_search_domains()->Swap(&request_search_domains);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = stub_->SetResolvConfig(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to set resolv config for VM " << vsock_cid_ << ": "
               << status.error_message();
    return false;
  }

  return true;
}

bool TerminaVm::SetTime(string* failure_reason) {
  DCHECK(failure_reason);

  base::Time now = base::Time::Now();
  struct timeval current = now.ToTimeVal();

  vm_tools::SetTimeRequest request;
  vm_tools::EmptyMessage response;

  google::protobuf::Timestamp* timestamp = request.mutable_time();
  timestamp->set_seconds(current.tv_sec);
  timestamp->set_nanos(current.tv_usec * 1000);

  grpc::ClientContext ctx;
  ctx.set_deadline(gpr_time_add(
      gpr_now(GPR_CLOCK_MONOTONIC),
      gpr_time_from_seconds(kDefaultTimeoutSeconds, GPR_TIMESPAN)));

  grpc::Status status = stub_->SetTime(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to set guest time on VM " << vsock_cid_ << ":"
               << status.error_message();

    *failure_reason = status.error_message();
    return false;
  }
  return true;
}

bool TerminaVm::GetVmEnterpriseReportingInfo(
    GetVmEnterpriseReportingInfoResponse* response) {
  LOG(INFO) << "Get enterprise reporting info";
  if (kernel_version_.empty()) {
    response->set_success(false);
    response->set_failure_reason(
        "Kernel version could not be recorded at startup.");
    return false;
  }

  response->set_success(true);
  response->set_vm_kernel_version(kernel_version_);
  return true;
}

// static
bool TerminaVm::SetVmCpuRestriction(CpuRestrictionState cpu_restriction_state) {
  // TODO(sonnyrao): Adjust |cpu_shares|.
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
  return UpdateCpuShares(base::FilePath(kTerminaCpuCgroup), cpu_shares);
}

void TerminaVm::SetContainerSubnet(
    std::unique_ptr<arc_networkd::Subnet> subnet) {
  container_subnet_ = std::move(subnet);
}

uint32_t TerminaVm::GatewayAddress() const {
  return subnet_->AddressAtOffset(kHostAddressOffset);
}

uint32_t TerminaVm::IPv4Address() const {
  return subnet_->AddressAtOffset(kGuestAddressOffset);
}

uint32_t TerminaVm::Netmask() const {
  return subnet_->Netmask();
}

uint32_t TerminaVm::ContainerNetmask() const {
  if (container_subnet_)
    return container_subnet_->Netmask();

  return INADDR_ANY;
}

size_t TerminaVm::ContainerPrefixLength() const {
  if (container_subnet_)
    return container_subnet_->PrefixLength();

  return 0;
}

uint32_t TerminaVm::ContainerSubnet() const {
  if (container_subnet_)
    return container_subnet_->AddressAtOffset(0);

  return INADDR_ANY;
}

VmInterface::Info TerminaVm::GetInfo() {
  VmInterface::Info info = {
      .ipv4_address = IPv4Address(),
      .pid = pid(),
      .cid = cid(),
      .seneschal_server_handle = seneschal_server_handle(),
      .status = IsTremplinStarted() ? VmInterface::Status::RUNNING
                                    : VmInterface::Status::STARTING,
  };

  return info;
}

void TerminaVm::set_kernel_version_for_testing(std::string kernel_version) {
  kernel_version_ = kernel_version;
}

void TerminaVm::set_stub_for_testing(
    std::unique_ptr<vm_tools::Maitred::Stub> stub) {
  stub_ = std::move(stub);
}

std::unique_ptr<TerminaVm> TerminaVm::CreateForTesting(
    arc_networkd::MacAddress mac_addr,
    std::unique_ptr<arc_networkd::Subnet> subnet,
    uint32_t vsock_cid,
    base::FilePath runtime_dir,
    std::string stateful_device,
    std::string kernel_version,
    std::unique_ptr<vm_tools::Maitred::Stub> stub) {
  VmFeatures features{
      .gpu = false,
      .software_tpm = false,
  };
  auto vm = base::WrapUnique(new TerminaVm(
      std::move(mac_addr), std::move(subnet), vsock_cid, nullptr,
      std::move(runtime_dir), std::move(stateful_device), features));
  vm->set_kernel_version_for_testing(kernel_version);
  vm->set_stub_for_testing(std::move(stub));

  return vm;
}

}  // namespace concierge
}  // namespace vm_tools
