/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* pread and pwrite want this */
#define _GNU_SOURCE 1

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <fcntl.h>
#include <mtd/ubi-user.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <verity/dm-bht.h>
#include <verity/dm-bht-userspace.h>

#define IO_BUF_SIZE (unsigned long)(1 * 1024 * 1024)

/* 512 bytes in a sector */
#define SECTOR_SHIFT (9ULL)

#define __cleanup__(fn) __attribute__((cleanup(fn)))

static void free_mem(char **mem) {
  free(*mem);
}

static void close_file(FILE **f) {
  if (*f) {
    if (fclose(*f)) {
      warn("Cannot close file");
    }
  }
}

static void close_fd(int *fd) {
  if (*fd >= 0) {
    if (close(*fd)) {
      warn("Cannot close file descriptor");
    }
  }
}

/* Obtain LEB size of a UBI volume. Return -1 if |dev| is not a UBI volume. */
static ssize_t get_ubi_leb_size(const char *dev) {
  struct stat stat_buf;
  if (stat(dev, &stat_buf)) {
    warn("Cannot stat %s", dev);
    return -1;
  }

  /* Make sure this is UBI. */
  int dev_major = major(stat_buf.st_rdev);
  int dev_minor = minor(stat_buf.st_rdev);
  char subsystem[PATH_MAX];
  snprintf(subsystem, sizeof(subsystem), "/sys/dev/char/%d:%d/subsystem",
           dev_major, dev_minor);
  subsystem[sizeof(subsystem) - 1] = '\0';
  char subsystem_target[PATH_MAX];
  ssize_t target_length = readlink(subsystem, subsystem_target,
                                   sizeof(subsystem_target) - 1);
  if (target_length < 0) {
    warn("Cannot tell where %s links to", subsystem);
    return -1;
  }
  subsystem_target[target_length] = '\0';
  const char *subsystem_name = basename(subsystem_target);
  if (strcmp(subsystem_name, "ubi") != 0) {
    return -1;
  }

  /* Only a volume has update marker. */
  char upd_marker[PATH_MAX];
  snprintf(upd_marker, sizeof(upd_marker), "/sys/dev/char/%d:%d/upd_marker",
           dev_major, dev_minor);
  upd_marker[sizeof(upd_marker) - 1] = '\0';
  if (access(upd_marker, F_OK) != 0) {
    return -1;
  }

  char usable_eb_size[PATH_MAX];
  snprintf(usable_eb_size, sizeof(usable_eb_size),
           "/sys/dev/char/%d:%d/usable_eb_size", dev_major, dev_minor);
  usable_eb_size[sizeof(usable_eb_size) - 1] = '\0';
  __cleanup__(close_file) FILE *f = NULL;
  f = fopen(usable_eb_size, "r");
  if (!f) {
    warn("Cannot open %s", usable_eb_size);
    return -1;
  }
  __cleanup__(free_mem) char *line = NULL;
  size_t line_len = 0;
  if (getline(&line, &line_len, f) < 0) {
    warn("Cannot read from %s", usable_eb_size);
    return -1;
  }
  char *end;
  ssize_t eb_size = strtol(line, &end, 10);
  if (*end != '\x0A') {
    warn("Cannot convert %s into decimal", line);
    eb_size = -1;
  }
  return eb_size;
}

/* Align |value| up to the nearest greater multiple of |block|. DO NOT assume
 * that |block| is a power of 2. */
static off64_t align_up(off64_t value, off64_t block) {
  off64_t t = value + block - 1;
  return t - (t % block);
}

static ssize_t pwrite_to_ubi(int fd, const void *src, size_t size,
                             off64_t offset, ssize_t eraseblock_size) {
  struct ubi_set_vol_prop_req prop = {
      .property = UBI_VOL_PROP_DIRECT_WRITE,
      .value = 1,
  };
  if (ioctl(fd, UBI_IOCSETVOLPROP, &prop)) {
    warn("Failed to enable direct write");
    return -1;
  }

  /* Save the cursor. */
  off64_t cur_pos = lseek64(fd, 0, SEEK_CUR);
  if (cur_pos < 0) {
    return -1;
  }

  /* Align up to next LEB. */
  offset = align_up(offset, eraseblock_size);
  if (lseek64(fd, offset, SEEK_SET) < 0) {
    return -1;
  }

  __cleanup__(free_mem) char *buf = NULL;
  buf = malloc(eraseblock_size);
  if (!buf) {
    return -1;
  }

  /* Start writing in blocks of LEB size. */
  size_t nr_written = 0;
  while (nr_written < size) {
    size_t to_write = size - nr_written;
    if (to_write > eraseblock_size) {
      to_write = eraseblock_size;
    } else {
      /* UBI layer requires 0xFF padding the erase block. */
      memset(buf + to_write, 0xFF, eraseblock_size - to_write);
    }
    memcpy(buf, src + nr_written, to_write);
    int32_t leb_no = (offset + nr_written) / eraseblock_size;
    if (ioctl(fd, UBI_IOCEBUNMAP, &leb_no) < 0) {
      warn("Cannot unmap LEB %d", leb_no);
      return -1;
    }
    ssize_t chunk_written = write(fd, buf, eraseblock_size);
    if (chunk_written != eraseblock_size) {
      warn("Failed to write to LEB %d", leb_no);
      return -1;
    }
    nr_written += chunk_written;
  }

  /* Reset the cursor. */
  if (lseek64(fd, cur_pos, SEEK_SET) < 0) {
    return -1;
  }

  return nr_written;
}

static int write_hash(const char *dev, void *buf, size_t size, off64_t offset) {
  ssize_t eraseblock_size = get_ubi_leb_size(dev);
  __cleanup__(close_fd) int fd = open(dev, O_WRONLY | O_CLOEXEC);
  if (fd < 0) {
    warn("Cannot open %s for writing", dev);
    return -1;
  }
  if (eraseblock_size <= 0) {
    return pwrite(fd, buf, size, offset);
  } else {
    return pwrite_to_ubi(fd, buf, size, offset, eraseblock_size);
  }
}

int chromeos_verity(const char *alg, const char *device, unsigned blocksize,
                    uint64_t fs_blocks, const char *salt, const char *expected,
                    int enforce_rootfs_verification)
{
  struct dm_bht bht;
  int ret, fd;
  uint8_t *io_buffer;
  uint8_t *hash_buffer;
  size_t hash_size;
  uint8_t digest[DM_BHT_MAX_DIGEST_SIZE];
  uint64_t cur_block = 0;

  /* blocksize better be a power of two and fit into 1 MB*/
  if (IO_BUF_SIZE % blocksize != 0) {
    printf("%s: blocksize %% %lu != 0\n", __func__,
           IO_BUF_SIZE);
    return -EINVAL;
  }

  /* we don't need to call dm_bht_destroy after this because we're */
  /* supplying our own buffer -- in fact calling it will trigger a bogus */
  /* assert */
  if ((ret = dm_bht_create(&bht, fs_blocks, alg))) {
    printf("%s: dm_bht_create failed %d\n", __func__, ret);
    return ret;
  }

  if ((ret = posix_memalign((void**)&io_buffer, blocksize, IO_BUF_SIZE))) {
    printf("%s: posix_memalign io_buffer failed %d\n", __func__, ret);
    return ret;
  }

  /* we aren't going to do any automatic reading */
  dm_bht_set_read_cb(&bht, dm_bht_zeroread_callback);
  dm_bht_set_salt(&bht, salt);
  hash_size = dm_bht_sectors(&bht) << SECTOR_SHIFT;

  if ((ret = posix_memalign((void**)&hash_buffer, blocksize, hash_size))) {
    printf("%s: posix_memalign hash_buffer failed %d\n", __func__, ret);
    free(io_buffer);
    return ret;
  }
  memset(hash_buffer, 0, hash_size);
  dm_bht_set_buffer(&bht, hash_buffer);

  fd = open(device, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    printf("%s error opening %s: %s\n", __func__, device, strerror(errno));
    free(io_buffer);
    free(hash_buffer);
    return errno;
  }

  while (cur_block < fs_blocks) {
    unsigned int i;
    ssize_t readb;
    size_t count = (fs_blocks - cur_block) * blocksize;

    if (count > IO_BUF_SIZE)
      count = IO_BUF_SIZE;

    readb = pread(fd, io_buffer, count, cur_block * blocksize);
    if (readb < 0) {
      printf("%s: read returned error %s\n", __func__, strerror(errno));
      close(fd);
      free(io_buffer);
      free(hash_buffer);
      return errno;
    }

    for (i = 0 ; i < (count / blocksize) ; i++) {
      ret = dm_bht_store_block(&bht, cur_block, io_buffer + (i * blocksize));
      if (ret) {
        printf("%s: dm_bht_store_block returned error %d\n", __func__, ret);
        close(fd);
        free(io_buffer);
        free(hash_buffer);
        return ret;
      }
      cur_block++;
    }
  }
  free(io_buffer);
  close(fd);

  ret = dm_bht_compute(&bht);
  if (ret) {
    printf("%s: dm_bht_compute returned error %d\n", __func__, ret);
    free(hash_buffer);
    return ret;
  }

  dm_bht_root_hexdigest(&bht, digest, DM_BHT_MAX_DIGEST_SIZE);

  if (memcmp(digest, expected, bht.digest_size)) {
    printf("Filesystem hash verification failed\n");
    printf("Expected %s != actual %s\n", expected, digest);
    if (enforce_rootfs_verification) {
      free(hash_buffer);
      return -1;
    } else {
      printf("Verified Boot not enabled; ignoring\n");
    }
  }


  if (write_hash(device, hash_buffer, hash_size, cur_block * blocksize) <
      (ssize_t)hash_size) {
    printf("%s: writing out hash failed %s\n", __func__, strerror(errno));
    free(hash_buffer);
    return errno;
  }
  free(hash_buffer);

  return 0;
}
