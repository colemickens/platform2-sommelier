/* gobi-modem-reset.c - finds all Gobi modems on the system and resets them.
 * This program runs setuid root, so we need to be extra-careful.
 * Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define _GNU_SOURCE /* O_NOFOLLOW is guarded by this */
#include <ctype.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

/* Returns true if the supplied devid string is valid. Valid devid strings are
 * of the form: [0-9]+ '-' [0-9]+ */
static bool isdevid(const char *devid)
{
  const char *d = devid;

  /* Check the part before the dash... */
  if (!d || !isdigit(*d++))
    return false;

  while (*d && isdigit(*d))
    d++;

  /* ... the dash itself ... */
  if (*d++ != '-')
    return false;

  /* ... and the part after the dash. */
  if (!isdigit(*d++))
    return false;

  while (*d && isdigit(*d))
    d++;

  return *d == '\0';
}

static int reset(const char *dev)
{
  char path[PATH_MAX];
  int fd;

  if (snprintf(path, sizeof(path), "%s/authorized", dev) == sizeof(path))
    return 1;
  fd = open(path, O_WRONLY | O_TRUNC | O_NOFOLLOW);
  if (fd < 0)
    return 1;
  if (write(fd, "0", 1) != 1 || write(fd, "1", 1) != 1) {
    close(fd);
    return 1;
  }
  close(fd);
  return 0;
}

static void usage(const char *progname)
{
  printf("Usage: %s <devid>\n", progname);
}

int main(int argc, char *argv[])
{
  char path[PATH_MAX];

  if (argc != 2 || !isdevid(argv[1])) {
    usage(argv[0]);
    return 1;
  }

  openlog("gobi-modem-reset", LOG_PID, LOG_USER);
  syslog(LOG_INFO, "resetting %s", argv[1]);
  closelog();
  snprintf(path, sizeof(path), "/sys/bus/usb/devices/%s", argv[1]);
  return reset(path);
}
