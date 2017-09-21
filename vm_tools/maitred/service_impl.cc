// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/maitred/service_impl.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/socket.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/posix/eintr_wrapper.h>
#include <base/process/launch.h>

using std::string;

namespace vm_tools {
namespace maitred {
namespace {

// Default name of the interface in the VM.
constexpr char kInterfaceName[] = "eth0";

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

}  // namespace

ServiceImpl::ServiceImpl(std::unique_ptr<Init> init) : init_(std::move(init)) {}

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
  if (HANDLE_EINTR(ioctl(fd.get(), SIOCGIFFLAGS, &ifr)) != 0) {
    int saved_errno = errno;
    PLOG(ERROR) << "Failed to fetch flags for interface " << kInterfaceName;
    return grpc::Status(grpc::INTERNAL, string("failed to fetch flags: ") +
                                            strerror(saved_errno));
  }

  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
  if (HANDLE_EINTR(ioctl(fd.get(), SIOCSIFFLAGS, &ifr)) != 0) {
    int saved_errno = errno;
    PLOG(ERROR) << "Failed to set flags for interface " << kInterfaceName;
    return grpc::Status(grpc::INTERNAL, string("failed to set flags: ") +
                                            strerror(saved_errno));
  }

  LOG(INFO) << "Set interface " << kInterfaceName << " up and running";

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

  if (HANDLE_EINTR(ioctl(fd.get(), SIOCADDRT, &route)) != 0) {
    int saved_errno = errno;
    PLOG(ERROR) << "Failed to set default IPv4 gateway for interface "
                << kInterfaceName << " to "
                << AddressToString(ipv4_config.gateway());
    return grpc::Status(grpc::INTERNAL, string("failed to set IPv4 gateway: ") +
                                            strerror(saved_errno));
  }

  LOG(INFO) << "Set default IPv4 gateway for interface " << kInterfaceName
            << " to " << AddressToString(ipv4_config.gateway());

  return grpc::Status::OK;
}

grpc::Status ServiceImpl::Shutdown(grpc::ServerContext* ctx,
                                   const EmptyMessage* request,
                                   EmptyMessage* response) {
  // TODO(chirantan): Give applications a chance to clean up.
  reboot(RB_AUTOBOOT);
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
  int ret = mount(request->source().c_str(),
                  request->target().c_str(),
                  request->fstype().c_str(),
                  request->mountflags(),
                  request->options().c_str());

  if (ret < 0) {
    response->set_error(errno);
  } else {
    response->set_error(0);
  }

  return grpc::Status::OK;
}

}  // namespace maitred
}  // namespace vm_tools
