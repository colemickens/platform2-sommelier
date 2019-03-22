// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "label_detect.h"

/* ioctl() with EINTR retry loop */
int do_ioctl(int fd, int request, void* arg) {
  int ret;
  do {
    ret = ioctl(fd, request, arg);
  } while (ret == -1 && errno == EINTR);
  return ret;
}

/* Loops files matching a pattern to find any device satisfied predicate
 * func(). */
bool is_any_device(const char* pattern, bool (*func)(int fd)) {
  int i;
  glob_t g;
  bool found = false;

  memset(&g, 0, sizeof(g));
  glob(pattern, 0, NULL, &g);
  for (i = 0; i < g.gl_pathc; i++) {
    TRACE("found device file %s\n", g.gl_pathv[i]);
    int fd = open(g.gl_pathv[i], O_RDWR);
    if (fd == -1) {
      TRACE("failed to open device\n");
      continue;
    }
    if (func(fd)) {
      found = true;
      close(fd);
      break;
    }
    close(fd);
  }
  globfree(&g);
  return found;
}

/* Loops files matching a pattern to find any device satisfied predicate
 * func() that requires the device's path.
 */
bool is_any_device_with_path(const char* pattern,
                             bool (*func)(const char* path, int fd)) {
  int i;
  glob_t g;
  bool found = false;

  memset(&g, 0, sizeof(g));
  glob(pattern, 0, NULL, &g);
  for (i = 0; i < g.gl_pathc; i++) {
    TRACE("found device file %s\n", g.gl_pathv[i]);
    int fd = open(g.gl_pathv[i], O_RDWR);
    if (fd == -1) {
      TRACE("failed to open device\n");
      continue;
    }
    if (func(g.gl_pathv[i], fd)) {
      found = true;
      close(fd);
      break;
    }
    close(fd);
  }
  globfree(&g);
  return found;
}

/* Converts FourCC 32 bits integer to printable string. */
void convert_fourcc_to_str(uint32_t fourcc, char* str) {
  str[0] = fourcc & 0xff;
  str[1] = fourcc >> 8 & 0xff;
  str[2] = fourcc >> 16 & 0xff;
  str[3] = fourcc >> 24 & 0xff;
  str[4] = '\0';
}
