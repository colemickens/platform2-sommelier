// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcontainer/libcontainer_util.h"

#include <errno.h>
#include <fcntl.h>
#if USE_device_mapper
#include <libdevmapper.h>
#endif
#include <linux/loop.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback_helpers.h>
#include <base/files/scoped_file.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/posix/eintr_wrapper.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

// New cgroup namespace might not be in linux-headers yet.
#ifndef CLONE_NEWCGROUP
#define CLONE_NEWCGROUP 0x02000000
#endif

namespace libcontainer {

namespace {

constexpr base::FilePath::CharType kLoopdevCtlPath[] =
    FILE_PATH_LITERAL("/dev/loop-control");
#if USE_device_mapper
constexpr base::FilePath::CharType kDevMapperPath[] =
    FILE_PATH_LITERAL("/dev/mapper/");
#endif

// Gets the namespace name for |nstype|.
std::string GetNamespaceNameForType(int nstype) {
  switch (nstype) {
    case CLONE_NEWCGROUP:
      return "cgroup";
    case CLONE_NEWIPC:
      return "ipc";
    case CLONE_NEWNET:
      return "net";
    case CLONE_NEWNS:
      return "mnt";
    case CLONE_NEWPID:
      return "pid";
    case CLONE_NEWUSER:
      return "user";
    case CLONE_NEWUTS:
      return "uts";
  }
  return std::string();
}

// Helper function that runs |callback| in all the namespaces identified by
// |nstypes|.
bool RunInNamespacesHelper(HookCallback callback,
                           std::vector<int> nstypes,
                           pid_t container_pid) {
  pid_t child = fork();
  if (child < 0) {
    PLOG_PRESERVE(ERROR) << "Failed to fork()";
    return false;
  }

  if (child == 0) {
    for (const int nstype : nstypes) {
      std::string nstype_name = GetNamespaceNameForType(nstype);
      if (nstype_name.empty()) {
        LOG(ERROR) << "Invalid namespace type " << nstype;
        _exit(-1);
      }
      base::FilePath ns_path = base::FilePath(base::StringPrintf(
          "/proc/%d/ns/%s", container_pid, nstype_name.c_str()));
      base::ScopedFD ns_fd(open(ns_path.value().c_str(), O_RDONLY));
      if (!ns_fd.is_valid()) {
        PLOG_PRESERVE(ERROR) << "Failed to open " << ns_path.value();
        _exit(-1);
      }
      if (setns(ns_fd.get(), nstype)) {
        PLOG_PRESERVE(ERROR) << "Failed to enter PID " << container_pid << "'s "
                             << nstype_name << " namespace";
        _exit(-1);
      }
    }

    // Preserve normal POSIX semantics of calling exit(2) with 0 for success and
    // non-zero for failure.
    _exit(callback.Run(container_pid) ? 0 : 1);
  }

  int status;
  if (HANDLE_EINTR(waitpid(child, &status, 0)) < 0) {
    PLOG_PRESERVE(ERROR) << "Failed to wait for callback";
    return false;
  }
  if (!WIFEXITED(status)) {
    LOG(ERROR) << "Callback terminated abnormally: " << std::hex << status;
    return false;
  }
  return static_cast<int8_t>(WEXITSTATUS(status)) == 0;
}

}  // namespace

SaveErrno::SaveErrno() : saved_errno_(errno) {}

SaveErrno::~SaveErrno() {
  errno = saved_errno_;
}

WaitablePipe::WaitablePipe() {
  if (pipe2(pipe_fds, O_CLOEXEC) < 0)
    PLOG(FATAL) << "Failed to create pipe";
}

WaitablePipe::~WaitablePipe() {
  if (pipe_fds[0] != -1)
    close(pipe_fds[0]);
  if (pipe_fds[1] != -1)
    close(pipe_fds[1]);
}

WaitablePipe::WaitablePipe(WaitablePipe&& other) {
  pipe_fds[0] = pipe_fds[1] = -1;
  std::swap(pipe_fds, other.pipe_fds);
}

void WaitablePipe::Wait() {
  char buf;

  close(pipe_fds[1]);
  HANDLE_EINTR(read(pipe_fds[0], &buf, sizeof(buf)));
  close(pipe_fds[0]);

  pipe_fds[0] = pipe_fds[1] = -1;
}

void WaitablePipe::Signal() {
  close(pipe_fds[0]);
  close(pipe_fds[1]);

  pipe_fds[0] = pipe_fds[1] = -1;
}

HookState::HookState() = default;
HookState::~HookState() = default;

HookState::HookState(HookState&& state) = default;

bool HookState::InstallHook(struct minijail* j, minijail_hook_event_t event) {
  if (installed_) {
    LOG(ERROR) << "Failed to install hook: already installed";
    return false;
  }

  // All these fds will be closed in WaitHook in the child process.
  for (size_t i = 0; i < 2; ++i) {
    if (minijail_preserve_fd(j, reached_pipe_.pipe_fds[i],
                             reached_pipe_.pipe_fds[i]) != 0) {
      LOG(ERROR) << "Failed to preserve reached pipe FDs to install hook";
      return false;
    }
    if (minijail_preserve_fd(j, ready_pipe_.pipe_fds[i],
                             ready_pipe_.pipe_fds[i]) != 0) {
      LOG(ERROR) << "Failed to preserve ready pipe FDs to install hook";
      return false;
    }
  }

  if (minijail_add_hook(j, &HookState::WaitHook, this, event) != 0) {
    LOG(ERROR) << "Failed to add hook";
    return false;
  }

  installed_ = true;
  return true;
}

bool HookState::WaitForHookAndRun(const std::vector<HookCallback>& callbacks,
                                  pid_t container_pid) {
  if (!installed_) {
    LOG(ERROR) << "Failed to wait for hook: not installed";
    return false;
  }
  reached_pipe_.Wait();
  base::ScopedClosureRunner teardown(
      base::Bind(&WaitablePipe::Signal, base::Unretained(&ready_pipe_)));

  for (auto& callback : callbacks) {
    bool success = callback.Run(container_pid);
    if (!success)
      return false;
  }
  return true;
}

// static
int HookState::WaitHook(void* payload) {
  HookState* self = reinterpret_cast<HookState*>(payload);

  self->reached_pipe_.Signal();
  self->ready_pipe_.Wait();

  return 0;
}

bool GetUsernsOutsideId(const std::string& map, int id, int* id_out) {
  if (map.empty()) {
    if (id_out)
      *id_out = id;
    return true;
  }

  std::string map_copy = map;
  base::StringPiece map_piece(map_copy);

  for (const auto& mapping : base::SplitStringPiece(
           map_piece, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    std::vector<base::StringPiece> tokens = base::SplitStringPiece(
        mapping, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (tokens.size() != 3) {
      LOG(ERROR) << "Malformed ugid mapping: '" << mapping << "'";
      return false;
    }

    uint32_t inside, outside, length;
    if (!base::StringToUint(tokens[0], &inside) ||
        !base::StringToUint(tokens[1], &outside) ||
        !base::StringToUint(tokens[2], &length)) {
      LOG(ERROR) << "Malformed ugid mapping: '" << mapping << "'";
      return false;
    }

    if (id >= inside && id <= (inside + length)) {
      if (id_out)
        *id_out = (id - inside) + outside;
      return true;
    }
  }
  VLOG(1) << "ugid " << id << " not found in mapping";

  return false;
}

bool MakeDir(const base::FilePath& path, int uid, int gid, int mode) {
  if (mkdir(path.value().c_str(), mode)) {
    PLOG_PRESERVE(ERROR) << "Failed to mkdir " << path.value();
    return false;
  }
  if (chmod(path.value().c_str(), mode)) {
    PLOG_PRESERVE(ERROR) << "Failed to chmod " << path.value();
    return false;
  }
  if (chown(path.value().c_str(), uid, gid)) {
    PLOG_PRESERVE(ERROR) << "Failed to chown " << path.value();
    return false;
  }
  return true;
}

bool TouchFile(const base::FilePath& path, int uid, int gid, int mode) {
  base::ScopedFD fd(open(path.value().c_str(), O_RDWR | O_CREAT, mode));
  if (!fd.is_valid()) {
    PLOG_PRESERVE(ERROR) << "Failed to create " << path.value();
    return false;
  }
  if (fchown(fd.get(), uid, gid)) {
    PLOG_PRESERVE(ERROR) << "Failed to chown " << path.value();
    return false;
  }
  return true;
}

bool LoopdevSetup(const base::FilePath& source,
                  base::FilePath* loopdev_path_out) {
  base::ScopedFD source_fd(open(source.value().c_str(), O_RDONLY | O_CLOEXEC));
  if (!source_fd.is_valid()) {
    PLOG_PRESERVE(ERROR) << "Failed to open " << source.value();
    return false;
  }

  base::ScopedFD control_fd(
      open(kLoopdevCtlPath, O_RDWR | O_NOFOLLOW | O_CLOEXEC));
  if (!control_fd.is_valid()) {
    PLOG_PRESERVE(ERROR) << "Failed to open " << source.value();
    return false;
  }

  while (true) {
    int num = ioctl(control_fd.get(), LOOP_CTL_GET_FREE);
    if (num < 0) {
      PLOG_PRESERVE(ERROR) << "Failed to open " << source.value();
      return false;
    }

    base::FilePath loopdev_path(base::StringPrintf("/dev/loop%i", num));
    base::ScopedFD loop_fd(
        open(loopdev_path.value().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
    if (!loop_fd.is_valid()) {
      PLOG_PRESERVE(ERROR) << "Failed to open " << loopdev_path.value();
      return false;
    }

    if (ioctl(loop_fd.get(), LOOP_SET_FD, source_fd.get()) == 0) {
      *loopdev_path_out = loopdev_path;
      break;
    }

    if (errno != EBUSY) {
      PLOG_PRESERVE(ERROR) << "Failed to ioctl(LOOP_SET_FD) "
                           << loopdev_path.value();
      return false;
    }
  }

  return true;
}

bool LoopdevDetach(const base::FilePath& loopdev) {
  base::ScopedFD fd(
      open(loopdev.value().c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC));
  if (!fd.is_valid()) {
    PLOG_PRESERVE(ERROR) << "Failed to open " << loopdev.value();
    return false;
  }
  if (ioctl(fd.get(), LOOP_CLR_FD) < 0) {
    PLOG_PRESERVE(ERROR) << "Failed to ioctl(LOOP_CLR_FD) for "
                         << loopdev.value();
    return false;
  }

  return true;
}

bool DeviceMapperSetup(const base::FilePath& source,
                       const std::string& verity_cmdline,
                       base::FilePath* dm_path_out,
                       std::string* dm_name_out) {
#if USE_device_mapper
  // Normalize the name into something unique-esque.
  std::string dm_name =
      base::StringPrintf("cros-containers-%s", source.value().c_str());
  base::ReplaceChars(dm_name, "/", "_", &dm_name);

  // Get the /dev path for the higher levels to mount.
  base::FilePath dm_path = base::FilePath(kDevMapperPath).Append(dm_name);

  // Insert the source path in the verity command line.
  std::string verity = verity_cmdline;
  base::ReplaceSubstringsAfterOffset(&verity, 0, "@DEV@", source.value());

  // Extract the first three parameters for dm-verity settings.
  char ttype[20];
  unsigned long long start, size;
  int n;
  if (sscanf(verity.c_str(), "%llu %llu %10s %n", &start, &size, ttype, &n) !=
      3) {
    PLOG_PRESERVE(ERROR) << "Malformed verity string " << verity;
    return false;
  }

  /* Finally create the device mapper. */
  std::unique_ptr<struct dm_task, decltype(&dm_task_destroy)> dmt(
      dm_task_create(DM_DEVICE_CREATE), &dm_task_destroy);
  if (dmt == nullptr) {
    PLOG_PRESERVE(ERROR) << "Failed to dm_task_create() for " << source.value();
    return false;
  }

  if (dm_task_set_name(dmt.get(), dm_name.c_str()) != 0) {
    PLOG_PRESERVE(ERROR) << "Failed to dm_task_set_name() for "
                         << source.value();
    return false;
  }

  if (dm_task_set_ro(dmt.get()) != 0) {
    PLOG_PRESERVE(ERROR) << "Failed to dm_task_set_ro() for " << source.value();
    return false;
  }

  if (dm_task_add_target(dmt.get(), start, size, ttype, verity.c_str() + n) !=
      0) {
    PLOG_PRESERVE(ERROR) << "Failed to dm_task_add_target() for "
                         << source.value();
    return false;
  }

  uint32_t cookie = 0;
  if (dm_task_set_cookie(dmt.get(), &cookie, 0) != 0) {
    PLOG_PRESERVE(ERROR) << "Failed to dm_task_set_cookie() for "
                         << source.value();
    return false;
  }

  if (dm_task_run(dmt.get()) != 0) {
    PLOG_PRESERVE(ERROR) << "Failed to dm_task_run() for " << source.value();
    return false;
  }

  /* Make sure the node exists before we continue. */
  dm_udev_wait(cookie);

  *dm_path_out = dm_path;
  *dm_name_out = dm_name;
#endif
  return true;
}

// Tear down the device mapper target.
bool DeviceMapperDetach(const std::string& dm_name) {
#if USE_device_mapper
  struct dm_task* dmt = dm_task_create(DM_DEVICE_REMOVE);
  if (dmt == nullptr) {
    PLOG_PRESERVE(ERROR) << "Failed to dm_task_run() for " << dm_name;
    return false;
  }

  base::ScopedClosureRunner teardown(
      base::Bind(base::IgnoreResult(&dm_task_destroy), base::Unretained(dmt)));

  if (dm_task_set_name(dmt, dm_name.c_str()) != 0) {
    PLOG_PRESERVE(ERROR) << "Failed to dm_task_set_name() for " << dm_name;
    return false;
  }

  if (dm_task_run(dmt) != 0) {
    PLOG_PRESERVE(ERROR) << "Failed to dm_task_run() for " << dm_name;
    return false;
  }
#endif
  return true;
}

bool MountExternal(const std::string& src,
                   const std::string& dest,
                   const std::string& type,
                   unsigned long flags,
                   const std::string& data) {
  bool remount_ro = false;

  // R/O bind mounts have to be remounted since 'bind' and 'ro' can't both be
  // specified in the original bind mount.  Remount R/O after the initial mount.
  if ((flags & MS_BIND) && (flags & MS_RDONLY)) {
    remount_ro = true;
    flags &= ~MS_RDONLY;
  }

  if (mount(src.c_str(), dest.c_str(), type.c_str(), flags,
            data.empty() ? nullptr : data.c_str()) != 0) {
    PLOG_PRESERVE(ERROR) << "Failed to mount " << src << " to " << dest;
    return false;
  }

  if (remount_ro) {
    flags |= MS_RDONLY;
    if (mount(src.c_str(), dest.c_str(), nullptr, flags | MS_REMOUNT,
              data.empty() ? nullptr : data.c_str()) != 0) {
      PLOG_PRESERVE(ERROR) << "Failed to remount " << src << " to " << dest;
      return false;
    }
  }

  return true;
}

HookCallback AdaptCallbackToRunInNamespaces(HookCallback callback,
                                            std::vector<int> nstypes) {
  return base::Bind(&RunInNamespacesHelper,
                    base::Passed(std::move(callback)),
                    base::Passed(std::move(nstypes)));
}

}  // namespace libcontainer
