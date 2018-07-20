// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FUSE_USE_VERSION 26

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libminijail.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sysexits.h>
#include <unistd.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <base/at_exit.h>
#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/memory/free_deleter.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "container_utils/fs_data.h"

namespace {

const char kDevfsPath[] = "/dev";

// Some wrappers around syscalls.

struct linux_dirent64 {
  uint64_t d_ino;
  int64_t d_off;
  unsigned short d_reclen;  // NOLINT(runtime/int)
  unsigned char d_type;
  char d_name[];
};

static int getdents(int fd, struct linux_dirent64* dirp, unsigned int count) {
  return syscall(SYS_getdents64, fd, dirp, count);
}

}  // namespace

namespace device_jail {

static FsData* GetFsData() {
  return static_cast<FsData*>(fuse_get_context()->private_data);
}

// Removes symlinks that are broken in the new filesystem and devices
// that aren't marked either passthrough or jailed. For jailed devices,
// spin up a jail if necessary, and stat that instead.
static int jail_stat(const char* path, struct stat* file_stat) {
  DVLOG(1) << "jail_stat(" << path << ")";
  int ret = fstatat(GetFsData()->root_fd(), path + 1, file_stat,
                    AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW);
  if (ret < 0)
    return ret;

  std::string real_path(kDevfsPath);
  real_path.append(path);

  // If it's a symlink and the path is relative, check that it's not
  // broken in the new filesystem.
  if (S_ISLNK(file_stat->st_mode)) {
    std::unique_ptr<char, base::FreeDeleter> resolved_path(
        realpath(real_path.c_str(), nullptr));
    if (!resolved_path)
      return -1;

    // If it doesn't end up somewhere in devfs, it doesn't matter to
    // us. Let it through.
    if (strncmp(resolved_path.get(), kDevfsPath, strlen(kDevfsPath)) != 0)
      return 0;

    // If this link ends up somewhere in devfs, check to make sure
    // it points at something that exists in the new filesystem.
    struct stat link_stat;
    // The path relative from our root is going to be after "/dev", so
    // move the pointer up accordingly.
    return jail_stat(resolved_path.get() + strlen(kDevfsPath), &link_stat);
  }

  // Allow all other files that aren't device files through.
  if (!(S_ISCHR(file_stat->st_mode) || S_ISBLK(file_stat->st_mode)))
    return ret;

  static const char* const kPassthroughDevices[] = {
    "/full",
    "/null",
    "/urandom",
    "/zero",
  };

  for (size_t i = 0; i < arraysize(kPassthroughDevices); i++) {
    if (strcmp(path, kPassthroughDevices[i]) == 0)
      return ret;
  }

  static const char* const kJailDevices[] = {
    "/bus/usb",
  };

  for (size_t i = 0; i < arraysize(kJailDevices); i++) {
    if (strncmp(path, kJailDevices[i], strlen(kJailDevices[i])) == 0)
      return GetFsData()->GetStatForJail(real_path, file_stat);
  }

  errno = ENOENT;
  return -1;
}

static int djfs_getattr(const char* path, struct stat* file_stat) {
  DVLOG(1) << "stat(" << path << ")";
  int ret = jail_stat(path, file_stat);
  if (ret < 0)
    return -errno;

  return ret;
}

static int djfs_readlink(const char* path, char* buf, size_t size) {
  DVLOG(1) << "readlink(" << path << ")";
  ssize_t ret = readlinkat(GetFsData()->root_fd(), path + 1, buf, size - 1);
  if (ret < 0)
    return -errno;

  buf[ret] = '\0';
  return 0;
}

static int djfs_opendir(const char* path, struct fuse_file_info* fi) {
  DVLOG(1) << "opendir(" << path << ", " << fi->flags << ")";
  int ret;
  if (strcmp("/", path) == 0) {
    ret = dup(GetFsData()->root_fd());
  } else {
    ret = openat(GetFsData()->root_fd(), path + 1,
                 fi->flags | O_DIRECTORY | O_CLOEXEC);
  }

  if (ret < 0)
    return -errno;

  fi->fh = ret;
  return 0;
}

static int djfs_release(const char* path, struct fuse_file_info* fi) {
  DVLOG(1) << "close(" << path << ")";
  return (close(fi->fh) < 0) ? -errno : 0;
}

static int djfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info* fi) {
  DVLOG(1) << "readdir(" << path << ", " << buf << ", " << offset << ")";
  const int kBufSize = 1024;
  char getdents_buf[kBufSize];
  ssize_t bytes_read;
  off_t dir_off;

  dir_off = lseek(fi->fh, 0, SEEK_SET);
  if (dir_off < 0) {
    DPLOG(ERROR) << "could not reset offset for directory " << path;
    return -errno;
  }

  for (;;) {
    bytes_read = getdents(
        fi->fh, reinterpret_cast<struct linux_dirent64*>(getdents_buf),
        kBufSize);
    if (bytes_read < 0)
      return -errno;
    if (bytes_read == 0)
      break;

    for (ssize_t i = 0; i < bytes_read;) {
      auto d = reinterpret_cast<struct linux_dirent64*>(getdents_buf + i);

      base::FilePath full_path = base::FilePath(path).Append(d->d_name);

      struct stat file_stat;
      if (jail_stat(full_path.value().c_str(), &file_stat) >= 0)
        filler(buf, d->d_name, &file_stat, 0);

      i += d->d_reclen;
    }
  }

  return 0;
}

static void* djfs_init(struct fuse_conn_info* conn) {
  // Drop root.
  struct minijail* j = minijail_new();
  minijail_change_user(j, "devicejail");
  minijail_change_group(j, "devicejail");
  minijail_inherit_usergroups(j);
  minijail_enter(j);
  minijail_destroy(j);

  // Whatever you return from the init function becomes the user_data,
  // even if there was already stuff in there.
  return GetFsData();
}

static struct fuse_operations fops = {
  .getattr = djfs_getattr,
  .readlink = djfs_readlink,
  .getdir = nullptr,
  .mknod = nullptr,
  .mkdir = nullptr,
  .unlink = nullptr,
  .rmdir = nullptr,
  .symlink = nullptr,
  .rename = nullptr,
  .link = nullptr,
  .chmod = nullptr,
  .chown = nullptr,
  .truncate = nullptr,
  .utime = nullptr,
  .open = nullptr,
  .read = nullptr,
  .write = nullptr,
  .statfs = nullptr,
  .flush = nullptr,
  .release = djfs_release,
  .fsync = nullptr,
  .setxattr = nullptr,
  .getxattr = nullptr,
  .listxattr = nullptr,
  .removexattr = nullptr,
  .opendir = djfs_opendir,
  .readdir = djfs_readdir,
  .releasedir = djfs_release,
  .fsyncdir = nullptr,
  .init = djfs_init,
  .destroy = nullptr,
  .access = nullptr,
  .create = nullptr,
  .ftruncate = nullptr,
  .fgetattr = nullptr,
  .lock = nullptr,
  .utimens = nullptr,
  .bmap = nullptr,
  .flag_nullpath_ok = 0,
  .flag_reserved = 0,  // placeholder
  .ioctl = nullptr,
  .poll = nullptr,
};

}  // namespace device_jail

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(argc, argv,
                           "Start a device jail on a mount point.");
  brillo::OpenLog("device_jail_fs", true);
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderrIfTty);

  const base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  if (args.size() != 1) {
    LOG(ERROR) << "Usage: device_jail_fs <mount point>";
    return EX_USAGE;
  }
  std::string mount_point = args[0];

  base::AtExitManager at_exit_manager;

  if (getuid() != 0) {
    LOG(ERROR) << "need root to mount with devices";
    return EX_USAGE;
  }

  DLOG(INFO) << "device_jail_fs mounting " << kDevfsPath << " onto "
             << mount_point;
  std::unique_ptr<device_jail::FsData> fs_data =
      device_jail::FsData::Create(kDevfsPath, mount_point);
  if (!fs_data) {
    LOG(ERROR) << "could not initialize filesystem";
    return EX_SOFTWARE;
  }

  struct fuse_args fargs = FUSE_ARGS_INIT(0, nullptr);
  fuse_opt_add_arg(&fargs, argv[0]);
  fuse_opt_add_arg(&fargs, "-f");
  fuse_opt_add_arg(&fargs, "-odev,allow_other,default_permissions");
  fuse_opt_add_arg(&fargs, mount_point.c_str());

  int ret = fuse_main(fargs.argc, fargs.argv, &device_jail::fops,
                      fs_data.get());
  fuse_opt_free_args(&fargs);
  DVLOG(1) << "device_jail_fs exiting";
  return ret;
}
