// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/x_server_runner.h"

#include <arpa/inet.h>
#include <grp.h>
#include <signal.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/files/file.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/process/launch.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <chromeos/userdb_utils.h>
#include <chromeos/bootstat/bootstat.h>

#include "chromeos/ui/util.h"

namespace chromeos {
namespace ui {

namespace {

// Path to the X server binary.
const char kXServerCommand[] = "/usr/bin/X";

// Writes |data| to |file|, returning true on success.
bool WriteString(base::File* file, const std::string& data) {
  return file->WriteAtCurrentPos(data.data(), data.size()) ==
      static_cast<int>(data.size());
}

// Writes |value| to |file| in big-endian order, returning true on success.
bool WriteUint16(base::File* file, uint16_t value) {
  value = htons(value);
  return file->WriteAtCurrentPos(
      reinterpret_cast<const char*>(&value), sizeof(value)) ==
      static_cast<int>(sizeof(value));
}

// Creates a new X authority file at |path|, returning true on success.
bool CreateXauthFile(const base::FilePath& path, uid_t uid, gid_t gid) {
  base::File file(path,
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid()) {
    PLOG(ERROR) << "Couldn't open " << path.value();
    return false;
  }
  if (!util::SetPermissions(path, uid, gid, 0600))
    return false;

  const int kCookieSize = 16;
  // TODO(derat): base/rand_util.h says not to use RandBytesAsString() for
  // security-related purposes, but crypto::RandBytes() (which we don't package)
  // just wraps RandBytes(). The base implementation uses /dev/urandom, which is
  // fine for our purposes (see e.g. http://www.2uo.de/myths-about-urandom/),
  // but to make this code self-documenting, this call should be changed to
  // crypto::RandBytes() if/when that gets packaged for Chrome OS.
  const std::string kCookie(base::RandBytesAsString(kCookieSize));
  const uint16_t kFamily = 0x100;
  const std::string kAddress = "localhost";
  const std::string kNumber = "0";
  const std::string kName = "MIT-MAGIC-COOKIE-1";

  if (!WriteUint16(&file, kFamily) ||
      !WriteUint16(&file, kAddress.size()) ||
      !WriteString(&file, kAddress) ||
      !WriteUint16(&file, kNumber.size()) ||
      !WriteString(&file, kNumber) ||
      !WriteUint16(&file, kName.size()) ||
      !WriteString(&file, kName) ||
      !WriteUint16(&file, kCookie.size()) ||
      !WriteString(&file, kCookie)) {
    PLOG(ERROR) << "Couldn't write to " << path.value();
    return false;
  }

  return true;
}

// Runs the X server, replacing the current process.
void ExecServer(int vt,
                int max_vt,
                const base::FilePath& xauth_file,
                const base::FilePath& log_file) {
  std::vector<std::string> args;
  args.push_back(kXServerCommand);
  args.push_back("-nohwaccess");
  args.push_back("-noreset");
  args.push_back("-maxvt");
  args.push_back(base::IntToString(max_vt));
  args.push_back("-nolisten");
  args.push_back("tcp");
  args.push_back(base::StringPrintf("vt%d", vt));
  args.push_back("-auth");
  args.push_back(xauth_file.value());
  args.push_back("-logfile");
  args.push_back(log_file.value());

  const size_t kMaxArgs = 32;
  char* argv[kMaxArgs + 1];
  CHECK_LE(args.size(), kMaxArgs);
  for (size_t i = 0; i < args.size(); ++i)
    argv[i] = const_cast<char*>(args[i].c_str());
  argv[args.size()] = nullptr;

  // This call doesn't return on success.
  PCHECK(execv(argv[0], argv) == 0) << "execv() failed";
}

// Helper for ExecAndWaitForServer() that reads signals sent from |server_pid|
// via signalfd-created |fd|. Returns true if the server started successfully.
bool WaitForSignalFromServer(pid_t server_pid, int fd) {
  LOG(INFO) << "X server started with PID " << server_pid;
  while (true) {
    struct signalfd_siginfo siginfo;
    int bytes_read = HANDLE_EINTR(read(fd, &siginfo, sizeof(siginfo)));
    PCHECK(bytes_read >= 0);
    if (bytes_read != sizeof(siginfo)) {
      LOG(ERROR) << "Read " << bytes_read << " byte(s); expected "
                 << sizeof(siginfo);
      return false;
    }

    switch (siginfo.ssi_signo) {
      case SIGUSR1:
        LOG(INFO) << "X server is ready for connections";
        return true;
      case SIGCHLD: {
        int status = 0;
        int result = waitpid(server_pid, &status, WNOHANG);
        if (result != 0) {
          PCHECK(result == server_pid) << "waitpid() returned " << result;
          if (WIFEXITED(status)) {
            LOG(ERROR) << "X server exited with " << WEXITSTATUS(status)
                       << " before sending SIGUSR1";
            return false;
          } else if (WIFSIGNALED(status)) {
            LOG(ERROR) << "X server was terminated with signal "
                       << WTERMSIG(status) << " before sending SIGUSR1";
            return false;
          }
        }
        // In the event of a non-exit SIGCHLD, ignore it and loop to
        // read the next signal.
        LOG(INFO) << "Ignoring non-exit SIGCHLD";
        continue;
      }
      default:
        CHECK(false) << "Unexpected signal " << siginfo.ssi_signo;
    }
  }
  return false;
}

// Drops privileges, forks-and-execs the X server, waits for it to emit SIGUSR1
// to indicate that it's ready for connections, and returns true on success.
bool ExecAndWaitForServer(const std::string& user,
                          uid_t uid,
                          gid_t gid,
                          const base::Closure& closure) {
  // Avoid some syscalls when not running as root in tests.
  if (getuid() == 0) {
    if (setpriority(PRIO_PROCESS, 0, -20) != 0)
      PLOG(WARNING) << "setpriority() failed";

    PCHECK(initgroups(user.c_str(), gid) == 0);
    PCHECK(setgid(gid) == 0);
    PCHECK(setuid(uid) == 0);
  }

  sigset_t mask;
  PCHECK(sigemptyset(&mask) == 0);
  PCHECK(sigaddset(&mask, SIGUSR1) == 0);
  PCHECK(sigaddset(&mask, SIGCHLD) == 0);
  const int fd = signalfd(-1, &mask, 0);
  PCHECK(fd != -1) << "signalfd() failed";
  PCHECK(sigprocmask(SIG_BLOCK, &mask, nullptr) == 0);

  bool success = false;
  switch (pid_t pid = fork()) {
    case -1:
      PLOG(ERROR) << "fork() failed";
      break;
    case 0:
      // Forked process: exec the X server.
      base::CloseSuperfluousFds(base::InjectiveMultimap());
      PCHECK(sigprocmask(SIG_UNBLOCK, &mask, nullptr) == 0);

      // Set SIGUSR1's disposition to SIG_IGN before exec-ing so that X will
      // emit SIGUSR1 once it's ready to accept connections.
      PCHECK(signal(SIGUSR1, SIG_IGN) != SIG_ERR);

      closure.Run();

      // We should never reach this point, but crash just in case to avoid
      // double-closing the FD.
      LOG(FATAL) << "Server closure returned unexpectedly";
      break;
    default:
      // Original process: wait for the forked process to become ready or exit.
      success = WaitForSignalFromServer(pid, fd);
      break;
  }

  close(fd);
  return success;
}

}  // namespace

const char XServerRunner::kDefaultUser[] = "xorg";
const int XServerRunner::kDefaultVt = 1;
const char XServerRunner::kSocketDir[] = "/tmp/.X11-unix";
const char XServerRunner::kIceDir[] = "/tmp/.ICE-unix";
const char XServerRunner::kLogFile[] = "/var/log/xorg/Xorg.0.log";
const char XServerRunner::kXkbDir[] = "/var/lib/xkb";

XServerRunner::XServerRunner() : child_pid_(0) {}

XServerRunner::~XServerRunner() {}

bool XServerRunner::StartServer(const std::string& user,
                                int vt,
                                bool allow_vt_switching,
                                const base::FilePath& xauth_file) {
  uid_t uid = 0;
  gid_t gid = 0;
  if (!userdb::GetUserInfo(user, &uid, &gid))
    return false;

  if (!CreateXauthFile(xauth_file, uid, gid))
    return false;

  if (!util::EnsureDirectoryExists(GetPath(kSocketDir), 0, 0, 01777) ||
      !util::EnsureDirectoryExists(GetPath(kIceDir), 0, 0, 01777))
    return false;

  const base::FilePath log_file(GetPath(kLogFile));
  if (!util::EnsureDirectoryExists(log_file.DirName(), uid, gid, 0755) ||
      !util::EnsureDirectoryExists(GetPath(kXkbDir), uid, gid, 0755))
    return false;

  // Create a relative symlink from one directory above |log_file| to the file
  // itself (e.g. /var/log/Xorg.0.log -> xorg/Xorg.0.log).
  base::CreateSymbolicLink(
      log_file.DirName().BaseName().Append(log_file.BaseName()),
      log_file.DirName().DirName().Append(log_file.BaseName()));

  // Disable all the Ctrl-Alt-Fn shortcuts for switching between virtual
  // terminals if requested. Otherwise, disable only Fn (n>=3) keys.
  int max_vt = allow_vt_switching ? 2 : 0;

  switch (child_pid_ = fork()) {
    case -1:
      PLOG(ERROR) << "fork() failed";
      return false;
    case 0: {
      base::Closure closure = !callback_for_testing_.is_null() ?
          callback_for_testing_ :
          base::Bind(&ExecServer, vt, max_vt, xauth_file, log_file);
      // The child process waits for the server to start and exits with 0.
      exit(ExecAndWaitForServer(user, uid, gid, closure) ? 0 : 1);
    }
    default:
      LOG(INFO) << "Child process " << child_pid_
                << " starting X server in background";
  }
  return true;
}

bool XServerRunner::WaitForServer() {
  CHECK_GT(child_pid_, 0);
  int status = 0;
  if (waitpid(child_pid_, &status, 0) != child_pid_) {
    PLOG(ERROR) << "waitpid() on " << child_pid_ << " failed";
    return false;
  }
  if (!WIFEXITED(status)) {
    LOG(ERROR) << "Child process " << child_pid_ << " didn't exit normally";
    return false;
  }
  if (WEXITSTATUS(status) != 0) {
    LOG(ERROR) << "Child process " << child_pid_ << " exited with "
               << WEXITSTATUS(status);
    return false;
  }

  if (getuid() == 0) {
    // TODO(derat): Move session_manager's UpstartSignalEmitter into libchromeos
    // and use it here.
    util::Run("initctl", "emit", "x-started", nullptr);
    bootstat_log("x-started");
  }

  return true;
}

base::FilePath XServerRunner::GetPath(const std::string& path) const {
  return util::GetReparentedPath(path, base_path_for_testing_);
}

}  // namespace ui
}  // namespace chromeos
