// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/virtual_machine.h"

#include <arpa/inet.h>
#include <linux/capability.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <utility>

#include <base/bind.h>
#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/guid.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>
#include <base/sys_info.h>
#include <base/time/time.h>
#include <google/protobuf/repeated_field.h>
#include <grpc++/grpc++.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/concierge/tap_device_builder.h"

using std::string;

namespace vm_tools {
namespace concierge {
namespace {

using ProcessExitBehavior = VirtualMachine::ProcessExitBehavior;
using ProcessStatus = VirtualMachine::ProcessStatus;

// Path to the crosvm binary.
constexpr char kCrosvmBin[] = "/usr/bin/crosvm";

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

// Waits for the |pid| to exit.  Returns true if |pid| successfully exited and
// false if it did not exit in time.
bool WaitForChild(pid_t child, base::TimeDelta timeout) {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);

  const base::Time deadline = base::Time::Now() + timeout;
  while (true) {
    pid_t ret = waitpid(child, nullptr, WNOHANG);
    if (ret == child || (ret < 0 && errno == ECHILD)) {
      // Either the child exited or it doesn't exist anymore.
      return true;
    }

    // ret == 0 means that the child is still alive
    if (ret < 0) {
      PLOG(ERROR) << "Failed to wait for child process";
      return false;
    }

    base::Time now = base::Time::Now();
    if (deadline <= now) {
      // Timed out.
      return false;
    }

    const struct timespec ts = (deadline - now).ToTimeSpec();
    if (sigtimedwait(&set, nullptr, &ts) < 0 && errno == EAGAIN) {
      // Timed out.
      return false;
    }
  }
}

}  // namespace

VirtualMachine::VirtualMachine(
    MacAddress mac_addr,
    std::unique_ptr<Subnet> subnet,
    uint32_t vsock_cid,
    std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
    base::FilePath runtime_dir)
    : mac_addr_(std::move(mac_addr)),
      subnet_(std::move(subnet)),
      vsock_cid_(vsock_cid),
      seneschal_server_proxy_(std::move(seneschal_server_proxy)) {
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
    std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
    base::FilePath runtime_dir) {
  auto vm = base::WrapUnique(new VirtualMachine(
      std::move(mac_addr), std::move(subnet), vsock_cid,
      std::move(seneschal_server_proxy), std::move(runtime_dir)));

  if (!vm->Start(std::move(kernel), std::move(rootfs), std::move(disks))) {
    vm.reset();
  }

  return vm;
}

bool VirtualMachine::Start(base::FilePath kernel,
                           base::FilePath rootfs,
                           std::vector<VirtualMachine::Disk> disks) {
  // Set up the tap device.
  base::ScopedFD tap_fd =
      BuildTapDevice(mac_addr_, GatewayAddress(), Netmask());
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
      "--socket",       runtime_dir_.GetPath().Append(kCrosvmSocket).value(),
      "--wayland-sock", kWaylandSocket,
  };
  // clang-format on

  if (USE_CROSVM_WL_DMABUF)
    args.emplace_back("--wayland-dmabuf");

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
  // whole process group doesn't kill us as well.
  process_.SetPreExecCallback(base::Bind(&SetPgid));

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
  brillo::ProcessImpl crosvm;
  crosvm.AddArg(kCrosvmBin);
  crosvm.AddArg("stop");
  crosvm.AddArg(runtime_dir_.GetPath().Append(kCrosvmSocket).value());
  crosvm.Run();

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

bool VirtualMachine::LaunchProcess(std::vector<string> args,
                                   std::map<string, string> env,
                                   bool respawn,
                                   bool wait_for_exit,
                                   int64_t timeout_seconds) {
  CHECK(!args.empty());
  DCHECK(!(respawn && wait_for_exit));

  LOG(INFO) << "Launching " << args[0] << " inside VM " << vsock_cid_;

  vm_tools::LaunchProcessRequest request;
  vm_tools::LaunchProcessResponse response;

  google::protobuf::RepeatedPtrField<string> argv(args.begin(), args.end());
  request.mutable_argv()->Swap(&argv);

  google::protobuf::Map<string, string> environ(env.begin(), env.end());
  request.mutable_env()->swap(environ);

  request.set_respawn(respawn);
  request.set_wait_for_exit(wait_for_exit);

  grpc::ClientContext ctx;
  ctx.set_deadline(
      gpr_time_add(gpr_now(GPR_CLOCK_MONOTONIC),
                   gpr_time_from_seconds(timeout_seconds, GPR_TIMESPAN)));

  grpc::Status status = stub_->LaunchProcess(&ctx, request, &response);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to launch " << args[0] << ": "
               << status.error_message();
    return false;
  }

  // If waiting for the process to exit, a success means the process had to
  // return 0. Otherwise, just check that the process launched successfully.
  if (response.status() == vm_tools::EXITED && wait_for_exit) {
    return response.code() == 0;
  } else if (response.status() == vm_tools::LAUNCHED && !wait_for_exit) {
    return true;
  }

  return false;
}

bool VirtualMachine::StartProcess(std::vector<string> args,
                                  std::map<string, string> env,
                                  ProcessExitBehavior exit_behavior) {
  return LaunchProcess(std::move(args), std::move(env),
                       exit_behavior == ProcessExitBehavior::RESPAWN_ON_EXIT,
                       false, kDefaultTimeoutSeconds);
}

bool VirtualMachine::RunProcess(std::vector<string> args,
                                std::map<string, string> env) {
  return LaunchProcess(std::move(args), std::move(env), false, true,
                       kDefaultTimeoutSeconds);
}

bool VirtualMachine::RunProcessWithTimeout(std::vector<string> args,
                                           std::map<string, string> env,
                                           base::TimeDelta timeout) {
  return LaunchProcess(std::move(args), std::move(env), false, true,
                       timeout.InSeconds());
}

bool VirtualMachine::ConfigureNetwork(
    const std::vector<string>& nameservers,
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

bool VirtualMachine::StartTermina(std::string lxd_subnet,
                                  std::string* out_error) {
  vm_tools::StartTerminaRequest request;
  vm_tools::StartTerminaResponse response;

  request.set_tremplin_ipv4_address(GatewayAddress());
  request.mutable_lxd_ipv4_subnet()->swap(lxd_subnet);

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

bool VirtualMachine::Mount9P(uint32_t port, string target) {
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

bool VirtualMachine::SetResolvConfig(
    const std::vector<string>& nameservers,
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

grpc::Status VirtualMachine::SetTime() {
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
  }
  return status;
}

void VirtualMachine::SetContainerSubnet(std::unique_ptr<Subnet> subnet) {
  container_subnet_ = std::move(subnet);
}

uint32_t VirtualMachine::GatewayAddress() const {
  return subnet_->AddressAtOffset(kHostAddressOffset);
}

uint32_t VirtualMachine::IPv4Address() const {
  return subnet_->AddressAtOffset(kGuestAddressOffset);
}

uint32_t VirtualMachine::Netmask() const {
  return subnet_->Netmask();
}

uint32_t VirtualMachine::ContainerNetmask() const {
  if (container_subnet_)
    return container_subnet_->Netmask();

  return INADDR_ANY;
}

size_t VirtualMachine::ContainerPrefix() const {
  if (container_subnet_)
    return container_subnet_->Prefix();

  return 0;
}

uint32_t VirtualMachine::ContainerSubnet() const {
  if (container_subnet_)
    return container_subnet_->AddressAtOffset(0);

  return INADDR_ANY;
}

void VirtualMachine::set_stub_for_testing(
    std::unique_ptr<vm_tools::Maitred::Stub> stub) {
  stub_ = std::move(stub);
}

std::unique_ptr<VirtualMachine> VirtualMachine::CreateForTesting(
    MacAddress mac_addr,
    std::unique_ptr<Subnet> subnet,
    uint32_t vsock_cid,
    base::FilePath runtime_dir,
    std::unique_ptr<vm_tools::Maitred::Stub> stub) {
  auto vm = base::WrapUnique(
      new VirtualMachine(std::move(mac_addr), std::move(subnet), vsock_cid,
                         nullptr, std::move(runtime_dir)));

  vm->set_stub_for_testing(std::move(stub));

  return vm;
}

}  // namespace concierge
}  // namespace vm_tools
