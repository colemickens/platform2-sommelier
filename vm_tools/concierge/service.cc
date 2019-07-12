// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/service.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <net/route.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/sendfile.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linux/vm_sockets.h>  // Needs to come after sys/socket.h

#include <algorithm>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

#include <arc/network/subnet.h>
#include <base/base64url.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/guid.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/memory/ptr_util.h>
#include <base/single_thread_task_runner.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <base/synchronization/waitable_event.h>
#include <base/sys_info.h>
#include <base/threading/thread_task_runner_handle.h>
#include <base/time/time.h>
#include <base/version.h>
#include <chromeos/dbus/service_constants.h>
#include <crosvm/qcow_utils.h>
#include <dbus/object_proxy.h>
#include <vm_cicerone/proto_bindings/cicerone_service.pb.h>
#include <vm_concierge/proto_bindings/service.pb.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/concierge/arc_vm.h"
#include "vm_tools/concierge/plugin_vm.h"
#include "vm_tools/concierge/seneschal_server_proxy.h"
#include "vm_tools/concierge/ssh_keys.h"
#include "vm_tools/concierge/vmplugin_dispatcher_interface.h"

using std::string;

namespace vm_tools {
namespace concierge {

namespace {

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

// How long we should wait for a VM to start up.
// While this timeout might be high, it's meant to be a final failure point, not
// the lower bound of how long it takes.  On a loaded system (like extracting
// large compressed files), it could take 10 seconds to boot.
constexpr base::TimeDelta kVmStartupTimeout = base::TimeDelta::FromSeconds(30);

// crosvm directory name.
constexpr char kCrosvmDir[] = "crosvm";

// Plugin VM directory name.
constexpr char kPluginVmDir[] = "pvm";

// Cryptohome root base path.
constexpr char kCryptohomeRoot[] = "/home/root";

// File extension for raw disk types
constexpr char kRawImageExtension[] = ".img";

// File extension for qcow2 disk types
constexpr char kQcowImageExtension[] = ".qcow2";

// File extension for Plugin VMs disk types
constexpr char kPluginVmImageExtension[] = ".pvm";

// Valid file extensions for disk images
constexpr const char* kDiskImageExtensions[] = {kRawImageExtension,
                                                kQcowImageExtension, nullptr};

// Valid file extensions for Plugin VM images
constexpr const char* kPluginVmImageExtensions[] = {kPluginVmImageExtension,
                                                    nullptr};

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

constexpr uint64_t kMinimumDiskSize = 1ll * 1024 * 1024 * 1024;  // 1 GiB
constexpr uint64_t kDiskSizeMask = ~4095ll;  // Round to disk block size.

constexpr uint64_t kDefaultIoLimit = 1024 * 1024;  // 1 Mib

// How often we should broadcast state of a disk operation (import or export).
constexpr base::TimeDelta kDiskOpReportInterval =
    base::TimeDelta::FromSeconds(15);

// Mac address to assign to ARCVM.
constexpr arc_networkd::MacAddress kArcVmMacAddress{0xd2, 0x47, 0xf7,
                                                    0xc5, 0x9e, 0x53};

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

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
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
bool GetDiskPathFromName(
    const std::string& disk_path,
    const std::string& cryptohome_id,
    StorageLocation storage_location,
    bool create_parent_dir,
    base::FilePath* path_out,
    enum DiskImageType preferred_image_type = DiskImageType::DISK_IMAGE_AUTO) {
  if (!base::ContainsOnlyChars(cryptohome_id, kValidCryptoHomeCharacters)) {
    LOG(ERROR) << "Invalid cryptohome_id specified";
    return false;
  }

  // Base64 encode the given disk name to ensure it only has valid characters.
  std::string disk_name;
  base::Base64UrlEncode(disk_path, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &disk_name);

  switch (storage_location) {
    case STORAGE_CRYPTOHOME_ROOT: {
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

      auto qcow2_path = crosvm_dir.Append(disk_name + kQcowImageExtension);
      auto raw_path = crosvm_dir.Append(disk_name + kRawImageExtension);
      bool qcow2_exists = base::PathExists(qcow2_path);
      bool raw_exists = base::PathExists(raw_path);

      // This scenario (both <name>.img and <name>.qcow2 exist) should never
      // happen. It is prevented by the later checks in this function.
      // However, in case it does happen somehow (e.g. user manually created
      // files in dev mode), bail out, since we can't tell which one the user
      // wants.
      if (qcow2_exists && raw_exists) {
        LOG(ERROR) << "Both qcow2 and raw variants of " << disk_path
                   << " already exist.";
        return false;
      }

      // Return the path to an existing image of any type, if one exists.
      // If not, generate a path based on the preferred image type.
      if (qcow2_exists) {
        *path_out = qcow2_path;
      } else if (raw_exists) {
        *path_out = raw_path;
      } else if (preferred_image_type == DISK_IMAGE_QCOW2) {
        *path_out = qcow2_path;
      } else if (preferred_image_type == DISK_IMAGE_RAW ||
                 preferred_image_type == DISK_IMAGE_AUTO) {
        *path_out = raw_path;
      } else {
        LOG(ERROR) << "Unknown image type " << preferred_image_type;
        return false;
      }

      return true;
    }
    case STORAGE_CRYPTOHOME_PLUGINVM: {
      base::FilePath pluginvm_dir = base::FilePath(kCryptohomeRoot)
                                        .Append(cryptohome_id)
                                        .Append(kPluginVmDir);
      base::File::Error dir_error;
      if (!base::DirectoryExists(pluginvm_dir)) {
        if (!create_parent_dir) {
          return false;
        }

        if (!base::CreateDirectoryAndGetError(pluginvm_dir, &dir_error)) {
          LOG(ERROR) << "Failed to create plugin directory in /home/root: "
                     << base::File::ErrorToString(dir_error);
          return false;
        }
      }

      *path_out = pluginvm_dir.Append(disk_name + kPluginVmImageExtension);
      return true;
    }
    default:
      LOG(ERROR) << "Unknown storage location type";
      return false;
  }
}

bool CheckVmExists(const std::string& vm_name,
                   const std::string& cryptohome_id,
                   base::FilePath* out_path = nullptr,
                   StorageLocation* storage_location = nullptr) {
  for (int l = StorageLocation_MIN; l <= StorageLocation_MAX; l++) {
    StorageLocation location = static_cast<StorageLocation>(l);
    base::FilePath disk_path;
    if (GetDiskPathFromName(vm_name, cryptohome_id, location,
                            false, /* create_parent_dir */
                            &disk_path) &&
        base::PathExists(disk_path)) {
      if (out_path) {
        *out_path = disk_path;
      }
      if (storage_location) {
        *storage_location = location;
      }
      return true;
    }
  }

  return false;
}

uint64_t CalculateDesiredDiskSize(uint64_t current_usage) {
  // If no disk size was specified, use 90% of free space.
  // Free space is calculated as if the disk image did not consume any space.
  uint64_t free_space =
      base::SysInfo::AmountOfFreeDiskSpace(base::FilePath("/home"));
  free_space += current_usage;
  uint64_t disk_size = ((free_space * 9) / 10) & kDiskSizeMask;

  return std::max(disk_size, kMinimumDiskSize);
}

bool GetPluginDirectory(const base::FilePath& prefix,
                        const string& extension,
                        const string& vm_id,
                        bool create,
                        base::FilePath* path_out) {
  string dirname;
  base::Base64UrlEncode(vm_id, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &dirname);

  base::FilePath path = prefix.Append(dirname).AddExtension(extension);
  if (create && !base::DirectoryExists(path)) {
    base::File::Error dir_error;
    if (!base::CreateDirectoryAndGetError(path, &dir_error)) {
      LOG(ERROR) << "Failed to create plugin directory " << path.value() << ": "
                 << base::File::ErrorToString(dir_error);
      return false;
    }
  }

  *path_out = path;
  return true;
}

bool GetPluginStatefulDirectory(const string& vm_id,
                                const string& cryptohome_id,
                                base::FilePath* path_out) {
  return GetPluginDirectory(base::FilePath(kCryptohomeRoot)
                                .Append(cryptohome_id)
                                .Append(kPluginVmDir),
                            "pvm", vm_id, true /* create */, path_out);
}

bool GetPluginIsoDirectory(const string& vm_id,
                           const string& cryptohome_id,
                           bool create,
                           base::FilePath* path_out) {
  return GetPluginDirectory(base::FilePath(kCryptohomeRoot)
                                .Append(cryptohome_id)
                                .Append(kPluginVmDir),
                            "iso", vm_id, create, path_out);
}

bool GetPluginRuntimeDirectory(const string& vm_id,
                               base::ScopedTempDir* runtime_dir_out) {
  base::FilePath path;
  if (GetPluginDirectory(base::FilePath("/run/pvm"), "", vm_id,
                         true /*create */, &path)) {
    // Take ownership of directory
    CHECK(runtime_dir_out->Set(path));
    return true;
  }

  return false;
}

bool GetPluginRootDirectory(const string& vm_id,
                            base::ScopedTempDir* root_dir_out) {
  base::FilePath path;
  if (!base::CreateTemporaryDirInDir(base::FilePath(kRuntimeDir), "vm.",
                                     &path)) {
    PLOG(ERROR) << "Unable to create root directory for VM";
    return false;
  }

  // Take ownership of directory
  CHECK(root_dir_out->Set(path));
  return true;
}

bool CreatePluginRootHierarchy(const base::FilePath& root_path) {
  base::File::Error dir_error;
  if (!CreateDirectoryAndGetError(root_path.Append("etc"), &dir_error)) {
    LOG(ERROR) << "Unable to create /etc in root directory for VM "
               << base::File::ErrorToString(dir_error);
    return false;
  }

  return true;
}

bool GetPlugin9PSocketPath(const string& vm_id, base::FilePath* path_out) {
  base::FilePath runtime_dir;
  if (!GetPluginDirectory(base::FilePath("/run/pvm"), "", vm_id,
                          true /* create */, &runtime_dir)) {
    LOG(ERROR) << "Unable to get runtime directory for 9P socket";
    return false;
  }

  *path_out = runtime_dir.Append("9p.sock");
  return true;
}

void FormatDiskImageStatus(const DiskImageOperation* op,
                           DiskImageStatusResponse* status) {
  status->set_status(op->status());
  status->set_command_uuid(op->uuid());
  status->set_failure_reason(op->failure_reason());
  status->set_progress(op->GetProgress());
}

uint64_t GetFileUsage(const base::FilePath& path) {
  struct stat st;
  if (stat(path.value().c_str(), &st) == 0) {
    // Use the st_blocks value to get the space usage (as in 'du') of the file.
    // st_blocks is always in units of 512 bytes, regardless of the underlying
    // filesystem and block device block size.
    return st.st_blocks * 512;
  }
  return 0;
}

bool ListVmDisksInLocation(const string& cryptohome_id,
                           StorageLocation location,
                           const string& lookup_name,
                           ListVmDisksResponse* response) {
  base::FilePath image_dir;
  base::FileEnumerator::FileType file_type = base::FileEnumerator::FILES;
  const char* const* allowed_ext = kDiskImageExtensions;
  switch (location) {
    case STORAGE_CRYPTOHOME_ROOT:
      image_dir = base::FilePath(kCryptohomeRoot)
                      .Append(cryptohome_id)
                      .Append(kCrosvmDir);
      break;

    case STORAGE_CRYPTOHOME_PLUGINVM:
      image_dir = base::FilePath(kCryptohomeRoot)
                      .Append(cryptohome_id)
                      .Append(kPluginVmDir);
      file_type = base::FileEnumerator::DIRECTORIES;
      allowed_ext = kPluginVmImageExtensions;
      break;

    default:
      response->set_success(false);
      response->set_failure_reason("Unsupported storage location for images");
      return false;
  }

  if (!base::DirectoryExists(image_dir)) {
    // No directory means no VMs, return the empty response.
    return true;
  }

  uint64_t total_size = 0;
  base::FileEnumerator dir_enum(image_dir, false, file_type);
  for (base::FilePath path = dir_enum.Next(); !path.empty();
       path = dir_enum.Next()) {
    string extension = path.BaseName().Extension();
    bool allowed = false;
    for (auto p = allowed_ext; *p; p++) {
      if (extension == *p) {
        allowed = true;
        break;
      }
    }
    if (!allowed) {
      continue;
    }

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
    if (!lookup_name.empty() && lookup_name != image_name) {
      continue;
    }

    uint64_t size = dir_enum.GetInfo().IsDirectory()
                        ? ComputeDirectorySize(path)
                        : GetFileUsage(path);
    total_size += size;

    VmDiskInfo* image = response->add_images();
    image->set_name(std::move(image_name));
    image->set_storage_location(location);
    image->set_size(size);
  }

  response->set_total_size(response->total_size() + total_size);
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
    : network_address_manager_({
          arc_networkd::AddressManager::Guest::VM_TERMINA,
          arc_networkd::AddressManager::Guest::VM_PLUGIN,
          arc_networkd::AddressManager::Guest::VM_ARC,
          arc_networkd::AddressManager::Guest::CONTAINER,
      }),
      watcher_(FROM_HERE),
      next_seneschal_server_port_(kFirstSeneschalServerPort),
      quit_closure_(std::move(quit_closure)),
#ifdef __arm__
      resync_vm_clocks_on_resume_(true),
#else
      resync_vm_clocks_on_resume_(false),
#endif
      weak_ptr_factory_(this) {
  plugin_subnet_ = network_address_manager_.AllocateIPv4Subnet(
      arc_networkd::AddressManager::Guest::VM_PLUGIN);

  // The first address is the gateway and cannot be used by VMs.
  plugin_gateway_ = plugin_subnet_->AllocateAtOffset(0);
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
      {kStartPluginVmMethod, &Service::StartPluginVm},
      {kStartArcVmMethod, &Service::StartArcVm},
      {kStopVmMethod, &Service::StopVm},
      {kStopAllVmsMethod, &Service::StopAllVms},
      {kGetVmInfoMethod, &Service::GetVmInfo},
      {kGetVmEnterpriseReportingInfoMethod,
       &Service::GetVmEnterpriseReportingInfo},
      {kCreateDiskImageMethod, &Service::CreateDiskImage},
      {kDestroyDiskImageMethod, &Service::DestroyDiskImage},
      {kExportDiskImageMethod, &Service::ExportDiskImage},
      {kImportDiskImageMethod, &Service::ImportDiskImage},
      {kDiskImageStatusMethod, &Service::CheckDiskImageStatus},
      {kCancelDiskImageMethod, &Service::CancelDiskImageOperation},
      {kListVmDisksMethod, &Service::ListVmDisks},
      {kGetContainerSshKeysMethod, &Service::GetContainerSshKeys},
      {kSyncVmTimesMethod, &Service::SyncVmTimes},
      {kAttachUsbDeviceMethod, &Service::AttachUsbDevice},
      {kDetachUsbDeviceMethod, &Service::DetachUsbDevice},
      {kListUsbDeviceMethod, &Service::ListUsbDevices},
      {kGetDnsSettingsMethod, &Service::GetDnsSettings},
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

  // Set up the D-Bus client for shill.
  shill_client_ = std::make_unique<ShillClient>(bus_);
  shill_client_->RegisterResolvConfigChangedHandler(base::Bind(
      &Service::OnResolvConfigChanged, weak_ptr_factory_.GetWeakPtr()));

  // Set up the D-Bus client for powerd and register suspend/resume handlers.
  power_manager_client_ = std::make_unique<PowerManagerClient>(bus_);
  power_manager_client_->RegisterSuspendDelay(
      base::Bind(&Service::HandleSuspendImminent,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&Service::HandleSuspendDone, weak_ptr_factory_.GetWeakPtr()));

  // Get the D-Bus proxy for communicating with cicerone.
  cicerone_service_proxy_ = bus_->GetObjectProxy(
      vm_tools::cicerone::kVmCiceroneServiceName,
      dbus::ObjectPath(vm_tools::cicerone::kVmCiceroneServicePath));
  if (!cicerone_service_proxy_) {
    LOG(ERROR) << "Unable to get dbus proxy for "
               << vm_tools::cicerone::kVmCiceroneServiceName;
    return false;
  }
  cicerone_service_proxy_->ConnectToSignal(
      vm_tools::cicerone::kVmCiceroneServiceName,
      vm_tools::cicerone::kTremplinStartedSignal,
      base::Bind(&Service::OnTremplinStartedSignal,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&Service::OnSignalConnected, weak_ptr_factory_.GetWeakPtr()));

  // Get the D-Bus proxy for communicating with seneschal.
  seneschal_service_proxy_ = bus_->GetObjectProxy(
      vm_tools::seneschal::kSeneschalServiceName,
      dbus::ObjectPath(vm_tools::seneschal::kSeneschalServicePath));
  if (!seneschal_service_proxy_) {
    LOG(ERROR) << "Unable to get dbus proxy for "
               << vm_tools::seneschal::kSeneschalServiceName;
    return false;
  }

  // Get the D-Bus proxy for communicating with Plugin VM dispatcher.
  vmplugin_service_proxy_ = pvm::dispatcher::GetServiceProxy(bus_);
  if (!vmplugin_service_proxy_) {
    LOG(ERROR) << "Unable to get dbus proxy for Plugin VM dispatcher service";
    return false;
  }

  // Setup & start the gRPC listener services.
  if (!SetupListenerService(
          &grpc_thread_vm_, &startup_listener_,
          base::StringPrintf("vsock:%u:%u", VMADDR_CID_ANY,
                             vm_tools::kDefaultStartupListenerPort),
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

  // Add CAP_SETGID to the list of ambient capabilities to allow crosvm
  // establish proper gid map in its plugin jail.
  if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_SETGID, 0, 0)) {
    PLOG(ERROR) << "Failed to add CAP_SETGID to the ambient capabilities";
    return false;
  }

  // Restore process' "dumpable" flag so that /proc will be writable.
  // We need it to properly set up jail for Plugin VM helper process.
  if (prctl(PR_SET_DUMPABLE, 1) < 0) {
    PLOG(ERROR) << "Failed to set PR_SET_DUMPABLE";
    return false;
  }

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
  DCHECK(sequence_checker_.CalledOnValidSequence());
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

    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) != 0) {
        LOG(INFO) << "Process " << pid << " exited with status "
                  << WEXITSTATUS(status);
      }
    } else if (WIFSIGNALED(status)) {
      LOG(INFO) << "Process " << pid << " killed by signal " << WTERMSIG(status)
                << (WCOREDUMP(status) ? " (core dumped)" : "");
    } else {
      LOG(WARNING) << "Unknown exit status " << status << " for process "
                   << pid;
    }

    // See if this is a process we launched.
    auto iter = std::find_if(vms_.begin(), vms_.end(), [=](auto& pair) {
      VmInterface::Info info = pair.second->GetInfo();
      return pid == info.pid;
    });

    if (iter != vms_.end()) {
      // Notify cicerone that the VM has exited.
      NotifyCiceroneOfVmStopped(iter->first);

      // Now remove it from the vm list.
      vms_.erase(iter);
    }
  }
}

void Service::HandleSigterm() {
  LOG(INFO) << "Shutting down due to SIGTERM";

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
}

std::unique_ptr<dbus::Response> Service::StartVm(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received StartVm request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  StartVmRequest request;
  StartVmResponse response;
  // We change to a success status later if necessary.
  response.set_status(VM_STATUS_FAILURE);

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

  // Make sure we have our signal connected if starting a Termina VM.
  if (request.start_termina() && !is_tremplin_started_signal_connected_) {
    LOG(ERROR) << "Can't start Termina VM without TremplinStartedSignal";
    response.set_failure_reason("TremplinStartedSignal not connected");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto iter = FindVm(request.owner_id(), request.name());
  if (iter != vms_.end()) {
    LOG(INFO) << "VM with requested name is already running";

    VmInterface::Info vm = iter->second->GetInfo();

    VmInfo* vm_info = response.mutable_vm_info();
    vm_info->set_ipv4_address(vm.ipv4_address);
    vm_info->set_pid(vm.pid);
    vm_info->set_cid(vm.cid);
    vm_info->set_seneschal_server_handle(vm.seneschal_server_handle);
    switch (vm.status) {
      case VmInterface::Status::STARTING: {
        response.set_status(VM_STATUS_STARTING);
        break;
      }
      case VmInterface::Status::RUNNING: {
        response.set_status(VM_STATUS_RUNNING);
        break;
      }
      default: {
        response.set_status(VM_STATUS_UNKNOWN);
        break;
      }
    }
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

  std::vector<TerminaVm::Disk> disks;
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
    disks.push_back(TerminaVm::Disk{
        .path = std::move(fd_path),
        .writable = true,
    });
  }

  for (const auto& disk : request.disks()) {
    if (!base::PathExists(base::FilePath(disk.path()))) {
      LOG(ERROR) << "Missing disk path: " << disk.path();
      response.set_failure_reason("One or more disk paths do not exist");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    disks.push_back(TerminaVm::Disk{
        .path = base::FilePath(disk.path()),
        .writable = disk.writable(),
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
  arc_networkd::MacAddress mac_address = mac_address_generator_.Generate();
  std::unique_ptr<arc_networkd::Subnet> subnet =
      network_address_manager_.AllocateIPv4Subnet(
          arc_networkd::AddressManager::Guest::VM_TERMINA);
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
  uint32_t seneschal_server_port = next_seneschal_server_port_++;
  std::unique_ptr<SeneschalServerProxy> server_proxy =
      SeneschalServerProxy::CreateVsockProxy(seneschal_service_proxy_,
                                             seneschal_server_port, vsock_cid);
  if (!server_proxy) {
    LOG(ERROR) << "Unable to start shared directory server";

    response.set_failure_reason("Unable to start shared directory server");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  uint32_t seneschal_server_handle = server_proxy->handle();

  // Associate a WaitableEvent with this VM.  This needs to happen before
  // starting the VM to avoid a race where the VM reports that it's ready
  // before it gets added as a pending VM.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  startup_listener_.AddPendingVm(vsock_cid, &event);

  // Start the VM and build the response.
  VmFeatures features{
      .gpu = request.enable_gpu(),
      .software_tpm = request.software_tpm(),
  };
  auto vm = TerminaVm::Create(
      std::move(kernel), std::move(rootfs), std::move(disks),
      std::move(mac_address), std::move(subnet), vsock_cid,
      std::move(server_proxy), std::move(runtime_dir), features);
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

    startup_listener_.RemovePendingVm(vsock_cid);
    response.set_failure_reason("VM failed to start in time");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // maitre'd is ready.  Finish setting up the VM.
  if (!vm->ConfigureNetwork(nameservers_, search_domains_)) {
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

  // Mount the 9p server.
  if (!vm->Mount9P(seneschal_server_port, "/mnt/shared")) {
    LOG(ERROR) << "Failed to mount " << request.shared_directory();

    response.set_failure_reason("Failed to mount shared directory");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Notify cicerone that we have started a VM.
  VmId vm_id(request.owner_id(), request.name());
  NotifyCiceroneOfVmStarted(vm_id, vm->cid(), "");

  string failure_reason;
  if (request.start_termina() && !StartTermina(vm.get(), &failure_reason)) {
    response.set_failure_reason(std::move(failure_reason));
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  LOG(INFO) << "Started VM with pid " << vm->pid();

  VmInfo* vm_info = response.mutable_vm_info();
  response.set_success(true);
  response.set_status(request.start_termina() ? VM_STATUS_STARTING
                                              : VM_STATUS_RUNNING);
  vm_info->set_ipv4_address(vm->IPv4Address());
  vm_info->set_pid(vm->pid());
  vm_info->set_cid(vsock_cid);
  vm_info->set_seneschal_server_handle(seneschal_server_handle);
  writer.AppendProtoAsArrayOfBytes(response);

  vms_[vm_id] = std::move(vm);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::StartPluginVm(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received StartPluginVm request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  StartPluginVmRequest request;
  StartVmResponse response;
  // We change to a success status later if necessary.
  response.set_status(VM_STATUS_FAILURE);

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse StartPluginVmRequest from message";
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

    VmInterface::Info vm = iter->second->GetInfo();

    VmInfo* vm_info = response.mutable_vm_info();
    vm_info->set_ipv4_address(vm.ipv4_address);
    vm_info->set_pid(vm.pid);
    vm_info->set_cid(vm.cid);
    vm_info->set_seneschal_server_handle(vm.seneschal_server_handle);
    switch (vm.status) {
      case VmInterface::Status::STARTING: {
        response.set_status(VM_STATUS_STARTING);
        break;
      }
      case VmInterface::Status::RUNNING: {
        response.set_status(VM_STATUS_RUNNING);
        break;
      }
      default: {
        response.set_status(VM_STATUS_UNKNOWN);
        break;
      }
    }
    response.set_success(true);

    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Mark the mac address as in use and make sure it is not already in use.
  if (request.host_mac_address().size() != sizeof(arc_networkd::MacAddress)) {
    LOG(ERROR) << "Mac address is not exactly "
               << sizeof(arc_networkd::MacAddress) << " bytes";
    response.set_failure_reason("Invalid mac address length");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Copy over the mac address.
  arc_networkd::MacAddress mac_addr;
  memcpy(&mac_addr, request.host_mac_address().data(),
         sizeof(arc_networkd::MacAddress));

  if (!mac_address_generator_.Insert(mac_addr)) {
    LOG(ERROR) << "Invalid mac address";
    response.set_failure_reason("Invalid mac address");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Mark the ip address as in use.
  auto ipv4_addr = plugin_subnet_->Allocate(request.guest_ipv4_address());
  if (!ipv4_addr) {
    LOG(ERROR) << "Invalid IP address or address already in use";
    response.set_failure_reason("Invalid IP address or address already in use");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Check the CPU count.
  if (request.cpus() == 0 ||
      request.cpus() > base::SysInfo::NumberOfProcessors()) {
    LOG(ERROR) << "Invalid number of CPUs: " << request.cpus();
    response.set_failure_reason("Invalid CPU count");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Get the stateful directory.
  base::FilePath stateful_dir;
  if (!GetPluginStatefulDirectory(request.name(), request.owner_id(),
                                  &stateful_dir)) {
    LOG(ERROR) << "Unable to create stateful directory for VM";

    response.set_failure_reason("Unable to create stateful directory");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Get the directory for ISO images.
  base::FilePath iso_dir;
  if (!GetPluginIsoDirectory(request.name(), request.owner_id(),
                             true /* create */, &iso_dir)) {
    LOG(ERROR) << "Unable to create directory holding ISOs for VM";

    response.set_failure_reason("Unable to create ISO directory");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Create the runtime directory.
  base::ScopedTempDir runtime_dir;
  if (!GetPluginRuntimeDirectory(request.name(), &runtime_dir)) {
    LOG(ERROR) << "Unable to create runtime directory for VM";

    response.set_failure_reason("Unable to create runtime directory");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Create the root directory.
  base::ScopedTempDir root_dir;
  if (!GetPluginRootDirectory(request.name(), &root_dir)) {
    LOG(ERROR) << "Unable to create runtime directory for VM";

    response.set_failure_reason("Unable to create runtime directory");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (!CreatePluginRootHierarchy(root_dir.GetPath())) {
    response.set_failure_reason("Unable to create plugin root hierarchy");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (!PluginVm::WriteResolvConf(root_dir.GetPath().Append("etc"), nameservers_,
                                 search_domains_)) {
    LOG(ERROR) << "Unable to seed resolv.conf for the Plugin VM";

    response.set_failure_reason("Unable to seed resolv.conf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Generate the token used by cicerone to identify the VM and write it to
  // a VM specific directory that gets mounted into the VM.
  std::string vm_token = base::GenerateGUID();
  if (base::WriteFile(runtime_dir.GetPath().Append("cicerone.token"),
                      vm_token.c_str(),
                      vm_token.length()) != vm_token.length()) {
    PLOG(ERROR) << "Failure writing out cicerone token to file";

    response.set_failure_reason("Unable to set cicerone token");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  base::FilePath p9_socket_path;
  if (!GetPlugin9PSocketPath(request.name(), &p9_socket_path)) {
    response.set_failure_reason("Internal error: unable to get 9P directory");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  base::ScopedFD p9_socket =
      PluginVm::CreateUnixSocket(p9_socket_path, SOCK_STREAM);
  if (!p9_socket.is_valid()) {
    LOG(ERROR) << "Failed creating 9P socket for file sharing";

    response.set_failure_reason("Internal error: unable to create 9P socket");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy =
      SeneschalServerProxy::CreateFdProxy(seneschal_service_proxy_, p9_socket);
  if (!seneschal_server_proxy) {
    LOG(ERROR) << "Unable to start shared directory server";

    response.set_failure_reason("Unable to start shared directory server");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Build the plugin params.
  std::vector<string> params(
      std::make_move_iterator(request.mutable_params()->begin()),
      std::make_move_iterator(request.mutable_params()->end()));

  // Now start the VM.
  VmId vm_id(request.owner_id(), request.name());
  auto vm = PluginVm::Create(
      vm_id, request.cpus(), std::move(params), std::move(mac_addr),
      std::move(ipv4_addr), plugin_subnet_->Netmask(),
      plugin_subnet_->AddressAtOffset(0), std::move(stateful_dir),
      std::move(iso_dir), root_dir.Take(), runtime_dir.Take(),
      std::move(seneschal_server_proxy), vmplugin_service_proxy_);
  if (!vm) {
    LOG(ERROR) << "Unable to start VM";
    response.set_failure_reason("Unable to start VM");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  VmInterface::Info info = vm->GetInfo();

  VmInfo* vm_info = response.mutable_vm_info();
  vm_info->set_ipv4_address(info.ipv4_address);
  vm_info->set_pid(info.pid);
  vm_info->set_cid(info.cid);
  vm_info->set_seneschal_server_handle(info.seneschal_server_handle);
  switch (info.status) {
    case VmInterface::Status::STARTING: {
      response.set_status(VM_STATUS_STARTING);
      break;
    }
    case VmInterface::Status::RUNNING: {
      response.set_status(VM_STATUS_RUNNING);
      break;
    }
    default: {
      response.set_status(VM_STATUS_UNKNOWN);
      break;
    }
  }
  response.set_success(true);
  writer.AppendProtoAsArrayOfBytes(response);

  NotifyCiceroneOfVmStarted(vm_id, 0 /* cid */, std::move(vm_token));

  vms_[vm_id] = std::move(vm);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::StartArcVm(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received StartArcVm request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  StartArcVmRequest request;
  StartVmResponse response;
  // We change to a success status later if necessary.
  response.set_status(VM_STATUS_FAILURE);

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse StartArcVmRequest from message";
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

    VmInterface::Info vm = iter->second->GetInfo();

    VmInfo* vm_info = response.mutable_vm_info();
    vm_info->set_ipv4_address(vm.ipv4_address);
    vm_info->set_pid(vm.pid);
    vm_info->set_cid(vm.cid);
    vm_info->set_seneschal_server_handle(vm.seneschal_server_handle);
    if (vm.status == VmInterface::Status::RUNNING) {
      response.set_status(VM_STATUS_RUNNING);
    } else {
      response.set_status(VM_STATUS_UNKNOWN);
    }
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

  const base::FilePath kernel(request.vm().kernel());
  const base::FilePath rootfs(request.vm().rootfs());

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

  std::vector<ArcVm::Disk> disks;
  for (const auto& disk : request.disks()) {
    if (!base::PathExists(base::FilePath(disk.path()))) {
      LOG(ERROR) << "Missing disk path: " << disk.path();
      response.set_failure_reason("One or more disk paths do not exist");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }
    disks.push_back(ArcVm::Disk{
        .path = base::FilePath(disk.path()),
        .writable = disk.writable(),
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
  std::unique_ptr<arc_networkd::Subnet> subnet =
      network_address_manager_.AllocateIPv4Subnet(
          arc_networkd::AddressManager::Guest::VM_ARC);
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
  uint32_t seneschal_server_port = next_seneschal_server_port_++;
  std::unique_ptr<SeneschalServerProxy> server_proxy =
      SeneschalServerProxy::CreateVsockProxy(seneschal_service_proxy_,
                                             seneschal_server_port, vsock_cid);
  if (!server_proxy) {
    LOG(ERROR) << "Unable to start shared directory server";

    response.set_failure_reason("Unable to start shared directory server");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  uint32_t seneschal_server_handle = server_proxy->handle();

  // Build the plugin params.
  std::vector<string> params(
      std::make_move_iterator(request.mutable_params()->begin()),
      std::make_move_iterator(request.mutable_params()->end()));

  // Start the VM and build the response.
  ArcVmFeatures features;
  // TODO(lepton): Enable GPU on non-x86_64 platforms.
  features.gpu = base::SysInfo::OperatingSystemArchitecture() == "x86_64";

  auto vm = ArcVm::Create(std::move(kernel), std::move(rootfs),
                          std::move(disks), kArcVmMacAddress, std::move(subnet),
                          vsock_cid, std::move(server_proxy),
                          std::move(runtime_dir), features, std::move(params));
  if (!vm) {
    LOG(ERROR) << "Unable to start VM";

    response.set_failure_reason("Unable to start VM");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // ARCVM is ready.
  LOG(INFO) << "Started VM with pid " << vm->pid();

  VmInfo* vm_info = response.mutable_vm_info();
  response.set_success(true);
  response.set_status(VM_STATUS_RUNNING);
  vm_info->set_ipv4_address(vm->IPv4Address());
  vm_info->set_pid(vm->pid());
  vm_info->set_cid(vsock_cid);
  vm_info->set_seneschal_server_handle(seneschal_server_handle);
  writer.AppendProtoAsArrayOfBytes(response);

  VmId vm_id(request.owner_id(), request.name());
  vms_[vm_id] = std::move(vm);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::StopVm(dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
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
    // This is not an error to Chrome
    response.set_success(true);
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
  NotifyCiceroneOfVmStopped(iter->first);

  vms_.erase(iter);
  response.set_success(true);
  writer.AppendProtoAsArrayOfBytes(response);

  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::StopAllVms(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received StopAllVms request";

  // Spawn a thread for each VM to shut it down.
  for (auto& iter : vms_) {
    // Notify cicerone that we have stopped a VM.
    NotifyCiceroneOfVmStopped(iter.first);

    // Resetting the unique_ptr will call the destructor for that VM,
    // which will try stopping it normally (and then forcibly) it if
    // it hasn't stopped yet.
    iter.second.reset();
  }

  vms_.clear();

  return nullptr;
}

std::unique_ptr<dbus::Response> Service::GetVmInfo(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
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

  VmInterface::Info vm = iter->second->GetInfo();

  VmInfo* vm_info = response.mutable_vm_info();
  vm_info->set_ipv4_address(vm.ipv4_address);
  vm_info->set_pid(vm.pid);
  vm_info->set_cid(vm.cid);
  vm_info->set_seneschal_server_handle(vm.seneschal_server_handle);

  response.set_success(true);
  writer.AppendProtoAsArrayOfBytes(response);

  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::GetVmEnterpriseReportingInfo(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received GetVmEnterpriseReportingInfo request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  GetVmEnterpriseReportingInfoRequest request;
  GetVmEnterpriseReportingInfoResponse response;

  response.set_success(false);

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    const std::string error_message =
        "Unable to parse GetVmEnterpriseReportingInfo from message";
    LOG(ERROR) << error_message;
    response.set_failure_reason(error_message);
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto iter = FindVm(request.owner_id(), request.vm_name());
  if (iter == vms_.end()) {
    const std::string error_message = "Requested VM does not exist";
    LOG(ERROR) << error_message;
    response.set_failure_reason(error_message);
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // failure_reason and success will be set by GetVmEnterpriseReportingInfo.
  if (!iter->second->GetVmEnterpriseReportingInfo(&response)) {
    LOG(ERROR) << "Failed to get VM enterprise reporting info";
  }
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::SyncVmTimes(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received SyncVmTimes request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageWriter writer(dbus_response.get());

  SyncVmTimesResponse response;
  int failures = 0;
  int requests = 0;
  for (auto& vm_entry : vms_) {
    requests++;
    string failure_reason;
    if (!vm_entry.second->SetTime(&failure_reason)) {
      failures++;
      response.add_failure_reason(std::move(failure_reason));
    }
  }
  response.set_requests(requests);
  response.set_failures(failures);

  writer.AppendProtoAsArrayOfBytes(response);

  return dbus_response;
}

bool Service::StartTermina(TerminaVm* vm, string* failure_reason) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Starting lxd";

  // Allocate the subnet for lxd's bridge to use.
  std::unique_ptr<arc_networkd::Subnet> container_subnet =
      network_address_manager_.AllocateIPv4Subnet(
          arc_networkd::AddressManager::Guest::CONTAINER);
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
  size_t prefix_length = vm->ContainerPrefixLength();

  std::string container_subnet_cidr =
      base::StringPrintf("%s/%zu", dst_addr.c_str(), prefix_length);

  std::string stateful_device = "/dev/vdb";
  string error;
  if (!vm->StartTermina(std::move(container_subnet_cidr),
                        std::move(stateful_device), &error)) {
    failure_reason->assign(error);
    return false;
  }

  return true;
}

std::unique_ptr<dbus::Response> Service::CreateDiskImage(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
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
  StorageLocation disk_location;
  if (CheckVmExists(request.disk_path(), request.cryptohome_id(), &disk_path,
                    &disk_location)) {
    if (disk_location != request.storage_location()) {
      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason(
          "VM/disk with same name already exists in another storage location");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    if (disk_location == STORAGE_CRYPTOHOME_PLUGINVM) {
      // We do not support extending Plugin VM images.
      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason("Plugin VM with such name already exists");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    struct stat st;
    if (stat(disk_path.value().c_str(), &st) < 0) {
      PLOG(ERROR) << "stat() of existing VM image failed for "
                  << disk_path.value();
      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason(
          "internal error: image exists but stat() failed");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    uint64_t current_size = st.st_size;
    uint64_t current_usage = st.st_blocks * 512ull;
    LOG(INFO) << "Found existing disk at " << disk_path.value()
              << " with current size " << current_size << " and usage "
              << current_usage;

    // Automatically extend existing disk images if disk_size was not specified.
    if (request.disk_size() == 0) {
      uint64_t disk_size = CalculateDesiredDiskSize(current_usage);
      if (disk_size > current_size) {
        LOG(INFO) << "Expanding disk image from " << current_size << " to "
                  << disk_size;
        if (expand_disk_image(disk_path.value().c_str(), disk_size) != 0) {
          // If expanding the disk failed, continue with a warning.
          // Currently, raw images can be resized, and qcow2 images cannot.
          LOG(WARNING) << "Failed to expand disk image " << disk_path.value();
        }
      } else {
        LOG(INFO) << "Current size " << current_size
                  << " is already at least requested size " << disk_size
                  << " - not expanding";
      }
    }

    response.set_status(DISK_STATUS_EXISTS);
    response.set_disk_path(disk_path.value());
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (!GetDiskPathFromName(request.disk_path(), request.cryptohome_id(),
                           request.storage_location(),
                           true, /* create_parent_dir */
                           &disk_path, request.image_type())) {
    response.set_status(DISK_STATUS_FAILED);
    response.set_failure_reason("Failed to create vm image");
    writer.AppendProtoAsArrayOfBytes(response);

    return dbus_response;
  }

  if (request.storage_location() == STORAGE_CRYPTOHOME_PLUGINVM) {
    // Get the FD to fill with disk image data.
    base::ScopedFD in_fd;
    if (!reader.PopFileDescriptor(&in_fd)) {
      LOG(ERROR) << "CreateDiskImage: no fd found";
      response.set_failure_reason("no source fd found");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    // Get the name of directory for ISO images. Do not create it - it will be
    // created by the PluginVmCreateOperation code.
    base::FilePath iso_dir;
    if (!GetPluginIsoDirectory(request.disk_path(), request.cryptohome_id(),
                               false /* create */, &iso_dir)) {
      LOG(ERROR) << "Unable to determine directory for ISOs";

      response.set_failure_reason("Unable to determine ISO directory");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    std::vector<string> params(
        std::make_move_iterator(request.mutable_params()->begin()),
        std::make_move_iterator(request.mutable_params()->end()));

    auto op = PluginVmCreateOperation::Create(
        std::move(in_fd), iso_dir, request.source_size(),
        VmId(request.cryptohome_id(), request.disk_path()), std::move(params));

    response.set_disk_path(disk_path.value());
    response.set_status(op->status());
    response.set_command_uuid(op->uuid());
    response.set_failure_reason(op->failure_reason());

    if (op->status() == DISK_STATUS_IN_PROGRESS) {
      std::string uuid = op->uuid();
      disk_image_ops_.emplace_back(DiskOpInfo(std::move(op)));
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&Service::RunDiskImageOperation,
                     weak_ptr_factory_.GetWeakPtr(), std::move(uuid)));
    }

    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  uint64_t disk_size =
      request.disk_size() ? request.disk_size() : CalculateDesiredDiskSize(0);

  if (request.image_type() == DISK_IMAGE_RAW ||
      request.image_type() == DISK_IMAGE_AUTO) {
    LOG(INFO) << "Creating raw disk at: " << disk_path.value() << " size "
              << disk_size;
    base::ScopedFD fd(
        open(disk_path.value().c_str(), O_CREAT | O_NONBLOCK | O_WRONLY, 0600));
    if (!fd.is_valid()) {
      PLOG(ERROR) << "Failed to create raw disk";
      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason("Failed to create raw disk file");
      writer.AppendProtoAsArrayOfBytes(response);

      return dbus_response;
    }

    int ret = ftruncate(fd.get(), disk_size);
    if (ret != 0) {
      PLOG(ERROR) << "Failed to truncate raw disk";
      unlink(disk_path.value().c_str());
      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason("Failed to truncate raw disk file");
      writer.AppendProtoAsArrayOfBytes(response);

      return dbus_response;
    }

    // If a raw disk was explicitly requested, return early without checking
    // for FALLOC_FL_PUNCH_HOLE support.
    if (request.image_type() == DISK_IMAGE_RAW) {
      response.set_status(DISK_STATUS_CREATED);
      response.set_disk_path(disk_path.value());
      writer.AppendProtoAsArrayOfBytes(response);

      return dbus_response;
    }

    ret = fallocate(fd.get(), FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0,
                    disk_size);
    if (ret == 0) {
      LOG(INFO) << "fallocate(FALLOC_FL_PUNCH_HOLE) is supported";
      response.set_status(DISK_STATUS_CREATED);
      response.set_disk_path(disk_path.value());
      writer.AppendProtoAsArrayOfBytes(response);

      return dbus_response;
    }

    // If hole punch is not available and the type is DISK_IMAGE_AUTO,
    // try to create a qcow2 file instead.
    LOG(INFO) << "fallocate(FALLOC_FL_PUNCH_HOLE) not supported for raw file: "
              << strerror(errno);
    unlink(disk_path.value().c_str());
    if (!GetDiskPathFromName(request.disk_path(), request.cryptohome_id(),
                             request.storage_location(),
                             true, /* create_parent_dir */
                             &disk_path, DISK_IMAGE_QCOW2)) {
      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason("Failed to create vm image");
      writer.AppendProtoAsArrayOfBytes(response);

      return dbus_response;
    }
  }

  LOG(INFO) << "Creating qcow2 disk at: " << disk_path.value() << " size "
            << disk_size;
  int ret = create_qcow_with_size(disk_path.value().c_str(), disk_size);
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
  DCHECK(sequence_checker_.CalledOnValidSequence());
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

  // Stop the associated VM if it is still running.
  auto iter = FindVm(request.cryptohome_id(), request.disk_path());
  if (iter != vms_.end()) {
    LOG(INFO) << "Shutting down VM";
    if (!iter->second->Shutdown()) {
      LOG(ERROR) << "Unable to shut down VM";

      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason("Unable to shut down VM");
      writer.AppendProtoAsArrayOfBytes(response);
      return dbus_response;
    }

    // Notify cicerone that we have stopped a VM.
    NotifyCiceroneOfVmStopped(iter->first);
    vms_.erase(iter);
  }

  base::FilePath disk_path;
  StorageLocation location;
  if (!CheckVmExists(request.disk_path(), request.cryptohome_id(), &disk_path,
                     &location)) {
    response.set_status(DISK_STATUS_DOES_NOT_EXIST);
    response.set_failure_reason("No such image");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (!EraseGuestSshKeys(request.cryptohome_id(), request.disk_path())) {
    // Don't return a failure here, just log an error because this is only a
    // side effect and not what the real request is about.
    LOG(ERROR) << "Failed removing guest SSH keys for VM "
               << request.disk_path();
  }

  if (location == STORAGE_CRYPTOHOME_PLUGINVM) {
    // Plugin VMs need to be unregistered before we can delete them.
    VmId vm_id(request.cryptohome_id(), request.disk_path());
    bool registered;
    if (!pvm::dispatcher::IsVmRegistered(vmplugin_service_proxy_, vm_id,
                                         &registered)) {
      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason(
          "failed to check Plugin VM registration status");
      writer.AppendProtoAsArrayOfBytes(response);

      return dbus_response;
    }

    if (registered &&
        !pvm::dispatcher::UnregisterVm(vmplugin_service_proxy_, vm_id)) {
      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason("failed to unregister Plugin VM");
      writer.AppendProtoAsArrayOfBytes(response);

      return dbus_response;
    }

    base::FilePath iso_dir;
    if (GetPluginIsoDirectory(vm_id.name(), vm_id.owner_id(),
                              false /* create */, &iso_dir) &&
        base::PathExists(iso_dir) &&
        !base::DeleteFile(iso_dir, true /* recursive */)) {
      LOG(ERROR) << "Unable to remove ISO directory for " << vm_id.name();

      response.set_status(DISK_STATUS_FAILED);
      response.set_failure_reason("Unable to remove ISO directory");
      writer.AppendProtoAsArrayOfBytes(response);

      return dbus_response;
    }
  }

  if (!base::DeleteFile(
          disk_path, location == STORAGE_CRYPTOHOME_PLUGINVM /* recursive */)) {
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
  DCHECK(sequence_checker_.CalledOnValidSequence());
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
  StorageLocation location;
  if (!CheckVmExists(request.disk_path(), request.cryptohome_id(), &disk_path,
                     &location)) {
    response.set_status(DISK_STATUS_DOES_NOT_EXIST);
    response.set_failure_reason("Export image doesn't exist");
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

  switch (location) {
    case STORAGE_CRYPTOHOME_ROOT: {
      base::ScopedFD disk_fd(HANDLE_EINTR(
          open(disk_path.value().c_str(), O_RDWR | O_NOFOLLOW | O_CLOEXEC)));
      if (!disk_fd.is_valid()) {
        LOG(ERROR) << "Failed opening VM disk for export";
        response.set_failure_reason("Failed opening VM disk for export");
        break;
      }

      int convert_res = convert_to_qcow2(disk_fd.get(), storage_fd.get());
      if (convert_res < 0) {
        response.set_failure_reason("convert_to_qcow2 failed");
        break;
      }

      response.set_status(DISK_STATUS_CREATED);
      break;
    }

    case STORAGE_CRYPTOHOME_PLUGINVM: {
      auto op = PluginVmExportOperation::Create(
          VmId(request.cryptohome_id(), request.disk_path()), disk_path,
          std::move(storage_fd));

      response.set_status(op->status());
      response.set_command_uuid(op->uuid());
      response.set_failure_reason(op->failure_reason());

      if (op->status() == DISK_STATUS_IN_PROGRESS) {
        std::string uuid = op->uuid();
        disk_image_ops_.emplace_back(DiskOpInfo(std::move(op)));
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::Bind(&Service::RunDiskImageOperation,
                       weak_ptr_factory_.GetWeakPtr(), std::move(uuid)));
      }
      break;
    }

    default:
      LOG(ERROR) << "Unsupported location for source image";
      response.set_failure_reason("Unsupported location for image");
      break;
  }

  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::ImportDiskImage(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received ImportDiskImage request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  ImportDiskImageResponse response;
  response.set_status(DISK_STATUS_FAILED);

  ImportDiskImageRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse ImportDiskImageRequest from message";
    response.set_failure_reason("Unable to parse ImportDiskRequest");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (CheckVmExists(request.disk_path(), request.cryptohome_id())) {
    response.set_status(DISK_STATUS_EXISTS);
    response.set_failure_reason("VM/disk with such name already exists");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.storage_location() != STORAGE_CRYPTOHOME_PLUGINVM) {
    LOG(ERROR)
        << "Locations other than STORAGE_CRYPTOHOME_PLUGINVM are not supported";
    response.set_failure_reason("Unsupported location for image");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  base::FilePath disk_path;
  if (!GetDiskPathFromName(request.disk_path(), request.cryptohome_id(),
                           request.storage_location(),
                           true, /* create_parent_dir */
                           &disk_path)) {
    response.set_failure_reason("Failed to set up vm image name");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Get the FD to fill with disk image data.
  base::ScopedFD in_fd;
  if (!reader.PopFileDescriptor(&in_fd)) {
    LOG(ERROR) << "import: no fd found";
    response.set_failure_reason("import: no fd found");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto op = PluginVmImportOperation::Create(
      std::move(in_fd), disk_path, request.source_size(),
      VmId(request.cryptohome_id(), request.disk_path()),
      vmplugin_service_proxy_);

  response.set_status(op->status());
  response.set_command_uuid(op->uuid());
  response.set_failure_reason(op->failure_reason());

  if (op->status() == DISK_STATUS_IN_PROGRESS) {
    std::string uuid = op->uuid();
    disk_image_ops_.emplace_back(DiskOpInfo(std::move(op)));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&Service::RunDiskImageOperation,
                              weak_ptr_factory_.GetWeakPtr(), std::move(uuid)));
  }

  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

void Service::RunDiskImageOperation(std::string uuid) {
  auto iter =
      std::find_if(disk_image_ops_.begin(), disk_image_ops_.end(),
                   [&uuid](auto& info) { return info.op->uuid() == uuid; });

  if (iter == disk_image_ops_.end()) {
    LOG(ERROR) << "RunDiskImageOperation called with unknown uuid";
    return;
  }

  if (iter->canceled) {
    // Operation was cancelled. Now that our posted task is running we can
    // remove it from the list and not reschedule ourselves.
    disk_image_ops_.erase(iter);
    return;
  }

  auto op = iter->op.get();
  op->Run(kDefaultIoLimit);
  if (base::TimeTicks::Now() - iter->last_report_time > kDiskOpReportInterval ||
      op->status() != DISK_STATUS_IN_PROGRESS) {
    LOG(INFO) << "Disk Image Operation: UUID=" << uuid
              << " progress: " << op->GetProgress()
              << " status: " << op->status();

    // Send the D-Bus signal out updating progress of the operation.
    DiskImageStatusResponse status;
    FormatDiskImageStatus(op, &status);
    dbus::Signal signal(kVmConciergeInterface, kDiskImageProgressSignal);
    dbus::MessageWriter(&signal).AppendProtoAsArrayOfBytes(status);
    exported_object_->SendSignal(&signal);

    // Note the time we sent out the notification.
    iter->last_report_time = base::TimeTicks::Now();
  }

  if (op->status() == DISK_STATUS_IN_PROGRESS) {
    // Reschedule ourselves so we can execute next chunk of work.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&Service::RunDiskImageOperation,
                              weak_ptr_factory_.GetWeakPtr(), std::move(uuid)));
  }
}

std::unique_ptr<dbus::Response> Service::CheckDiskImageStatus(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received DiskImageStatus request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  DiskImageStatusResponse response;
  response.set_status(DISK_STATUS_FAILED);

  DiskImageStatusRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse DiskImageStatusRequest from message";
    response.set_failure_reason("Unable to parse DiskImageStatusRequest");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Locate the pending command in the list.
  auto iter = std::find_if(disk_image_ops_.begin(), disk_image_ops_.end(),
                           [&request](auto& info) {
                             return info.op->uuid() == request.command_uuid();
                           });

  if (iter == disk_image_ops_.end() || iter->canceled) {
    LOG(ERROR) << "Unknown command uuid in DiskImageStatusRequest";
    response.set_failure_reason("Unknown command uuid");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto op = iter->op.get();
  FormatDiskImageStatus(op, &response);
  writer.AppendProtoAsArrayOfBytes(response);

  // Erase operation form the list if it is no longer in progress.
  if (op->status() != DISK_STATUS_IN_PROGRESS) {
    disk_image_ops_.erase(iter);
  }

  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::CancelDiskImageOperation(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received CancelDiskImage request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  CancelDiskImageResponse response;
  response.set_success(false);

  CancelDiskImageRequest request;
  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse CancelDiskImageRequest from message";
    response.set_failure_reason("Unable to parse CancelDiskImageRequest");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Locate the pending command in the list.
  auto iter = std::find_if(disk_image_ops_.begin(), disk_image_ops_.end(),
                           [&request](auto& info) {
                             return info.op->uuid() == request.command_uuid();
                           });

  if (iter == disk_image_ops_.end()) {
    LOG(ERROR) << "Unknown command uuid in CancelDiskImageRequest";
    response.set_failure_reason("Unknown command uuid");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto op = iter->op.get();
  if (op->status() != DISK_STATUS_IN_PROGRESS) {
    response.set_failure_reason("Command is no longer in progress");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  // Mark the operation as canceled. We can't erase it from the list right
  // away as there is a task posted for it. The task will erase this operation
  // when it gets to run.
  iter->canceled = true;

  response.set_success(true);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::ListVmDisks(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
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
  response.set_total_size(0);

  for (int location = StorageLocation_MIN; location <= StorageLocation_MAX;
       location++) {
    if (request.all_locations() || location == request.storage_location()) {
      if (!ListVmDisksInLocation(request.cryptohome_id(),
                                 static_cast<StorageLocation>(location),
                                 request.vm_name(), &response)) {
        break;
      }
    }
  }

  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::GetContainerSshKeys(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
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

std::unique_ptr<dbus::Response> Service::AttachUsbDevice(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received AttachUsbDevice request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  AttachUsbDeviceRequest request;
  AttachUsbDeviceResponse response;
  base::ScopedFD fd;

  response.set_success(false);

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse AttachUsbDeviceRequest from message";
    response.set_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (!reader.PopFileDescriptor(&fd)) {
    LOG(ERROR) << "Unable to parse file descriptor from dbus message";
    response.set_reason("Unable to parse file descriptor");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto iter = FindVm(request.owner_id(), request.vm_name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "Requested VM " << request.vm_name() << " does not exist";
    response.set_reason("Requested VM does not exist");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.bus_number() > 0xFF) {
    LOG(ERROR) << "Bus number out of valid range " << request.bus_number();
    response.set_reason("Invalid bus number");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.port_number() > 0xFF) {
    LOG(ERROR) << "Port number out of valid range " << request.port_number();
    response.set_reason("Invalid port number");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.vendor_id() > 0xFFFF) {
    LOG(ERROR) << "Vendor ID out of valid range " << request.vendor_id();
    response.set_reason("Invalid vendor ID");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.product_id() > 0xFFFF) {
    LOG(ERROR) << "Product ID out of valid range " << request.product_id();
    response.set_reason("Invalid product ID");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  UsbControlResponse usb_response;
  if (!iter->second->AttachUsbDevice(
          request.bus_number(), request.port_number(), request.vendor_id(),
          request.product_id(), fd.get(), &usb_response)) {
    LOG(ERROR) << "Failed to attach USB device: " << usb_response.reason;
    response.set_reason(std::move(usb_response.reason));
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }
  response.set_success(true);
  response.set_guest_port(usb_response.port);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::DetachUsbDevice(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received DetachUsbDevice request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  DetachUsbDeviceRequest request;
  DetachUsbDeviceResponse response;

  response.set_success(false);

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse DetachUsbDeviceRequest from message";
    response.set_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto iter = FindVm(request.owner_id(), request.vm_name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "Requested VM does not exist";
    response.set_reason("Requested VM does not exist");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  if (request.guest_port() > 0xFF) {
    LOG(ERROR) << "Guest port number out of valid range "
               << request.guest_port();
    response.set_reason("Invalid guest port number");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  UsbControlResponse usb_response;
  if (!iter->second->DetachUsbDevice(request.guest_port(), &usb_response)) {
    LOG(ERROR) << "Failed to detach USB device";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }
  response.set_success(true);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

std::unique_ptr<dbus::Response> Service::ListUsbDevices(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received ListUsbDevices request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  ListUsbDeviceRequest request;
  ListUsbDeviceResponse response;

  response.set_success(false);

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse ListUsbDeviceRequest from message";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  auto iter = FindVm(request.owner_id(), request.vm_name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "Requested VM does not exist";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  std::vector<UsbDevice> usb_list;
  if (!iter->second->ListUsbDevice(&usb_list)) {
    LOG(ERROR) << "Failed to list USB devices";
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }
  for (auto usb : usb_list) {
    UsbDeviceMessage* usb_proto = response.add_usb_devices();
    usb_proto->set_guest_port(usb.port);
    usb_proto->set_vendor_id(usb.vid);
    usb_proto->set_product_id(usb.pid);
  }
  response.set_success(true);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

void Service::ComposeDnsResponse(dbus::MessageWriter* writer) {
  DnsSettings dns_settings;
  for (const auto& server : nameservers_) {
    dns_settings.add_nameservers(server);
  }
  for (const auto& domain : search_domains_) {
    dns_settings.add_search_domains(domain);
  }
  writer->AppendProtoAsArrayOfBytes(dns_settings);
}

std::unique_ptr<dbus::Response> Service::GetDnsSettings(
    dbus::MethodCall* method_call) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  LOG(INFO) << "Received GetDnsSettings request";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageWriter writer(dbus_response.get());
  ComposeDnsResponse(&writer);
  return dbus_response;
}

void Service::OnResolvConfigChanged(std::vector<string> nameservers,
                                    std::vector<string> search_domains) {
  nameservers_ = std::move(nameservers);
  search_domains_ = std::move(search_domains);

  if (vms_suspended_) {
    // The VMs are currently suspended and will not respond to RPCs.  Instead
    // update the resolv.conf files after we get a SuspendDone from powerd.
    update_resolv_config_on_resume_ = true;
    return;
  }

  for (auto& vm_entry : vms_) {
    vm_entry.second->SetResolvConfig(nameservers_, search_domains_);
  }

  // Broadcast DnsSettingsChanged signal so Plugin VM dispatcher is aware as
  // well.
  dbus::Signal signal(kVmConciergeInterface, kDnsSettingsChangedSignal);
  dbus::MessageWriter writer(&signal);
  ComposeDnsResponse(&writer);
  exported_object_->SendSignal(&signal);
}

void Service::NotifyCiceroneOfVmStarted(const VmId& vm_id,
                                        uint32_t cid,
                                        std::string vm_token) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                               vm_tools::cicerone::kNotifyVmStartedMethod);
  dbus::MessageWriter writer(&method_call);
  vm_tools::cicerone::NotifyVmStartedRequest request;
  request.set_owner_id(vm_id.owner_id());
  request.set_vm_name(vm_id.name());
  request.set_cid(cid);
  request.set_vm_token(std::move(vm_token));
  writer.AppendProtoAsArrayOfBytes(request);
  std::unique_ptr<dbus::Response> dbus_response =
      cicerone_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed notifying cicerone of VM startup";
  }
}

void Service::NotifyCiceroneOfVmStopped(const VmId& vm_id) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                               vm_tools::cicerone::kNotifyVmStoppedMethod);
  dbus::MessageWriter writer(&method_call);
  vm_tools::cicerone::NotifyVmStoppedRequest request;
  request.set_owner_id(vm_id.owner_id());
  request.set_vm_name(vm_id.name());
  writer.AppendProtoAsArrayOfBytes(request);
  std::unique_ptr<dbus::Response> dbus_response =
      cicerone_service_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed notifying cicerone of VM stopped";
  }
}

std::string Service::GetContainerToken(const VmId& vm_id,
                                       const std::string& container_name) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  dbus::MethodCall method_call(vm_tools::cicerone::kVmCiceroneInterface,
                               vm_tools::cicerone::kGetContainerTokenMethod);
  dbus::MessageWriter writer(&method_call);
  vm_tools::cicerone::ContainerTokenRequest request;
  vm_tools::cicerone::ContainerTokenResponse response;
  request.set_owner_id(vm_id.owner_id());
  request.set_vm_name(vm_id.name());
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

void Service::OnTremplinStartedSignal(dbus::Signal* signal) {
  DCHECK_EQ(signal->GetInterface(), vm_tools::cicerone::kVmCiceroneInterface);
  DCHECK_EQ(signal->GetMember(), vm_tools::cicerone::kTremplinStartedSignal);

  vm_tools::cicerone::TremplinStartedSignal tremplin_started_signal;
  dbus::MessageReader reader(signal);
  if (!reader.PopArrayOfBytesAsProto(&tremplin_started_signal)) {
    LOG(ERROR) << "Failed to parse TremplinStartedSignal from DBus Signal";
    return;
  }

  auto iter = FindVm(tremplin_started_signal.owner_id(),
                     tremplin_started_signal.vm_name());
  if (iter == vms_.end()) {
    LOG(ERROR) << "Received signal from an unknown vm.";
    return;
  }
  LOG(INFO) << "Received TremplinStartedSignal for " << iter->first;
  iter->second->SetTremplinStarted();
}

void Service::OnSignalConnected(const std::string& interface_name,
                                const std::string& signal_name,
                                bool is_connected) {
  if (!is_connected) {
    LOG(ERROR) << "Failed to connect to interface name: " << interface_name
               << " for signal " << signal_name;
  } else {
    LOG(INFO) << "Connected to interface name: " << interface_name
              << " for signal " << signal_name;
  }

  if (interface_name == vm_tools::cicerone::kVmCiceroneInterface) {
    DCHECK_EQ(signal_name, vm_tools::cicerone::kTremplinStartedSignal);
    is_tremplin_started_signal_connected_ = is_connected;
  }
}

void Service::HandleSuspendImminent() {
  vms_suspended_ = true;

  for (const auto& pair : vms_) {
    pair.second->HandleSuspendImminent();
  }
}

void Service::HandleSuspendDone() {
  for (const auto& pair : vms_) {
    pair.second->HandleSuspendDone();
  }
  vms_suspended_ = false;

  // Now that all VMs have been woken up, resync the VM clocks if necessary.
  if (resync_vm_clocks_on_resume_) {
    int successes = 0;
    for (auto& vm_entry : vms_) {
      string failure_reason;
      if (vm_entry.second->SetTime(&failure_reason)) {
        successes++;
      } else {
        LOG(ERROR) << "Failed to set VM clock in " << vm_entry.first << ": "
                   << failure_reason;
      }
    }

    LOG(INFO) << "Successfully set " << successes << " VM clocks.";
  }

  if (update_resolv_config_on_resume_) {
    for (auto& vm_entry : vms_) {
      vm_entry.second->SetResolvConfig(nameservers_, search_domains_);
    }

    update_resolv_config_on_resume_ = false;
  }
}

Service::VmMap::iterator Service::FindVm(const std::string& owner_id,
                                         const std::string& vm_name) {
  auto it = vms_.find(VmId(owner_id, vm_name));
  // TODO(nverne): remove this fallback when Chrome is correctly seting owner_id
  if (it == vms_.end()) {
    return vms_.find(VmId("", vm_name));
  }
  return it;
}

}  // namespace concierge
}  // namespace vm_tools
