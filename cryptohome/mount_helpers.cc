/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is a collection of helper utilities for use with the "mount-encrypted"
 * utility.
 *
 */
#define _FILE_OFFSET_BITS 64
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/fs.h>
#include <linux/loop.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "mount_encrypted.h"
#include "mount_helpers.h"

static const gchar kRootDir[] = "/";
static const gchar kSysBlockPath[] = "/sys/block";
static const gchar kLoopPrefix[] = "loop";
static const gchar kLoopTemplate[] = "/dev/loop%u";
static const gchar kLoopControl[] = "/dev/loop-control";
static const gchar kDevTemplate[] = "/dev/%s";
static const int kLoopMajor = 7;
static const unsigned int kResizeStepSeconds = 2;
static const uint64_t kResizeBlocks = 32768 * 10;
static const uint64_t kBlocksPerGroup = 32768;
static const uint64_t kInodeRatioDefault = 16384;
static const uint64_t kInodeRatioMinimum = 2048;
static const gchar* const kExt4ExtendedOptions = "discard,lazy_itable_init";

int runcmd(const gchar* argv[], gchar** output) {
  gint rc;
  gchar *out = NULL, *errout = NULL;
  GError* err = NULL;

  g_spawn_sync(kRootDir, (gchar**)argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, &out,
               &errout, &rc, &err);
  if (err) {
    ERROR("%s: %s", argv[0], err->message);
    g_error_free(err);
    return -1;
  }

  if (rc)
    ERROR("%s failed (%d)\n%s\n%s", argv[0], rc, out, errout);

  if (output)
    *output = out;
  else
    g_free(out);
  g_free(errout);

  return rc;
}


/* Overwrite file contents. Useless on SSD. :( */
void shred(const char* pathname) {
  uint8_t patterns[] = {0xA5, 0x5A, 0xFF, 0x00};
  FILE* target;
  struct stat info;
  uint8_t* pattern;
  int fd, i;

  /* Give up if we can't safely open or stat the target. */
  if ((fd = open(pathname, O_WRONLY | O_NOFOLLOW | O_CLOEXEC)) < 0) {
    PERROR("%s", pathname);
    return;
  }
  if (fstat(fd, &info)) {
    close(fd);
    PERROR("%s", pathname);
    return;
  }
  if (!(target = fdopen(fd, "w"))) {
    close(fd);
    PERROR("%s", pathname);
    return;
  }
  /* Ignore errors here, since there's nothing we can really do. */
  pattern = (uint8_t*)malloc(info.st_size);
  for (i = 0; i < sizeof(patterns); ++i) {
    memset(pattern, patterns[i], info.st_size);
    if (fseek(target, 0, SEEK_SET))
      PERROR("%s", pathname);
    if (fwrite(pattern, info.st_size, 1, target) != 1)
      PERROR("%s", pathname);
    if (fflush(target))
      PERROR("%s", pathname);
    if (fdatasync(fd))
      PERROR("%s", pathname);
  }
  free(pattern);
  /* fclose() closes the fd too. */
  fclose(target);
}

static int is_loop_device(int fd) {
  struct stat info;

  return (fstat(fd, &info) == 0 && S_ISBLK(info.st_mode) &&
          major(info.st_rdev) == kLoopMajor);
}

static int loop_is_attached(int fd, struct loop_info64* info) {
  struct loop_info64 local_info;

  return ioctl(fd, LOOP_GET_STATUS64, info ? info : &local_info) == 0;
}

/* Returns the matching loopback name. */
static int loop_locate(gchar** loopback, const char* name) {
  int fd = -1;
  size_t namelen = 0;
  DIR* sysfs_block_dir = NULL;

  namelen = strlen(name);
  if (namelen >= LO_NAME_SIZE) {
    ERROR("'%s' too long (>= %d)", name, LO_NAME_SIZE);
    return -1;
  }

  *loopback = NULL;
  /* Read /sys/block to discover all loop devices. */
  sysfs_block_dir = opendir(kSysBlockPath);
  if (!sysfs_block_dir) {
    PERROR("open(%s)", kSysBlockPath);
    return -1;
  }

  while (1) {
    int attached = 0;
    struct loop_info64 info;
    struct dirent* dirent = NULL;

    errno = 0;
    dirent = readdir(sysfs_block_dir);
    if (!dirent) {
      if (errno) {
        PERROR("readdir(%s)", kSysBlockPath);
        goto failed;
      }
      break;
    }

    if (strncmp(dirent->d_name, kLoopPrefix, sizeof(kLoopPrefix) - 1)) {
      continue;
    }

    g_free(*loopback);
    *loopback = g_strdup_printf(kDevTemplate, dirent->d_name);
    if (!*loopback) {
      PERROR("g_strdup_printf");
      goto failed;
    }

    fd = open(*loopback, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
      PERROR("open(%s)", *loopback);
      goto failed;
    }
    if (!is_loop_device(fd)) {
      close(fd);
      continue;
    }

    memset(&info, 0, sizeof(info));
    attached = loop_is_attached(fd, &info);
    close(fd);

    DEBUG("Saw %s on %s", info.lo_file_name, *loopback);

    if (attached && name &&
        strncmp((char*)info.lo_file_name, name, namelen) == 0) {
      DEBUG("Using %s", *loopback);

      /* Reopen for working on it. Note that strictly
       * speaking, there is a TOCTOU issue here because other
       * code can theoretically tear down and re-use the loop
       * device at any point in time. However, in practice we
       * assume that the devices cryptohomed has created are
       * only manipulated subsequently by cryptohomed, so we
       * should be safe.
       */
      fd = open(*loopback, O_RDWR | O_NOFOLLOW | O_CLOEXEC);
      if (fd < 0) {
        PERROR("open(%s)", *loopback);
        goto failed;
      }

      if (!is_loop_device(fd) || !loop_is_attached(fd, NULL)) {
        ERROR("%s in bad state", *loopback);
        close(fd);
        goto failed;
      }

      closedir(sysfs_block_dir);
      return fd;
    }
  }

  ERROR("Ran out of loopback devices");

failed:
  closedir(sysfs_block_dir);
  g_free(*loopback);
  *loopback = NULL;
  return -1;
}

static int loop_detach_fd(int fd) {
  if (ioctl(fd, LOOP_CLR_FD, 0)) {
    PERROR("LOOP_CLR_FD");
    return 0;
  }
  return 1;
}

int loop_detach(const gchar* loopback) {
  int fd, rc = 1;

  fd = open(loopback, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) {
    PERROR("open(%s)", loopback);
    return 0;
  }
  if (!is_loop_device(fd) || !loop_is_attached(fd, NULL) || !loop_detach_fd(fd))
    rc = 0;

  close(fd);
  return rc;
}

int loop_detach_name(const char* name) {
  gchar* loopback = NULL;
  int loopfd, rc;

  loopfd = loop_locate(&loopback, name);
  if (loopfd < 0)
    return 0;
  rc = loop_detach_fd(loopfd);

  close(loopfd);
  g_free(loopback);
  return rc;
}

/* Closes fd, returns name of loopback device pathname. */
gchar* loop_attach(int fd, const char* name) {
  gchar* loopback = NULL;
  gchar* result = NULL;
  int control_fd = -1;
  int loop_fd = -1;
  int num = -1;
  struct loop_info64 info;

  control_fd = open(kLoopControl, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (control_fd < 0) {
    PERROR("open(%s)", kLoopControl);
    goto out;
  }

  while (1) {
    /* LOOP_CTL_GET_FREE returns the number of an unused loop device
     * or if there is none, creates a new loop device and returns
     * its number. Note that this races against other code trying
     * to get a loop device concurrently, so it's possible another
     * process picks the same "free" loop device as we do, and then
     * collides with us binding it to a backing file...
     *
     * Fortunately, LOOP_SET_FD is atomic, i.e. it fails when the
     * loop device is already attached to a file. We use this for
     * detecting collisions and retry on EBUSY.
     */
    num = ioctl(control_fd, LOOP_CTL_GET_FREE);
    if (num < 0) {
      PERROR("ioctl(LOOP_CTL_GET_FREE)");
      goto out;
    }

    g_free(loopback);
    loopback = g_strdup_printf(kLoopTemplate, num);
    if (!loopback) {
      PERROR("g_strdup_printf");
      goto out;
    }

    loop_fd = open(loopback, O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    if (loop_fd < 0) {
      PERROR("open(%s)", loopback);
      goto out;
    }

    if (!is_loop_device(loop_fd)) {
      goto out;
    }

    if (ioctl(loop_fd, LOOP_SET_FD, fd) == 0) {
      break;
    }

    /* Retry on LOOP_SET_FD coming back with EBUSY. */
    if (errno != EBUSY) {
      PERROR("LOOP_SET_FD");
      goto out;
    }
  }

  DEBUG("Allocated loop device %d\n", num);

  memset(&info, 0, sizeof(info));
  strncpy((char*)info.lo_file_name, name, LO_NAME_SIZE);
  if (ioctl(loop_fd, LOOP_SET_STATUS64, &info)) {
    PERROR("LOOP_SET_STATUS64");
    goto out;
  }

  result = loopback;
  loopback = NULL;

out:
  close(control_fd);
  close(loop_fd);
  close(fd);
  g_free(loopback);
  return result;
}

int dm_setup(uint64_t sectors,
             const gchar* encryption_key,
             const char* name,
             const gchar* device,
             const char* path,
             int discard) {
  /* Mount loopback device with dm-crypt using the encryption key. */
  gchar* table = g_strdup_printf("0 %" PRIu64
                                 " crypt "
                                 "aes-cbc-essiv:sha256 %s "
                                 "0 %s 0%s",
                                 sectors, encryption_key, device,
                                 discard ? " 1 allow_discards" : "");
  if (!table) {
    PERROR("g_strdup_printf");
    return 0;
  }

  const gchar* argv[] = {"/sbin/dmsetup", "create",  name,
                         "--table", table, NULL};

  /* TODO(keescook): replace with call to libdevmapper. */
  if (runcmd(argv, NULL) != 0) {
    g_free(table);
    return 0;
  }
  g_free(table);

  /* Make sure udev is done with events. */
  const gchar* settle_argv[] = {
      "/bin/udevadm", "settle", "-t", "10", "-E", path, NULL};
  if (runcmd(settle_argv, NULL) != 0) {
    return 0;
  }

  /* Make sure the dm-crypt device showed up. */
  if (access(path, R_OK)) {
    ERROR("%s does not exist", path);
    return 0;
  }

  return 1;
}

int dm_teardown(const gchar* device) {
  const char* argv[] = {"/sbin/dmsetup", "remove", device, NULL};
  /* TODO(keescook): replace with call to libdevmapper. */
  if (runcmd(argv, NULL) != 0)
    return 0;

  /* Make sure udev is done with events. */
  const gchar* settle_argv[] = {"/bin/udevadm", "settle", NULL};
  if (runcmd(settle_argv, NULL) != 0) {
    return 0;
  }

  return 1;
}

char* dm_get_key(const gchar* device) {
  gchar* output = NULL;
  char* key;
  int i;
  const char* argv[] = {"/sbin/dmsetup", "table", "--showkeys", device, NULL};
  /* TODO(keescook): replace with call to libdevmapper. */
  if (runcmd(argv, &output) != 0)
    return NULL;

  /* Key is 4th field in the output. */
  for (i = 0, key = strtok(output, " "); i < 4 && key;
       ++i, key = strtok(NULL, " ")) {
  }

  /* Create a copy of the key and free the output buffer. */
  if (key) {
    key = strdup(key);
    g_free(output);
  }

  return key;
}

/* When creating a filesystem that will grow, the inode ratio is calculated
 * using the starting size not the hinted "resize" size, which means the
 * number of inodes can be highly constrained on tiny starting filesystems.
 * Instead, calculate what the correct inode ratio should be for a given
 * filesystem based on its expected starting and ending sizes.
 *
 * inode-ratio_mkfs =
 *
 *               ceil(blocks_max / group-ratio) * size_mkfs
 *      ------------------------------------------------------------------
 *      ceil(size_max / inode-ratio_max) * ceil(blocks_mkfs / group-ratio)
 */
static uint64_t get_inode_ratio(uint64_t block_bytes_in,
                                uint64_t blocks_mkfs_in,
                                uint64_t blocks_max_in) {
  double block_bytes = (double)block_bytes_in;
  double blocks_mkfs = (double)blocks_mkfs_in;
  double blocks_max = (double)blocks_max_in;

  double size_max, size_mkfs, groups_max, groups_mkfs, inodes_max;
  double denom, inode_ratio_mkfs;

  size_max = block_bytes * blocks_max;
  size_mkfs = block_bytes * blocks_mkfs;

  groups_max = ceil(blocks_max / kBlocksPerGroup);
  groups_mkfs = ceil(blocks_mkfs / kBlocksPerGroup);

  inodes_max = ceil(size_max / kInodeRatioDefault);

  denom = inodes_max * groups_mkfs;
  /* Make sure we never trigger divide-by-zero. */
  if (denom == 0.0)
    goto failure;
  inode_ratio_mkfs = (groups_max * size_mkfs) / denom;

  /* Make sure we never calculate anything totally huge. */
  if (inode_ratio_mkfs > blocks_mkfs)
    goto failure;
  /* Make sure we never calculate anything totally tiny. */
  if (inode_ratio_mkfs < kInodeRatioMinimum)
    goto failure;

  return (uint64_t)inode_ratio_mkfs;

failure:
  return kInodeRatioDefault;
}

/* Creates an ext4 filesystem.
 * device: path to block device to create filesystem on.
 * block_bytes: bytes per block to use for filesystem.
 * blocks_min: starting number of blocks on filesystem.
 * blocks_max: largest expected size in blocks of filesystem, for growth hints.
 *
 * Returns 1 on success, 0 on failure.
 */
int filesystem_build(const char* device,
                     uint64_t block_bytes,
                     uint64_t blocks_min,
                     uint64_t blocks_max) {
  int rc = 0;
  uint64_t inode_ratio;

  gchar* blocksize = g_strdup_printf("%" PRIu64, block_bytes);
  if (!blocksize) {
    PERROR("g_strdup_printf");
    goto out;
  }

  gchar* blocks_str;
  blocks_str = g_strdup_printf("%" PRIu64, blocks_min);
  if (!blocks_str) {
    PERROR("g_strdup_printf");
    goto free_blocksize;
  }

  gchar* extended;
  if (blocks_min < blocks_max) {
    extended =
        g_strdup_printf("%s,resize=%" PRIu64, kExt4ExtendedOptions, blocks_max);
  } else {
    extended = g_strdup_printf("%s", kExt4ExtendedOptions);
  }
  if (!extended) {
    PERROR("g_strdup_printf");
    goto free_blocks_str;
  }

  inode_ratio = get_inode_ratio(block_bytes, blocks_min, blocks_max);
  gchar* inode_ratio_str;
  inode_ratio_str = g_strdup_printf("%" PRIu64, inode_ratio);
  if (!inode_ratio_str) {
    PERROR("g_strdup_printf");
    goto free_extended;
  }

  /* Add scope to ensure compilation with C++: "error: jump bypasses
   * initialization" goto * above jumps over the definition of mkfs[] and
   * tune2fs[] */
  {
    const gchar* mkfs[] = {"/sbin/mkfs.ext4",
                           "-T",
                           "default",
                           "-b",
                           blocksize,
                           "-m",
                           "0",
                           "-O",
                           "^huge_file,^flex_bg",
                           "-i",
                           inode_ratio_str,
                           "-E",
                           extended,
                           device,
                           blocks_str,
                           NULL};

    rc = (runcmd(mkfs, NULL) == 0);
    if (!rc)
      goto free_inode_ratio_str;
  }

  {
    const gchar* tune2fs[] = {"/sbin/tune2fs", "-c", "0", "-i", "0",
                              device,          NULL};
    rc = (runcmd(tune2fs, NULL) == 0);
  }
free_inode_ratio_str:
  g_free(inode_ratio_str);
free_extended:
  g_free(extended);
free_blocks_str:
  g_free(blocks_str);
free_blocksize:
  g_free(blocksize);
out:
  return rc;
}

/* Spawns a filesystem resizing process. */
int filesystem_resize(const char* device,
                      uint64_t blocks,
                      uint64_t blocks_max) {
  /* Ignore resizing if we know the filesystem was built to max size. */
  if (blocks >= blocks_max) {
    INFO("Resizing aborted. blocks:%" PRIu64 " >= blocks_max:%" PRIu64, blocks,
         blocks_max);
    return 1;
  }

  /* TODO(keescook): Read superblock to find out the current size of
   * the filesystem (since statvfs does not report the correct value).
   * For now, instead of doing multi-step resizing, just resize to the
   * full size of the block device in one step.
   */
  blocks = blocks_max;

  INFO("Resizing started in %d second steps.", kResizeStepSeconds);

  do {
    gchar* blocks_str;

    sleep(kResizeStepSeconds);

    blocks += kResizeBlocks;
    if (blocks > blocks_max)
      blocks = blocks_max;

    blocks_str = g_strdup_printf("%" PRIu64, blocks);
    if (!blocks_str) {
      PERROR("g_strdup_printf");
      return 0;
    }

    const gchar* resize[] = {"/sbin/resize2fs", "-f", device, blocks_str, NULL};

    INFO("Resizing filesystem on %s to %" PRIu64 ".", device, blocks);
    if (runcmd(resize, NULL)) {
      ERROR("resize2fs failed");
      return 0;
    }
    g_free(blocks_str);
  } while (blocks < blocks_max);

  INFO("Resizing finished.");
  return 1;
}
