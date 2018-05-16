// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <sys/socket.h>

#include <linux/vm_sockets.h>  // Needs to come after sys/socket.h

#include <memory>
#include <string>

#include <base/at_exit.h>
#include <base/files/scoped_file.h>
#include <base/message_loop/message_loop.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_split.h>
#include <brillo/flag_helper.h>
#include <brillo/message_loops/base_message_loop.h>
#include <brillo/syslog_logging.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/message.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <vm_concierge/proto_bindings/service.pb.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/vsh/scoped_termios.h"
#include "vm_tools/vsh/utils.h"
#include "vm_tools/vsh/vsh_client.h"

using std::string;
using vm_tools::vsh::ScopedTermios;
using vm_tools::vsh::VshClient;

namespace {

constexpr int kDefaultTimeoutMs = 30 * 1000;

constexpr char kVshUsage[] =
    "vsh client\n"
    "Usage: vsh [flags] -- ENV1=VALUE1 ENV2=VALUE2 command arg1 arg2...";

// Connect to the supplied |bus| and return a dbus::ObjectProxy for
// the vm_concierge service.
dbus::ObjectProxy* GetConciergeProxy(const scoped_refptr<dbus::Bus>& bus) {
  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return nullptr;
  }

  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      vm_tools::concierge::kVmConciergeServiceName,
      dbus::ObjectPath(vm_tools::concierge::kVmConciergeServicePath));
  if (!proxy) {
    LOG(ERROR) << "Unable to get dbus proxy for "
               << vm_tools::concierge::kVmConciergeServiceName;
    return nullptr;
  }

  return proxy;
}

bool GetCid(dbus::ObjectProxy* concierge_proxy,
            const std::string& vm_name,
            unsigned int* cid) {
  dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                               vm_tools::concierge::kGetVmInfoMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::concierge::GetVmInfoRequest request;
  request.set_name(vm_name);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode GetVmInfo protobuf";
    return false;
  }

  std::unique_ptr<dbus::Response> dbus_response =
      concierge_proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send dbus message to concierge service";
    return false;
  }

  dbus::MessageReader reader(dbus_response.get());
  vm_tools::concierge::GetVmInfoResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed to parse response protobuf";
    return false;
  }

  if (!response.success()) {
    LOG(ERROR) << "Failed to get VM info for " << vm_name;
    return false;
  }

  *cid = response.vm_info().cid();
  return true;
}

bool LaunchVshd(dbus::ObjectProxy* concierge_proxy,
                const std::string& vm_name,
                const std::string& container_name,
                unsigned int port) {
  dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                               vm_tools::concierge::kLaunchVshdMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::concierge::LaunchVshdRequest request;
  request.set_vm_name(vm_name);
  request.set_container_name(container_name);
  request.set_port(port);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode LaunchVshdRequest protobuf";
    return false;
  }

  std::unique_ptr<dbus::Response> dbus_response =
      concierge_proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send dbus message to concierge service";
    return false;
  }

  dbus::MessageReader reader(dbus_response.get());
  vm_tools::concierge::LaunchVshdResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed to parse response protobuf";
    return false;
  }

  if (!response.success()) {
    LOG(ERROR) << "Failed to launch vshd for " << vm_name << ":"
               << container_name;
    return false;
  }

  return true;
}

bool ListenForVshd(dbus::ObjectProxy* concierge_proxy,
                   unsigned int port,
                   base::ScopedFD* peer_sock_fd,
                   const std::string& vm_name,
                   const std::string& container_name) {
  DCHECK(peer_sock_fd);

  // Create a socket to listen for incoming vsh connections.
  base::ScopedFD listen_fd(
      socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
  if (!listen_fd.is_valid()) {
    PLOG(ERROR) << "Failed to create socket";
    return false;
  }

  struct sockaddr_vm addr;
  memset(&addr, 0, sizeof(addr));
  addr.svm_family = AF_VSOCK;
  addr.svm_port = port;
  addr.svm_cid = VMADDR_CID_ANY;

  if (bind(listen_fd.get(), reinterpret_cast<const struct sockaddr*>(&addr),
           sizeof(addr)) < 0) {
    PLOG(ERROR) << "Failed to bind vsh port";
    return false;
  }

  socklen_t addr_len = sizeof(addr);
  if (getsockname(listen_fd.get(), reinterpret_cast<struct sockaddr*>(&addr),
                  &addr_len) < 0) {
    PLOG(ERROR) << "Failed to get bound vsh port";
    return false;
  }

  if (listen(listen_fd.get(), 1) < 0) {
    PLOG(ERROR) << "Failed to listen";
    return false;
  }

  // The socket is listening. Request that concierge start vshd.
  if (!LaunchVshd(concierge_proxy, vm_name, container_name, addr.svm_port))
    return false;

  struct pollfd pollfds[] = {
      {listen_fd.get(), POLLIN, 0},
  };
  const int num_pollfds = arraysize(pollfds);

  if (HANDLE_EINTR(poll(pollfds, num_pollfds, 5000)) < 0) {
    PLOG(ERROR) << "Failed to poll";
    return false;
  }

  struct sockaddr_vm peer_addr;
  socklen_t addr_size = sizeof(peer_addr);
  peer_sock_fd->reset(HANDLE_EINTR(
      accept4(listen_fd.get(), reinterpret_cast<struct sockaddr*>(&peer_addr),
              &addr_size, SOCK_CLOEXEC)));
  if (!peer_sock_fd->is_valid()) {
    PLOG(ERROR) << "Failed to accept connection from daemon";
    return false;
  }

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  brillo::InitLog(brillo::kLogToStderrIfTty);

  DEFINE_uint64(listen_port, VMADDR_PORT_ANY, "Port to listen on");
  DEFINE_uint64(cid, 0, "Cid of VM");
  DEFINE_string(vm_name, "", "Target VM name");
  DEFINE_string(user, "chronos", "Target user in the VM");
  DEFINE_string(target_container, "", "Target container");

  brillo::FlagHelper::Init(argc, argv, kVshUsage);

  brillo::BaseMessageLoop message_loop;
  message_loop.SetAsCurrent();
  std::unique_ptr<VshClient> client;

  dbus::Bus::Options opts;
  opts.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(std::move(opts)));

  if (FLAGS_listen_port != VMADDR_PORT_ANY || !FLAGS_target_container.empty()) {
    unsigned int port = 0;
    if (FLAGS_listen_port != 0) {
      port = static_cast<unsigned int>(FLAGS_listen_port);
      if (FLAGS_listen_port < 0 ||
          static_cast<uint64_t>(port) != FLAGS_listen_port) {
        LOG(ERROR) << "Port " << FLAGS_listen_port << " is not a valid port";
        return EXIT_FAILURE;
      }
    }

    dbus::ObjectProxy* proxy = GetConciergeProxy(bus);
    if (!proxy)
      return EXIT_FAILURE;

    base::ScopedFD sock_fd;
    if (!ListenForVshd(proxy, port, &sock_fd, FLAGS_vm_name,
                       FLAGS_target_container)) {
      return EXIT_FAILURE;
    }

    client = VshClient::Create(std::move(sock_fd), FLAGS_user,
                               FLAGS_target_container);

    if (!client) {
      return EXIT_FAILURE;
    }
  } else {
    if ((FLAGS_cid != 0 && !FLAGS_vm_name.empty()) ||
        (FLAGS_cid == 0 && FLAGS_vm_name.empty())) {
      LOG(ERROR) << "Exactly one of --cid or --vm_name is required";
      return EXIT_FAILURE;
    }
    unsigned int cid;
    if (FLAGS_cid != 0) {
      cid = FLAGS_cid;
      if (static_cast<uint64_t>(cid) != FLAGS_cid) {
        LOG(ERROR) << "Cid value (" << FLAGS_cid << ") is too large.  Largest "
                   << "valid value is "
                   << std::numeric_limits<unsigned int>::max();
        return EXIT_FAILURE;
      }
    } else {
      dbus::ObjectProxy* proxy = GetConciergeProxy(bus);
      if (!proxy)
        return EXIT_FAILURE;
      if (!GetCid(proxy, FLAGS_vm_name, &cid))
        return EXIT_FAILURE;
    }

    base::ScopedFD sock_fd(socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (!sock_fd.is_valid()) {
      PLOG(ERROR) << "Failed to open vsock socket";
      return EXIT_FAILURE;
    }

    struct sockaddr_vm addr;
    memset(&addr, 0, sizeof(addr));
    addr.svm_family = AF_VSOCK;
    addr.svm_port = vm_tools::kVshPort;
    addr.svm_cid = cid;

    if (HANDLE_EINTR(connect(sock_fd.get(),
                             reinterpret_cast<struct sockaddr*>(&addr),
                             sizeof(addr)) < 0)) {
      PLOG(ERROR) << "Failed to connect to vshd";
      return EXIT_FAILURE;
    }

    client = VshClient::Create(std::move(sock_fd), FLAGS_user,
                               FLAGS_target_container);
  }

  if (!client) {
    return EXIT_FAILURE;
  }

  base::ScopedFD ttyfd(
      HANDLE_EINTR(open(vm_tools::vsh::kDevTtyPath,
                        O_RDONLY | O_NOCTTY | O_CLOEXEC | O_NONBLOCK)));
  if (!ttyfd.is_valid()) {
    PLOG(ERROR) << "Failed to open /dev/tty";
    return EXIT_FAILURE;
  }

  // Set terminal to raw mode. Note that the client /must/ cleanly exit
  // the message loop below to restore termios settings.
  ScopedTermios termios(std::move(ttyfd));
  if (isatty(termios.GetRawFD()) &&
      !termios.SetTermiosMode(ScopedTermios::TermiosMode::RAW)) {
    return EXIT_FAILURE;
  }

  message_loop.Run();

  return client->exit_code();
}
