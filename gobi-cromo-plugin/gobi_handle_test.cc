// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Small test for gobi modem handles.  Avoids glog and many other
// dependencies.  Code liberally cribbed from gobi-modem-reset.c

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "QCWWANCMAPI2k.h"

static void log(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fputc('\n', stderr);
  fflush(stderr);
  vsyslog(LOG_INFO, format, args);
  va_end(args);
}

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
  int fd = -1;
  int to_return = 1;

  if (snprintf(path, sizeof(path), "%s/authorized", dev) == sizeof(path)) {
    log("Could not build path");
    goto done;
  }

  fd = open(path, O_WRONLY | O_TRUNC | O_NOFOLLOW);
  if (fd < 0) {
    log("Could not open authorized file: %s", path);
    goto done;
  }

  log("deauthorizing: %s", path);
  if (write(fd, "0", 1) != 1) {
    log("write(0) failed: %d", errno);
    goto done;
  }

  log("sleeping for 1 second");
  sleep(1);

  log("reauthorizing: %s", path);
  if (write(fd, "1", 1) != 1) {
    log("write(1) failed: %d", errno);
    goto done;
  }
  to_return = 0;

done:
  if (fd >= 0) {
    close(fd);
  }
  return to_return;
}

static void usage(const char *progname)
{
  fprintf(stderr, "Usage: %s <file | api> <usb-dev-id> <qcqmi-dev>\n",
          progname);
  fprintf(stderr, "\tExample: %s api 1-2 /dev/qcqmi0\n", progname);
  fprintf(stderr, "To determine the usb dev id, run \n");
  fprintf(stderr, "\tls -d /sys/bus/usb/drivers/QCUSBNet2k/*-*\n");
  fprintf(stderr, "and use the string before the :\n");
}

struct DEVICE_ELEMENT {
  char deviceNode[256];
  char deviceKey[16];
};

// Connect to first modem present.  Returns 0 on success.
int connect_modem()
{
  const int MAX_MODEMS = 16;
  ULONG rc;

  DEVICE_ELEMENT devices[MAX_MODEMS];
  BYTE num_devices = MAX_MODEMS;

  rc = QCWWANEnumerateDevices(&num_devices,
                              (BYTE *) devices);
  if (rc != 0) {
    log("Could not enumerate: %d", rc);
    return -1;
  }

  if (num_devices == 0) {
    log("No devices found");
    return -1;
  }

  rc = QCWWANConnect(devices[0].deviceNode, devices[0].deviceKey);
  if (rc !=0) {
    log("Could not QCWWANConnect to modem: %d", rc);
    return -1;
  }
  return 0;
}


int main(int argc, char *argv[])
{
  char usb_path[PATH_MAX];
  bool use_qcwwan = 1;

  openlog("gobi_handle_tester", LOG_PID, LOG_USER);
  if (argc != 4) {
    usage(argv[0]);
    return 1;
  }

  if (getuid() != 0) {
    fprintf(stderr, "This program must be run as root\n");
    return 1;
  }

  int argument = 1;
  const char *operation = argv[argument++];
  const char *usb_device_id = argv[argument++];
  const char *qmi_device_path = argv[argument++];

  if (!isdevid(usb_device_id)) {
    fprintf(stderr, "Could not parse device id %s\n", usb_device_id);
    usage(argv[0]);
    return 1;
  }

  if (strcmp(operation, "file") == 0) {
    // Just open /dev/qcqmi0 as a file
    use_qcwwan = 0;
  } else if (strcmp(operation, "api") == 0) {
    // Use QCWWANConnect to open a connection to the device
    use_qcwwan = 1;
  } else {
    fprintf(stderr, "Could not understand operation: %s\n", operation);
    usage(argv[0]);
    return 1;
  }

  snprintf(usb_path,
           sizeof(usb_path),
           "/sys/bus/usb/devices/%s", usb_device_id);

  log("operation: %s  use_qcwwan: %d", operation, use_qcwwan);
  log("USB path: %s", usb_path);
  log("device path: %s", qmi_device_path);

  int fd;
  ULONG rc;

  if (use_qcwwan) {
    if (connect_modem() != 0) {
      log("Failure connecting to modem");
      return 4;
    } else {
      fd = open(qmi_device_path, O_RDWR);
      if (fd < 0) {
        fprintf(stderr, "Could not open device %s\n", qmi_device_path);
        return 3;
      }
    }
  }

  if (reset(usb_path) != 0)  {
    log("Reset failed.  Exiting");
    return 6;
  }

  log("sleeping while waiting for kernel handles to expire");

  for (int i = 0 ; i < 45; ++i) {
    sleep(1);
    fprintf(stderr, ".");
  }
  fputc('\n', stderr);

  log("closing");

  if (use_qcwwan) {
    rc = QCWWANDisconnect();
    if (rc != 0) {
      log("Failed on disconnect: %d", rc);
    }
  } else {
    rc = close(fd);
    if (rc != 0) {
      log("Failed on close: %d", rc);
    }
  }
  log("exiting");
  return 0;
}
