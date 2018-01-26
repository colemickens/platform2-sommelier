// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/service.h"

#include <arpa/inet.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linux/vm_sockets.h>  // Needs to come after sys/socket.h

#include <map>
#include <thread>  // NOLINT(build/c++11)
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/memory/ptr_util.h>
#include <base/single_thread_task_runner.h>
#include <base/strings/stringprintf.h>
#include <base/synchronization/waitable_event.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <vm_concierge/proto_bindings/service.pb.h>

#include "vm_tools/common/constants.h"

using std::string;

namespace vm_tools {
namespace concierge {

namespace {

using Subnet = SubnetPool::Subnet;
using LaunchProcessResult = VirtualMachine::LaunchProcessResult;
using ProcessExitBehavior = VirtualMachine::ProcessExitBehavior;
using ProcessStatus = VirtualMachine::ProcessStatus;

// Path to the runtime directory used by VMs.
constexpr char kRuntimeDir[] = "/run/vm";

// Maximum number of extra disks to be mounted inside the VM.
constexpr int kMaxExtraDisks = 10;

// How long we should wait for a VM to start up.
constexpr base::TimeDelta kVmStartupTimeout = base::TimeDelta::FromSeconds(5);

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns NULL, an empty response is created
// and sent.
void HandleSynchronousDBusMethodCall(
    base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*)> handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);
  response_sender.Run(std::move(response));
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

// Posted to the grpc thread to run the StartupListener service.  Puts a copy
// of the pointer to the grpc server in |server_copy| and then signals |event|.
void RunStartupListenerService(StartupListenerImpl* listener,
                               base::WaitableEvent* event,
                               std::shared_ptr<grpc::Server>* server_copy) {
  // We are not interested in getting SIGCHLD on this thread.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, nullptr);

  // Build the grpc server.
  grpc::ServerBuilder builder;
  builder.AddListeningPort(base::StringPrintf("vsock:%u:%u", VMADDR_CID_ANY,
                                              vm_tools::kStartupListenerPort),
                           grpc::InsecureServerCredentials());
  builder.RegisterService(listener);

  std::shared_ptr<grpc::Server> server(builder.BuildAndStart().release());

  *server_copy = server;
  event->Signal();

  if (server) {
    server->Wait();
  }
}

}  // namespace

std::unique_ptr<Service> Service::Create() {
  auto service = base::WrapUnique(new Service());

  if (!service->Init()) {
    service.reset();
  }

  return service;
}

Service::Service() : watcher_(FROM_HERE) {}

Service::~Service() {
  if (grpc_server_) {
    grpc_server_->Shutdown();
  }
}

void Service::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(signal_fd_.get(), fd);

  struct signalfd_siginfo siginfo;
  if (read(signal_fd_.get(), &siginfo, sizeof(siginfo)) != sizeof(siginfo)) {
    PLOG(ERROR) << "Failed to read from signalfd";
    return;
  }
  DCHECK_EQ(siginfo.ssi_signo, SIGCHLD);

  // We can't just rely on the information in the siginfo structure because
  // more than one child may have exited but only one SIGCHLD will be
  // generated.
  while (true) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid <= 0) {
      if (pid == -1 && errno != ECHILD) {
        PLOG(ERROR) << "Unable to reap child processes";
      }
      break;
    }

    // See if this is a process we launched.
    string name;
    for (const auto& pair : vms_) {
      if (pid == pair.second->pid()) {
        name = pair.first;
        break;
      }
    }

    if (WIFEXITED(status)) {
      LOG(INFO) << " Process " << pid << " exited with status "
                << WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      LOG(INFO) << " Process " << pid << " killed by signal "
                << WTERMSIG(status)
                << (WCOREDUMP(status) ? " (core dumped)" : "");
    } else {
      LOG(WARNING) << "Unknown exit status " << status << " for process "
                   << pid;
    }

    // Remove this process from the our set of VMs.
    vms_.erase(std::move(name));
  }
}

void Service::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

bool Service::Init() {
  dbus::Bus::Options opts;
  opts.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(std::move(opts));

  if (!bus_->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return false;
  }

  exported_object_ =
      bus_->GetExportedObject(dbus::ObjectPath(kVmConciergeServicePath));
  if (!exported_object_) {
    LOG(ERROR) << "Failed to export " << kVmConciergeServicePath << " object";
    return false;
  }

  using ServiceMethod =
      std::unique_ptr<dbus::Response> (Service::*)(dbus::MethodCall*);
  const std::map<const char*, ServiceMethod> kServiceMethods = {
      {kStartVmMethod, &Service::StartVm},
      {kStopVmMethod, &Service::StopVm},
      {kStopAllVmsMethod, &Service::StopAllVms},
  };

  for (const auto& iter : kServiceMethods) {
    bool ret = exported_object_->ExportMethodAndBlock(
        kVmConciergeInterface, iter.first,
        base::Bind(&HandleSynchronousDBusMethodCall,
                   base::Bind(iter.second, base::Unretained(this))));
    if (!ret) {
      LOG(ERROR) << "Failed to export method " << iter.first;
      return false;
    }
  }

  if (!bus_->RequestOwnershipAndBlock(kVmConciergeServiceName,
                                      dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(ERROR) << "Failed to take ownership of " << kVmConciergeServiceName;
    return false;
  }

  // Start the grpc thread.
  if (!grpc_thread_.Start()) {
    LOG(ERROR) << "Failed to start grpc thread";
    return false;
  }

  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  bool ret = grpc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&RunStartupListenerService, &startup_listener_,
                            &event, &grpc_server_));
  if (!ret) {
    LOG(ERROR) << "Failed to post server startup task to grpc thread";
    return false;
  }

  // Wait for the grpc server to start.
  event.Wait();

  if (!grpc_server_) {
    LOG(ERROR) << "grpc server failed to start";
    return false;
  }

  // Change the umask so that the runtime directory for each VM will get the
  // right permissions.
  umask(002);

  // Set up the signalfd for receiving SIGCHLD events.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);

  signal_fd_.reset(signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC));
  if (!signal_fd_.is_valid()) {
    PLOG(ERROR) << "Failed to create signalfd";
    return false;
  }

  ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      signal_fd_.get(), true /*persistent*/, base::MessageLoopForIO::WATCH_READ,
      &watcher_, this);
  if (!ret) {
    LOG(ERROR) << "Failed to watch signalfd";
    return false;
  }

  // Now block SIGCHLD from the normal signal handling path so that we will get
  // it via the signalfd.
  if (sigprocmask(SIG_BLOCK, &mask, nullptr) < 0) {
    PLOG(ERROR) << "Failed to block SIGCHLD via sigprocmask";
    return false;
  }

  return true;
}

std::unique_ptr<dbus::Response> Service::StartVm(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Received StartVm request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  StartVmRequest request;
  StartVmResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse StartVmRequest from message";

    response.set_failure_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Make sure the VM has a name.
  if (request.name().empty()) {
    LOG(ERROR) << "Ignoring request with empty name";

    response.set_failure_reason("Missing VM name");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (vms_.find(request.name()) != vms_.end()) {
    LOG(ERROR) << "VM with requested name is already running";

    response.set_failure_reason("VM name is taken");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.disks_size() > kMaxExtraDisks) {
    LOG(ERROR) << "Rejecting request with " << request.disks_size()
               << " extra disks";

    response.set_failure_reason("Too many extra disks");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  base::FilePath kernel(request.vm().kernel());
  if (!base::PathExists(kernel)) {
    LOG(ERROR) << "Missing VM kernel path: " << kernel.value();

    response.set_failure_reason("Kernel path does not exist");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  base::FilePath rootfs(request.vm().rootfs());
  if (!base::PathExists(rootfs)) {
    LOG(ERROR) << "Missing VM rootfs path: " << rootfs.value();

    response.set_failure_reason("Rootfs path does not exist");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  std::vector<VirtualMachine::Disk> disks;
  for (const auto& disk : request.disks()) {
    if (!base::PathExists(base::FilePath(disk.path()))) {
      LOG(ERROR) << "Missing disk path: " << disk.path();
      response.set_failure_reason("One or more disk paths do not exist");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    disks.emplace_back(VirtualMachine::Disk{
        .path = base::FilePath(disk.path()), .writable = disk.writable(),
    });
  }

  // Create the runtime directory.
  base::FilePath runtime_dir;
  if (!base::CreateTemporaryDirInDir(base::FilePath(kRuntimeDir), "vm.",
                                     &runtime_dir)) {
    PLOG(ERROR) << "Unable to create runtime directory for VM";

    response.set_failure_reason(
        "Internal error: unable to create runtime directory");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Allocate resources for the VM.
  MacAddress mac_address = mac_address_generator_.Generate();
  std::unique_ptr<Subnet> subnet = subnet_pool_.Allocate();
  if (!subnet) {
    LOG(ERROR) << "No available subnets; unable to start VM";

    response.set_failure_reason("No available subnets");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }
  uint32_t vsock_cid = vsock_cid_pool_.Allocate();

  // Get the address in network byte order.
  uint32_t addr;
  if (!StringToIPv4Address(subnet->IPv4Address(), &addr)) {
    LOG(ERROR) << "Unable to parse subnet address " << subnet->IPv4Address();

    response.set_failure_reason(
        "Internal error: unable to parse subnet address");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Associate a WaitableEvent with this VM.  This needs to happen before
  // starting the VM to avoid a race where the VM reports that it's ready
  // before it gets added as a pending VM.
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  startup_listener_.AddPendingVm(vsock_cid, &event);

  // Start the VM and build the response.
  auto vm = VirtualMachine::Create(std::move(kernel), std::move(rootfs),
                                   std::move(disks), std::move(mac_address),
                                   std::move(subnet), vsock_cid,
                                   std::move(runtime_dir));
  if (!vm) {
    LOG(ERROR) << "Unable to start VM";

    startup_listener_.RemovePendingVm(vsock_cid);
    response.set_failure_reason("Unable to start VM");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Wait for the VM to finish starting up and for maitre'd to signal that it's
  // ready.
  if (!event.TimedWait(kVmStartupTimeout)) {
    LOG(ERROR) << "VM failed to start in " << kVmStartupTimeout.InSeconds()
               << " seconds";

    response.set_failure_reason("VM failed to start in time");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // maitre'd is ready.  Finish setting up the VM.
  if (!vm->ConfigureNetwork()) {
    LOG(ERROR) << "Failed to configure VM network";

    response.set_failure_reason("Failed to configure VM network");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Do all the mounts.  Assume that the rootfs filesystem was assigned
  // /dev/vda and that every subsequent image was assigned a letter in
  // alphabetical order starting from 'b'.
  unsigned char disk_letter = 'b';
  unsigned char offset = 0;
  for (const auto& disk : request.disks()) {
    string src = base::StringPrintf("/dev/vd%c", disk_letter + offset);
    ++offset;

    if (!disk.do_mount())
      continue;

    uint64_t flags = disk.flags();
    if (!disk.writable()) {
      flags |= MS_RDONLY;
    }
    if (!vm->Mount(std::move(src), disk.mount_point(), disk.fstype(), flags,
                   disk.data())) {
      LOG(ERROR) << "Failed to mount " << disk.path() << " -> "
                 << disk.mount_point();

      response.set_failure_reason("Failed to mount extra disk");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }
  }

  // If at least one extra disk was given, assume that one of them was a
  // container disk image mounted to /mnt/container_rootfs.  Try to start it
  // with run_oci.  TODO: Remove this once all the lxc/lxd stuff is ready.
  if (request.disks_size() > 0) {
    std::vector<string> run_oci = {
        "run_oci",
        "run",
        "--cgroup_parent=chronos_containers",
        "--container_path=/mnt/container_rootfs",
        "termina_container",
    };
    LaunchProcessResult result =
        vm->StartProcess(std::move(run_oci), ProcessExitBehavior::ONE_SHOT);
    switch (result.status) {
      case ProcessStatus::UNKNOWN:
        LOG(WARNING) << "run_oci may or may not be running in the VM";
        break;
      case ProcessStatus::EXITED:
        LOG(WARNING) << "run_oci exited with status " << result.code;
        break;
      case ProcessStatus::SIGNALED:
        LOG(WARNING) << "run_oci was killed by signal" << result.code;
        break;
      case ProcessStatus::FAILED:
        LOG(WARNING) << "Failed to launch run_oci";
        break;
      case ProcessStatus::LAUNCHED:
        break;
    }
  }

  LOG(INFO) << "Started VM with pid " << vm->pid();

  response.set_success(true);
  response.set_ipv4_address(addr);
  response.set_pid(vm->pid());
  response.set_cid(vsock_cid);
  writer.AppendProtoAsArrayOfBytes(response);

  vms_[request.name()] = std::move(vm);

  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::StopVm(dbus::MethodCall* method_call) {
  LOG(INFO) << "Received StopVm request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  StopVmRequest request;
  StopVmResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse StopVmRequest from message";

    response.set_failure_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto iter = vms_.find(request.name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "Requested VM does not exist";

    response.set_failure_reason("Requested VM does not exist");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (!iter->second->Shutdown()) {
    LOG(ERROR) << "Unable to shut down VM";

    response.set_failure_reason("Unable to shut down VM");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  vms_.erase(iter);
  response.set_success(true);
  writer.AppendProtoAsArrayOfBytes(response);

  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::StopAllVms(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Received StopAllVms request";

  std::vector<std::thread> threads;
  threads.reserve(vms_.size());

  // Spawn a thread for each VM to shut it down.
  for (auto& pair : vms_) {
    // By resetting the unique_ptr, each thread calls the destructor for that
    // VM, which will shut it down.
    threads.emplace_back([](std::unique_ptr<VirtualMachine> vm) { vm.reset(); },
                         std::move(pair.second));
  }

  // Wait for all VMs to shutdown.
  for (auto& thread : threads) {
    thread.join();
  }

  vms_.clear();

  return nullptr;
}

}  // namespace concierge
}  // namespace vm_tools
