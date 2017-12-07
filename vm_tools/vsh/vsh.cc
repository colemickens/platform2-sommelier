// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <linux/vm_sockets.h>  // Needs to come after sys/socket.h

#include <memory>
#include <string>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/location.h>
#include <base/message_loop/message_loop.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_split.h>
#include <brillo/asynchronous_signal_handler.h>
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
#include "vsh.pb.h"  // NOLINT(build/include)

using std::string;
using vm_tools::vsh::RecvMessage;
using vm_tools::vsh::ScopedTermios;
using vm_tools::vsh::SendMessage;
using vm_tools::vsh::Shutdown;

namespace {

constexpr int kDefaultTimeoutMs = 30 * 1000;

constexpr char kVshUsage[] =
    "vsh client\n"
    "Usage: vsh [flags] -- ENV1=VALUE1 ENV2=VALUE2 ...";

// Sends a WindowResizeMessage with the current window size of |ttyfd|
// to |sockfd|.
bool SendCurrentWindowSize(int sockfd, int ttyfd) {
  vm_tools::vsh::GuestMessage guest_message;
  vm_tools::vsh::WindowResizeMessage* resize_message =
      guest_message.mutable_resize_message();

  struct winsize winsize;

  if (ioctl(ttyfd, TIOCGWINSZ, &winsize) < 0) {
    PLOG(ERROR) << "Failed to get tty window size";
    Shutdown();
    return false;
  }

  resize_message->set_rows(winsize.ws_row);
  resize_message->set_cols(winsize.ws_col);

  if (!SendMessage(sockfd, guest_message)) {
    LOG(ERROR) << "Failed to send tty window resize message";
    Shutdown();
    return false;
  }

  return true;
}

// Handles a signal that is expected to terminate the process by exiting
// the main message loop.
bool HandleTermSignal(const struct signalfd_siginfo& siginfo) {
  Shutdown();
  return true;
}

// Handles a window resize signal by sending the current window size to the
// remote.
bool HandleWindowResizeSignal(int sockfd,
                              int ttyfd,
                              const struct signalfd_siginfo& siginfo) {
  DCHECK_EQ(siginfo.ssi_signo, SIGWINCH);

  SendCurrentWindowSize(sockfd, ttyfd);

  // This return value indicates whether or not the signal handler should be
  // unregistered! So, even if this succeeds, this should return false.
  return false;
}

// Receives a host message from the guest and takes action.
void HandleVsockReadable(int sockfd) {
  vm_tools::vsh::HostMessage host_message;
  if (!RecvMessage(sockfd, &host_message)) {
    PLOG(ERROR) << "Failed to receive message from server";
    Shutdown();
    return;
  }

  switch (host_message.msg_case()) {
    case vm_tools::vsh::HostMessage::kDataMessage: {
      // Data messages from the guest should go to stdout.
      vm_tools::vsh::DataMessage data_message = host_message.data_message();
      DCHECK_EQ(data_message.stream(), vm_tools::vsh::STDOUT_STREAM);

      if (!base::WriteFileDescriptor(STDOUT_FILENO, data_message.data().data(),
                                     data_message.data().size())) {
        PLOG(ERROR) << "Failed to write data to stdout";
        return;
      }
      break;
    }
    case vm_tools::vsh::HostMessage::kStatusMessage: {
      // The remote side has an updated connection status, which likely means
      // it's time to Shutdown().
      vm_tools::vsh::ConnectionStatusMessage status_message =
          host_message.status_message();
      vm_tools::vsh::ConnectionStatus status = status_message.status();

      if (status == vm_tools::vsh::EXITED) {
        Shutdown();
      } else if (status != vm_tools::vsh::READY) {
        LOG(ERROR) << "vsh connection has exited abnormally: " << status;
        Shutdown();
        return;
      }
      break;
    }
    default:
      LOG(ERROR) << "Received unknown host message of type: "
                 << host_message.msg_case();
  }
}

// Forwards input from the host to the remote pseudoterminal.
void HandleStdinReadable(int sockfd) {
  uint8_t buf[vm_tools::vsh::kMaxDataSize];
  vm_tools::vsh::GuestMessage guest_message;
  vm_tools::vsh::DataMessage* data_message =
      guest_message.mutable_data_message();

  ssize_t count = HANDLE_EINTR(read(STDIN_FILENO, buf, sizeof(buf)));

  if (count < 0) {
    PLOG(ERROR) << "Failed to read from stdin";
    Shutdown();
  } else if (count == 0) {
    Shutdown();
  }

  data_message->set_stream(vm_tools::vsh::STDIN_STREAM);
  data_message->set_data(buf, count);

  if (!SendMessage(sockfd, guest_message)) {
    LOG(ERROR) << "Failed to send guest data message";
    // Sending a partial message will break framing. Shut down the connection.
    Shutdown();
    return;
  }
}

bool GetCid(const std::string& vm_name, unsigned int* cid) {
  dbus::Bus::Options opts;
  opts.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(std::move(opts)));

  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return false;
  }

  dbus::ObjectProxy* proxy = bus->GetObjectProxy(
      vm_tools::concierge::kVmConciergeServiceName,
      dbus::ObjectPath(vm_tools::concierge::kVmConciergeServicePath));
  if (!proxy) {
    LOG(ERROR) << "Unable to get dbus proxy for "
               << vm_tools::concierge::kVmConciergeServiceName;
    return false;
  }

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
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
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

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  brillo::InitLog(brillo::kLogToStderrIfTty);

  DEFINE_uint64(cid, 0, "Cid of VM");
  DEFINE_string(vm_name, "", "Target VM name");
  DEFINE_string(user, "chronos", "Target user in the VM");

  brillo::FlagHelper::Init(argc, argv, kVshUsage);

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
    if (!GetCid(FLAGS_vm_name, &cid))
      return EXIT_FAILURE;
  }

  base::ScopedFD sockfd(socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!sockfd.is_valid()) {
    PLOG(ERROR) << "Failed to open vsock socket";
    return EXIT_FAILURE;
  }

  struct sockaddr_vm addr;
  memset(&addr, 0, sizeof(addr));
  addr.svm_family = AF_VSOCK;
  addr.svm_port = vm_tools::kVshPort;
  addr.svm_cid = cid;

  if (HANDLE_EINTR(connect(sockfd.get(),
                           reinterpret_cast<struct sockaddr*>(&addr),
                           sizeof(addr)) < 0)) {
    PLOG(ERROR) << "Failed to connect to vshd";
    return EXIT_FAILURE;
  }

  // Set up the connection with the guest. The setup process is:
  //
  // 1) Client opens connection and sends a SetupConnectionRequest.
  // 2) Server responds with a SetupConnectionResponse. If the response
  //    does not indicate READY status, the client must exit immediately.
  // 3) If the client receives READY, the server and client may exchange
  //    HostMessage and GuestMessage protobufs, with GuestMessages flowing
  //    from client(host) to server(guest), and vice versa for HostMessages.
  // 4) If the client or server receives a message with a new ConnectionStatus
  //    that does not indicate READY, the recepient must exit.
  // TODO(smbarber): Connect to an lxd container instead of the VM.
  vm_tools::vsh::SetupConnectionRequest connection_request;
  connection_request.set_target(vm_tools::vsh::kVmShell);
  connection_request.set_user(FLAGS_user);

  auto env = connection_request.mutable_env();

  // Default to forwarding the current TERM variable.
  const char* term_env = getenv("TERM");
  if (term_env)
    (*env)["TERM"] = std::string(term_env);

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  std::vector<std::string> args = cl->GetArgs();

  // Forward any additional environment variables passed on the command line.
  for (const auto& arg : args) {
    std::vector<std::string> components = base::SplitString(
        arg, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (components.size() != 2) {
      LOG(WARNING) << "Argument '" << arg
                   << "' is not a valid environment variable";
      continue;
    }

    (*env)[components[0]] = components[1];
  }

  if (!SendMessage(sockfd.get(), connection_request)) {
    LOG(ERROR) << "Failed to send connection request";
    return EXIT_FAILURE;
  }

  vm_tools::vsh::SetupConnectionResponse connection_response;
  if (!RecvMessage(sockfd.get(), &connection_response)) {
    LOG(ERROR) << "Failed to receive response from vshd";
    return EXIT_FAILURE;
  }

  vm_tools::vsh::ConnectionStatus status = connection_response.status();
  if (status != vm_tools::vsh::READY) {
    LOG(ERROR) << "Server was unable to set up connection: "
               << connection_response.description();
    return EXIT_FAILURE;
  }

  base::ScopedFD ttyfd(open(vm_tools::vsh::kDevTtyPath,
                            O_RDONLY | O_NOCTTY | O_CLOEXEC | O_NONBLOCK));
  if (!ttyfd.is_valid()) {
    PLOG(ERROR) << "Failed to open /dev/tty";
    return EXIT_FAILURE;
  }

  if (!SendCurrentWindowSize(sockfd.get(), ttyfd.get())) {
    LOG(ERROR) << "Unable to send initial window size";
    return EXIT_FAILURE;
  }

  // Set terminal to raw mode. Note that the client /must/ cleanly exit
  // the message loop below to restore termios settings.
  ScopedTermios termios(std::move(ttyfd));
  if (isatty(termios.GetRawFD()) &&
      !termios.SetTermiosMode(ScopedTermios::TermiosMode::RAW))
    return EXIT_FAILURE;

  brillo::BaseMessageLoop message_loop;
  message_loop.SetAsCurrent();

  // Handle termination signals and SIGWINCH.
  brillo::AsynchronousSignalHandler signal_handler;
  signal_handler.Init();
  for (int signal : {SIGINT, SIGTERM, SIGHUP, SIGQUIT}) {
    signal_handler.RegisterHandler(signal, base::Bind(&HandleTermSignal));
  }
  signal_handler.RegisterHandler(
      SIGWINCH,
      base::Bind(&HandleWindowResizeSignal, sockfd.get(), termios.GetRawFD()));

  message_loop.WatchFileDescriptor(
      FROM_HERE, sockfd.get(), brillo::MessageLoop::WatchMode::kWatchRead, true,
      base::Bind(&HandleVsockReadable, sockfd.get()));
  message_loop.WatchFileDescriptor(
      FROM_HERE, STDIN_FILENO, brillo::MessageLoop::WatchMode::kWatchRead, true,
      base::Bind(&HandleStdinReadable, sockfd.get()));
  message_loop.Run();

  return EXIT_SUCCESS;
}
