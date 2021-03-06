// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/concierge/plugin_vm.h"

#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <algorithm>
#include <utility>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>

#include "vm_tools/concierge/plugin_vm_helper.h"
#include "vm_tools/concierge/tap_device_builder.h"
#include "vm_tools/concierge/vm_util.h"
#include "vm_tools/concierge/vmplugin_dispatcher_interface.h"

using std::string;

namespace vm_tools {
namespace concierge {
namespace {

// Path to the plugin binaries and other assets.
constexpr char kPluginBinDir[] = "/opt/pita";
constexpr char kDlcPluginBinDir[] =
    "/run/imageloader/pita/package/root/opt/pita";

// Name of the plugin VM binary.
constexpr char kPluginBinName[] = "pvm";

// Name of the runtime directory inside the jail.
constexpr char kRuntimeDir[] = "/run/pvm";

// Name of the stateful directory inside the jail.
constexpr char kStatefulDir[] = "/pvm";

// Name of the directory holding ISOs inside the jail.
constexpr char kIsoDir[] = "/iso";

// How long to wait before timing out on child process exits.
constexpr base::TimeDelta kChildExitTimeout = base::TimeDelta::FromSeconds(10);

// The CPU cgroup where all the PluginVm crosvm processes should belong to.
constexpr char kPluginVmCpuCgroup[] = "/sys/fs/cgroup/cpu/vms/plugin";

}  // namespace

// static
std::unique_ptr<PluginVm> PluginVm::Create(
    VmId id,
    uint32_t cpus,
    std::vector<string> params,
    arc_networkd::MacAddress mac_addr,
    std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr,
    uint32_t ipv4_netmask,
    uint32_t ipv4_gateway,
    base::FilePath stateful_dir,
    base::FilePath iso_dir,
    base::FilePath root_dir,
    base::FilePath runtime_dir,
    std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
    dbus::ObjectProxy* vmplugin_service_proxy) {
  auto vm = base::WrapUnique(new PluginVm(
      std::move(id), std::move(mac_addr), std::move(ipv4_addr), ipv4_netmask,
      ipv4_gateway, std::move(seneschal_server_proxy), vmplugin_service_proxy,
      std::move(iso_dir), std::move(root_dir), std::move(runtime_dir)));

  if (!vm->CreateUsbListeningSocket() ||
      !vm->Start(cpus, std::move(params), std::move(stateful_dir))) {
    vm.reset();
  }

  return vm;
}
// static
std::unique_ptr<PluginVm> PluginVm::Create(
    VmId id,
    uint32_t cpus,
    std::vector<string> params,
    arc_networkd::MacAddress mac_addr,
    std::unique_ptr<arc_networkd::Subnet> ipv4_subnet,
    std::unique_ptr<arc_networkd::SubnetAddress> ipv4_gw,
    std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr,
    base::FilePath stateful_dir,
    base::FilePath iso_dir,
    base::FilePath root_dir,
    base::FilePath runtime_dir,
    std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
    dbus::ObjectProxy* vmplugin_service_proxy) {
  auto vm = base::WrapUnique(new PluginVm(
      std::move(id), std::move(mac_addr), std::move(ipv4_subnet),
      std::move(ipv4_gw), std::move(ipv4_addr),
      std::move(seneschal_server_proxy), vmplugin_service_proxy,
      std::move(iso_dir), std::move(root_dir), std::move(runtime_dir)));

  if (!vm->CreateUsbListeningSocket() ||
      !vm->Start(cpus, std::move(params), std::move(stateful_dir))) {
    vm.reset();
  }

  return vm;
}

PluginVm::~PluginVm() {
  StopVm();
}

bool PluginVm::StopVm() {
  // Do a sanity check here to make sure the process is still around.
  if (!CheckProcessExists(process_.pid())) {
    // The process is already gone.
    process_.Release();
    return true;
  }

  if (pvm::dispatcher::SuspendVm(vmplugin_service_proxy_, id_)) {
    process_.Release();
    return true;
  }

  // Kill the process with SIGTERM. For Plugin VMs this will cause plugin to
  // try to suspend the VM.
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

bool PluginVm::Shutdown() {
  return !CheckProcessExists(process_.pid()) ||
         pvm::dispatcher::ShutdownVm(vmplugin_service_proxy_, id_);
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

bool PluginVm::GetVmEnterpriseReportingInfo(
    GetVmEnterpriseReportingInfoResponse* response) {
  response->set_success(false);
  response->set_failure_reason("Not implemented");
  return false;
}

// static
base::ScopedFD PluginVm::CreateUnixSocket(const base::FilePath& path,
                                          int type) {
  base::ScopedFD fd(socket(AF_UNIX, type, 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "Failed to create AF_UNIX socket";
    return base::ScopedFD();
  }

  struct sockaddr_un sa;

  if (path.value().length() >= sizeof(sa.sun_path)) {
    LOG(ERROR) << "Path is too long for a UNIX socket: " << path.value();
    return base::ScopedFD();
  }

  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  // The memset above took care of NUL terminating, and we already verified
  // the length before that.
  memcpy(sa.sun_path, path.value().data(), path.value().length());

  // Delete any old socket instances.
  if (PathExists(path) && !DeleteFile(path, false)) {
    PLOG(ERROR) << "failed to delete " << path.value();
    return base::ScopedFD();
  }

  // Bind the socket.
  if (bind(fd.get(), reinterpret_cast<const sockaddr*>(&sa), sizeof(sa)) < 0) {
    PLOG(ERROR) << "failed to bind " << path.value();
    return base::ScopedFD();
  }

  return fd;
}

// static
bool PluginVm::SetVmCpuRestriction(CpuRestrictionState cpu_restriction_state) {
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
  return UpdateCpuShares(base::FilePath(kPluginVmCpuCgroup), cpu_shares);
}

bool PluginVm::CreateUsbListeningSocket() {
  usb_listen_fd_ = CreateUnixSocket(runtime_dir_.GetPath().Append("usb.sock"),
                                    SOCK_SEQPACKET);
  if (!usb_listen_fd_.is_valid()) {
    return false;
  }

  // Start listening for connections. We only expect one client at a time.
  if (listen(usb_listen_fd_.get(), 1) < 0) {
    PLOG(ERROR) << "Unable to listen for connections on USB socket";
    return false;
  }

  usb_listen_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      usb_listen_fd_.get(),
      base::BindRepeating(&PluginVm::OnListenFileCanReadWithoutBlocking,
                          base::Unretained(this)));
  if (!usb_listen_watcher_) {
    LOG(ERROR) << "Failed to watch USB listening socket";
    return false;
  }

  return true;
}

bool PluginVm::AttachUsbDevice(uint8_t bus,
                               uint8_t addr,
                               uint16_t vid,
                               uint16_t pid,
                               int fd,
                               UsbControlResponse* response) {
  base::ScopedFD dup_fd(dup(fd));
  if (!dup_fd.is_valid()) {
    PLOG(ERROR) << "Unable to duplicate incoming file descriptor";
    return false;
  }

  if (usb_vm_fd_.is_valid() && usb_req_waiting_xmit_.empty()) {
    usb_listen_watcher_.reset();
    usb_vm_read_watcher_ = base::FileDescriptorWatcher::WatchReadable(
        usb_vm_fd_.get(),
        base::BindRepeating(&PluginVm::OnVmFileCanReadWithoutBlocking,
                            base::Unretained(this)));
    usb_vm_write_watcher_ = base::FileDescriptorWatcher::WatchWritable(
        usb_vm_fd_.get(),
        base::BindRepeating(&PluginVm::OnVmFileCanWriteWithoutBlocking,
                            base::Unretained(this)));
    if (!usb_vm_read_watcher_ || !usb_vm_write_watcher_) {
      LOG(ERROR) << "Failed to start watching USB VM socket";
      usb_vm_read_watcher_.reset();
      usb_vm_write_watcher_.reset();
      return false;
    }
  }

  UsbCtrlRequest req{};
  req.type = ATTACH_DEVICE;
  req.handle = ++usb_last_handle_;
  req.DevInfo.bus = bus;
  req.DevInfo.addr = addr;
  req.DevInfo.vid = vid;
  req.DevInfo.pid = pid;
  usb_req_waiting_xmit_.emplace_back(std::move(req), std::move(dup_fd));

  // TODO(dtor): report status only when plugin responds; requires changes to
  // the VM interface API.
  response->type = UsbControlResponseType::OK;
  response->port = usb_last_handle_;
  return true;
}

bool PluginVm::DetachUsbDevice(uint8_t port, UsbControlResponse* response) {
  auto iter = std::find_if(
      usb_devices_.begin(), usb_devices_.end(),
      [port](const auto& info) { return std::get<2>(info) == port; });
  if (iter == usb_devices_.end()) {
    response->type = UsbControlResponseType::NO_SUCH_PORT;
    return true;
  }

  if (usb_vm_fd_.is_valid() && usb_req_waiting_xmit_.empty()) {
    usb_listen_watcher_.reset();
    usb_vm_read_watcher_ = base::FileDescriptorWatcher::WatchReadable(
        usb_vm_fd_.get(),
        base::BindRepeating(&PluginVm::OnVmFileCanReadWithoutBlocking,
                            base::Unretained(this)));
    usb_vm_write_watcher_ = base::FileDescriptorWatcher::WatchWritable(
        usb_vm_fd_.get(),
        base::BindRepeating(&PluginVm::OnVmFileCanWriteWithoutBlocking,
                            base::Unretained(this)));
    if (!usb_vm_read_watcher_ || !usb_vm_write_watcher_) {
      LOG(ERROR) << "Failed to start watching USB VM socket";
      usb_vm_read_watcher_.reset();
      usb_vm_write_watcher_.reset();
      return false;
    }
  }

  UsbCtrlRequest req{};
  req.type = DETACH_DEVICE;
  req.handle = port;
  usb_req_waiting_xmit_.emplace_back(req, base::ScopedFD());

  // TODO(dtor): report status only when plugin responds; requires changes to
  // the VM interface API.
  response->type = UsbControlResponseType::OK;
  response->port = port;
  return true;
}

bool PluginVm::ListUsbDevice(std::vector<UsbDevice>* device) {
  device->resize(usb_devices_.size());
  std::transform(usb_devices_.begin(), usb_devices_.end(), device->begin(),
                 [](const auto& info) {
                   UsbDevice d{};
                   std::tie(d.vid, d.pid, d.port) = info;
                   return d;
                 });
  return true;
}

void PluginVm::HandleUsbControlResponse() {
  UsbCtrlResponse resp;
  int ret = HANDLE_EINTR(read(usb_vm_fd_.get(), &resp, sizeof(resp)));
  if (ret <= 0) {
    // If we get 0 bytes from read() that means that the other side closed
    // the connection. Disconnect the socket and wait for new connection.
    // Do the same if we get any error besides EAGAIN.
    if (ret == 0 || errno != EAGAIN) {
      usb_vm_read_watcher_.reset();
      usb_vm_write_watcher_.reset();
      usb_vm_fd_.reset();

      usb_listen_watcher_ = base::FileDescriptorWatcher::WatchReadable(
          usb_listen_fd_.get(),
          base::BindRepeating(&PluginVm::OnListenFileCanReadWithoutBlocking,
                              base::Unretained(this)));
      if (!usb_listen_watcher_) {
        LOG(ERROR) << "Failed to restart watching USB listening socket";
      }
    }
  } else if (ret != sizeof(resp)) {
    LOG(ERROR) << "Partial read of " << ret
               << " from USB VM socket, discarding";
  } else {
    auto req_iter = std::find_if(
        usb_req_waiting_response_.begin(), usb_req_waiting_response_.end(),
        [&resp](const auto& req) {
          return resp.type == req.type && resp.handle == req.handle;
        });
    if (req_iter == usb_req_waiting_response_.end()) {
      LOG(ERROR) << "Unexpected response (type " << resp.type << ", handle "
                 << resp.handle << ")";
      return;
    }

    if (resp.status != UsbCtrlResponse::Status::OK) {
      LOG(ERROR) << "Request (type " << resp.type << ", handle " << resp.handle
                 << ") failed: " << resp.status;
    }

    switch (req_iter->type) {
      case ATTACH_DEVICE:
        if (resp.status == UsbCtrlResponse::Status::OK) {
          usb_devices_.emplace_back(req_iter->DevInfo.vid,
                                    req_iter->DevInfo.pid, req_iter->handle);
        }
        break;
      case DETACH_DEVICE: {
        // We will attempt to clean up even if plugin signalled error as there
        // is no way the device will continue be operational.
        auto dev_iter = std::find_if(usb_devices_.begin(), usb_devices_.end(),
                                     [&resp](const auto& info) {
                                       return std::get<2>(info) == resp.handle;
                                     });
        if (dev_iter == usb_devices_.end()) {
          LOG(ERROR) << "Received detach response for unknown handle "
                     << resp.handle;
        }
      } break;
      default:
        NOTREACHED();
    }

    usb_req_waiting_response_.erase(req_iter);
  }
}

void PluginVm::OnListenFileCanReadWithoutBlocking() {
  int ret = HANDLE_EINTR(accept4(usb_listen_fd_.get(), nullptr, nullptr,
                                 SOCK_CLOEXEC | SOCK_NONBLOCK));
  if (ret < 0) {
    PLOG(ERROR) << "Unable to accept connection on USB listening socket";
    return;
  }

  // Start managing socket connected to the VM.
  usb_vm_fd_.reset(ret);

  // Switch watcher from listener FD to connected socket FD.
  // We monitor both writes and reads to detect disconnects.
  usb_listen_watcher_.reset();
  usb_vm_read_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      usb_vm_fd_.get(),
      base::BindRepeating(&PluginVm::OnVmFileCanReadWithoutBlocking,
                          base::Unretained(this)));
  if (!usb_req_waiting_xmit_.empty()) {
    usb_vm_write_watcher_ = base::FileDescriptorWatcher::WatchWritable(
        usb_vm_fd_.get(),
        base::BindRepeating(&PluginVm::OnVmFileCanWriteWithoutBlocking,
                            base::Unretained(this)));
  }
  if (!usb_vm_read_watcher_ ||
      (!usb_req_waiting_xmit_.empty() && !usb_vm_write_watcher_)) {
    LOG(ERROR) << "Failed to start watching USB VM socket";
    usb_vm_write_watcher_.reset();
    usb_vm_read_watcher_.reset();
    usb_vm_fd_.reset();
    return;
  }
}

void PluginVm::OnVmFileCanReadWithoutBlocking() {
  PluginVm::HandleUsbControlResponse();
}

void PluginVm::OnVmFileCanWriteWithoutBlocking() {
  DCHECK(usb_vm_fd_.is_valid());

  if (!usb_req_waiting_xmit_.empty()) {
    UsbCtrlRequest req = usb_req_waiting_xmit_.front().first;
    struct iovec io {};
    io.iov_base = &req;
    io.iov_len = sizeof(req);

    struct msghdr msg {};
    msg.msg_iov = &io;
    msg.msg_iovlen = 1;

    char buf[CMSG_SPACE(sizeof(int))];
    base::ScopedFD fd(std::move(usb_req_waiting_xmit_.front().second));
    if (fd.is_valid()) {
      msg.msg_control = buf;
      msg.msg_controllen = CMSG_LEN(sizeof(int));

      struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
      cmsg->cmsg_len = CMSG_LEN(sizeof(int));
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;

      *(reinterpret_cast<int*> CMSG_DATA(cmsg)) = fd.get();
    }

    int ret = HANDLE_EINTR(sendmsg(usb_vm_fd_.get(), &msg, MSG_EOR));
    if (ret == sizeof(req)) {
      usb_req_waiting_response_.emplace_back(req);
      usb_req_waiting_xmit_.pop_front();
    } else if (ret >= 0) {
      PLOG(ERROR) << "Partial write of " << ret
                  << " while sending USB request; will retry";
    } else if (errno != EINTR) {
      PLOG(ERROR) << "Failed to send USB request";
    }
  } else {
    usb_listen_watcher_.reset();
    usb_vm_write_watcher_.reset();

    usb_vm_read_watcher_ = base::FileDescriptorWatcher::WatchReadable(
        usb_vm_fd_.get(),
        base::BindRepeating(&PluginVm::OnVmFileCanReadWithoutBlocking,
                            base::Unretained(this)));
    if (!usb_vm_read_watcher_) {
      LOG(ERROR) << "Failed to switch to watching USB VM socket for reads";
    }
  }
}

// static
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

void PluginVm::VmToolsStateChanged(bool running) {
  LOG(INFO) << "Tools are " << (running ? "" : "not ")
            << "running in plugin VM";

  if (running)
    pvm::helper::CleanUpAfterInstall(id_, iso_dir_);
}

PluginVm::PluginVm(VmId id,
                   arc_networkd::MacAddress mac_addr,
                   std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr,
                   uint32_t ipv4_netmask,
                   uint32_t ipv4_gateway,
                   std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
                   dbus::ObjectProxy* vmplugin_service_proxy,
                   base::FilePath iso_dir,
                   base::FilePath root_dir,
                   base::FilePath runtime_dir)
    : id_(std::move(id)),
      iso_dir_(std::move(iso_dir)),
      mac_addr_(std::move(mac_addr)),
      ipv4_addr_(std::move(ipv4_addr)),
      netmask_(ipv4_netmask),
      gateway_(ipv4_gateway),
      seneschal_server_proxy_(std::move(seneschal_server_proxy)),
      vmplugin_service_proxy_(vmplugin_service_proxy),
      usb_last_handle_(0) {
  CHECK(ipv4_addr_);
  CHECK(vmplugin_service_proxy_);
  CHECK(base::DirectoryExists(iso_dir_));
  CHECK(base::DirectoryExists(root_dir));
  CHECK(base::DirectoryExists(runtime_dir));

  // Take ownership of the root and runtime directories.
  CHECK(root_dir_.Set(root_dir));
  CHECK(runtime_dir_.Set(runtime_dir));
}

PluginVm::PluginVm(VmId id,
                   arc_networkd::MacAddress mac_addr,
                   std::unique_ptr<arc_networkd::Subnet> ipv4_subnet,
                   std::unique_ptr<arc_networkd::SubnetAddress> ipv4_gw,
                   std::unique_ptr<arc_networkd::SubnetAddress> ipv4_addr,
                   std::unique_ptr<SeneschalServerProxy> seneschal_server_proxy,
                   dbus::ObjectProxy* vmplugin_service_proxy,
                   base::FilePath iso_dir,
                   base::FilePath root_dir,
                   base::FilePath runtime_dir)
    : id_(std::move(id)),
      iso_dir_(std::move(iso_dir)),
      mac_addr_(std::move(mac_addr)),
      ipv4_subnet_(std::move(ipv4_subnet)),
      ipv4_gw_(std::move(ipv4_gw)),
      ipv4_addr_(std::move(ipv4_addr)),
      seneschal_server_proxy_(std::move(seneschal_server_proxy)),
      vmplugin_service_proxy_(vmplugin_service_proxy),
      usb_last_handle_(0) {
  CHECK(ipv4_subnet_);
  CHECK(ipv4_gw_);
  CHECK(ipv4_addr_);
  CHECK(vmplugin_service_proxy_);
  CHECK(base::DirectoryExists(iso_dir_));
  CHECK(base::DirectoryExists(root_dir));
  CHECK(base::DirectoryExists(runtime_dir));

  // Take ownership of the root and runtime directories.
  CHECK(root_dir_.Set(root_dir));
  CHECK(runtime_dir_.Set(runtime_dir));

  gateway_ = ipv4_gw_->Address();
  netmask_ = ipv4_subnet_->Netmask();
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

  auto bin_dir = pvm::helper::IsDlcVm() ? kDlcPluginBinDir : kPluginBinDir;
  auto plugin_bin_path = base::FilePath(bin_dir).Append(kPluginBinName);
  // Build up the process arguments.
  // clang-format off
  std::vector<string> args = {
    kCrosvmBin,                 "run",
    "--cpus",                   std::to_string(cpus),
    "--tap-fd",                 std::to_string(tap_fd.get()),
    "--plugin",                 plugin_bin_path.value(),
    "--plugin-gid-map-file",    plugin_bin_path
                                    .AddExtension("gid_maps")
                                    .value(),
  };
  // clang-format on

  // These are bind mounts with parts may change (i.e. they are either VM
  // or config specific).
  std::vector<string> bind_mounts = {
      base::StringPrintf("%s:%s:false", bin_dir, kPluginBinDir),
      // This is directory where the VM image resides.
      base::StringPrintf("%s:%s:true", stateful_dir.value().c_str(),
                         kStatefulDir),
      // This is directory where ISO images for the VM reside.
      base::StringPrintf("%s:%s:false", iso_dir_.value().c_str(), kIsoDir),
      // This is directory where control socket, 9p socket, and other axillary
      // runtime data lives.
      base::StringPrintf("%s:%s:true", runtime_dir_.GetPath().value().c_str(),
                         kRuntimeDir),
      // Plugin '/etc' directory.
      base::StringPrintf("%s:%s:true",
                         root_dir_.GetPath().Append("etc").value().c_str(),
                         "/etc"),
  };

  // TODO(kimjae): This is a temporary hack to have relative files to be found
  // even when started from DLC paths. Clean this up once a cleaner solution can
  // be leveraged.
  if (pvm::helper::IsDlcVm())
    bind_mounts.push_back(base::StringPrintf("%s:%s:false", bin_dir, bin_dir));

  // Put everything into the brillo::ProcessImpl.
  for (auto& arg : args) {
    process_.AddArg(std::move(arg));
  }

  for (auto& mount : bind_mounts) {
    process_.AddArg("--plugin-mount");
    process_.AddArg(std::move(mount));
  }

  // Because some of the static paths are mounted in /run/pvm... in the
  // plugin jail, they have to come after the dynamic paths above.
  process_.AddArg("--plugin-mount-file");
  process_.AddArg(plugin_bin_path.AddExtension("bind_mounts").value());

  for (auto& param : params) {
    // Because additional parameters may start with a '--', we should use
    // --params=<Param> instead of --params <Param> to make explicit <Param>
    // is a parameter for the plugin rather than just another parameter to
    // the crosvm process.
    process_.AddArg(std::string("--params=") + param);
  }

  // Change the process group before exec so that crosvm sending SIGKILL to the
  // whole process group doesn't kill us as well. The function also changes the
  // cpu cgroup for PluginVm crosvm processes.
  process_.SetPreExecCallback(base::Bind(
      &SetUpCrosvmProcess, base::FilePath(kPluginVmCpuCgroup).Append("tasks")));

  if (!process_.Start()) {
    LOG(ERROR) << "Failed to start VM process";
    return false;
  }

  return true;
}

}  // namespace concierge
}  // namespace vm_tools
