// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vm_tools/maitred/init.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <sys/mount.h>
#include <sys/signalfd.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <list>
#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/location.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/message_loop/message_loop.h>
#include <base/posix/eintr_wrapper.h>
#include <base/time/time.h>

using std::string;

namespace vm_tools {
namespace maitred {
namespace {

// Path to the root directory for cgroups.
constexpr char kCgroupRootDir[] = "/sys/fs/cgroup";

// Name of the directory in every cgroup subsystem for dealing with containers.
constexpr char kCgroupContainerSuffix[] = "chronos_containers";

// Default value of the PATH environment variable.
constexpr char kDefaultPath[] = "/usr/bin:/usr/sbin:/bin:/sbin";

// Uid and Gid for the chronos user and group, respectively.
constexpr uid_t kChronosUid = 1000;
constexpr gid_t kChronosGid = 1000;

// Retry threshould and duration for processes that respawn.  If a process needs
// to be respawned more than kMaxRespawnCount times in the last
// kRespawnWindowSeconds, then it will stop being respawned.
constexpr size_t kMaxRespawnCount = 10;
constexpr base::TimeDelta kRespawnWindowSeconds =
    base::TimeDelta::FromSeconds(30);

// Mounts that must be created on boot.
constexpr struct {
  const char* source;
  const char* target;
  const char* fstype;
  unsigned long flags;  // NOLINT(runtime/int)
  const void* data;
} mounts[] = {
    {
        .source = "proc",
        .target = "/proc",
        .fstype = "proc",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = nullptr,
    },
    {
        .source = "sys",
        .target = "/sys",
        .fstype = "sysfs",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = nullptr,
    },
    {
        .source = "tmp",
        .target = "/tmp",
        .fstype = "tmpfs",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = nullptr,
    },
    {
        .source = "run",
        .target = "/run",
        .fstype = "tmpfs",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = "mode=0755",
    },
    {
        .source = "shmfs",
        .target = "/dev/shm",
        .fstype = "tmpfs",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = nullptr,
    },
    {
        .source = "devpts",
        .target = "/dev/pts",
        .fstype = "devpts",
        .flags = MS_NOSUID | MS_NOEXEC,
        .data = "gid=5,mode=0620,ptmxmode=666",
    },
    {
        .source = "var",
        .target = "/var",
        .fstype = "tmpfs",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = "mode=0755",
    },
    {
        .source = "none",
        .target = kCgroupRootDir,
        .fstype = "tmpfs",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = "mode=0755",
    },
    {
        .source = "cgroup",
        .target = "/sys/fs/cgroup/cpu",
        .fstype = "cgroup",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = "cpu",
    },
    {
        .source = "cgroup",
        .target = "/sys/fs/cgroup/freezer",
        .fstype = "cgroup",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = "freezer",
    },
    {
        .source = "cgroup",
        .target = "/sys/fs/cgroup/devices",
        .fstype = "cgroup",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = "devices",
    },
    {
        .source = "cgroup",
        .target = "/sys/fs/cgroup/cpuacct",
        .fstype = "cgroup",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = "cpuacct",
    },
    {
        .source = "cgroup",
        .target = "/sys/fs/cgroup/cpuset",
        .fstype = "cgroup",
        .flags = MS_NOSUID | MS_NODEV | MS_NOEXEC,
        .data = "cpuset,noprefix",
    },
};

// Directories to be created on boot.  These are created only after all the
// mounts have completed.
constexpr struct {
  const char* path;
  mode_t mode;
} boot_dirs[] = {
    {
        .path = "/run/lock",
        .mode = 01777,
    },
    {
        .path = "/run/sshd",
        .mode = 0755,
    },
    {
        .path = "/var/cache",
        .mode = 0755,
    },
    {
        .path = "/var/db",
        .mode = 0755,
    },
    {
        .path = "/var/empty",
        .mode = 0755,
    },
    {
        .path = "/var/log",
        .mode = 0755,
    },
    {
        .path = "/var/spool",
        .mode = 0755,
    },
    {
        .path = "/var/lib",
        .mode = 0755,
    },
    {
        .path = "/var/lib/misc",
        .mode = 0755,
    },
    {
        .path = "/sys/fs/cgroup/cpu/chronos_containers",
        .mode = 0755,
    },
    {
        .path = "/sys/fs/cgroup/devices/chronos_containers",
        .mode = 0755,
    },
    {
        .path = "/sys/fs/cgroup/freezer/chronos_containers",
        .mode = 0755,
    },
    {
        .path = "/sys/fs/cgroup/cpuacct/chronos_containers",
        .mode = 0755,
    },
    {
        .path = "/sys/fs/cgroup/cpuset/chronos_containers",
        .mode = 0755,
    },
};

// Number of defined signals that the process could receive (not including
// real time signals).
constexpr int kNumSignals = 32;

// Resets all signal handlers to the default.  This is called in child processes
// immediately before exec-ing so that signals are not unexpectedly blocked.
// Returns true if all signal handlers were successfully set to their default
// dispositions and false if even one of them failed.  Callers should inspect
// errno for the error.
bool ResetSignalHandlers() {
  for (int signo = 1; signo < kNumSignals; ++signo) {
    if (signo == SIGKILL || signo == SIGSTOP) {
      // sigaction returns an error if we try to set the disposition of these
      // signals to SIG_DFL.
      continue;
    }
    struct sigaction act = {
      .sa_handler = SIG_DFL,
      .sa_flags = 0,
    };
    sigemptyset(&act.sa_mask);

    if (sigaction(signo, &act, nullptr) != 0) {
      return false;
    }
  }

  return true;
}

// Recursively changes the owner and group for all files and directories in
// |path| (including |path|) to |uid| and |gid|, respectively.
bool ChangeOwnerAndGroup(base::FilePath path, uid_t uid, gid_t gid) {
  base::FileEnumerator enumerator(
      path, true /*recursive*/,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    if (chown(current.value().c_str(), uid, gid) != 0) {
      PLOG(ERROR) << "Failed to change owner and group for " << current.value()
                  << " to " << uid << ":" << gid;
      return false;
    }
  }

  // FileEnumerator doesn't include the root path so change it manually here.
  if (chown(path.value().c_str(), uid, gid) != 0) {
    PLOG(ERROR) << "Failed to change owner and group for " << path.value()
                << " to " << uid << ":" << gid;
    return false;
  }

  return true;
}

}  // namespace

class Init::Worker : public base::MessageLoopForIO::Watcher {
 public:
  // Relevant information about processes launched by this process.
  struct ChildInfo {
    std::vector<string> argv;
    std::map<string, string> env;
    bool respawn;
    bool use_console;
    bool wait_for_exit;

    std::list<base::Time> spawn_times;
  };

  Worker() = default;
  ~Worker() = default;

  // Start the worker.  This will set up a signalfd for receiving SIGHCHLD
  // events.
  void Start();

  // Actually spawns a child process.  Waits until it receives confirmation from
  // the child that the requested program was actually started and fills in
  // |exit_info| with information about the process.  Additionally if
  // |info.wait_for_exit| is true, then waits until the child process exits or
  // is killed before returning.
  void Spawn(struct ChildInfo info, int semfd, ProcessExitInfo* exit_info);

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 private:
  // File descriptor on which we will receive SIGCHLD events.
  base::ScopedFD signal_fd_;
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;

  // Information about processes launched by this process.
  std::map<pid_t, ChildInfo> children_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

void Init::Worker::Start() {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);

  // Block SIGCHLD so that we can get it via the signalfd.
  if (sigprocmask(SIG_BLOCK, &mask, nullptr) != 0) {
    PLOG(ERROR) << "Failed to block SIGCHLD";
  }

  signal_fd_.reset(signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK));
  PCHECK(signal_fd_.is_valid()) << "Unable to create signal fd";

  bool ret = base::MessageLoopForIO::current()->WatchFileDescriptor(
      signal_fd_.get(), true /*persistent*/, base::MessageLoopForIO::WATCH_READ,
      &watcher_, this);

  CHECK(ret) << "Failed to watch SIGHCHLD file descriptor";
}

void Init::Worker::Spawn(struct ChildInfo info,
                         int semfd,
                         ProcessExitInfo* exit_info) {
  DCHECK_GT(info.argv.size(), 0);

  // Build the argv.
  std::vector<const char*> argv(info.argv.size());
  std::transform(info.argv.begin(), info.argv.end(), argv.begin(),
                 [](const string& arg) -> const char* { return arg.c_str(); });
  argv.emplace_back(nullptr);

  // Block all signals before forking to prevent signals from arriving in the
  // child.
  sigset_t mask, omask;
  sigfillset(&mask);
  sigprocmask(SIG_BLOCK, &mask, &omask);

  pid_t pid = fork();
  if (pid < 0) {
    PLOG(ERROR) << "Failed to fork";
    return;
  }

  if (pid == 0) {
    // Create a new session and process group.
    if (setsid() == -1) {
      _exit(errno);
    }

    // File descriptor for the child's stdio.
    const char* console = info.use_console ? "/dev/console" : "/dev/null";
    int fd = open(console, O_RDWR | O_NOCTTY);
    if (fd < 0) {
      _exit(errno);
    }

    // Override the parent's stdio fds with the console fd.
    for (int newfd = 0; newfd < 3; ++newfd) {
      if (dup2(fd, newfd) < 0) {
        _exit(errno);
      }
    }

    // Close the console fd, if necessary.
    if (fd >= 3) {
      close(fd);
    }

    // Set the umask back to a reasonable default.
    umask(0022);

    // Set the environment variables.
    for (const auto& pair : info.env) {
      if (setenv(pair.first.c_str(), pair.second.c_str(), 1) != 0) {
        _exit(errno);
      }
    }

    // Restore signal handlers and unblock all signals.
    if (!ResetSignalHandlers()) {
      _exit(errno);
    }
    sigprocmask(SIG_UNBLOCK, &mask, nullptr);

    // Launch the process.
    execvp(argv[0], const_cast<char* const*>(argv.data()));

    // execvp never returns except in case of an error.
    _exit(errno);
  }

  // Parent process.
  if (info.wait_for_exit) {
    DCHECK_NE(semfd, -1);
    DCHECK(exit_info);

    int status = 0;
    pid_t child = waitpid(pid, &status, 0);
    DCHECK_EQ(child, pid);

    if (WIFEXITED(status)) {
      exit_info->reason = ProcessExitReason::EXITED;
      exit_info->status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      exit_info->reason = ProcessExitReason::SIGNALED;
      exit_info->status = WTERMSIG(status);
    } else {
      exit_info->reason = ProcessExitReason::UNKNOWN;
    }

    uint64_t done = 1;
    ssize_t ret = write(semfd, &done, sizeof(done));
    DCHECK_EQ(ret, sizeof(done));
  } else {
    info.spawn_times.emplace_back(base::Time::Now());

    // result is a pair<iterator, bool>.
    auto result = children_.emplace(pid, std::move(info));
    DCHECK(result.second);
  }

  // Restore the signal mask.
  sigprocmask(SIG_SETMASK, &omask, nullptr);
}

void Init::Worker::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, signal_fd_.get());

  // Pull information about the signal sender out of the fd to ack the signal.
  struct signalfd_siginfo siginfo;
  if (HANDLE_EINTR(read(signal_fd_.get(), &siginfo, sizeof(siginfo))) !=
      sizeof(siginfo)) {
    PLOG(ERROR) << "Failed to read from signalfd";
    return;
  }
  DCHECK_EQ(siginfo.ssi_signo, SIGCHLD);

  // We can't just rely on the information in the siginfo structure because
  // more than one child may have exited but only one SIGCHLD will be
  // generated.
  while (true) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (pid <= 0) {
      if (pid == -1) {
        PLOG(ERROR) << "Unable to reap child processes";
      }
      break;
    }

    // See if this is a process we launched.
    struct ChildInfo info = {};
    auto iter = children_.find(pid);
    if (iter != children_.end()) {
      info = std::move(iter->second);
      children_.erase(iter);
    }

    if (WIFEXITED(status)) {
      LOG(INFO) << (info.argv.size() == 0 ? "<unknown process>"
                                          : info.argv[0].c_str())
                << " (" << pid << ") exited with status "
                << WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      LOG(INFO) << (info.argv.size() == 0 ? "<unknown process>"
                                          : info.argv[0].c_str())
                << " (" << pid << ") killed by signal " << WTERMSIG(status)
                << (WCOREDUMP(status) ? " (core dumped)" : "");
    } else {
      LOG(WARNING) << "Unknown exit status " << status << " for process "
                   << pid;
    }

    if (!info.respawn) {
      continue;
    }

    // The process needs to be respawned.  First remove any spawn times older
    // than the respawn counter window.
    base::Time now = base::Time::Now();
    while (info.spawn_times.size() > 0 &&
           now - info.spawn_times.front() > kRespawnWindowSeconds) {
      info.spawn_times.pop_front();
    }

    // Check if the process has respawned too often.
    if (info.spawn_times.size() >= kMaxRespawnCount) {
      LOG(WARNING) << info.argv[0] << " respawning too frequently; stopped";
      continue;
    }

    // Respawn the process.
    Spawn(std::move(info), -1, nullptr);
  }
}

void Init::Worker::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

std::unique_ptr<Init> Init::Create() {
  auto init = base::WrapUnique<Init>(new Init());

  if (!init->Setup()) {
    init.reset();
  }

  return init;
}

Init::~Init() {
  if (worker_) {
    // worker_ is created after worker_thread_ is started so we don't need to
    // check if it is running.
    worker_thread_.task_runner()->DeleteSoon(FROM_HERE, worker_.release());
  }
}

bool Init::Spawn(std::vector<string> argv,
                 std::map<string, string> env,
                 bool respawn,
                 bool use_console,
                 bool wait_for_exit,
                 ProcessExitInfo* exit_info) {
  CHECK(!argv.empty());
  CHECK(!(respawn && wait_for_exit));

  if (!worker_) {
    // If there's no worker then we are currently in the process of shutting
    // down.
    return false;
  }

  struct Worker::ChildInfo info = {.argv = std::move(argv),
                                   .env = std::move(env),
                                   .respawn = respawn,
                                   .use_console = use_console,
                                   .wait_for_exit = wait_for_exit};
  if (!wait_for_exit) {
    return worker_thread_.task_runner()->PostTask(
        FROM_HERE, base::Bind(&Worker::Spawn, base::Unretained(worker_.get()),
                              base::Passed(std::move(info)), -1, nullptr));
  }

  // Need to wait for this process to exit.
  CHECK(exit_info);

  base::ScopedFD sem(eventfd(0, EFD_CLOEXEC | EFD_SEMAPHORE));
  if (!sem.is_valid()) {
    PLOG(ERROR) << "Failed to create semaphore eventfd";
    return false;
  }

  bool ret = worker_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Worker::Spawn, base::Unretained(worker_.get()),
                 base::Passed(std::move(info)), sem.get(), exit_info));
  if (!ret) {
    return false;
  }

  uint64_t done = 0;
  ssize_t count = HANDLE_EINTR(read(sem.get(), &done, sizeof(done)));
  DCHECK_EQ(count, sizeof(done));
  DCHECK_EQ(done, 1);

  return true;
}

bool Init::Setup() {
  // Set the umask properly or the directory modes will not work.
  umask(0000);

  // Do all the mounts.
  for (const auto& mt : mounts) {
    if (mkdir(mt.target, 0755) != 0 && errno != EEXIST) {
      PLOG(ERROR) << "Failed to create " << mt.target;
      return false;
    }

    if (mount(mt.source, mt.target, mt.fstype, mt.flags, mt.data) != 0) {
      PLOG(ERROR) << "Failed to mount " << mt.target;
      return false;
    }
  }

  // Create all the directories.
  for (const auto& dir : boot_dirs) {
    if (mkdir(dir.path, dir.mode) != 0 && errno != EEXIST) {
      PLOG(ERROR) << "Failed to create " << dir.path;
      return false;
    }
  }

  // Change the ownership of the kCgroupContainerSuffix directory in each cgroup
  // subsystem to "chronos".
  base::FileEnumerator enumerator(base::FilePath(kCgroupRootDir),
                                  false /*recursive*/,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    DCHECK(base::DirectoryExists(current.Append(kCgroupContainerSuffix)));
    if (!ChangeOwnerAndGroup(current.Append(kCgroupContainerSuffix),
                             kChronosUid, kChronosGid)) {
      return false;
    }
  }

  // Create and setup the container cpusets with the default settings (all cpus,
  // all mems).
  const char* sets[] = {"cpus", "mems"};
  base::FilePath root_dir = base::FilePath(kCgroupRootDir).Append("cpuset");
  base::FilePath chronos_dir = root_dir.Append(kCgroupContainerSuffix);
  for (const char* set : sets) {
    string contents;
    if (!base::ReadFileToString(root_dir.Append(set), &contents)) {
      PLOG(ERROR) << "Failed to read contents from "
                  << root_dir.Append(set).value();
      return false;
    }

    if (base::WriteFile(chronos_dir.Append(set), contents.c_str(),
                        contents.length()) != contents.length()) {
      PLOG(ERROR) << "Failed to write cpuset contents to "
                  << chronos_dir.Append(set).value();
      return false;
    }
  }

  // Become the session leader.
  if (setsid() == -1) {
    PLOG(ERROR) << "Failed to become session leader";
    return false;
  }

  // Set the controlling terminal.
  if (ioctl(STDIN_FILENO, TIOCSCTTY, 1) != 0) {
    PLOG(ERROR) << "Failed to set controlling terminal";
    return false;
  }

  // Setup up PATH.
  if (clearenv() != 0) {
    PLOG(ERROR) << "Failed to clear environment";
    return false;
  }
  if (setenv("PATH", kDefaultPath, 1 /*overwrite*/) != 0) {
    PLOG(ERROR) << "Failed to set PATH";
    return false;
  }

  // Block SIGCHLD here because we want to handle it in the worker thread.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  if (sigprocmask(SIG_BLOCK, &mask, nullptr) != 0) {
    PLOG(ERROR) << "Failed to block SIGCHLD";
    return false;
  }

  // Start the worker.
  base::Thread::Options opts(base::MessageLoop::TYPE_IO, 0 /*stack_size*/);
  if (!worker_thread_.StartWithOptions(opts)) {
    LOG(ERROR) << "Failed to start worker thread";
    return false;
  }

  worker_ = base::MakeUnique<Worker>();
  bool ret = worker_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&Worker::Start, base::Unretained(worker_.get())));
  if (!ret) {
    LOG(ERROR) << "Failed to post task to worker thread";
    return false;
  }

  return true;
}

}  // namespace maitred
}  // namespace vm_tools
