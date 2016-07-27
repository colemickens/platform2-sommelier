// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "broker_client.h"

#include <errno.h>
#include <limits.h>  // PATH_MAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

// Keep sending data until |len| data is sent. If partial data sent, return -1.
int SendAll(int sockfd, const void* buf, int len, int flags) {
  int sent = 0;
  while (sent < len) {
    int rc = send(sockfd, buf+sent, len-sent, flags);
    if (rc < 0 && errno != EINTR)
      return -1;

    sent += len;
  }
  return sent;
}

int ConnectToBroker(const char *sockname, int socknamelen) {
  struct sockaddr_un addr = {0};
  int maxpath = sizeof(addr.sun_path);
  int pathlen = socknamelen;
  if (pathlen >= maxpath) {
    errno = EINVAL;
    return -1;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  memcpy(addr.sun_path, sockname, socknamelen);

  int s = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s < 0)
    return -1;

  socklen_t addrlen = pathlen + sizeof(addr.sun_family);

  if (connect(s, (struct sockaddr*)&addr, addrlen) < 0)
    return -1;

  return s;
}

int OpenWithPermissions(int sockfd, const char* path) {
  ssize_t rc = SendAll(sockfd, path, strlen(path) + 1, 0);
  if (rc < 0)
    return -1;

  struct iovec iov = {0};
  char msg_buf[PATH_MAX];
  iov.iov_base = msg_buf;
  iov.iov_len = sizeof(msg_buf);
  struct msghdr msg = {0};
  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  char control_buf[PATH_MAX];
  msg.msg_control = control_buf;
  msg.msg_controllen = sizeof(control_buf);

  rc = recvmsg(sockfd, &msg, 0);
  if (rc < 0)
    return -1;

  int fd;
  if (msg.msg_controllen == 0) {
    fd = -1;
  } else {
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    int* recv_cmsg = (int *) CMSG_DATA(cmsg);
    fd = recv_cmsg[0];
  }

  return fd;
}
