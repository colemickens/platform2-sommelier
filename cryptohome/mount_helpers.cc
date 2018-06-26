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