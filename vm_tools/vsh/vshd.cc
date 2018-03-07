// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <linux/vm_sockets.h>  // Needs to come after sys/socket.h

#include <algorithm>
#include <memory>
#include <string>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <brillo/asynchronous_signal_handler.h>
#include <brillo/flag_helper.h>
#include <brillo/message_loops/base_message_loop.h>
#include <brillo/syslog_logging.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/vsh/utils.h"
#include "vsh.pb.h"  // NOLINT(build/include)

using std::string;
using vm_tools::vsh::RecvMessage;
using vm_tools::vsh::SendMessage;
using vm_tools::vsh::Shutdown;

namespace {

// Path to lsb-release file.
constexpr char kLsbReleasePath[] = "/etc/lsb-release";

// Chrome OS release track.
constexpr char kChromeosReleaseTrackKey[] = "CHROMEOS_RELEASE_TRACK";

// String denoting a test image.
constexpr char kTestImageChannel[] = "testimage-channel";

bool IsTestImage() {
  std::string lsb_release;
  if (!base::ReadFileToString(base::FilePath(kLsbReleasePath), &lsb_release)) {
    LOG(ERROR) << "Could not read lsb-release";
    return false;
  }

  std::vector<base::StringPiece> lines = base::SplitStringPiece(
      lsb_release, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (auto& line : lines) {
    std::vector<base::StringPiece> components = base::SplitStringPiece(
        line, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (components.size() != 2)
      continue;

    // An image is only a test image if it's on the testimage-channel release
    // track.
    if (components[0] == kChromeosReleaseTrackKey &&
        components[1] == kTestImageChannel)
      return true;
  }

  return false;
}

bool SendConnectionResponse(int sockfd,
                            vm_tools::vsh::ConnectionStatus status,
                            const std::string& description) {
  vm_tools::vsh::SetupConnectionResponse connection_response;
  connection_response.set_status(status);
  connection_response.set_description(description);

  if (!SendMessage(sockfd, connection_response)) {
    LOG(ERROR) << "Failed to send connection response";
    return false;
  }
  return true;
}

void PrepareExec(
    const char* pts,
    const struct passwd* passwd,
    const vm_tools::vsh::SetupConnectionRequest& connection_request) {
  base::ScopedFD pty(HANDLE_EINTR(open(pts, O_RDWR | O_CLOEXEC | O_NOCTTY)));
  if (!pty.is_valid()) {
    PLOG(ERROR) << "Failed to open pseudoterminal slave";
    return;
  }

  // Dup the pty fd into stdin/stdout/stderr.
  for (int fd : {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO}) {
    if (dup2(pty.get(), fd) < 0) {
      PLOG(ERROR) << "Failed to dup pty into fd " << fd;
      return;
    }
  }

  // This is required for job control to work in a shell. The shell must
  // be a process group leader. This is expected to succeed since this
  // has just forked.
  if (setsid() < 0) {
    PLOG(ERROR) << "Failed to create new session";
    return;
  }

  // Set the controlling terminal for the process.
  if (ioctl(pty.get(), TIOCSCTTY, nullptr) < 0) {
    PLOG(ERROR) << "Failed to set controlling terminal";
    return;
  }

  if (pty.get() != STDIN_FILENO && pty.get() != STDOUT_FILENO &&
      pty.get() != STDERR_FILENO) {
    pty.reset();
  }

  if (chdir(passwd->pw_dir) < 0) {
    PLOG(WARNING) << "Failed to change to home directory: " << passwd->pw_dir;
    // Fall back to root directory if home isn't available.
    if (chdir("/") < 0) {
      PLOG(ERROR) << "Failed to change to root directory";
      return;
    }
  }

  // Get shell from passwd file and prefix argv[0] with "-" to indicate a
  // login shell.
  std::string login_shell = base::FilePath(passwd->pw_shell).BaseName().value();
  login_shell.insert(0, "-");

  // Set up the environment. The request is const, so copy it into a local map
  // first.
  auto request_env = connection_request.env();
  std::map<std::string, std::string> env_map(request_env.begin(),
                                             request_env.end());

  // Fallback to TERM=linux in case the remote didn't forward its own TERM.
  auto term_it = env_map.find("TERM");
  if (term_it == env_map.end()) {
    env_map["TERM"] = "linux";
  }

  // Set SHELL and HOME as basic required environment variables. It doesn't
  // make sense for the remote to override these anyway.
  env_map["SHELL"] = std::string(passwd->pw_shell);
  env_map["HOME"] = std::string(passwd->pw_dir);

  // Collapse the map into a vector of key-value pairs, then create the final
  // vector of C-string pointers with a terminating nullptr.
  std::vector<std::string> envp_strings;
  envp_strings.reserve(env_map.size());
  for (const auto& pair : env_map) {
    envp_strings.emplace_back(pair.first + "=" + pair.second);
  }

  std::vector<char*> envp;
  envp.reserve(envp_strings.size() + 1);
  for (const auto& env_var : envp_strings) {
    envp.push_back(const_cast<char*>(env_var.c_str()));
  }
  envp.emplace_back(nullptr);

  std::vector<string> args(connection_request.argv().begin(),
                           connection_request.argv().end());
  std::vector<const char*> argv;
  const char* executable = nullptr;

  if (connection_request.argv().empty()) {
    argv = std::vector<const char*>({login_shell.c_str(),
                                     nullptr});
    executable = passwd->pw_shell;
  } else {
    // Add nullptr at end.
    argv.resize(args.size() + 1);
    std::transform(args.begin(), args.end(), argv.begin(),
            [](const string& arg) -> const char* { return arg.c_str(); });
    executable = argv[0];
  }

  if (execvpe(executable,
              const_cast<char* const*>(argv.data()),
              envp.data()) < 0) {
      PLOG(ERROR) << "Failed to exec '" << executable << "'";
  }
}

// Handler for SIGCHLD received in the forwarder process, indicating that
// the target process has exited and the forwarder should shut down.
bool HandleSigchld(int sockfd, const struct signalfd_siginfo& siginfo) {
  vm_tools::vsh::HostMessage host_message;
  vm_tools::vsh::ConnectionStatusMessage* status_message =
      host_message.mutable_status_message();
  status_message->set_status(vm_tools::vsh::EXITED);
  status_message->set_description("target process has exited");
  status_message->set_code(siginfo.ssi_status);

  if (!SendMessage(sockfd, host_message)) {
    LOG(ERROR) << "Failed to send host message";
  }

  Shutdown();
  return true;
}

// Receives a guest message from the host and takes action.
void HandleVsockReadable(int ptmfd, int sockfd) {
  vm_tools::vsh::GuestMessage guest_message;
  if (!RecvMessage(sockfd, &guest_message)) {
    PLOG(ERROR) << "Failed to receive message from client";
    Shutdown();
    return;
  }

  switch (guest_message.msg_case()) {
    case vm_tools::vsh::GuestMessage::kDataMessage: {
      vm_tools::vsh::DataMessage data_message = guest_message.data_message();
      DCHECK_EQ(data_message.stream(), vm_tools::vsh::STDIN_STREAM);

      if (!base::WriteFileDescriptor(ptmfd, data_message.data().data(),
                                     data_message.data().size())) {
        PLOG(ERROR) << "Failed to write data to ptm";
        return;
      }
      break;
    }
    case vm_tools::vsh::GuestMessage::kStatusMessage: {
      // The remote side has an updated connection status, which likely means
      // it's time to Shutdown().
      vm_tools::vsh::ConnectionStatusMessage status_message =
          guest_message.status_message();
      vm_tools::vsh::ConnectionStatus status = status_message.status();

      if (status == vm_tools::vsh::EXITED) {
        Shutdown();
      } else if (status != vm_tools::vsh::READY) {
        LOG(ERROR) << "vshd connection has exited abnormally: " << status;
        Shutdown();
        return;
      }
      break;
    }
    case vm_tools::vsh::GuestMessage::kResizeMessage: {
      vm_tools::vsh::WindowResizeMessage resize_message =
          guest_message.resize_message();
      struct winsize winsize;
      winsize.ws_row = resize_message.rows();
      winsize.ws_col = resize_message.cols();
      if (ioctl(ptmfd, TIOCSWINSZ, &winsize) < 0) {
        PLOG(ERROR) << "Failed to resize window";
        return;
      }
      break;
    }
    default:
      LOG(ERROR) << "Received unknown guest message of type: "
                 << guest_message.msg_case();
  }
}

// Forwards output from the pseudoterminal master to the host.
void HandlePtmReadable(int ptmfd, int sockfd) {
  char buf[vm_tools::vsh::kMaxDataSize];
  vm_tools::vsh::HostMessage host_message;
  vm_tools::vsh::DataMessage* data_message =
      host_message.mutable_data_message();

  ssize_t count = HANDLE_EINTR(read(ptmfd, buf, sizeof(buf)));

  if (count < 0) {
    // It's likely that we'll get an EIO from the ptm before getting a SIGCHLD,
    // so don't treat that as an error. We'll shut down normally with the
    // SIGCHLD that will be processed later.
    if (errno == EAGAIN || errno == EIO)
      return;
    PLOG(ERROR) << "Failed to read from ptm";
    return;
  }

  data_message->set_stream(vm_tools::vsh::STDOUT_STREAM);
  data_message->set_data(buf, count);

  if (!SendMessage(sockfd, host_message)) {
    LOG(ERROR) << "Failed to forward ptm to host";
    Shutdown();
  }
}

// Child process that will forward data between pty and vsock.
int RunForwarder(base::ScopedFD sockfd) {
  vm_tools::vsh::SetupConnectionRequest connection_request;

  if (!RecvMessage(sockfd.get(), &connection_request)) {
    LOG(ERROR) << "Failed to recv connection request";
    return EXIT_FAILURE;
  }

  const std::string target = connection_request.target();
  const std::string user = connection_request.user();
  if (target == vm_tools::vsh::kVmShell) {
    if (user != "chronos" && !IsTestImage()) {
      // This limitation is arbitrary to give us some policy to start from.
      LOG(ERROR) << "Only chronos is allowed login on the VM shell";
      return EXIT_FAILURE;
    }
  } else {
    LOG(ERROR) << "Container shells are not yet supported";
    return EXIT_FAILURE;
  }

  // We're not using threads, so getpwnam is safe.
  const struct passwd* passwd =
      getpwnam(user.c_str());  // NOLINT(runtime/threadsafe_fn)
  if (!passwd) {
    PLOG(ERROR) << "Failed to get passwd entry for " << user;
    SendConnectionResponse(sockfd.get(), vm_tools::vsh::FAILED,
                           std::string("could not find user: ") + user);
    return EXIT_FAILURE;
  }

  // Set supplementary groups from passwd file.
  if (initgroups(user.c_str(), passwd->pw_gid) < 0) {
    PLOG(ERROR) << "Failed to set supplementary groups";
    SendConnectionResponse(sockfd.get(), vm_tools::vsh::FAILED,
                           "could not set supplementary groups");
    return EXIT_FAILURE;
  }

  // Switch to target uid/gid.
  uid_t target_uid = passwd->pw_uid;
  gid_t target_gid = passwd->pw_gid;
  if (setresgid(target_gid, target_gid, target_gid) < 0) {
    PLOG(ERROR) << "Failed to set gid";
    SendConnectionResponse(
        sockfd.get(), vm_tools::vsh::FAILED,
        base::StringPrintf("could not set gid to %u", target_gid));
    return EXIT_FAILURE;
  }
  if (setresuid(target_uid, target_uid, target_uid) < 0) {
    PLOG(ERROR) << "Failed to set uid";
    SendConnectionResponse(
        sockfd.get(), vm_tools::vsh::FAILED,
        base::StringPrintf("could not set uid to %u", target_uid));
    return EXIT_FAILURE;
  }

  base::ScopedFD ptmfd(
      HANDLE_EINTR(posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC)));
  if (!ptmfd.is_valid()) {
    PLOG(ERROR) << "Failed to open pseudoterminal master";
    SendConnectionResponse(sockfd.get(), vm_tools::vsh::FAILED,
                           "could not allocate pty");
    return EXIT_FAILURE;
  }

  if (grantpt(ptmfd.get()) < 0) {
    PLOG(ERROR) << "Failed to grant psuedoterminal";
    SendConnectionResponse(sockfd.get(), vm_tools::vsh::FAILED,
                           "could not grant pty");
    return EXIT_FAILURE;
  }

  if (unlockpt(ptmfd.get()) < 0) {
    PLOG(ERROR) << "Failed to unlock psuedoterminal";
    SendConnectionResponse(sockfd.get(), vm_tools::vsh::FAILED,
                           "could not unlock pty");
    return EXIT_FAILURE;
  }

  // fork() a child process that will exec the target process/shell.
  int pid = fork();
  if (pid == 0) {
    const char* pts = ptsname(ptmfd.get());
    if (!pts) {
      PLOG(ERROR) << "Failed to find pts";
      return EXIT_FAILURE;
    }

    // These fds are CLOEXEC, but close them manually for good measure.
    sockfd.reset();
    ptmfd.reset();
    PrepareExec(pts, passwd, connection_request);

    // This line shouldn't be reached if exec succeeds.
    return EXIT_FAILURE;
  }

  // Set up and start the message loop.
  brillo::BaseMessageLoop message_loop;
  message_loop.SetAsCurrent();
  message_loop.WatchFileDescriptor(
      FROM_HERE, sockfd.get(), brillo::MessageLoop::WatchMode::kWatchRead, true,
      base::Bind(&HandleVsockReadable, ptmfd.get(), sockfd.get()));
  message_loop.WatchFileDescriptor(
      FROM_HERE, ptmfd.get(), brillo::MessageLoop::WatchMode::kWatchRead, true,
      base::Bind(&HandlePtmReadable, ptmfd.get(), sockfd.get()));

  brillo::AsynchronousSignalHandler signal_handler;
  signal_handler.Init();
  signal_handler.RegisterHandler(SIGCHLD,
                                 base::Bind(&HandleSigchld, sockfd.get()));

  SendConnectionResponse(sockfd.get(), vm_tools::vsh::READY, "vsh ready");

  message_loop.Run();
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  brillo::FlagHelper::Init(argc, argv, "vsh daemon");
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->GetArgs().size() > 0) {
    LOG(ERROR) << "Unknown extra command line arguments; exiting";
    return EXIT_FAILURE;
  }

  // Create a socket to listen for incoming vsh connections.
  base::ScopedFD sockfd(socket(AF_VSOCK, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!sockfd.is_valid()) {
    PLOG(ERROR) << "Failed to create socket";
    return EXIT_FAILURE;
  }

  struct sockaddr_vm addr;
  memset(&addr, 0, sizeof(addr));
  addr.svm_family = AF_VSOCK;
  addr.svm_port = vm_tools::kVshPort;
  addr.svm_cid = VMADDR_CID_ANY;

  if (bind(sockfd.get(), reinterpret_cast<const struct sockaddr*>(&addr),
           sizeof(addr)) < 0) {
    PLOG(ERROR) << "Failed to bind vshd port";
    return EXIT_FAILURE;
  }

  // Allow a backlog of up to 32 connections. This is exceedingly generous since
  // this daemon forks after accepting a connection.
  if (listen(sockfd.get(), 32) < 0) {
    PLOG(ERROR) << "Failed to listen";
    return EXIT_FAILURE;
  }

  // Block SIGCHLD and set up a signalfd so the main daemon can reap its
  // children.
  sigset_t sigchld_mask, saved_mask;
  DCHECK_EQ(sigemptyset(&sigchld_mask), 0);
  DCHECK_EQ(sigaddset(&sigchld_mask, SIGCHLD), 0);
  if (sigprocmask(SIG_BLOCK, &sigchld_mask, &saved_mask) < 0) {
    PLOG(ERROR) << "Failed to block SIGCHLD";
    return EXIT_FAILURE;
  }

  base::ScopedFD sigfd(signalfd(-1, &sigchld_mask, SFD_NONBLOCK | SFD_CLOEXEC));
  if (!sigfd.is_valid()) {
    PLOG(ERROR) << "Failed to set up signalfd";
    return EXIT_FAILURE;
  }

  struct pollfd pollfds[] = {
      {sigfd.get(), POLLIN, 0}, {sockfd.get(), POLLIN, 0},
  };
  const int num_pollfds = arraysize(pollfds);

  while (true) {
    if (poll(pollfds, num_pollfds, -1) < 0) {
      PLOG(ERROR) << "Failed to poll";
      return EXIT_FAILURE;
    }

    for (int i = 0; i < num_pollfds; i++) {
      if (!(pollfds[i].revents & POLLIN))
        continue;

      if (i == 0) {
        // signalfd.
        struct signalfd_siginfo siginfo;
        if (read(sigfd.get(), &siginfo, sizeof(siginfo)) != sizeof(siginfo)) {
          PLOG(ERROR) << "Failed to read entire signalfd siginfo";
          continue;
        }
        DCHECK_EQ(siginfo.ssi_signo, SIGCHLD);

        // Reap any child exit statuses.
        while (waitpid(-1, nullptr, WNOHANG) > 0)
          continue;
      } else if (i == 1) {
        // sockfd.
        struct sockaddr_vm peer_addr;
        socklen_t addr_size;
        base::ScopedFD peer_sockfd(HANDLE_EINTR(accept4(
            sockfd.get(), reinterpret_cast<struct sockaddr*>(&peer_addr),
            &addr_size, SOCK_CLOEXEC)));
        if (!peer_sockfd.is_valid()) {
          PLOG(ERROR) << "Failed to accept connection from client";
          continue;
        }

        int pid = fork();

        if (pid == 0) {
          // The child needs to restore the original signal mask, and close
          // the listening sockfd and signalfd manually. These fds will be
          // closed automatically on exec() anyway, but it's better not to allow
          // the unprivileged forwarder to have access to either of these.
          if (sigprocmask(SIG_SETMASK, &saved_mask, nullptr) < 0) {
            PLOG(ERROR) << "Failed to restore signal mask after fork";
          }
          sockfd.reset();
          sigfd.reset();
          return RunForwarder(std::move(peer_sockfd));
        }
      }
    }
  }

  return EXIT_SUCCESS;
}
