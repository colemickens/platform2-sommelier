// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/vsh/vsh_forwarder.h"

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
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
#include <brillo/key_value_store.h>
#include <brillo/message_loops/base_message_loop.h>
#include <brillo/syslog_logging.h>

#include "vm_tools/common/constants.h"
#include "vm_tools/vsh/utils.h"
#include "vsh.pb.h"  // NOLINT(build/include)

using std::string;

namespace {

// Path to lsb-release file.
constexpr char kLsbReleasePath[] = "/etc/lsb-release";

// Chrome OS release track.
constexpr char kChromeosReleaseTrackKey[] = "CHROMEOS_RELEASE_TRACK";

// String denoting a test image.
constexpr char kTestImageChannel[] = "testimage-channel";

bool IsTestImage() {
  brillo::KeyValueStore store;
  if (!store.Load(base::FilePath(kLsbReleasePath))) {
    LOG(ERROR) << "Could not read lsb-release";
    return false;
  }

  std::string release;
  if (!store.GetString(kChromeosReleaseTrackKey, &release)) {
    // If the key isn't set, then assume not a test image.
    return false;
  }

  return release == kTestImageChannel;
}

}  // namespace

namespace vm_tools {
namespace vsh {

std::unique_ptr<VshForwarder> VshForwarder::Create(base::ScopedFD sock_fd,
                                                   bool inherit_env) {
  auto forwarder = std::unique_ptr<VshForwarder>(
      new VshForwarder(std::move(sock_fd), inherit_env));

  if (!forwarder->Init()) {
    return nullptr;
  }

  return forwarder;
}

VshForwarder::VshForwarder(base::ScopedFD sock_fd, bool inherit_env)
    : sock_fd_(std::move(sock_fd)),
      inherit_env_(inherit_env),
      exit_pending_(false) {}

bool VshForwarder::Init() {
  SetupConnectionRequest connection_request;

  if (!RecvMessage(sock_fd_.get(), &connection_request)) {
    LOG(ERROR) << "Failed to recv connection request";
    return false;
  }

  const std::string target = connection_request.target();
  std::string user = connection_request.user();
  if (target == kVmShell) {
    // The default user for VM shells should be chronos.
    if (user.empty()) {
      user = "chronos";
    }

    if (user != "chronos" && !IsTestImage()) {
      LOG(ERROR) << "Only chronos is allowed login on the VM shell";
      SendConnectionResponse(FAILED,
                             "only chronos is allowed login on the VM shell");
      return false;
    }
  }

  struct passwd* passwd = nullptr;
  uid_t current_uid = geteuid();
  // If the user is unspecified, run as the current user.
  if (user.empty()) {
    // We're not using threads, so getpwuid is safe.
    passwd = getpwuid(current_uid);  // NOLINT(runtime/threadsafe_fn)
    if (!passwd) {
      PLOG(ERROR) << "Failed to get passwd entry for uid " << current_uid;
      SendConnectionResponse(
          FAILED, base::StringPrintf("could not find uid: %u", current_uid));
      return false;
    }
  } else {
    // We're not using threads, so getpwnam is safe.
    passwd = getpwnam(user.c_str());  // NOLINT(runtime/threadsafe_fn)
    if (!passwd) {
      PLOG(ERROR) << "Failed to get passwd entry for user " << user;
      SendConnectionResponse(FAILED,
                             std::string("could not find user: ") + user);
      return false;
    }
  }

  if (passwd->pw_uid != current_uid && current_uid != 0) {
    LOG(ERROR) << "Cannot change to requested user: " << user;
    SendConnectionResponse(FAILED,
                           std::string("cannot change to user: ") + user);
    return false;
  }

  // If changing users, set up supplementary groups and switch to that user.
  if (passwd->pw_uid != current_uid && current_uid == 0) {
    // Set supplementary groups from passwd file.
    if (initgroups(user.c_str(), passwd->pw_gid) < 0) {
      PLOG(ERROR) << "Failed to set supplementary groups";
      SendConnectionResponse(FAILED, "could not set supplementary groups");
      return false;
    }

    // Switch to target uid/gid.
    uid_t target_uid = passwd->pw_uid;
    gid_t target_gid = passwd->pw_gid;
    if (setresgid(target_gid, target_gid, target_gid) < 0) {
      PLOG(ERROR) << "Failed to set gid";
      SendConnectionResponse(
          FAILED, base::StringPrintf("could not set gid to %u", target_gid));
      return false;
    }
    if (setresuid(target_uid, target_uid, target_uid) < 0) {
      PLOG(ERROR) << "Failed to set uid";
      SendConnectionResponse(
          FAILED, base::StringPrintf("could not set uid to %u", target_uid));
      return false;
    }
  }

  ptm_fd_.reset(HANDLE_EINTR(posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC)));
  if (!ptm_fd_.is_valid()) {
    PLOG(ERROR) << "Failed to open pseudoterminal master";
    SendConnectionResponse(FAILED, "could not allocate pty");
    return false;
  }

  if (grantpt(ptm_fd_.get()) < 0) {
    PLOG(ERROR) << "Failed to grant psuedoterminal";
    SendConnectionResponse(FAILED, "could not grant pty");
    return false;
  }

  if (unlockpt(ptm_fd_.get()) < 0) {
    PLOG(ERROR) << "Failed to unlock psuedoterminal";
    SendConnectionResponse(FAILED, "could not unlock pty");
    return false;
  }

  // Block SIGCHLD until the parent is ready to handle it with the
  // RegisterHandler() call below. At that point any queued SIGCHLD
  // signals will be handled.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, nullptr);

  // fork() a child process that will exec the target process/shell.
  int pid = fork();
  if (pid == 0) {
    const char* pts = ptsname(ptm_fd_.get());
    if (!pts) {
      PLOG(ERROR) << "Failed to find pts";
      return false;
    }

    // These fds are CLOEXEC, but close them manually for good measure.
    sock_fd_.reset();
    ptm_fd_.reset();
    PrepareExec(pts, passwd, connection_request);

    // This line shouldn't be reached if exec succeeds.
    return false;
  }

  brillo::MessageLoop* message_loop = brillo::MessageLoop::current();
  message_loop->WatchFileDescriptor(
      FROM_HERE, sock_fd_.get(), brillo::MessageLoop::WatchMode::kWatchRead,
      true,
      base::Bind(&VshForwarder::HandleVsockReadable, base::Unretained(this)));
  message_loop->WatchFileDescriptor(
      FROM_HERE, ptm_fd_.get(), brillo::MessageLoop::WatchMode::kWatchRead,
      true,
      base::Bind(&VshForwarder::HandlePtmReadable, base::Unretained(this)));

  SendConnectionResponse(READY, "vsh ready");

  // Add the SIGCHLD handler. This will block SIGCHLD again, which has no
  // effect since it was blocked before the fork(), but the underlying
  // signalfd will still have any queued SIGCHLD.
  signal_handler_.Init();
  signal_handler_.RegisterHandler(
      SIGCHLD,
      base::Bind(&VshForwarder::HandleSigchld, base::Unretained(this)));

  return true;
}

bool VshForwarder::SendConnectionResponse(ConnectionStatus status,
                                          const std::string& description) {
  SetupConnectionResponse connection_response;
  connection_response.set_status(status);
  connection_response.set_description(description);

  if (!SendMessage(sock_fd_.get(), connection_response)) {
    LOG(ERROR) << "Failed to send connection response";
    return false;
  }
  return true;
}

void VshForwarder::PrepareExec(
    const char* pts,
    const struct passwd* passwd,
    const SetupConnectionRequest& connection_request) {
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

  // Set up the environment. First include any inherited environment variables,
  // then allow the client to override them.
  std::map<std::string, std::string> env_map;
  if (inherit_env_) {
    for (size_t i = 0; environ[i] != nullptr; i++) {
      size_t len = strlen(environ[i]);
      char* eq = strchr(environ[i], '=');
      if (eq == nullptr) {
        LOG(WARNING) << "Invalid environment variable; ignoring";
        continue;
      }

      std::string key(environ[i], eq - environ[i]);
      std::string val(eq + 1, environ[i] + len - eq);
      env_map[key] = val;
    }
  }

  auto request_env = connection_request.env();
  env_map.insert(request_env.begin(), request_env.end());

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
    argv = std::vector<const char*>({login_shell.c_str(), nullptr});
    executable = passwd->pw_shell;
  } else {
    // Add nullptr at end.
    argv.resize(args.size() + 1);
    std::transform(
        args.begin(), args.end(), argv.begin(),
        [](const string& arg) -> const char* { return arg.c_str(); });
    executable = argv[0];
  }

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_UNBLOCK, &mask, nullptr);

  if (execvpe(executable, const_cast<char* const*>(argv.data()), envp.data()) <
      0) {
    PLOG(ERROR) << "Failed to exec '" << executable << "'";
  }
}

// Handler for SIGCHLD received in the forwarder process, indicating that
// the target process has exited and the forwarder should shut down.
bool VshForwarder::HandleSigchld(const struct signalfd_siginfo& siginfo) {
  HostMessage host_message;
  ConnectionStatusMessage* status_message =
      host_message.mutable_status_message();
  status_message->set_status(EXITED);
  status_message->set_description("target process has exited");
  status_message->set_code(siginfo.ssi_status);

  exit_pending_ = true;

  if (!SendMessage(sock_fd_.get(), host_message)) {
    LOG(ERROR) << "Failed to send host message";
  }

  return true;
}

// Receives a guest message from the host and takes action.
void VshForwarder::HandleVsockReadable() {
  GuestMessage guest_message;

  // If we're expecting the remote side to exit, check that the socket
  // is actually still valid before using it.
  if (exit_pending_) {
    char dummy;
    if (recv(sock_fd_.get(), &dummy, 1, MSG_PEEK) < 0) {
      Shutdown();
      return;
    }
  }

  if (!RecvMessage(sock_fd_.get(), &guest_message)) {
    PLOG(ERROR) << "Failed to receive message from client";
    Shutdown();
    return;
  }

  switch (guest_message.msg_case()) {
    case GuestMessage::kDataMessage: {
      DataMessage data_message = guest_message.data_message();
      DCHECK_EQ(data_message.stream(), STDIN_STREAM);

      const string& data = data_message.data();
      // On EOF, send EOT character. This will be interpreted by the tty
      // driver/line discipline and generate an EOF.
      if (data.size() == 0) {
        if (!base::WriteFileDescriptor(ptm_fd_.get(), "\004", 1)) {
          PLOG(ERROR) << "Failed to write EOF to ptm";
        }
        return;
      }

      if (!base::WriteFileDescriptor(ptm_fd_.get(), data.data(), data.size())) {
        PLOG(ERROR) << "Failed to write data to ptm";
        return;
      }
      break;
    }
    case GuestMessage::kStatusMessage: {
      // The remote side has an updated connection status, which likely means
      // it's time to Shutdown().
      ConnectionStatusMessage status_message = guest_message.status_message();
      ConnectionStatus status = status_message.status();

      if (status == EXITED) {
        Shutdown();
      } else if (status != READY) {
        LOG(ERROR) << "vshd connection has exited abnormally: " << status;
        Shutdown();
        return;
      }
      break;
    }
    case GuestMessage::kResizeMessage: {
      WindowResizeMessage resize_message = guest_message.resize_message();
      struct winsize winsize;
      winsize.ws_row = resize_message.rows();
      winsize.ws_col = resize_message.cols();
      if (ioctl(ptm_fd_.get(), TIOCSWINSZ, &winsize) < 0) {
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
void VshForwarder::HandlePtmReadable() {
  char buf[kMaxDataSize];
  HostMessage host_message;
  DataMessage* data_message = host_message.mutable_data_message();

  ssize_t count = HANDLE_EINTR(read(ptm_fd_.get(), buf, sizeof(buf)));

  if (count < 0) {
    // It's likely that we'll get an EIO from the ptm before getting a SIGCHLD,
    // so don't treat that as an error. We'll shut down normally with the
    // SIGCHLD that will be processed later.
    if (errno == EAGAIN || errno == EIO)
      return;
    PLOG(ERROR) << "Failed to read from ptm";
    return;
  }

  data_message->set_stream(STDOUT_STREAM);
  data_message->set_data(buf, count);

  if (!SendMessage(sock_fd_.get(), host_message)) {
    LOG(ERROR) << "Failed to forward ptm to host";
    Shutdown();
  }
}

}  // namespace vsh
}  // namespace vm_tools
