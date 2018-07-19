// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/service.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <net/route.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/sendfile.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linux/vm_sockets.h>  // Needs to come after sys/socket.h

#include <map>
#include <utility>
#include <vector>

#include <base/base64url.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/memory/ptr_util.h>
#include <base/single_thread_task_runner.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/thread_task_runner_handle.h>
#include <base/time/time.h>
#include <base/version.h>
#include <chromeos/dbus/service_constants.h>
#include <crosvm/qcow_utils.h>
#include <dbus/object_proxy.h>
#include <vm_cicerone/proto_bindings/cicerone_service.pb.h>
#include <vm_concierge/proto_bindings/service.pb.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/concierge/ssh_keys.h"

using std::string;

namespace vm_tools {
namespace concierge {

namespace {

using Subnet = SubnetPool::Subnet;
using ProcessExitBehavior = VirtualMachine::ProcessExitBehavior;
using ProcessStatus = VirtualMachine::ProcessStatus;

// Path to the runtime directory used by VMs.
constexpr char kRuntimeDir[] = "/run/vm";

// Default path to VM kernel image and rootfs.
constexpr char kVmDefaultPath[] = "/run/imageloader/cros-termina";

// Name of the VM kernel image.
constexpr char kVmKernelName[] = "vm_kernel";

// Name of the VM rootfs image.
constexpr char kVmRootfsName[] = "vm_rootfs.img";

// Maximum number of extra disks to be mounted inside the VM.
constexpr int kMaxExtraDisks = 10;

// How long to wait before timing out on `lxd waitready`.
constexpr int kLxdWaitreadyTimeoutSeconds = 150;
constexpr base::TimeDelta kLxdWaitreadyTimeout =
    base::TimeDelta::FromSeconds(kLxdWaitreadyTimeoutSeconds);

// How long we should wait for a VM to start up.
// While this timeout might be high, it's meant to be a final failure point, not
// the lower bound of how long it takes.  On a loaded system (like extracting
// large compressed files), it could take 10 seconds to boot.
constexpr base::TimeDelta kVmStartupTimeout = base::TimeDelta::FromSeconds(30);

// crosvm directory name.
constexpr char kCrosvmDir[] = "crosvm";

// Cryptohome root base path.
constexpr char kCryptohomeRoot[] = "/home/root";

// Cryptohome user base path.
constexpr char kCryptohomeUser[] = "/home/user";

// Downloads directory for a user.
constexpr char kDownloadsDir[] = "Downloads";

// File extenstion for qcow2 disk types
constexpr char kQcowImageExtension[] = ".qcow2";

// Default name to use for a container.
constexpr char kDefaultContainerName[] = "penguin";

// Path to process file descriptors.
constexpr char kProcFileDescriptorsPath[] = "/proc/self/fd/";

// Only allow hex digits in the cryptohome id.
constexpr char kValidCryptoHomeCharacters[] = "abcdefABCDEF0123456789";

// Common environment for all LXD functionality.
const std::map<string, string> kLxdEnv = {
    {"LXD_DIR", "/mnt/stateful/lxd"},
    {"LXD_CONF", "/mnt/stateful/lxd_conf"},
    {"LXD_UNPRIVILEGED_ONLY", "true"},
};

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

// Posted to a grpc thread to startup a listener service. Puts a copy of
// the pointer to the grpc server in |server_copy| and then signals |event|.
// It will listen on the address specified in |listener_address|.
void RunListenerService(grpc::Service* listener,
                        const std::string& listener_address,
                        base::WaitableEvent* event,
                        std::shared_ptr<grpc::Server>* server_copy) {
  // We are not interested in getting SIGCHLD or SIGTERM on this thread.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigaddset(&mask, SIGTERM);
  sigprocmask(SIG_BLOCK, &mask, nullptr);

  // Build the grpc server.
  grpc::ServerBuilder builder;
  builder.AddListeningPort(listener_address, grpc::InsecureServerCredentials());
  builder.RegisterService(listener);

  std::shared_ptr<grpc::Server> server(builder.BuildAndStart().release());

  *server_copy = server;
  event->Signal();

  if (server) {
    server->Wait();
  }
}

// Sets up a gRPC listener service by starting the |grpc_thread| and posting the
// main task to run for the thread. |listener_address| should be the address the
// gRPC server is listening on. A copy of the pointer to the server is put in
// |server_copy|. Returns true if setup & started successfully, false otherwise.
bool SetupListenerService(base::Thread* grpc_thread,
                          grpc::Service* listener_impl,
                          const std::string& listener_address,
                          std::shared_ptr<grpc::Server>* server_copy) {
  // Start the grpc thread.
  if (!grpc_thread->Start()) {
    LOG(ERROR) << "Failed to start grpc thread";
    return false;
  }

  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  bool ret = grpc_thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&RunListenerService, listener_impl,
                            listener_address, &event, server_copy));
  if (!ret) {
    LOG(ERROR) << "Failed to post server startup task to grpc thread";
    return false;
  }

  // Wait for the VM grpc server to start.
  event.Wait();

  if (!server_copy) {
    LOG(ERROR) << "grpc server failed to start";
    return false;
  }

  return true;
}

// Converts an IPv4 address to a string. The result will be stored in |str|
// on success.
bool IPv4AddressToString(const uint32_t address, std::string* str) {
  CHECK(str);

  char result[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &address, result, sizeof(result)) != result) {
    return false;
  }
  *str = std::string(result);
  return true;
}

// Get the path to the latest available cros-termina component.
base::FilePath GetLatestVMPath() {
  base::FilePath component_dir(kVmDefaultPath);
  base::FileEnumerator dir_enum(component_dir, false,
                                base::FileEnumerator::DIRECTORIES);

  base::Version latest_version("0");
  base::FilePath latest_path;

  for (base::FilePath path = dir_enum.Next(); !path.empty();
       path = dir_enum.Next()) {
    base::Version version(path.BaseName().value());
    if (!version.IsValid())
      continue;

    if (version > latest_version) {
      latest_version = version;
      latest_path = path;
    }
  }

  return latest_path;
}

// Gets the path to a VM disk given the name, user id, and location.
bool GetDiskPathFromName(const std::string& disk_path,
                         const std::string& cryptohome_id,
                         StorageLocation storage_location,
                         bool create_parent_dir,
                         base::FilePath* path_out) {
  if (!base::ContainsOnlyChars(cryptohome_id, kValidCryptoHomeCharacters)) {
    LOG(ERROR) << "Invalid cryptohome_id specified";
    return false;
  }

  // Base64 encode the given disk name to ensure it only has valid characters.
  std::string disk_name;
  base::Base64UrlEncode(disk_path, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &disk_name);

  if (storage_location == STORAGE_CRYPTOHOME_ROOT) {
    base::FilePath crosvm_dir = base::FilePath(kCryptohomeRoot)
                                    .Append(cryptohome_id)
                                    .Append(kCrosvmDir);
    base::File::Error dir_error;
    if (!base::DirectoryExists(crosvm_dir)) {
      if (!create_parent_dir) {
        return false;
      }

      if (!base::CreateDirectoryAndGetError(crosvm_dir, &dir_error)) {
        LOG(ERROR) << "Failed to create crosvm directory in /home/root: "
                   << base::File::ErrorToString(dir_error);
        return false;
      }
    }
    *path_out = crosvm_dir.Append(disk_name + kQcowImageExtension);
  } else if (storage_location == STORAGE_CRYPTOHOME_DOWNLOADS) {
    *path_out = base::FilePath(kCryptohomeUser)
                    .Append(cryptohome_id)
                    .Append(kDownloadsDir)
                    .Append(disk_name + kQcowImageExtension);
  } else {
    LOG(ERROR) << "Unknown storage location type";
    return false;
  }

  return true;
}

}  // namespace

std::unique_ptr<Service> Service::Create(base::Closure quit_closure) {
  auto service = base::WrapUnique(new Service(std::move(quit_closure)));

  if (!service->Init()) {
    service.reset();
  }

  return service;
}

Service::Service(base::Closure quit_closure)
    : watcher_(FROM_HERE),
      quit_closure_(std::move(quit_closure)),
      weak_ptr_factory_(this) {
  startup_listener_ =
      std::make_unique<StartupListenerImpl>(weak_ptr_factory_.GetWeakPtr());
}

Service::~Service() {
  if (grpc_server_vm_) {
    grpc_server_vm_->Shutdown();
  }
}

void Service::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(signal_fd_.get(), fd);

  struct signalfd_siginfo siginfo;
  if (read(signal_fd_.get(), &siginfo, sizeof(siginfo)) != sizeof(siginfo)) {
    PLOG(ERROR) << "Failed to read from signalfd";
    return;
  }

  if (siginfo.ssi_signo == SIGCHLD) {
    HandleChildExit();
  } else if (siginfo.ssi_signo == SIGTERM) {
    HandleSigterm();
  } else {
    LOG(ERROR) << "Received unknown signal from signal fd: "
               << strsignal(siginfo.ssi_signo);
  }
}

void Service::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void Service::ContainerStartupFailed(const std::string& container_name,
                                     const uint32_t cid) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  VmMap::key_type vm_key;
  for (auto& vm : vms_) {
    if (vm.second->cid() == cid) {
      vm_key = vm.first;
      break;
    }
  }
  if (vm_key.second.empty()) {
    LOG(ERROR) << "Received indication container startup failed but could not "
               << "match to VM cid: " << cid;
    return;
  }

  LOG(ERROR) << "Startup of container " << container_name << " for owner "
             << vm_key.first << " VM " << vm_key.second << " failed.";

  // Send the D-Bus signal out to indicate the container startup failed.
  dbus::Signal signal(kVmConciergeInterface, kContainerStartupFailedSignal);
  ContainerStartedSignal proto;
  proto.set_owner_id(vm_key.first);
  proto.set_vm_name(vm_key.second);
  proto.set_container_name(container_name);
  dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(proto);
  exported_object_->SendSignal(&signal);
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
      {kGetVmInfoMethod, &Service::GetVmInfo},
      {kCreateDiskImageMethod, &Service::CreateDiskImage},
      {kDestroyDiskImageMethod, &Service::DestroyDiskImage},
      {kExportDiskImageMethod, &Service::ExportDiskImage},
      {kListVmDisksMethod, &Service::ListVmDisks},
      {kStartContainerMethod, &Service::StartContainer},
      {kGetContainerSshKeysMethod, &Service::GetContainerSshKeys},
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

  // Get the D-Bus proxy for communicating with cicerone.
  cicerone_service_proxy_ = bus_->GetObjectProxy(
      vm_tools::cicerone::kVmCiceroneServiceName,
      dbus::ObjectPath(vm_tools::cicerone::kVmCiceroneServicePath));
  if (!cicerone_service_proxy_) {
    LOG(ERROR) << "Unable to get dbus proxy for "
               << vm_tools::cicerone::kVmCiceroneServiceName;
    return false;
  }

  // Setup & start the gRPC listener service.
  if (!SetupListenerService(&grpc_thread_vm_, startup_listener_.get(),
                            base::StringPrintf("vsock:%u:%u", VMADDR_CID_ANY,
                                               vm_tools::kStartupListenerPort),
                            &grpc_server_vm_)) {
    LOG(ERROR) << "Failed to setup/startup the VM grpc server";
    return false;
  }

  // Change the umask so that the runtime directory for each VM will get the
  // right permissions.
  umask(002);

  // Set up the signalfd for receiving SIGCHLD and SIGTERM.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigaddset(&mask, SIGTERM);

  signal_fd_.reset(signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC));
  if (!signal_fd_.is_valid()) {
    PLOG(ERROR) << "Failed to create signalfd";
    return false;
  }

  bool ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      signal_fd_.get(), true /*persistent*/, base::MessageLoopForIO::WATCH_READ,
      &watcher_, this);
  if (!ret) {
    LOG(ERROR) << "Failed to watch signalfd";
    return false;
  }

  // Now block signals from the normal signal handling path so that we will get
  // them via the signalfd.
  if (sigprocmask(SIG_BLOCK, &mask, nullptr) < 0) {
    PLOG(ERROR) << "Failed to block signals via sigprocmask";
    return false;
  }

  return true;
}

void Service::HandleChildExit() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
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
    VmMap::key_type key;
    for (const auto& pair : vms_) {
      if (pid == pair.second->pid()) {
        key = pair.first;
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
    vms_.erase(std::move(key));
  }
}

void Service::HandleSigterm() {
  LOG(INFO) << "Shutting down due to SIGTERM";

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
}

std::unique_ptr<dbus::Response> Service::StartVm(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
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

  auto iter = FindVm(request.owner_id(), request.name());
  if (iter != vms_.end()) {
    LOG(INFO) << "VM with requested name is already running";
    auto& vm = iter->second;
    VmInfo* vm_info = response.mutable_vm_info();
    vm_info->set_ipv4_address(vm->IPv4Address());
    vm_info->set_pid(vm->pid());
    vm_info->set_cid(vm->cid());

    response.set_success(true);
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

  base::FilePath kernel, rootfs;

  if (request.start_termina()) {
    base::FilePath component_path = GetLatestVMPath();
    if (component_path.empty()) {
      LOG(ERROR) << "Termina component is not loaded";

      response.set_failure_reason("Termina component is not loaded");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    kernel = component_path.Append(kVmKernelName);
    rootfs = component_path.Append(kVmRootfsName);
  } else {
    kernel = base::FilePath(request.vm().kernel());
    rootfs = base::FilePath(request.vm().rootfs());
  }

  if (!base::PathExists(kernel)) {
    LOG(ERROR) << "Missing VM kernel path: " << kernel.value();

    response.set_failure_reason("Kernel path does not exist");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (!base::PathExists(rootfs)) {
    LOG(ERROR) << "Missing VM rootfs path: " << rootfs.value();

    response.set_failure_reason("Rootfs path does not exist");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  std::vector<VirtualMachine::Disk> disks;
  base::ScopedFD storage_fd;
  // Check if an opened storage image was passed over D-BUS.
  if (request.use_fd_for_storage()) {
    if (!reader.PopFileDescriptor(&storage_fd)) {
      LOG(ERROR) << "use_fd_for_storage is set but no fd found";

      response.set_failure_reason("use_fd_for_storage is set but no fd found");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }
    // Clear close-on-exec as this FD needs to be passed to crosvm.
    int raw_fd = storage_fd.get();
    int flags = fcntl(raw_fd, F_GETFD);
    if (flags == -1) {
      LOG(ERROR) << "Failed to get flags for passed fd";

      response.set_failure_reason("Failed to get flags for passed fd");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }
    flags &= ~FD_CLOEXEC;
    if (fcntl(raw_fd, F_SETFD, flags) == -1) {
      LOG(ERROR) << "Failed to clear close-on-exec flag for fd";

      response.set_failure_reason("Failed to clear close-on-exec flag for fd");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    base::FilePath fd_path = base::FilePath(kProcFileDescriptorsPath)
                                 .Append(base::IntToString(raw_fd));
    disks.emplace_back(VirtualMachine::Disk{
        .path = std::move(fd_path),
        .writable = true,
        .image_type = VirtualMachine::DiskImageType::QCOW2,
    });
  }

  for (const auto& disk : request.disks()) {
    if (!base::PathExists(base::FilePath(disk.path()))) {
      LOG(ERROR) << "Missing disk path: " << disk.path();
      response.set_failure_reason("One or more disk paths do not exist");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    VirtualMachine::DiskImageType image_type;
    if (disk.image_type() == vm_tools::concierge::DISK_IMAGE_RAW) {
      image_type = VirtualMachine::DiskImageType::RAW;
    } else if (disk.image_type() == vm_tools::concierge::DISK_IMAGE_QCOW2) {
      image_type = VirtualMachine::DiskImageType::QCOW2;
    } else {
      LOG(ERROR) << "Invalid disk type";
      response.set_failure_reason("Invalid disk type specified");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    disks.emplace_back(VirtualMachine::Disk{
        .path = base::FilePath(disk.path()),
        .writable = disk.writable(),
        .image_type = image_type,
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
  std::unique_ptr<Subnet> subnet = subnet_pool_.AllocateVM();
  if (!subnet) {
    LOG(ERROR) << "No available subnets; unable to start VM";

    response.set_failure_reason("No available subnets");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }
  uint32_t vsock_cid = vsock_cid_pool_.Allocate();
  if (vsock_cid == 0) {
    LOG(ERROR) << "Unable to allocate vsock context id";

    response.set_failure_reason("Unable to allocate vsock cid");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Associate a WaitableEvent with this VM.  This needs to happen before
  // starting the VM to avoid a race where the VM reports that it's ready
  // before it gets added as a pending VM.
  base::WaitableEvent event(false /*manual_reset*/,
                            false /*initially_signaled*/);
  startup_listener_->AddPendingVm(vsock_cid, &event);

  // Start the VM and build the response.
  auto vm = VirtualMachine::Create(std::move(kernel), std::move(rootfs),
                                   std::move(disks), std::move(mac_address),
                                   std::move(subnet), vsock_cid,
                                   std::move(runtime_dir));
  if (!vm) {
    LOG(ERROR) << "Unable to start VM";

    startup_listener_->RemovePendingVm(vsock_cid);
    response.set_failure_reason("Unable to start VM");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Wait for the VM to finish starting up and for maitre'd to signal that it's
  // ready.
  if (!event.TimedWait(kVmStartupTimeout)) {
    LOG(ERROR) << "VM failed to start in " << kVmStartupTimeout.InSeconds()
               << " seconds";

    startup_listener_->RemovePendingVm(vsock_cid);
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

  string failure_reason;
  if (request.start_termina() && !StartTermina(vm.get(), &failure_reason)) {
    response.set_failure_reason(std::move(failure_reason));
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  LOG(INFO) << "Started VM with pid " << vm->pid();

  // Notify cicerone that we have started a VM.
  NotifyCiceroneOfVmStarted(request.owner_id(), request.name(),
                            vm->ContainerSubnet(), vm->ContainerNetmask(),
                            vm->IPv4Address());

  VmInfo* vm_info = response.mutable_vm_info();
  response.set_success(true);
  vm_info->set_ipv4_address(vm->IPv4Address());
  vm_info->set_pid(vm->pid());
  vm_info->set_cid(vsock_cid);
  writer.AppendProtoAsArrayOfBytes(response);

  vms_[std::make_pair(request.owner_id(), request.name())] = std::move(vm);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::StopVm(dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
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

  auto iter = FindVm(request.owner_id(), request.name());
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

  // Notify cicerone that we have stopped a VM.
  NotifyCiceroneOfVmStopped(request.owner_id(), request.name());

  vms_.erase(iter);
  response.set_success(true);
  writer.AppendProtoAsArrayOfBytes(response);

  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::StopAllVms(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received StopAllVms request";

  // Spawn a thread for each VM to shut it down.
  for (auto& iter : vms_) {
    // Notify cicerone that we have stopped a VM.
    NotifyCiceroneOfVmStopped(iter.first.first, iter.first.second);

    // Resetting the unique_ptr will call the destructor for that VM, which
    // will shut it down.
    iter.second.reset();
  }

  vms_.clear();

  return nullptr;
}

std::unique_ptr<dbus::Response> Service::GetVmInfo(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received GetVmInfo request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  GetVmInfoRequest request;
  GetVmInfoResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse GetVmInfoRequest from message";

    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto iter = FindVm(request.owner_id(), request.name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "Requested VM does not exist";

    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto& vm = iter->second;
  VmInfo* vm_info = response.mutable_vm_info();
  vm_info->set_ipv4_address(vm->IPv4Address());
  vm_info->set_pid(vm->pid());
  vm_info->set_cid(vm->cid());

  response.set_success(true);
  writer.AppendProtoAsArrayOfBytes(response);

  return dbus_response;
}

bool Service::StartTermina(VirtualMachine* vm, string* failure_reason) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Starting lxd";

  // Set up the stateful disk. This will format the disk if necessary, then
  // mount it.
  if (!vm->RunProcess({"stateful_setup.sh"}, kLxdEnv)) {
    LOG(ERROR) << "Stateful setup failed";
    *failure_reason = "stateful setup failed";
    return false;
  }

  // Launch the main lxd process.
  if (!vm->StartProcess({"lxd", "--group", "lxd"}, kLxdEnv,
                        ProcessExitBehavior::RESPAWN_ON_EXIT)) {
    LOG(ERROR) << "lxd failed to start";
    *failure_reason = "lxd failed to start";
    return false;
  }

  // Wait for lxd to be ready. The first start may take a few seconds, so use
  // a longer timeout than the default.
  string timeout = std::to_string(kLxdWaitreadyTimeoutSeconds);
  if (!vm->RunProcessWithTimeout({"lxd", "waitready", "--timeout", timeout},
                                 kLxdEnv, kLxdWaitreadyTimeout)) {
    LOG(ERROR) << "lxd waitready failed";
    *failure_reason = "lxd waitready failed";
    return false;
  }

  // Perform any setup for lxd to be usable. On first run, this sets up the
  // lxd configuration (network bridge, storage pool, etc).
  if (!vm->RunProcess({"lxd_setup.sh"}, kLxdEnv)) {
    LOG(ERROR) << "lxd setup failed";
    *failure_reason = "lxd setup failed";
    return false;
  }

  // Allocate the subnet for lxd's bridge to use.
  std::unique_ptr<SubnetPool::Subnet> container_subnet =
      subnet_pool_.AllocateContainer();
  if (!container_subnet) {
    LOG(ERROR) << "Could not allocate container subnet";
    *failure_reason = "could not allocate container subnet";
    return false;
  }
  vm->SetContainerSubnet(std::move(container_subnet));

  // Set up a route for the container using the VM as a gateway.
  uint32_t container_gateway_addr = vm->IPv4Address();
  uint32_t container_netmask = vm->ContainerNetmask();
  uint32_t container_subnet_addr = vm->ContainerSubnet();

  struct rtentry route;
  memset(&route, 0, sizeof(route));

  struct sockaddr_in* gateway =
      reinterpret_cast<struct sockaddr_in*>(&route.rt_gateway);
  gateway->sin_family = AF_INET;
  gateway->sin_addr.s_addr = static_cast<in_addr_t>(container_gateway_addr);

  struct sockaddr_in* dst =
      reinterpret_cast<struct sockaddr_in*>(&route.rt_dst);
  dst->sin_family = AF_INET;
  dst->sin_addr.s_addr = (container_subnet_addr & container_netmask);

  struct sockaddr_in* genmask =
      reinterpret_cast<struct sockaddr_in*>(&route.rt_genmask);
  genmask->sin_family = AF_INET;
  genmask->sin_addr.s_addr = container_netmask;

  route.rt_flags = RTF_UP | RTF_GATEWAY;

  base::ScopedFD fd(socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to create socket";
    *failure_reason = "failed to create socket";
    return false;
  }

  if (HANDLE_EINTR(ioctl(fd.get(), SIOCADDRT, &route)) != 0) {
    PLOG(ERROR) << "Failed to set route for container";
    *failure_reason = "failed to set route for container";
    return false;
  }

  std::string dst_addr;
  IPv4AddressToString(container_subnet_addr, &dst_addr);
  size_t prefix = vm->ContainerPrefix();

  // The route has been installed on the host, so inform lxd of its subnet.
  std::string container_subnet_cidr =
      base::StringPrintf("%s/%zu", dst_addr.c_str(), prefix);
  if (!vm->RunProcess({"lxc", "network", "set", "lxdbr0", "ipv4.address",
                       std::move(container_subnet_cidr)},
                      kLxdEnv)) {
    LOG(ERROR) << "lxc network config failed";
    *failure_reason = "lxc network config failed";
    return false;
  }

  return true;
}

std::unique_ptr<dbus::Response> Service::CreateDiskImage(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received CreateDiskImage request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  CreateDiskImageRequest request;
  CreateDiskImageResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse CreateDiskImageRequest from message";
    response.set_status(DISK_STATUS_FAILED);
    response.set_failure_reason("Unable to parse CreateImageDiskRequest");

    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  base::FilePath disk_path;
  if (!GetDiskPathFromName(request.disk_path(), request.cryptohome_id(),
                           request.storage_location(),
                           true, /* create_parent_dir */
                           &disk_path)) {
    response.set_status(DISK_STATUS_FAILED);
    response.set_failure_reason("Failed to create vm image");
    writer.AppendProtoAsArrayOfBytes(response);

    return dbus_response;
  }

  if (base::PathExists(disk_path)) {
    response.set_status(DISK_STATUS_EXISTS);
    response.set_disk_path(disk_path.value());
    writer.AppendProtoAsArrayOfBytes(response);

    return dbus_response;
  }

  if (request.image_type() == DISK_IMAGE_RAW) {
    LOG(INFO) << "Creating raw disk at: " << disk_path.value() << " size "
              << request.disk_size();
    base::ScopedFD fd(
        open(disk_path.value().c_str(), O_CREAT | O_NONBLOCK | O_WRONLY, 0600));
    if (!fd.is_valid()) {
      PLOG(ERROR) << "Failed to create raw disk";
      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason("Failed to create raw disk file");
      writer.AppendProtoAsArrayOfBytes(response);

      return dbus_response;
    }

    int ret = ftruncate(fd.get(), request.disk_size());
    if (ret != 0) {
      PLOG(ERROR) << "Failed to truncate raw disk";
      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason("Failed to truncate raw disk file");
      writer.AppendProtoAsArrayOfBytes(response);

      return dbus_response;
    }
    response.set_status(DISK_STATUS_CREATED);
    response.set_disk_path(disk_path.value());
    writer.AppendProtoAsArrayOfBytes(response);

    return dbus_response;
  }

  LOG(INFO) << "Creating qcow2 disk at: " << disk_path.value() << " size "
            << request.disk_size();
  int ret =
      create_qcow_with_size(disk_path.value().c_str(), request.disk_size());
  if (ret != 0) {
    LOG(ERROR) << "Failed to create qcow2 disk image: " << strerror(ret);
    response.set_status(DISK_STATUS_FAILED);
    response.set_failure_reason("Failed to create qcow2 disk image");
    writer.AppendProtoAsArrayOfBytes(response);

    return dbus_response;
  }

  response.set_disk_path(disk_path.value());
  response.set_status(DISK_STATUS_CREATED);
  writer.AppendProtoAsArrayOfBytes(response);

  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::DestroyDiskImage(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received DestroyDiskImage request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  DestroyDiskImageRequest request;
  DestroyDiskImageResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse DestroyDiskImageRequest from message";
    response.set_status(DISK_STATUS_FAILED);
    response.set_failure_reason("Unable to parse DestroyDiskRequest");

    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  base::FilePath disk_path;
  if (!GetDiskPathFromName(request.disk_path(), request.cryptohome_id(),
                           request.storage_location(),
                           false, /* create_parent_dir */
                           &disk_path)) {
    response.set_status(DISK_STATUS_FAILED);
    response.set_failure_reason("Failed to delete vm image");
    writer.AppendProtoAsArrayOfBytes(response);

    return dbus_response;
  }

  if (!EraseGuestSshKeys(request.cryptohome_id(), request.disk_path())) {
    // Don't return a failure here, just log an error because this is only a
    // side effect and not what the real request is about.
    LOG(ERROR) << "Failed removing guest SSH keys for VM "
               << request.disk_path();
  }

  if (!base::PathExists(disk_path)) {
    response.set_status(DISK_STATUS_DOES_NOT_EXIST);
    writer.AppendProtoAsArrayOfBytes(response);

    return dbus_response;
  }

  if (!base::DeleteFile(disk_path, false)) {
    response.set_status(DISK_STATUS_FAILED);
    response.set_failure_reason("Disk removal failed");
    writer.AppendProtoAsArrayOfBytes(response);

    return dbus_response;
  }

  response.set_status(DISK_STATUS_DESTROYED);
  writer.AppendProtoAsArrayOfBytes(response);

  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::ExportDiskImage(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received ExportDiskImage request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  ExportDiskImageResponse response;
  response.set_status(DISK_STATUS_FAILED);

  ExportDiskImageRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse ExportDiskImageRequest from message";
    response.set_failure_reason("Unable to parse ExportDiskRequest");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  base::FilePath disk_path;
  if (!GetDiskPathFromName(request.disk_path(), request.cryptohome_id(),
                           STORAGE_CRYPTOHOME_ROOT,
                           false, /* create_parent_dir */
                           &disk_path)) {
    response.set_failure_reason("Failed to delete vm image");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (!base::PathExists(disk_path)) {
    response.set_status(DISK_STATUS_DOES_NOT_EXIST);
    response.set_failure_reason("Export image doesn't exist");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  int64_t disk_size;
  if (!base::GetFileSize(disk_path, &disk_size)) {
    LOG(ERROR) << "Disk path size unknown";
    response.set_failure_reason("Failed to get size of disk");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  base::ScopedFD disk_fd(HANDLE_EINTR(open(disk_path.value().c_str(),
                                           O_RDWR | O_NOFOLLOW | O_CLOEXEC)));
  if (!disk_fd.is_valid()) {
    LOG(ERROR) << "Failed opening VM disk for export";
    response.set_failure_reason("Failed opening VM disk for export");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Get the FD to fill with disk image data.
  base::ScopedFD storage_fd;
  if (!reader.PopFileDescriptor(&storage_fd)) {
    LOG(ERROR) << "export: no fd found";
    response.set_failure_reason("export: no fd found");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  ssize_t bytes_sent;
  size_t total_bytes_sent = 0;
  while (total_bytes_sent < disk_size) {
    bytes_sent = HANDLE_EINTR(sendfile(storage_fd.get(), disk_fd.get(), nullptr,
                                       disk_size - total_bytes_sent));
    if (bytes_sent < 0) {
      response.set_failure_reason("sendfile failed");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }
    total_bytes_sent += bytes_sent;
  }

  response.set_status(DISK_STATUS_CREATED);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::ListVmDisks(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  ListVmDisksRequest request;
  ListVmDisksResponse response;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse ListVmDisksRequest from message";
    response.set_success(false);
    response.set_failure_reason("Unable to parse ListVmDisksRequest");

    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  response.set_success(true);

  base::FilePath image_dir;
  if (request.storage_location() == STORAGE_CRYPTOHOME_ROOT) {
    image_dir = base::FilePath(kCryptohomeRoot)
                    .Append(request.cryptohome_id())
                    .Append(kCrosvmDir);
  } else if (request.storage_location() == STORAGE_CRYPTOHOME_DOWNLOADS) {
    image_dir = base::FilePath(kCryptohomeUser)
                    .Append(request.cryptohome_id())
                    .Append(kDownloadsDir);
  }
  if (!base::DirectoryExists(image_dir)) {
    // No directory means no VMs, return the empty response.
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  uint64_t total_size = 0;
  // Returns *.qcow2 in the given storage area.
  base::FileEnumerator dir_enum(image_dir, false, base::FileEnumerator::FILES,
                                "*.qcow2");

  for (base::FilePath path = dir_enum.Next(); !path.empty();
       path = dir_enum.Next()) {
    base::FilePath bare_name = path.BaseName().RemoveExtension();
    if (bare_name.empty()) {
      continue;
    }
    std::string image_name;
    if (!base::Base64UrlDecode(bare_name.value(),
                               base::Base64UrlDecodePolicy::IGNORE_PADDING,
                               &image_name)) {
      continue;
    }
    std::string* name = response.add_images();
    *name = std::move(image_name);
    int64_t file_size;
    if (base::GetFileSize(path, &file_size))
      total_size += file_size;
  }
  response.set_total_size(total_size);

  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::StartContainer(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received StartContainer request";
  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  StartContainerRequest request;
  StartContainerResponse response;
  response.set_status(CONTAINER_STATUS_UNKNOWN);
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse StartContainerRequest from message";
    response.set_status(CONTAINER_STATUS_FAILURE);
    response.set_failure_reason("Unable to parse StartContainerRequest");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto iter = FindVm(request.cryptohome_id(), request.vm_name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "Requested VM does not exist:" << request.vm_name();
    response.set_status(CONTAINER_STATUS_FAILURE);
    response.set_failure_reason("Requested VM does not exist");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  std::string container_name = request.container_name().empty()
                                   ? kDefaultContainerName
                                   : request.container_name();
  LOG(INFO) << "Checking if container " << container_name << " is running";
  if (IsContainerRunning(request.cryptohome_id(), request.vm_name(),
                         container_name)) {
    LOG(INFO) << "Container " << container_name << " is already running.";
    response.set_status(CONTAINER_STATUS_RUNNING);
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  std::string guest_private_key;
  std::string host_public_key;
  if (!request.cryptohome_id().empty()) {
    // Get the SSH keys needed by the container.
    host_public_key = GetHostSshPublicKey(request.cryptohome_id());
    if (host_public_key.empty()) {
      LOG(ERROR) << "Failed getting the host ssh public key";
      response.set_status(CONTAINER_STATUS_FAILURE);
      response.set_failure_reason(
          "Failed generating/loading host ssh public key");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }
    guest_private_key = GetGuestSshPrivateKey(
        request.cryptohome_id(), request.vm_name(), container_name);
    if (guest_private_key.empty()) {
      LOG(ERROR) << "Failed getting the guest ssh private key";
      response.set_status(CONTAINER_STATUS_FAILURE);
      response.set_failure_reason(
          "Failed generating/loading guest ssh private key");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }
  } else {
    LOG(WARNING) << "No cyptohome_id set in the StartContainer call, SSH will "
                 << "not be started for SFTP mounting";
  }

  // This executes the run_container.sh script in the VM which will startup
  // a container. We need to construct the command for that with the proper
  // parameters.
  std::string container_token = GetContainerToken(
      request.cryptohome_id(), request.vm_name(), container_name);
  if (container_token.empty()) {
    // If we don't have a valid container token, then we will never be able to
    // notify the caller of successful container startup so don't even try.
    LOG(ERROR) << "Failed to get a container token from cicerone";
    response.set_status(CONTAINER_STATUS_FAILURE);
    response.set_failure_reason(
        "Failed getting a container token from cicerone");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }
  std::vector<std::string> container_args = {
      "/sbin/minijail0",
      "-u",
      "chronos",
      "-G",
      "/usr/bin/run_container.sh",
      "--container_name",
      container_name,
      "--container_token",
      container_token,
  };
  if (!request.container_username().empty()) {
    container_args.emplace_back("--user");
    container_args.emplace_back(request.container_username());
  }
  if (!guest_private_key.empty() && !host_public_key.empty()) {
    container_args.emplace_back("--guest_private_key");
    container_args.emplace_back(std::move(guest_private_key));
    container_args.emplace_back("--host_public_key");
    container_args.emplace_back(std::move(host_public_key));
  }

  // Now execute the startup script in the VM.
  response.set_status(CONTAINER_STATUS_STARTING);
  if (!iter->second->StartProcess(std::move(container_args), kLxdEnv,
                                  ProcessExitBehavior::ONE_SHOT)) {
    response.set_status(CONTAINER_STATUS_FAILURE);
    response.set_failure_reason("Failed asynchronous container startup");
  }

  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::GetContainerSshKeys(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  LOG(INFO) << "Received GetContainerSshKeys request";
  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  ContainerSshKeysRequest request;
  ContainerSshKeysResponse response;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse ContainerSshKeysRequest from message";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.cryptohome_id().empty()) {
    LOG(ERROR) << "Cryptohome ID is not set in ContainerSshKeysRequest";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto iter = FindVm(request.cryptohome_id(), request.vm_name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "Requested VM does not exist:" << request.vm_name();
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  std::string container_name = request.container_name().empty()
                                   ? kDefaultContainerName
                                   : request.container_name();
  response.set_container_public_key(GetGuestSshPublicKey(
      request.cryptohome_id(), request.vm_name(), container_name));
  response.set_container_private_key(GetGuestSshPrivateKey(
      request.cryptohome_id(), request.vm_name(), container_name));
  response.set_host_public_key(GetHostSshPublicKey(request.cryptohome_id()));
  response.set_host_private_key(GetHostSshPrivateKey(request.cryptohome_id()));
  response.set_hostname(base::StringPrintf(
      "%s.%s.linux.test", container_name.c_str(), request.vm_name().c_str()));
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

void Service::NotifyCiceroneOfVmStarted(const std::string& owner_id,
                                        const std::string& vm_name,
                                        uint32_t container_subnet,
                                        uint32_t container_netmask,
                                        uint32_t ipv4_address) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                               vm_tools::cicerone::kNotifyVmStartedMethod);
  dbus::MessageWriter writer(&method_call);
  vm_tools::cicerone::NotifyVmStartedRequest request;
  request.set_owner_id(owner_id);
  request.set_vm_name(vm_name);
  request.set_container_ipv4_subnet(container_subnet);
  request.set_container_ipv4_netmask(container_netmask);
  request.set_ipv4_address(ipv4_address);
  writer.AppendProtoAsArrayOfBytes(request);
  std::unique_ptr<dbus::Response> dbus_response =
      cicerone_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed notifying cicerone of VM startup";
  }
}

void Service::NotifyCiceroneOfVmStopped(const std::string& owner_id,
                                        const std::string& vm_name) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                               vm_tools::cicerone::kNotifyVmStoppedMethod);
  dbus::MessageWriter writer(&method_call);
  vm_tools::cicerone::NotifyVmStoppedRequest request;
  request.set_owner_id(owner_id);
  request.set_vm_name(vm_name);
  writer.AppendProtoAsArrayOfBytes(request);
  std::unique_ptr<dbus::Response> dbus_response =
      cicerone_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed notifying cicerone of VM stopped";
  }
}

std::string Service::GetContainerToken(const std::string& owner_id,
                                       const std::string& vm_name,
                                       const std::string& container_name) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                               vm_tools::cicerone::kGetContainerTokenMethod);
  dbus::MessageWriter writer(&method_call);
  vm_tools::cicerone::ContainerTokenRequest request;
  vm_tools::cicerone::ContainerTokenResponse response;
  request.set_owner_id(owner_id);
  request.set_vm_name(vm_name);
  request.set_container_name(container_name);
  writer.AppendProtoAsArrayOfBytes(request);
  std::unique_ptr<dbus::Response> dbus_response =
      cicerone_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed getting container token from cicerone";
    return "";
  }
  dbus::MessageReader reader(dbus_response.get());
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed parsing proto response";
    return "";
  }
  return response.container_token();
}

bool Service::IsContainerRunning(const std::string& owner_id,
                                 const std::string& vm_name,
                                 const std::string& container_name) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                               vm_tools::cicerone::kIsContainerRunningMethod);
  dbus::MessageWriter writer(&method_call);
  vm_tools::cicerone::IsContainerRunningRequest request;
  vm_tools::cicerone::IsContainerRunningResponse response;
  request.set_owner_id(owner_id);
  request.set_vm_name(vm_name);
  request.set_container_name(container_name);
  writer.AppendProtoAsArrayOfBytes(request);
  std::unique_ptr<dbus::Response> dbus_response =
      cicerone_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed querying cicerone for container running";
    return false;
  }
  dbus::MessageReader reader(dbus_response.get());
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed parsing proto response";
    return false;
  }
  return response.container_running();
}

Service::VmMap::iterator Service::FindVm(std::string owner_id,
                                         std::string vm_name) {
  auto it = vms_.find(std::make_pair(owner_id, vm_name));
  // TODO(nverne): remove this fallback when Chrome is correctly seting owner_id
  if (it == vms_.end()) {
    return vms_.find(std::make_pair("", vm_name));
  }
  return it;
}

}  // namespace concierge
}  // namespace vm_tools
