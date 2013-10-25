// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/process.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <map>

// TODO(cmasone): Come back and clear this out (http://crosbug.com/38805).
#if BASE_VER > 125070
#include <base/posix/eintr_wrapper.h>
#else
#include <base/eintr_wrapper.h>
#endif
#include <base/file_util.h>
#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/time.h>

namespace chromeos {

bool ReturnTrue() { return true; }

Process::Process() {
}

Process::~Process() {
}

bool Process::ProcessExists(pid_t pid) {
  return file_util::DirectoryExists(FilePath(StringPrintf("/proc/%d", pid)));
}

ProcessImpl::ProcessImpl() : pid_(0), uid_(-1), gid_(-1),
                             pre_exec_(base::Bind(&ReturnTrue)) {
}

ProcessImpl::~ProcessImpl() {
  Reset(0);
}

void ProcessImpl::AddArg(const std::string& arg) {
  arguments_.push_back(arg);
}

void ProcessImpl::RedirectOutput(const std::string& output_file) {
  output_file_ = output_file;
}

void ProcessImpl::RedirectUsingPipe(int child_fd, bool is_input) {
  PipeInfo info;
  info.is_input_ = is_input;
  info.is_bound_ = false;
  pipe_map_[child_fd] = info;
}

void ProcessImpl::BindFd(int parent_fd, int child_fd) {
  PipeInfo info;
  info.is_bound_ = true;

  // info.child_fd_ is the 'child half' of the pipe, which gets dup2()ed into
  // place over child_fd. Since we already have the child we want to dup2() into
  // place, we can set info.child_fd_ to parent_fd and leave info.parent_fd_
  // invalid.
  info.child_fd_ = parent_fd;
  info.parent_fd_ = -1;
  pipe_map_[child_fd] = info;
}

void ProcessImpl::SetUid(uid_t uid) {
  uid_ = uid;
}

void ProcessImpl::SetGid(gid_t gid) {
  gid_ = gid;
}

void ProcessImpl::SetPreExecCallback(const PreExecCallback& cb) {
  pre_exec_ = cb;
}

int ProcessImpl::GetPipe(int child_fd) {
  PipeMap::iterator i = pipe_map_.find(child_fd);
  if (i == pipe_map_.end())
    return -1;
  else
    return i->second.parent_fd_;
}

bool ProcessImpl::PopulatePipeMap() {
  // Verify all target fds are already open.  With this assumption we
  // can be sure that the pipe fds created below do not overlap with
  // any of the target fds which simplifies how we dup2 to them.  Note
  // that multi-threaded code could close i->first between this loop
  // and the next.
  for (PipeMap::iterator i = pipe_map_.begin(); i != pipe_map_.end(); ++i) {
    struct stat stat_buffer;
    if (fstat(i->first, &stat_buffer) < 0) {
      int saved_errno = errno;
      LOG(ERROR) << "Unable to fstat fd " << i->first << ": " << saved_errno;
      return false;
    }
  }

  for (PipeMap::iterator i = pipe_map_.begin(); i != pipe_map_.end(); ++i) {
    if (i->second.is_bound_) {
      // already have a parent fd, and the child fd gets dup()ed later.
      continue;
    }
    int pipefds[2];
    if (pipe(pipefds) < 0) {
      int saved_errno = errno;
      LOG(ERROR) << "pipe call failed with: " << saved_errno;
      return false;
    }
    if (i->second.is_input_) {
      // pipe is an input from the prospective of the child.
      i->second.parent_fd_ = pipefds[1];
      i->second.child_fd_ = pipefds[0];
    } else {
      i->second.parent_fd_ = pipefds[0];
      i->second.child_fd_ = pipefds[1];
    }
  }
  return true;
}

bool ProcessImpl::Start() {
  scoped_array<char*> argv(new char*[arguments_.size() + 1]);

  for (size_t i = 0; i < arguments_.size(); ++i)
    argv[i] = const_cast<char*>(arguments_[i].c_str());

  argv[arguments_.size()] = NULL;

  if (!PopulatePipeMap()) {
    LOG(ERROR) << "Failing to start because pipe creation failed";
    return false;
  }

  pid_t pid = fork();
  int saved_errno = errno;
  if (pid < 0) {
    LOG(ERROR) << "Fork failed: " << saved_errno;
    Reset(0);
    return false;
  }

  if (pid == 0) {
    // Executing inside the child process.
    // Close parent's side of the child pipes. dup2 ours into place and
    // then close our ends.
    for (PipeMap::iterator i = pipe_map_.begin(); i != pipe_map_.end(); ++i) {
      HANDLE_EINTR(close(i->second.parent_fd_));
      HANDLE_EINTR(dup2(i->second.child_fd_, i->first));
    }
    // Defer the actual close() of the child fd until afterward; this lets the
    // same child fd be bound to multiple fds using BindFd
    for (PipeMap::iterator i = pipe_map_.begin(); i != pipe_map_.end(); ++i) {
      HANDLE_EINTR(close(i->second.child_fd_));
    }
    if (!output_file_.empty()) {
      int output_handle = HANDLE_EINTR(
          open(output_file_.c_str(),
               O_CREAT | O_WRONLY | O_TRUNC | O_EXCL, 0666));
      int saved_errno = errno;
      if (output_handle < 0) {
        LOG(ERROR) << "Could not create " << output_file_
                   << ": " << saved_errno;
        // Avoid exit() to avoid atexit handlers from parent.
        _exit(kErrorExitStatus);
      }
      HANDLE_EINTR(dup2(output_handle, STDOUT_FILENO));
      HANDLE_EINTR(dup2(output_handle, STDERR_FILENO));
      // Only close output_handle if it does not happen to be one of
      // the two standard file descriptors we are trying to redirect.
      if (output_handle != STDOUT_FILENO && output_handle != STDERR_FILENO) {
        HANDLE_EINTR(close(output_handle));
      }
    }
    if (gid_ != static_cast<gid_t>(-1) && setresgid(gid_, gid_, gid_) < 0) {
      int saved_errno = errno;
      LOG(ERROR) << "Unable to set GID to " << gid_ << ": " << saved_errno;
      _exit(kErrorExitStatus);
    }
    if (uid_ != static_cast<uid_t>(-1) && setresuid(uid_, uid_, uid_) < 0) {
      int saved_errno = errno;
      LOG(ERROR) << "Unable to set UID to " << uid_ << ": " << saved_errno;
      _exit(kErrorExitStatus);
    }
    if (!pre_exec_.Run()) {
      LOG(ERROR) << "Pre-exec callback failed";
      _exit(kErrorExitStatus);
    }
    execv(argv[0], &argv[0]);
    saved_errno = errno;
    LOG(ERROR) << "Exec of " << argv[0] << " failed: " << saved_errno;
    _exit(kErrorExitStatus);
  } else {
    // Still executing inside the parent process with known child pid.
    arguments_.clear();
    UpdatePid(pid);
    // Close our copy of child side pipes.
    for (PipeMap::iterator i = pipe_map_.begin(); i != pipe_map_.end(); ++i) {
      HANDLE_EINTR(close(i->second.child_fd_));
    }
  }
  return true;
}

int ProcessImpl::Wait() {
  int status = 0;
  if (pid_ == 0) {
    LOG(ERROR) << "Process not running";
    return -1;
  }
  if (HANDLE_EINTR(waitpid(pid_, &status, 0)) < 0) {
    int saved_errno = errno;
    LOG(ERROR) << "Problem waiting for pid " << pid_ << ": " << saved_errno;
    return -1;
  }
  pid_t old_pid = pid_;
  // Update the pid to 0 - do not Reset as we do not want to try to
  // kill the process that has just exited.
  UpdatePid(0);
  if (!WIFEXITED(status)) {
    DCHECK(WIFSIGNALED(status)) << old_pid
                                << " neither exited, nor died on a signal?";
    LOG(ERROR) << "Process " << old_pid << " did not exit normally: "
               << WTERMSIG(status);
    return -1;
  }
  return WEXITSTATUS(status);
}

int ProcessImpl::Run() {
  if (!Start()) {
    return -1;
  }
  return Wait();
}

pid_t ProcessImpl::pid() {
  return pid_;
}

bool ProcessImpl::Kill(int signal, int timeout) {
  if (pid_ == 0) {
    // Passing pid == 0 to kill is committing suicide.  Check specifically.
    LOG(ERROR) << "Process not running";
    return false;
  }
  if (kill(pid_, signal) < 0) {
    int saved_errno = errno;
    LOG(ERROR) << "Unable to send signal to " << pid_ << " error "
               << saved_errno;
    return false;
  }
  base::TimeTicks start_signal = base::TimeTicks::Now();
  do {
    int status = 0;
    pid_t w = waitpid(pid_, &status, WNOHANG);
    int saved_errno = errno;
    if (w < 0) {
      if (saved_errno == ECHILD)
        return true;
      LOG(ERROR) << "Waitpid returned " << w << ", errno " << saved_errno;
      return false;
    }
    if (w > 0) {
      Reset(0);
      return true;
    }
    usleep(100);
  } while ((base::TimeTicks::Now() - start_signal).InSecondsF() <= timeout);
  LOG(INFO) << "process " << pid_ << " did not exit from signal " << signal
            << " in " << timeout << " seconds";
  return false;
}

void ProcessImpl::UpdatePid(pid_t new_pid) {
  pid_ = new_pid;
}

void ProcessImpl::Reset(pid_t new_pid) {
  arguments_.clear();
  // Close our side of all pipes to this child giving the child to
  // handle sigpipes and shutdown nicely, though likely it won't
  // have time.
  for (PipeMap::iterator i = pipe_map_.begin(); i != pipe_map_.end(); ++i)
    HANDLE_EINTR(close(i->second.parent_fd_));
  pipe_map_.clear();
  if (pid_)
    Kill(SIGKILL, 0);
  UpdatePid(new_pid);
}

bool ProcessImpl::ResetPidByFile(const std::string& pid_file) {
  std::string contents;
  if (!file_util::ReadFileToString(FilePath(pid_file), &contents)) {
    LOG(ERROR) << "Could not read pid file" << pid_file;
    return false;
  }
  TrimWhitespaceASCII(contents, TRIM_TRAILING, &contents);
  int64 pid_int64 = 0;
  if (!base::StringToInt64(contents, &pid_int64)) {
    LOG(ERROR) << "Unexpected pid file contents";
    return false;
  }
  Reset(pid_int64);
  return true;
}

pid_t ProcessImpl::Release() {
  pid_t old_pid = pid_;
  pid_ = 0;
  return old_pid;
}

}  // namespace chromeos
