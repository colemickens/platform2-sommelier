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