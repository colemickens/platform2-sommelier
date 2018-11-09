/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define FUSE_USE_VERSION 26

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse/fuse.h>
#include <fuse/fuse_common.h>
#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>

#include <string>

#define USER_NS_SHIFT 655360
#define CHRONOS_UID 1000
#define CHRONOS_GID 1000

#define WRAP_FS_CALL(res) ((res) < 0 ? -errno : 0)

namespace {

int passthrough_create(const char* path,
                       mode_t mode,
                       struct fuse_file_info* fi) {
  // Ignore specified |mode| and always use a fixed mode since we do not allow
  // chmod anyway. Note that we explicitly set the umask to 0022 in main().
  int fd = open(path, fi->flags, 0644);
  if (fd < 0) {
    return -errno;
  }
  fi->fh = fd;
  return 0;
}

int passthrough_fgetattr(const char*,
                         struct stat* buf,
                         struct fuse_file_info* fi) {
  int fd = static_cast<int>(fi->fh);
  // File owner is overridden by uid/gid options passed to fuse.
  return WRAP_FS_CALL(fstat(fd, buf));
}

int passthrough_fsync(const char*, int datasync, struct fuse_file_info* fi) {
  int fd = static_cast<int>(fi->fh);
  return datasync ? WRAP_FS_CALL(fdatasync(fd)) : WRAP_FS_CALL(fsync(fd));
}

int passthrough_fsyncdir(const char*, int datasync, struct fuse_file_info* fi) {
  DIR* dirp = reinterpret_cast<DIR*>(fi->fh);
  int fd = dirfd(dirp);
  return datasync ? WRAP_FS_CALL(fdatasync(fd)) : WRAP_FS_CALL(fsync(fd));
}

int passthrough_ftruncate(const char*, off_t size, struct fuse_file_info* fi) {
  int fd = static_cast<int>(fi->fh);
  return WRAP_FS_CALL(ftruncate(fd, size));
}

int passthrough_getattr(const char* path, struct stat* buf) {
  // File owner is overridden by uid/gid options passed to fuse.
  return WRAP_FS_CALL(lstat(path, buf));
}

int passthrough_mkdir(const char* path, mode_t mode) {
  return WRAP_FS_CALL(mkdir(path, mode));
}

int passthrough_open(const char* path, struct fuse_file_info* fi) {
  int fd = open(path, fi->flags);
  if (fd < 0) {
    return -errno;
  }
  fi->fh = fd;
  return 0;
}

int passthrough_opendir(const char* path, struct fuse_file_info* fi) {
  DIR* dirp = opendir(path);
  if (!dirp) {
    return -errno;
  }
  fi->fh = reinterpret_cast<uint64_t>(dirp);
  return 0;
}

int passthrough_read(
    const char*, char* buf, size_t size, off_t off, struct fuse_file_info* fi) {
  int fd = static_cast<int>(fi->fh);
  int res = pread(fd, buf, size, off);
  if (res < 0) {
    return -errno;
  }
  return res;
}

int passthrough_read_buf(const char*,
                         struct fuse_bufvec** srcp,
                         size_t size,
                         off_t off,
                         struct fuse_file_info* fi) {
  int fd = static_cast<int>(fi->fh);
  struct fuse_bufvec* src =
      static_cast<struct fuse_bufvec*>(malloc(sizeof(struct fuse_bufvec)));
  *src = FUSE_BUFVEC_INIT(size);
  src->buf[0].flags =
      static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
  src->buf[0].fd = fd;
  src->buf[0].pos = off;
  *srcp = src;
  return 0;
}

int passthrough_readdir(const char*,
                        void* buf,
                        fuse_fill_dir_t filler,
                        off_t off,
                        struct fuse_file_info* fi) {
  // TODO(nya): This implementation returns all files at once and thus
  // inefficient. Make use of offset and be better to memory.
  DIR* dirp = reinterpret_cast<DIR*>(fi->fh);
  errno = 0;
  for (;;) {
    struct dirent* entry = readdir(dirp);
    if (entry == nullptr) {
      break;
    }
    // Only IF part of st_mode matters. See fill_dir() in fuse.c.
    struct stat stbuf = {};
    stbuf.st_mode = DTTOIF(entry->d_type);
    filler(buf, entry->d_name, &stbuf, 0);
  }
  return -errno;
}

int passthrough_release(const char*, struct fuse_file_info* fi) {
  int fd = static_cast<int>(fi->fh);
  return WRAP_FS_CALL(close(fd));
}

int passthrough_releasedir(const char*, struct fuse_file_info* fi) {
  DIR* dirp = reinterpret_cast<DIR*>(fi->fh);
  return WRAP_FS_CALL(closedir(dirp));
}

int passthrough_rename(const char* oldpath, const char* newpath) {
  return WRAP_FS_CALL(rename(oldpath, newpath));
}

int passthrough_rmdir(const char* path) {
  return WRAP_FS_CALL(rmdir(path));
}

int passthrough_statfs(const char* path, struct statvfs* buf) {
  return WRAP_FS_CALL(statvfs(path, buf));
}

int passthrough_truncate(const char* path, off_t size) {
  return WRAP_FS_CALL(truncate(path, size));
}

int passthrough_unlink(const char* path) {
  return WRAP_FS_CALL(unlink(path));
}

int passthrough_utimens(const char* path, const struct timespec tv[2]) {
  return WRAP_FS_CALL(utimensat(AT_FDCWD, path, tv, 0));
}

int passthrough_write(const char*,
                      const char* buf,
                      size_t size,
                      off_t off,
                      struct fuse_file_info* fi) {
  int fd = static_cast<int>(fi->fh);
  int res = pwrite(fd, buf, size, off);
  if (res < 0) {
    return -errno;
  }
  return res;
}

int passthrough_write_buf(const char*,
                          struct fuse_bufvec* src,
                          off_t off,
                          struct fuse_file_info* fi) {
  int fd = static_cast<int>(fi->fh);
  struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(src));
  dst.buf[0].flags =
      static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
  dst.buf[0].fd = fd;
  dst.buf[0].pos = off;
  return fuse_buf_copy(&dst, src, static_cast<fuse_buf_copy_flags>(0));
}

void setup_passthrough_ops(struct fuse_operations* passthrough_ops) {
  memset(passthrough_ops, 0, sizeof(*passthrough_ops));
#define FILL_OP(name) passthrough_ops->name = passthrough_##name
  FILL_OP(create);
  FILL_OP(fgetattr);
  FILL_OP(fsync);
  FILL_OP(fsyncdir);
  FILL_OP(ftruncate);
  FILL_OP(getattr);
  FILL_OP(mkdir);
  FILL_OP(open);
  FILL_OP(opendir);
  FILL_OP(read);
  FILL_OP(read_buf);
  FILL_OP(readdir);
  FILL_OP(release);
  FILL_OP(releasedir);
  FILL_OP(rename);
  FILL_OP(rmdir);
  FILL_OP(statfs);
  FILL_OP(truncate);
  FILL_OP(unlink);
  FILL_OP(utimens);
  FILL_OP(write);
  FILL_OP(write_buf);
#undef FILL_OP
  passthrough_ops->flag_nullpath_ok = 1;
  passthrough_ops->flag_nopath = 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 6) {
    fprintf(stderr, "usage: %s <source> <destination> <umask> <uid> <gid>\n",
            argv[0]);
    return 1;
  }

  if (getuid() != CHRONOS_UID) {
    fprintf(stderr, "This daemon must run as chronos user.\n");
    return 1;
  }
  if (getgid() != CHRONOS_GID) {
    fprintf(stderr, "This daemon must run as chronos group.\n");
    return 1;
  }

  struct fuse_operations passthrough_ops;
  setup_passthrough_ops(&passthrough_ops);

  uid_t uid = std::stoi(argv[4]) + USER_NS_SHIFT;
  gid_t gid = std::stoi(argv[5]) + USER_NS_SHIFT;
  std::string fuse_subdir_opt(std::string("subdir=") + argv[1]);
  std::string fuse_uid_opt(std::string("uid=") + std::to_string(uid));
  std::string fuse_gid_opt(std::string("gid=") + std::to_string(gid));
  std::string fuse_umask_opt(std::string("umask=") + argv[3]);
  fprintf(stderr, "subdir_opt(%s) uid_opt(%s) gid_opt(%s) umask_opt(%s)",
          fuse_subdir_opt.c_str(), fuse_uid_opt.c_str(), fuse_gid_opt.c_str(),
          fuse_umask_opt.c_str());

  const char* fuse_argv[] = {
      argv[0], argv[2], "-f", "-o", "allow_other", "-o", "default_permissions",
      // Never cache attr/dentry since our backend storage is not exclusive to
      // this process.
      "-o", "attr_timeout=0", "-o", "entry_timeout=0", "-o",
      "negative_timeout=0", "-o", "ac_attr_timeout=0", "-o",
      "fsname=passthrough", "-o", fuse_uid_opt.c_str(), "-o",
      fuse_gid_opt.c_str(), "-o", "modules=subdir", "-o",
      fuse_subdir_opt.c_str(), "-o", "direct_io", "-o", fuse_umask_opt.c_str(),
  };
  int fuse_argc = sizeof(fuse_argv) / sizeof(fuse_argv[0]);

  umask(0022);
  return fuse_main(fuse_argc, const_cast<char**>(fuse_argv), &passthrough_ops,
                   NULL);
}
