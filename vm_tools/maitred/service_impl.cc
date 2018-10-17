// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/maitred/service_impl.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/vm_sockets.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/posix/safe_strerror.h>
#include <base/process/launch.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>

using std::string;

namespace vm_tools {
namespace maitred {
namespace {

// Default name of the interface in the VM.
constexpr char kInterfaceName[] = "eth0";
constexpr char kLoopbackName[] = "lo";

constexpr char kHostIpPath[] = "/run/host_ip";

constexpr char kResolvConfOptions[] =
    "options single-request timeout:1 attempts:5\n";
constexpr char kResolvConfPath[] = "/run/resolv.conf";
constexpr char kRunPath[] = "/run";
constexpr char kTmpResolvConfPath[] = "/run/resolv.conf.tmp";

// How long to wait before timing out on `lxd waitready`.
constexpr int kLxdWaitreadyTimeoutSeconds = 120;

// Common environment for all LXD functionality.
const std::map<string, string> kLxdEnv = {
    {"LXD_DIR", "/mnt/stateful/lxd"},
    {"LXD_CONF", "/mnt/stateful/lxd_conf"},
    {"LXD_UNPRIVILEGED_ONLY", "true"},
};

// Convert a 32-bit int in network byte order into a printable string.
string AddressToString(uint32_t address) {
  struct in_addr in = {
      .s_addr = address,
  };
  char buf[INET_ADDRSTRLEN];
  if (inet_ntop(AF_INET, &in, buf, INET_ADDRSTRLEN) == nullptr) {
    PLOG(ERROR) << "Failed to parse address " << address;
    return string("<unknown>");
  }

  return string(buf);
}

// Set a network interface's flags to be up and running. Returns 0 on success,
// or the saved errno otherwise.
int EnableInterface(int sockfd, const char* ifname) {
  struct ifreq ifr;
  int ret;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

  ret = HANDLE_EINTR(ioctl(sockfd, SIOCGIFFLAGS, &ifr));
  if (ret) {
    int saved_errno = errno;
    PLOG(ERROR) << "Failed to fetch flags for interface " << ifname;
    return saved_errno;
  }

  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
  ret = HANDLE_EINTR(ioctl(sockfd, SIOCSIFFLAGS, &ifr));
  if (ret) {
    int saved_errno = errno;
    PLOG(ERROR) << "Failed to set flags for interface " << ifname;
    return saved_errno;
  }

  return 0;
}

// Prints an error log with the message in |error| concatenated with the
// string representation of the current value of errno. The same error message
// will also be stored in |out_error|.
void PLogAndSaveError(const string& error, string* out_error) {
  string error_with_strerror = error + ": " + base::safe_strerror(errno);
  LOG(ERROR) << error_with_strerror;
  out_error->assign(error_with_strerror);
}

// Writes a resolv.conf with the supplied |nameservers| and |search_domains|.
// The default Chrome OS resolver options will be used. Returns true on
// success, and returns false on failure with an error message stored in
// |out_error|.
bool WriteResolvConf(const std::vector<string> nameservers,
                     const std::vector<string> search_domains,
                     string* out_error) {
  DCHECK(out_error);

  base::ScopedFD resolv_fd(
      HANDLE_EINTR(open(kRunPath, O_TMPFILE | O_WRONLY | O_CLOEXEC, 0644)));
  if (!resolv_fd.is_valid()) {
    PLogAndSaveError(
        base::StringPrintf("failed to open tmpfile in %s", kRunPath),
        out_error);
    return false;
  }

  for (auto& ns : nameservers) {
    string nameserver_line = base::StringPrintf("nameserver %s\n", ns.c_str());
    if (!base::WriteFileDescriptor(resolv_fd.get(), nameserver_line.c_str(),
                                   nameserver_line.length())) {
      PLogAndSaveError("failed to write nameserver to tmpfile", out_error);
      return false;
    }
  }

  if (!search_domains.empty()) {
    string search_domains_line = base::StringPrintf(
        "search %s\n", base::JoinString(search_domains, " ").c_str());
    if (!base::WriteFileDescriptor(resolv_fd.get(), search_domains_line.c_str(),
                                   search_domains_line.length())) {
      PLogAndSaveError("failed to write search domains to tmpfile", out_error);
      return false;
    }
  }

  if (!base::WriteFileDescriptor(resolv_fd.get(), kResolvConfOptions,
                                 strlen(kResolvConfOptions))) {
    PLogAndSaveError("failed to write resolver options to tmpfile", out_error);
    return false;
  }

  // The file has been successfully written to, so link it into place.
  // First link it to a named file with linkat(2), then atomically move it
  // into place with rename(2). linkat(2) will not overwrite the destination,
  // hence the need to do this in two steps.
  const base::FilePath source_path(
      base::StringPrintf("/proc/self/fd/%d", resolv_fd.get()));
  if (HANDLE_EINTR(linkat(AT_FDCWD, source_path.value().c_str(), AT_FDCWD,
                          kTmpResolvConfPath, AT_SYMLINK_FOLLOW)) < 0) {
    PLogAndSaveError(
        base::StringPrintf("failed to link tmpfile to %s", kTmpResolvConfPath),
        out_error);
    return false;
  }

  if (HANDLE_EINTR(rename(kTmpResolvConfPath, kResolvConfPath)) < 0) {
    PLogAndSaveError(
        base::StringPrintf("failed to rename tmpfile to %s", kResolvConfPath),
        out_error);
    return false;
  }

  return true;
}

}  // namespace

ServiceImpl::ServiceImpl(std::unique_ptr<vm_tools::maitred::Init> init)
    : init_(std::move(init)) {}

bool ServiceImpl::Init() {
  std::vector<string> default_nameservers{"8.8.8.8", "8.8.4.4"};
  string error;

  return WriteResolvConf(std::move(default_nameservers), {}, &error);
}

grpc::Status ServiceImpl::ConfigureNetwork(grpc::ServerContext* ctx,
                                           const NetworkConfigRequest* request,
                                           EmptyMessage* response) {
  static_assert(sizeof(uint32_t) == sizeof(in_addr_t),
                "in_addr_t is not the same width as uint32_t");
  LOG(INFO) << "Received network configuration request";

  const IPv4Config& ipv4_config = request->ipv4_config();
  if (ipv4_config.address() == 0) {
    return grpc::Status(grpc::INVALID_ARGUMENT, "IPv4 address cannot be 0");
  }
  if (ipv4_config.netmask() == 0) {
    return grpc::Status(grpc::INVALID_ARGUMENT, "IPv4 netmask cannot be 0");
  }
  if (ipv4_config.gateway() == 0) {
    return grpc::Status(grpc::INVALID_ARGUMENT, "IPv4 gateway cannot be 0");
  }

  base::ScopedFD fd(socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0));
  if (!fd.is_valid()) {
    int saved_errno = errno;
    PLOG(ERROR) << "Failed to create socket";
    return grpc::Status(grpc::INTERNAL, string("failed to create socket: ") +
                                            strerror(saved_errno));
  }

  // Set up the address.
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, kInterfaceName, sizeof(ifr.ifr_name));

  // Holy fuck, who designed this interface?  Did you know that ifr_addr and
  // ifr_name are actually macros?!?  For example, ifr_addr expands to
  // ifr_ifru.ifru_addr and ifr_name expands to ifr_ifrn.ifrn_name.  This is
  // because the address, the flags, the netmask, and basically everything
  // else all share the same underlying storage via a union.  "Let's just put
  // everything into one union.  Who needs type safety anyway?". smh.
  struct sockaddr_in* addr =
      reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = static_cast<in_addr_t>(ipv4_config.address());

  if (HANDLE_EINTR(ioctl(fd.get(), SIOCSIFADDR, &ifr)) != 0) {
    int saved_errno = errno;
    PLOG(ERROR) << "Failed to set IPv4 address for interface " << kInterfaceName
                << " to " << AddressToString(ipv4_config.address());
    return grpc::Status(grpc::INTERNAL, string("failed to set IPv4 address: ") +
                                            strerror(saved_errno));
  }

  LOG(INFO) << "Set IPv4 address for interface " << kInterfaceName << " to "
            << AddressToString(ipv4_config.address());

  // Set the netmask.
  struct sockaddr_in* netmask =
      reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_netmask);
  netmask->sin_family = AF_INET;
  netmask->sin_addr.s_addr = static_cast<in_addr_t>(ipv4_config.netmask());

  if (HANDLE_EINTR(ioctl(fd.get(), SIOCSIFNETMASK, &ifr)) != 0) {
    int saved_errno = errno;
    PLOG(ERROR) << "Failed to set IPv4 netmask for interface " << kInterfaceName
                << " to " << AddressToString(ipv4_config.netmask());
    return grpc::Status(grpc::INTERNAL, string("failed to set IPv4 netmask: ") +
                                            strerror(saved_errno));
  }

  LOG(INFO) << "Set IPv4 netmask for interface " << kInterfaceName << " to "
            << AddressToString(ipv4_config.netmask());

  // Set the interface up and running.  This needs to happen before the kernel
  // will let us set the gateway.
  int ret = EnableInterface(fd.get(), kInterfaceName);
  if (ret) {
    return grpc::Status(
        grpc::INTERNAL,
        string("failed to enable network interface: ") + strerror(ret));
  }
  LOG(INFO) << "Set interface " << kInterfaceName << " up and running";

  // Bring up the loopback interface too.
  ret = EnableInterface(fd.get(), kLoopbackName);
  if (ret) {
    return grpc::Status(
        grpc::INTERNAL,
        string("failed to enable loopback interface") + strerror(ret));
  }

  // Set the gateway.
  struct rtentry route;
  memset(&route, 0, sizeof(route));

  struct sockaddr_in* gateway =
      reinterpret_cast<struct sockaddr_in*>(&route.rt_gateway);
  gateway->sin_family = AF_INET;
  gateway->sin_addr.s_addr = static_cast<in_addr_t>(ipv4_config.gateway());

  struct sockaddr_in* dst =
      reinterpret_cast<struct sockaddr_in*>(&route.rt_dst);
  dst->sin_family = AF_INET;
  dst->sin_addr.s_addr = INADDR_ANY;

  struct sockaddr_in* genmask =
      reinterpret_cast<struct sockaddr_in*>(&route.rt_genmask);
  genmask->sin_family = AF_INET;
  genmask->sin_addr.s_addr = INADDR_ANY;

  route.rt_flags = RTF_UP | RTF_GATEWAY;

  string gateway_str = AddressToString(ipv4_config.gateway());
  if (HANDLE_EINTR(ioctl(fd.get(), SIOCADDRT, &route)) != 0) {
    int saved_errno = errno;
    PLOG(ERROR) << "Failed to set default IPv4 gateway for interface "
                << kInterfaceName << " to " << gateway_str;
    return grpc::Status(grpc::INTERNAL, string("failed to set IPv4 gateway: ") +
                                            strerror(saved_errno));
  }

  LOG(INFO) << "Set default IPv4 gateway for interface " << kInterfaceName
            << " to " << gateway_str;

  // Write the host IP address to a file for LXD containers to use.
  base::FilePath host_ip_path(kHostIpPath);
  size_t gateway_str_len = gateway_str.size();
  if (base::WriteFile(host_ip_path, gateway_str.c_str(), gateway_str_len) !=
      gateway_str_len) {
    LOG(ERROR) << "Failed to write host IPv4 address to file";
    return grpc::Status(grpc::INTERNAL, "failed to write host IPv4 address");
  }

  if (!base::SetPosixFilePermissions(host_ip_path, 0644)) {
    LOG(ERROR) << "Failed to set host IPv4 address file permissions";
    return grpc::Status(grpc::INTERNAL,
                        "failed to set host IPv4 address permissions");
  }

  return grpc::Status::OK;
}

grpc::Status ServiceImpl::Shutdown(grpc::ServerContext* ctx,
                                   const EmptyMessage* request,
                                   EmptyMessage* response) {
  LOG(INFO) << "Received shutdown request";

  if (!init_) {
    return grpc::Status(grpc::FAILED_PRECONDITION, "not running as init");
  }

  init_->Shutdown();

  shutdown_cb_.Run();

  return grpc::Status::OK;
}

grpc::Status ServiceImpl::LaunchProcess(
    grpc::ServerContext* ctx,
    const vm_tools::LaunchProcessRequest* request,
    vm_tools::LaunchProcessResponse* response) {
  LOG(INFO) << "Received request to launch process";
  if (!init_) {
    return grpc::Status(grpc::FAILED_PRECONDITION, "not running as init");
  }

  if (request->argv_size() <= 0) {
    return grpc::Status(grpc::INVALID_ARGUMENT, "missing argv");
  }

  if (request->respawn() && request->wait_for_exit()) {
    return grpc::Status(grpc::INVALID_ARGUMENT,
                        "respawn and wait_for_exit cannot both be true");
  }

  std::vector<string> argv(request->argv().begin(), request->argv().end());
  std::map<string, string> env;
  for (const auto& pair : request->env()) {
    env[pair.first] = pair.second;
  }

  Init::ProcessLaunchInfo launch_info;
  if (!init_->Spawn(std::move(argv), std::move(env), request->respawn(),
                    request->use_console(), request->wait_for_exit(),
                    &launch_info)) {
    return grpc::Status(grpc::INTERNAL, "failed to spawn process");
  }

  switch (launch_info.status) {
    case Init::ProcessStatus::UNKNOWN:
      LOG(WARNING) << "Child process has unknown status";

      response->set_status(vm_tools::UNKNOWN);
      break;
    case Init::ProcessStatus::EXITED:
      LOG(INFO) << "Requested process " << request->argv()[0] << " exited with "
                << "status " << launch_info.code;

      response->set_status(vm_tools::EXITED);
      response->set_code(launch_info.code);
      break;
    case Init::ProcessStatus::SIGNALED:
      LOG(INFO) << "Requested process " << request->argv()[0] << " killed by "
                << "signal " << launch_info.code;

      response->set_status(vm_tools::SIGNALED);
      response->set_code(launch_info.code);
      break;
    case Init::ProcessStatus::LAUNCHED:
      LOG(INFO) << "Launched process " << request->argv()[0];

      response->set_status(vm_tools::LAUNCHED);
      break;
    case Init::ProcessStatus::FAILED:
      LOG(ERROR) << "Failed to launch requested process";

      response->set_status(vm_tools::FAILED);
      break;
  }

  // Return OK no matter what because the RPC itself succeeded even if there
  // was an issue with launching the process.
  return grpc::Status::OK;
}

grpc::Status ServiceImpl::Mount(grpc::ServerContext* ctx,
                                const MountRequest* request,
                                MountResponse* response) {
  LOG(INFO) << "Received mount request";
  int ret = mount(request->source().c_str(), request->target().c_str(),
                  request->fstype().c_str(), request->mountflags(),
                  request->options().c_str());

  if (ret < 0) {
    response->set_error(errno);
    PLOG(ERROR) << "Failed to mount \"" << request->source() << "\" on \""
                << request->target() << "\"";
  } else {
    response->set_error(0);
    LOG(INFO) << "Mounted \"" << request->source() << "\" on \""
              << request->target() << "\"";
  }

  return grpc::Status::OK;
}

grpc::Status ServiceImpl::StartTermina(grpc::ServerContext* ctx,
                                       const StartTerminaRequest* request,
                                       StartTerminaResponse* response) {
  LOG(INFO) << "Received StartTermina request";
  if (!init_) {
    return grpc::Status(grpc::FAILED_PRECONDITION, "not running as init");
  }

  Init::ProcessLaunchInfo launch_info;
  if (!init_->Spawn({"mkfs.btrfs", "/dev/vdb"}, kLxdEnv, false /*respawn*/,
                    false /*use_console*/, true /*wait_for_exit*/,
                    &launch_info)) {
    return grpc::Status(grpc::INTERNAL, "failed to spawn mkfs.btrfs");
  }
  if (launch_info.status != Init::ProcessStatus::EXITED) {
    return grpc::Status(grpc::INTERNAL, "mkfs.btrfs did not complete");
  }
  // mkfs.btrfs will fail if the disk is already formatted as btrfs.
  // Optimistically continue on - if the mount fails, then return an error.

  int ret = mount("/dev/vdb", "/mnt/stateful", "btrfs", 0,
                  "user_subvol_rm_allowed,discard");
  if (ret != 0) {
    int saved_errno = errno;
    PLOG(ERROR) << "Failed to mount stateful disk";
    return grpc::Status(grpc::INTERNAL, string("failed to mount stateful: ") +
                                            strerror(saved_errno));
  }

  if (!init_->Spawn({"lxd", "--group", "lxd", "--syslog"}, kLxdEnv,
                    true /*respawn*/, false /*use_console*/,
                    false /*wait_for_exit*/, &launch_info)) {
    return grpc::Status(grpc::INTERNAL, "failed to spawn lxd");
  }
  if (launch_info.status != Init::ProcessStatus::LAUNCHED) {
    return grpc::Status(grpc::INTERNAL, "lxd did not launch");
  }

  string timeout = std::to_string(kLxdWaitreadyTimeoutSeconds);
  if (!init_->Spawn({"lxd", "waitready", "--timeout", timeout}, kLxdEnv,
                    false /*respawn*/, false /*use_console*/,
                    true /*wait_for_exit*/, &launch_info)) {
    return grpc::Status(grpc::INTERNAL, "failed to spawn lxd waitready");
  }
  if (launch_info.status != Init::ProcessStatus::EXITED) {
    return grpc::Status(grpc::INTERNAL, "lxd waitready did not complete");
  } else if (launch_info.code != 0) {
    return grpc::Status(grpc::INTERNAL, "lxd waitready returned non-zero");
  }

  if (!init_->Spawn({"tremplin", "-lxd_subnet", request->lxd_ipv4_subnet()},
                    kLxdEnv, true /*respawn*/, false /*use_console*/,
                    false /*wait_for_exit*/, &launch_info)) {
    return grpc::Status(grpc::INTERNAL, "failed to spawn tremplin");
  }
  if (launch_info.status != Init::ProcessStatus::LAUNCHED) {
    return grpc::Status(grpc::INTERNAL, "tremplin did not launch");
  }

  return grpc::Status::OK;
}

grpc::Status ServiceImpl::Mount9P(grpc::ServerContext* ctx,
                                  const Mount9PRequest* request,
                                  MountResponse* response) {
  LOG(INFO) << "Received request to mount 9P file system";
  base::ScopedFD server(socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!server.is_valid()) {
    response->set_error(errno);
    PLOG(ERROR) << "Failed to create vsock socket";
    return grpc::Status(grpc::INTERNAL, "unable to create vsock socket");
  }

  struct sockaddr_vm svm = {
      .svm_family = AF_VSOCK,
      .svm_cid = VMADDR_CID_HOST,
      .svm_port = static_cast<unsigned int>(request->port()),
  };
  if (connect(server.get(), reinterpret_cast<struct sockaddr*>(&svm),
              sizeof(svm)) != 0) {
    response->set_error(errno);
    PLOG(ERROR) << "Unable to connect to server";
    return grpc::Status(grpc::INTERNAL, "unable to connect to server");
  }

  // Do the mount.
  string data = base::StringPrintf(
      "trans=fd,rfdno=%d,wfdno=%d,cache=none,access=any,version=9p2000.L",
      server.get(), server.get());
  if (mount("9p", request->target().c_str(), "9p",
            MS_NOSUID | MS_NODEV | MS_NOEXEC, data.c_str()) != 0) {
    response->set_error(errno);
    PLOG(ERROR) << "Failed to mount 9p file system";
    return grpc::Status(grpc::INTERNAL, "failed to mount file system");
  }

  LOG(INFO) << "Mounted 9P file system on " << request->target();
  return grpc::Status::OK;
}

grpc::Status ServiceImpl::SetResolvConfig(grpc::ServerContext* ctx,
                                          const SetResolvConfigRequest* request,
                                          EmptyMessage* response) {
  LOG(INFO) << "Received request to update VM resolv.conf";
  const vm_tools::ResolvConfig& resolv_config = request->resolv_config();

  std::vector<string> nameservers(resolv_config.nameservers().begin(),
                                  resolv_config.nameservers().end());
  std::vector<string> search_domains(resolv_config.search_domains().begin(),
                                     resolv_config.search_domains().end());
  string error;
  if (!WriteResolvConf(nameservers, search_domains, &error)) {
    return grpc::Status(grpc::INTERNAL, error);
  }

  return grpc::Status::OK;
}

grpc::Status ServiceImpl::SetTime(grpc::ServerContext* ctx,
                                  const vm_tools::SetTimeRequest* request,
                                  EmptyMessage* response) {
  struct timeval new_time;
  new_time.tv_sec = request->time().seconds();
  new_time.tv_usec = request->time().nanos() / 1000;

  LOG(INFO) << "Recieved request to set time to " << new_time.tv_sec << "s, "
            << new_time.tv_usec << "us";

  if (new_time.tv_sec == 0) {
    LOG(ERROR) << "Ignored attempt to set time to the epoch";

    return grpc::Status(grpc::INVALID_ARGUMENT,
                        "ignored attempt to set time to the epoch");
  }

  if (settimeofday(&new_time, /*tz=*/nullptr) < 0) {
    string error = strerror(errno);
    LOG(ERROR) << "Failed to set time: " << error;
    return grpc::Status(
        grpc::INTERNAL,
        base::StringPrintf("failed to set time: %s", error.c_str()));
  }

  LOG(INFO) << "Successfully set time.";
  return grpc::Status::OK;
}

}  // namespace maitred
}  // namespace vm_tools
