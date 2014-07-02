/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* pread and pwrite want this */
#define _GNU_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

int chromeos_verity(const char *alg, const char *device, unsigned blocksize,
                    uint64_t fs_blocks, const char *salt, const char *expected,
                    int warn)
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

  fd = open(device, O_RDWR );
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

  ret = dm_bht_compute(&bht);
  if (ret) {
    printf("%s: dm_bht_compute returned error %d\n", __func__, ret);
    close(fd);
    free(hash_buffer);
    return ret;
  }

  dm_bht_root_hexdigest(&bht, digest, DM_BHT_MAX_DIGEST_SIZE);

  if (warn && memcmp(digest, expected, bht.digest_size)) {
    printf("Filesystem hash verification failed\n");
    printf("Expected %s != %s\n",digest, expected);
    free(hash_buffer);
    close(fd);
    return -1;
  }

  if (pwrite(fd, hash_buffer, hash_size, cur_block * blocksize) !=
             (ssize_t)hash_size) {
    printf("%s: writing out hash failed %s\n", __func__, strerror(errno));
    free(hash_buffer);
    close(fd);
    return errno;
  }
  free(hash_buffer);
  close(fd);

  return 0;
}

