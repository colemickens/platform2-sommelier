// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/callback_helpers.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <libminijail.h>
#include <scoped_minijail.h>

#include "libcontainer/container_cgroup.h"
#include "libcontainer/libcontainer.h"
#include "libcontainer/libcontainer_util.h"

namespace {

using libcontainer::DeviceMapperDetach;
using libcontainer::DeviceMapperSetup;
using libcontainer::GetUsernsOutsideId;
using libcontainer::LoopdevDetach;
using libcontainer::LoopdevSetup;
using libcontainer::MakeDir;
using libcontainer::MountExternal;
using libcontainer::TouchFile;

constexpr size_t kMaxNumSetfilesArgs = 128;
constexpr size_t kMaxRlimits = 32;  // Linux defines 15 at the time of writing.

struct Mount {
  std::string name;
  base::FilePath source;
  base::FilePath destination;
  std::string type;
  std::string data;
  std::string verity;
  int flags;
  int uid;
  int gid;
  int mode;

  // True if mount should happen in new vfs ns.
  bool mount_in_ns;

  // True if target should be created if it doesn't exist.
  bool create;

  // True if target should be mounted via loopback.
  bool loopback;
};

struct Device {
  // 'c' or 'b' for char or block
  char type;
  base::FilePath path;
  int fs_permissions;
  int major;
  int minor;

  // Copy the minor from existing node, ignores |minor|.
  bool copy_minor;
  int uid;
  int gid;
};

struct CgroupDevice {
  bool allow;
  char type;

  // -1 for either major or minor means all.
  int major;
  int minor;

  bool read;
  bool write;
  bool modify;
};

struct CpuCgroup {
  int shares;
  int quota;
  int period;
  int rt_runtime;
  int rt_period;
};

struct Rlimit {
  int type;
  uint32_t cur;
  uint32_t max;
};

}  // namespace

// Structure that configures how the container is run.
struct container_config {
  // Path to the root of the container itself.
  base::FilePath config_root;

  // Path to the root of the container's filesystem.
  base::FilePath rootfs;

  // Flags that will be passed to mount() for the rootfs.
  unsigned long rootfs_mount_flags;

  // Path to where the container will be run.
  base::FilePath premounted_runfs;

  // Path to the file where the pid should be written.
  base::FilePath pid_file_path;

  // The program to run and args, e.g. "/sbin/init".
  std::vector<std::string> program_argv;

  // The uid the container will run as.
  uid_t uid;

  // Mapping of UIDs in the container, e.g. "0 100000 1024"
  std::string uid_map;

  // The gid the container will run as.
  gid_t gid;

  // Mapping of GIDs in the container, e.g. "0 100000 1024"
  std::string gid_map;

  // Syscall table to use or nullptr if none.
  std::string alt_syscall_table;

  // Filesystems to mount in the new namespace.
  std::vector<Mount> mounts;

  // Device nodes to create.
  std::vector<Device> devices;

  // Device node cgroup permissions.
  std::vector<CgroupDevice> cgroup_devices;

  // Should run setfiles on mounts to enable selinux.
  std::string run_setfiles;

  // CPU cgroup params.
  CpuCgroup cpu_cgparams;

  // Parent dir for cgroup creation
  base::FilePath cgroup_parent;

  // uid to own the created cgroups
  uid_t cgroup_owner;

  // gid to own the created cgroups
  gid_t cgroup_group;

  // Enable sharing of the host network namespace.
  int share_host_netns;

  // Allow the child process to keep open FDs (for stdin/out/err).
  int keep_fds_open;

  // Array of rlimits for the contained process.
  Rlimit rlimits[kMaxRlimits];

  // The number of elements in `rlimits`.
  int num_rlimits;
  int use_capmask;
  int use_capmask_ambient;
  uint64_t capmask;

  // The mask of securebits to skip when restricting caps.
  uint64_t securebits_skip_mask;

  // Whether the container needs an extra process to be run as init.
  int do_init;

  // The SELinux context name the container will run under.
  std::string selinux_context;

  // A function pointer to be called prior to calling execve(2).
  minijail_hook_t pre_start_hook;

  // Parameter that will be passed to pre_start_hook().
  void* pre_start_hook_payload;

  std::vector<int> inherited_fds;
};

// Container manipulation
struct container {
  struct container_cgroup* cgroup;
  ScopedMinijail jail;
  pid_t init_pid;
  base::FilePath config_root;
  base::FilePath runfs;
  base::FilePath rundir;
  base::FilePath runfsroot;
  base::FilePath pid_file_path;

  // Mounts made outside of the minijail.
  std::vector<base::FilePath> ext_mounts;
  std::vector<base::FilePath> loopdev_paths;
  std::vector<std::string> device_mappers;
  std::string name;
};

namespace {

// Returns the path for |path_in_container| in the outer namespace.
base::FilePath GetPathInOuterNamespace(
    const base::FilePath& root, const base::FilePath& path_in_container) {
  if (path_in_container.IsAbsolute())
    return base::FilePath(root.value() + path_in_container.value());
  return root.Append(path_in_container);
}

// Make sure the mount target exists in the new rootfs. Create if needed and
// possible.
int SetupMountDestination(const struct container_config* config,
                          const Mount& mount,
                          const base::FilePath& source,
                          const base::FilePath& dest) {
  int rc;

  struct stat st_buf;
  rc = stat(dest.value().c_str(), &st_buf);
  if (rc == 0) {
    // destination exists.
    return 0;
  }

  // Try to create the destination. Either make directory or touch a file
  // depending on the source type.
  int uid_userns = GetUsernsOutsideId(config->uid_map, mount.uid);
  if (uid_userns < 0)
    return uid_userns;
  int gid_userns = GetUsernsOutsideId(config->gid_map, mount.gid);
  if (gid_userns < 0)
    return gid_userns;

  rc = stat(source.value().c_str(), &st_buf);
  if (rc || S_ISDIR(st_buf.st_mode) || S_ISBLK(st_buf.st_mode))
    return MakeDir(dest, uid_userns, gid_userns, mount.mode);

  return TouchFile(dest, uid_userns, gid_userns, mount.mode);
}

// Fork and exec the setfiles command to configure the selinux policy.
int RunSetfilesCommand(const struct container* c,
                       const struct container_config* config,
                       const std::vector<base::FilePath>& destinations) {
  if (config->run_setfiles.empty())
    return 0;

  int pid = fork();
  if (pid == 0) {
    size_t arg_index = 0;
    const char* argv[kMaxNumSetfilesArgs];
    const char* env[] = {
        nullptr,
    };

    base::FilePath context_path = c->runfsroot.Append("file_contexts");

    argv[arg_index++] = config->run_setfiles.c_str();
    argv[arg_index++] = "-r";
    argv[arg_index++] = c->runfsroot.value().c_str();
    argv[arg_index++] = context_path.value().c_str();
    if (arg_index + destinations.size() >= kMaxNumSetfilesArgs)
      _exit(-E2BIG);
    for (const auto& destination : destinations)
      argv[arg_index++] = destination.value().c_str();
    argv[arg_index] = nullptr;

    execve(
        argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(env));

    /* Command failed to exec if execve returns. */
    _exit(-errno);
  }
  if (pid < 0)
    return -errno;

  int rc;
  int status;
  do {
    rc = waitpid(pid, &status, 0);
  } while (rc == -1 && errno == EINTR);
  if (rc < 0)
    return -errno;
  return status;
}

// Unmounts anything we mounted in this mount namespace in the opposite order
// that they were mounted.
int UnmountExternalMounts(struct container* c) {
  int ret = 0;

  for (auto it = c->ext_mounts.rbegin(); it != c->ext_mounts.rend(); ++it) {
    if (umount(it->value().c_str()))
      ret = -errno;
  }
  c->ext_mounts.clear();

  for (auto it = c->loopdev_paths.rbegin(); it != c->loopdev_paths.rend();
       ++it) {
    int rc = LoopdevDetach(*it);
    if (rc)
      ret = rc;
  }
  c->loopdev_paths.clear();

  for (auto it = c->device_mappers.rbegin(); it != c->device_mappers.rend();
       ++it) {
    if (DeviceMapperDetach(*it))
      ret = -errno;
  }
  c->device_mappers.clear();

  return ret;
}

int DoContainerMount(struct container* c,
                     const struct container_config* config,
                     const Mount& mount) {
  int rc = 0;

  base::FilePath dest =
      GetPathInOuterNamespace(c->runfsroot, mount.destination);

  // If it's a bind mount relative to rootfs, append source to
  // rootfs path, otherwise source path is absolute.
  base::FilePath source;
  if ((mount.flags & MS_BIND) && !mount.source.IsAbsolute()) {
    source = GetPathInOuterNamespace(c->runfsroot, mount.source);
  } else if (mount.loopback && !mount.source.IsAbsolute() &&
             !c->config_root.empty()) {
    source = GetPathInOuterNamespace(c->config_root, mount.source);
  } else {
    source = mount.source;
  }

  // Only create the destinations for external mounts, minijail will take
  // care of those mounted in the new namespace.
  if (mount.create && !mount.mount_in_ns) {
    rc = SetupMountDestination(config, mount, source, dest);
    if (rc)
      return rc;
  }
  if (mount.loopback) {
    // Record this loopback file for cleanup later.
    base::FilePath loop_source = source;
    rc = LoopdevSetup(loop_source, &source);
    if (rc)
      return rc;

    // Save this to cleanup when shutting down.
    c->loopdev_paths.push_back(source);
  }
  if (!mount.verity.empty()) {
    // Set this device up via dm-verity.
    std::string dm_name;
    base::FilePath dm_source = source;
    rc = DeviceMapperSetup(dm_source, mount.verity, &source, &dm_name);
    if (rc)
      return rc;

    // Save this to cleanup when shutting down.
    c->device_mappers.push_back(dm_name);
  }
  if (mount.mount_in_ns) {
    // We can mount this with minijail.
    rc = minijail_mount_with_data(
        c->jail.get(),
        source.value().c_str(),
        mount.destination.value().c_str(),
        mount.type.c_str(),
        mount.flags,
        mount.data.empty() ? nullptr : mount.data.c_str());
    if (rc)
      return rc;
  } else {
    // Mount this externally and unmount it on exit.
    rc = MountExternal(source.value(),
                       dest.value(),
                       mount.type,
                       mount.flags,
                       mount.data);
    if (rc)
      return rc;
    // Save this to unmount when shutting down.
    c->ext_mounts.push_back(dest);
  }

  return 0;
}

int DoContainerMounts(struct container* c,
                      const struct container_config* config) {
  UnmountExternalMounts(c);

  // This will run in all the error cases.
  base::ScopedClosureRunner teardown(base::Bind(
      base::IgnoreResult(&UnmountExternalMounts), base::Unretained(c)));

  for (const auto& mount : config->mounts) {
    int rc = DoContainerMount(c, config, mount);
    if (rc)
      return rc;
  }

  // The mounts have been done successfully, no need to tear them down anymore.
  ignore_result(teardown.Release());

  return 0;
}

int ContainerCreateDevice(const struct container* c,
                          const struct container_config* config,
                          const Device& dev,
                          int minor) {
  mode_t mode = dev.fs_permissions;
  switch (dev.type) {
    case 'b':
      mode |= S_IFBLK;
      break;
    case 'c':
      mode |= S_IFCHR;
      break;
    default:
      return -EINVAL;
  }

  int uid_userns = GetUsernsOutsideId(config->uid_map, dev.uid);
  if (uid_userns < 0)
    return uid_userns;
  int gid_userns = GetUsernsOutsideId(config->gid_map, dev.gid);
  if (gid_userns < 0)
    return gid_userns;

  base::FilePath path = GetPathInOuterNamespace(c->runfsroot, dev.path);
  if (mknod(path.value().c_str(), mode, makedev(dev.major, minor)) &&
      errno != EEXIST) {
    return -errno;
  }
  if (chown(path.value().c_str(), uid_userns, gid_userns))
    return -errno;
  if (chmod(path.value().c_str(), dev.fs_permissions))
    return -errno;

  return 0;
}

int MountRunfs(struct container* c, const struct container_config* config) {
  {
    std::string runfs_template = base::StringPrintf(
        "%s/%s_XXXXXX", c->rundir.value().c_str(), c->name.c_str());
    // TODO(lhchavez): Replace this with base::CreateTemporaryDirInDir().
    char* runfs_path = mkdtemp(const_cast<char*>(runfs_template.c_str()));
    if (!runfs_path)
      return -errno;
    c->runfs = base::FilePath(runfs_path);
  }

  int uid_userns = GetUsernsOutsideId(config->uid_map, config->uid);
  if (uid_userns < 0)
    return uid_userns;
  int gid_userns = GetUsernsOutsideId(config->gid_map, config->gid);
  if (gid_userns < 0)
    return gid_userns;

  // Make sure the container uid can access the rootfs.
  if (chmod(c->runfs.value().c_str(), 0700))
    return -errno;
  if (chown(c->runfs.value().c_str(), uid_userns, gid_userns))
    return -errno;

  c->runfsroot = c->runfs.Append("root");

  constexpr mode_t kRootDirMode = 0660;
  if (mkdir(c->runfsroot.value().c_str(), kRootDirMode))
    return -errno;
  if (chmod(c->runfsroot.value().c_str(), kRootDirMode))
    return -errno;

  if (mount(config->rootfs.value().c_str(),
            c->runfsroot.value().c_str(),
            "",
            MS_BIND | (config->rootfs_mount_flags & MS_REC),
            nullptr)) {
    return -errno;
  }

  // MS_BIND ignores any flags passed to it (except MS_REC). We need a
  // second call to mount() to actually set them.
  if (config->rootfs_mount_flags &&
      mount(config->rootfs.value().c_str(),
            c->runfsroot.value().c_str(),
            "",
            (config->rootfs_mount_flags & ~MS_REC),
            nullptr)) {
    return -errno;
  }

  return 0;
}

int DeviceSetup(struct container* c, const struct container_config* config) {
  int rc;

  c->cgroup->ops->deny_all_devices(c->cgroup);

  for (const auto& dev : config->cgroup_devices) {
    rc = c->cgroup->ops->add_device(c->cgroup,
                                    dev.allow,
                                    dev.major,
                                    dev.minor,
                                    dev.read,
                                    dev.write,
                                    dev.modify,
                                    dev.type);
    if (rc)
      return rc;
  }

  for (const auto& dev : config->devices) {
    int minor = dev.minor;

    if (dev.copy_minor) {
      struct stat st_buff;
      if (stat(dev.path.value().c_str(), &st_buff) < 0)
        continue;
      minor = minor(st_buff.st_rdev);
    }
    if (minor >= 0) {
      rc = ContainerCreateDevice(c, config, dev, minor);
      if (rc)
        return rc;
    }
  }

  for (const auto& loopdev_path : c->loopdev_paths) {
    struct stat st;

    rc = stat(loopdev_path.value().c_str(), &st);
    if (rc < 0)
      return -errno;
    rc = c->cgroup->ops->add_device(
        c->cgroup, 1, major(st.st_rdev), minor(st.st_rdev), 1, 0, 0, 'b');
    if (rc)
      return rc;
  }

  return 0;
}

int Setexeccon(void* payload) {
  char* init_domain = reinterpret_cast<char*>(payload);
  pid_t tid = syscall(SYS_gettid);

  if (tid == -1)
    return -errno;

  std::string exec_path =
      base::StringPrintf("/proc/self/task/%d/attr/exec", tid);

  base::ScopedFD fd(open(exec_path.c_str(), O_WRONLY | O_CLOEXEC));
  if (!fd.is_valid())
    return -errno;

  if (write(fd.get(), init_domain, strlen(init_domain)) !=
      (ssize_t)strlen(init_domain)) {
    return -errno;
  }

  return 0;
}

int ContainerTeardown(struct container* c) {
  int ret = 0;

  UnmountExternalMounts(c);
  if (!c->runfsroot.empty() && !c->runfs.empty()) {
    /* |c->runfsroot| may have been mounted recursively. Thus use
     * MNT_DETACH to "immediately disconnect the filesystem and all
     * filesystems mounted below it from each other and from the
     * mount table". Otherwise one would need to unmount every
     * single dependent mount before unmounting |c->runfsroot|
     * itself.
     */
    if (umount2(c->runfsroot.value().c_str(), MNT_DETACH))
      ret = -errno;
    if (rmdir(c->runfsroot.value().c_str()))
      ret = -errno;
  }
  if (!c->pid_file_path.empty()) {
    if (unlink(c->pid_file_path.value().c_str()))
      ret = -errno;
  }
  if (!c->runfs.empty()) {
    if (rmdir(c->runfs.value().c_str()))
      ret = -errno;
  }
  return ret;
}


}  // namespace

struct container_config* container_config_create() {
  return new (std::nothrow) struct container_config();
}

void container_config_destroy(struct container_config* c) {
  if (c == nullptr)
    return;
  delete c;
}

int container_config_config_root(struct container_config* c,
                                 const char* config_root) {
  c->config_root = base::FilePath(config_root);
  return 0;
}

const char* container_config_get_config_root(const struct container_config* c) {
  return c->config_root.value().c_str();
}

int container_config_rootfs(struct container_config* c, const char* rootfs) {
  c->rootfs = base::FilePath(rootfs);
  return 0;
}

const char* container_config_get_rootfs(const struct container_config* c) {
  return c->rootfs.value().c_str();
}

void container_config_rootfs_mount_flags(struct container_config* c,
                                         unsigned long rootfs_mount_flags) {
  /* Since we are going to add MS_REMOUNT anyways, add it here so we can
   * simply check against zero later. MS_BIND is also added to avoid
   * re-mounting the original filesystem, since the rootfs is always
   * bind-mounted.
   */
  c->rootfs_mount_flags = MS_REMOUNT | MS_BIND | rootfs_mount_flags;
}

unsigned long container_config_get_rootfs_mount_flags(
    const struct container_config* c) {
  return c->rootfs_mount_flags;
}

int container_config_premounted_runfs(struct container_config* c,
                                      const char* runfs) {
  c->premounted_runfs = base::FilePath(runfs);
  return 0;
}

const char* container_config_get_premounted_runfs(
    const struct container_config* c) {
  return c->premounted_runfs.value().c_str();
}

int container_config_pid_file(struct container_config* c, const char* path) {
  c->pid_file_path = base::FilePath(path);
  return 0;
}

const char* container_config_get_pid_file(const struct container_config* c) {
  return c->pid_file_path.value().c_str();
}

int container_config_program_argv(struct container_config* c,
                                  const char** argv,
                                  size_t num_args) {
  if (num_args < 1)
    return -EINVAL;
  c->program_argv.clear();
  c->program_argv.reserve(num_args);
  for (size_t i = 0; i < num_args; ++i)
    c->program_argv.emplace_back(argv[i]);
  return 0;
}

size_t container_config_get_num_program_args(const struct container_config* c) {
  return c->program_argv.size();
}

const char* container_config_get_program_arg(const struct container_config* c,
                                             size_t index) {
  if (index >= c->program_argv.size())
    return nullptr;
  return c->program_argv[index].c_str();
}

void container_config_uid(struct container_config* c, uid_t uid) {
  c->uid = uid;
}

uid_t container_config_get_uid(const struct container_config* c) {
  return c->uid;
}

int container_config_uid_map(struct container_config* c, const char* uid_map) {
  c->uid_map = uid_map;
  return 0;
}

void container_config_gid(struct container_config* c, gid_t gid) {
  c->gid = gid;
}

gid_t container_config_get_gid(const struct container_config* c) {
  return c->gid;
}

int container_config_gid_map(struct container_config* c, const char* gid_map) {
  c->gid_map = gid_map;
  return 0;
}

int container_config_alt_syscall_table(struct container_config* c,
                                       const char* alt_syscall_table) {
  c->alt_syscall_table = alt_syscall_table;
  return 0;
}

int container_config_add_rlimit(struct container_config* c,
                                int type,
                                uint32_t cur,
                                uint32_t max) {
  if (c->num_rlimits >= kMaxRlimits)
    return -ENOMEM;
  c->rlimits[c->num_rlimits].type = type;
  c->rlimits[c->num_rlimits].cur = cur;
  c->rlimits[c->num_rlimits].max = max;
  c->num_rlimits++;
  return 0;
}

int container_config_add_mount(struct container_config* c,
                               const char* name,
                               const char* source,
                               const char* destination,
                               const char* type,
                               const char* data,
                               const char* verity,
                               int flags,
                               int uid,
                               int gid,
                               int mode,
                               int mount_in_ns,
                               int create,
                               int loopback) {
  if (name == nullptr || source == nullptr || destination == nullptr ||
      type == nullptr)
    return -EINVAL;

  c->mounts.emplace_back(Mount{name,
                               base::FilePath(source),
                               base::FilePath(destination),
                               type,
                               data ? data : "",
                               verity ? verity : "",
                               flags,
                               uid,
                               gid,
                               mode,
                               mount_in_ns != 0,
                               create != 0,
                               loopback != 0});

  return 0;
}

int container_config_add_cgroup_device(struct container_config* c,
                                       int allow,
                                       char type,
                                       int major,
                                       int minor,
                                       int read,
                                       int write,
                                       int modify) {
  c->cgroup_devices.emplace_back(CgroupDevice{
      allow != 0, type, major, minor, read != 0, write != 0, modify != 0});

  return 0;
}

int container_config_add_device(struct container_config* c,
                                char type,
                                const char* path,
                                int fs_permissions,
                                int major,
                                int minor,
                                int copy_minor,
                                int uid,
                                int gid,
                                int read_allowed,
                                int write_allowed,
                                int modify_allowed) {
  if (path == nullptr)
    return -EINVAL;
  /* If using a dynamic minor number, ensure that minor is -1. */
  if (copy_minor && (minor != -1))
    return -EINVAL;

  if (read_allowed || write_allowed || modify_allowed) {
    if (container_config_add_cgroup_device(c,
                                           1,
                                           type,
                                           major,
                                           minor,
                                           read_allowed,
                                           write_allowed,
                                           modify_allowed)) {
      return -ENOMEM;
    }
  }

  c->devices.emplace_back(Device{
      type,
      base::FilePath(path),
      fs_permissions,
      major,
      minor,
      copy_minor != 0,
      uid,
      gid,
  });

  return 0;
}

int container_config_run_setfiles(struct container_config* c,
                                  const char* setfiles_cmd) {
  c->run_setfiles = setfiles_cmd;
  return 0;
}

const char* container_config_get_run_setfiles(
    const struct container_config* c) {
  return c->run_setfiles.c_str();
}

int container_config_set_cpu_shares(struct container_config* c, int shares) {
  /* CPU shares must be 2 or higher. */
  if (shares < 2)
    return -EINVAL;

  c->cpu_cgparams.shares = shares;
  return 0;
}

int container_config_set_cpu_cfs_params(struct container_config* c,
                                        int quota,
                                        int period) {
  /*
   * quota could be set higher than period to utilize more than one CPU.
   * quota could also be set as -1 to indicate the cgroup does not adhere
   * to any CPU time restrictions.
   */
  if (quota <= 0 && quota != -1)
    return -EINVAL;
  if (period <= 0)
    return -EINVAL;

  c->cpu_cgparams.quota = quota;
  c->cpu_cgparams.period = period;
  return 0;
}

int container_config_set_cpu_rt_params(struct container_config* c,
                                       int rt_runtime,
                                       int rt_period) {
  /*
   * rt_runtime could be set as 0 to prevent the cgroup from using
   * realtime CPU.
   */
  if (rt_runtime < 0 || rt_runtime >= rt_period)
    return -EINVAL;

  c->cpu_cgparams.rt_runtime = rt_runtime;
  c->cpu_cgparams.rt_period = rt_period;
  return 0;
}

int container_config_get_cpu_shares(struct container_config* c) {
  return c->cpu_cgparams.shares;
}

int container_config_get_cpu_quota(struct container_config* c) {
  return c->cpu_cgparams.quota;
}

int container_config_get_cpu_period(struct container_config* c) {
  return c->cpu_cgparams.period;
}

int container_config_get_cpu_rt_runtime(struct container_config* c) {
  return c->cpu_cgparams.rt_runtime;
}

int container_config_get_cpu_rt_period(struct container_config* c) {
  return c->cpu_cgparams.rt_period;
}

int container_config_set_cgroup_parent(struct container_config* c,
                                       const char* parent,
                                       uid_t cgroup_owner,
                                       gid_t cgroup_group) {
  c->cgroup_owner = cgroup_owner;
  c->cgroup_group = cgroup_group;
  c->cgroup_parent = base::FilePath(parent);
  return 0;
}

const char* container_config_get_cgroup_parent(struct container_config* c) {
  return c->cgroup_parent.value().c_str();
}

void container_config_share_host_netns(struct container_config* c) {
  c->share_host_netns = 1;
}

int get_container_config_share_host_netns(struct container_config* c) {
  return c->share_host_netns;
}

void container_config_keep_fds_open(struct container_config* c) {
  c->keep_fds_open = 1;
}

void container_config_set_capmask(struct container_config* c,
                                  uint64_t capmask,
                                  int ambient) {
  c->use_capmask = 1;
  c->capmask = capmask;
  c->use_capmask_ambient = ambient;
}

void container_config_set_securebits_skip_mask(struct container_config* c,
                                               uint64_t securebits_skip_mask) {
  c->securebits_skip_mask = securebits_skip_mask;
}

void container_config_set_run_as_init(struct container_config* c,
                                      int run_as_init) {
  c->do_init = !run_as_init;
}

int container_config_set_selinux_context(struct container_config* c,
                                         const char* context) {
  if (!context)
    return -EINVAL;
  c->selinux_context = context;
  return 0;
}

void container_config_set_pre_execve_hook(struct container_config* c,
                                          int (*hook)(void*),
                                          void* payload) {
  c->pre_start_hook = hook;
  c->pre_start_hook_payload = payload;
}

int container_config_inherit_fds(struct container_config* c,
                                 int* inherited_fds,
                                 size_t inherited_fd_count) {
  if (!c->inherited_fds.empty())
    return -EINVAL;
  for (size_t i = 0; i < inherited_fd_count; ++i)
    c->inherited_fds.emplace_back(inherited_fds[i]);
  return 0;
}

struct container* container_new(const char* name, const char* rundir) {
  struct container* c = new (std::nothrow) container();
  if (!c)
    return nullptr;
  c->rundir = base::FilePath(rundir);
  c->name = name;
  return c;
}

void container_destroy(struct container* c) {
  if (c->cgroup)
    container_cgroup_destroy(c->cgroup);
  delete c;
}

int container_start(struct container* c,
                    const struct container_config* config) {
  int rc = 0;

  if (!c)
    return -EINVAL;
  if (!config)
    return -EINVAL;
  if (config->program_argv.empty())
    return -EINVAL;

  // This will run in all the error cases.
  base::ScopedClosureRunner teardown(
      base::Bind(base::IgnoreResult(&ContainerTeardown), base::Unretained(c)));

  if (!config->config_root.empty())
    c->config_root = config->config_root;
  if (!config->premounted_runfs.empty()) {
    c->runfs.clear();
    c->runfsroot = config->premounted_runfs;
  } else {
    rc = MountRunfs(c, config);
    if (rc)
      return rc;
  }

  c->jail.reset(minijail_new());
  if (!c->jail)
    return -ENOMEM;

  rc = DoContainerMounts(c, config);
  if (rc)
    return rc;

  int cgroup_uid = GetUsernsOutsideId(config->uid_map, config->cgroup_owner);
  if (cgroup_uid < 0)
    return cgroup_uid;
  int cgroup_gid = GetUsernsOutsideId(config->gid_map, config->cgroup_group);
  if (cgroup_gid < 0)
    return cgroup_gid;

  c->cgroup = container_cgroup_new(c->name.c_str(),
                                   "/sys/fs/cgroup",
                                   config->cgroup_parent.value().c_str(),
                                   cgroup_uid,
                                   cgroup_gid);
  if (!c->cgroup)
    return -errno;

  /* Must be root to modify device cgroup or mknod */
  if (getuid() == 0) {
    rc = DeviceSetup(c, config);
    if (rc)
      return rc;
  }

  /* Potentially run setfiles on mounts configured outside of the jail */
  const base::FilePath kDataPath("/data");
  const base::FilePath kCachePath("/cache");
  std::vector<base::FilePath> destinations;
  for (const auto& mnt : config->mounts) {
    if (mnt.mount_in_ns)
      continue;
    if (mnt.flags & MS_RDONLY)
      continue;

    /* A hack to avoid setfiles on /data and /cache. */
    if (mnt.destination == kDataPath || mnt.destination == kCachePath)
      continue;

    destinations.emplace_back(
        GetPathInOuterNamespace(c->runfsroot, mnt.destination));
  }
  if (!destinations.empty()) {
    rc = RunSetfilesCommand(c, config, destinations);
    if (rc)
      return rc;
  }

  /* Setup CPU cgroup params. */
  if (config->cpu_cgparams.shares) {
    rc = c->cgroup->ops->set_cpu_shares(c->cgroup, config->cpu_cgparams.shares);
    if (rc)
      return rc;
  }
  if (config->cpu_cgparams.period) {
    rc = c->cgroup->ops->set_cpu_quota(c->cgroup, config->cpu_cgparams.quota);
    if (rc)
      return rc;
    rc = c->cgroup->ops->set_cpu_period(c->cgroup, config->cpu_cgparams.period);
    if (rc)
      return rc;
  }
  if (config->cpu_cgparams.rt_period) {
    rc = c->cgroup->ops->set_cpu_rt_runtime(c->cgroup,
                                            config->cpu_cgparams.rt_runtime);
    if (rc)
      return rc;
    rc = c->cgroup->ops->set_cpu_rt_period(c->cgroup,
                                           config->cpu_cgparams.rt_period);
    if (rc)
      return rc;
  }

  /* Setup and start the container with libminijail. */
  if (!config->pid_file_path.empty())
    c->pid_file_path = config->pid_file_path;
  else if (!c->runfs.empty())
    c->pid_file_path = c->runfs.Append("container.pid");

  if (!c->pid_file_path.empty())
    minijail_write_pid_file(c->jail.get(), c->pid_file_path.value().c_str());
  minijail_reset_signal_mask(c->jail.get());

  /* Setup container namespaces. */
  minijail_namespace_ipc(c->jail.get());
  minijail_namespace_vfs(c->jail.get());
  if (!config->share_host_netns)
    minijail_namespace_net(c->jail.get());
  minijail_namespace_pids(c->jail.get());
  minijail_namespace_user(c->jail.get());
  if (getuid() != 0)
    minijail_namespace_user_disable_setgroups(c->jail.get());
  minijail_namespace_cgroups(c->jail.get());
  rc = minijail_uidmap(c->jail.get(), config->uid_map.c_str());
  if (rc)
    return rc;
  rc = minijail_gidmap(c->jail.get(), config->gid_map.c_str());
  if (rc)
    return rc;

  /* Set the UID/GID inside the container if not 0. */
  rc = GetUsernsOutsideId(config->uid_map, config->uid);
  if (rc < 0)
    return rc;
  else if (config->uid > 0)
    minijail_change_uid(c->jail.get(), config->uid);
  rc = GetUsernsOutsideId(config->gid_map, config->gid);
  if (rc < 0)
    return rc;
  else if (config->gid > 0)
    minijail_change_gid(c->jail.get(), config->gid);

  rc = minijail_enter_pivot_root(c->jail.get(), c->runfsroot.value().c_str());
  if (rc)
    return rc;

  /* Add the cgroups configured above. */
  for (int i = 0; i < NUM_CGROUP_TYPES; i++) {
    if (c->cgroup->cgroup_tasks_paths[i]) {
      rc = minijail_add_to_cgroup(c->jail.get(),
                                  c->cgroup->cgroup_tasks_paths[i]);
      if (rc)
        return rc;
    }
  }

  if (!config->alt_syscall_table.empty())
    minijail_use_alt_syscall(c->jail.get(), config->alt_syscall_table.c_str());

  for (int i = 0; i < config->num_rlimits; i++) {
    const Rlimit& lim = config->rlimits[i];
    rc = minijail_rlimit(c->jail.get(), lim.type, lim.cur, lim.max);
    if (rc)
      return rc;
  }

  if (!config->selinux_context.empty()) {
    rc = minijail_add_hook(c->jail.get(),
                           &Setexeccon,
                           const_cast<char*>(config->selinux_context.c_str()),
                           MINIJAIL_HOOK_EVENT_PRE_EXECVE);
    if (rc)
      return rc;
  }

  if (config->pre_start_hook) {
    rc = minijail_add_hook(c->jail.get(),
                           config->pre_start_hook,
                           config->pre_start_hook_payload,
                           MINIJAIL_HOOK_EVENT_PRE_EXECVE);
    if (rc)
      return rc;
  }

  for (int fd : config->inherited_fds) {
    rc = minijail_preserve_fd(c->jail.get(), fd, fd);
    if (rc)
      return rc;
  }

  /* TODO(dgreid) - remove this once shared mounts are cleaned up. */
  minijail_skip_remount_private(c->jail.get());

  if (!config->keep_fds_open)
    minijail_close_open_fds(c->jail.get());

  if (config->use_capmask) {
    minijail_use_caps(c->jail.get(), config->capmask);
    if (config->use_capmask_ambient) {
      minijail_set_ambient_caps(c->jail.get());
    }
    if (config->securebits_skip_mask) {
      minijail_skip_setting_securebits(c->jail.get(),
                                       config->securebits_skip_mask);
    }
  }

  if (!config->do_init)
    minijail_run_as_init(c->jail.get());

  std::vector<char*> argv_cstr;
  argv_cstr.reserve(config->program_argv.size() + 1);
  for (const auto& arg : config->program_argv)
    argv_cstr.emplace_back(const_cast<char*>(arg.c_str()));
  argv_cstr.emplace_back(nullptr);

  rc = minijail_run_pid_pipes_no_preload(c->jail.get(),
                                         argv_cstr[0],
                                         argv_cstr.data(),
                                         &c->init_pid,
                                         nullptr,
                                         nullptr,
                                         nullptr);
  if (rc)
    return rc;

  // The container has started successfully, no need to tear it down anymore.
  ignore_result(teardown.Release());
  return 0;
}

const char* container_root(struct container* c) {
  return c->runfs.value().c_str();
}

int container_pid(struct container* c) {
  return c->init_pid;
}

int container_wait(struct container* c) {
  int rc;

  do {
    rc = minijail_wait(c->jail.get());
  } while (rc == -EINTR);

  // If the process had already been reaped, still perform teardown.
  if (rc == -ECHILD || rc >= 0) {
    rc = ContainerTeardown(c);
  }
  return rc;
}

int container_kill(struct container* c) {
  if (kill(c->init_pid, SIGKILL) && errno != ESRCH)
    return -errno;
  return container_wait(c);
}
