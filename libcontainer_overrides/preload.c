// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "broker_client.h"

#define _GNU_SOURCE
#include <dlfcn.h>  // dlsym(...)
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <brillo/brillo_export.h>
#define _FCNTL_H
#include <bits/fcntl.h>

// Route all requests starting with /dev/bus/usb to broker_service, which
// talks to permission_broker to get the fds, if they are legit.
#define OUR_USB_PATH "/dev/bus/usb"

BRILLO_EXPORT int (*_open)(const char* pathname, int flags, ...);
BRILLO_EXPORT int (*_open64)(const char* pathname, int flags, ...);

// Returns 1 if 'str' starts with 'prefix' else 0.
int StartsWith(const char* prefix, const char* str) {
  size_t lenp = strlen(prefix), lens = strlen(str);
  return lens < lenp ? 0 : strncmp(prefix, str, lenp) == 0;
}

static int open_override(const char *func, const char* pathname, int flags, ...)
{
  // get the next function call
  _open = (int (*)(const char*, int, ...)) dlsym(RTLD_NEXT, func);

  // Asking permission_broker to OpenPath(...) is equivalent to opening the
  // same thing as root, and with O_RDWR.
  if (StartsWith(OUR_USB_PATH, pathname) && ((flags & O_RDWR) != 0)) {
    int broker_sockfd = ConnectToBroker(BROKER_SOCKET_PATH, BROKER_SOCKET_PATH_LEN);
    int return_val = OpenWithPermissions(broker_sockfd, pathname);
    close(broker_sockfd);
    // If permission_broker gives -1, behave as if it was never asked.
    if (return_val >= 0) {
      return return_val;
    }
  }

  if (flags & (O_CREAT | O_TMPFILE)) {
    va_list ap;
    va_start(ap, flags);
    mode_t mode = va_arg(ap, mode_t);
    va_end(ap);
    return _open(pathname, flags, mode);
  }
  return _open(pathname, flags);
}

static int open64_override(const char *func, const char* pathname, int flags, ...)
{
  // get the next function call
  _open64 = (int (*)(const char*, int, ...)) dlsym(RTLD_NEXT, func);

  // Asking permission_broker to OpenPath(...) is equivalent to opening the
  // same thing as root, and with O_RDWR.
  if (StartsWith(OUR_USB_PATH, pathname) && ((flags & O_RDWR) != 0)) {
    int broker_sockfd = ConnectToBroker(BROKER_SOCKET_PATH, BROKER_SOCKET_PATH_LEN);
    int return_val = OpenWithPermissions(broker_sockfd, pathname);
    close(broker_sockfd);
    // If permission_broker gives -1, behave as if it was never asked.
    if (return_val >= 0) {
      return return_val;
    }
  }

  if (flags & (O_CREAT | O_TMPFILE)) {
    va_list ap;
    va_start(ap, flags);
    mode_t mode = va_arg(ap, mode_t);
    va_end(ap);
    return _open64(pathname, flags, mode);
  }
  return _open64(pathname, flags);
}

BRILLO_EXPORT int open(const char* pathname, int flags, ...) {
  return open_override("open", pathname, flags);
}

BRILLO_EXPORT int __open_2(const char* pathname, int flags, ...) {
  return open_override("__open_2", pathname, flags);
}

BRILLO_EXPORT int open64(const char* pathname, int flags, ...) {
  return open64_override("open64", pathname, flags);
}

BRILLO_EXPORT int __open64_2(const char* pathname, int flags, ...) {
  return open64_override("__open64_2", pathname, flags);
}
