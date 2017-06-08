// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "midis/libmidis/clientlib.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

int MidisConnectToServer(const char* socket_path) {
  if (!socket_path) {
    return -EINVAL;
  }

  struct sockaddr_un addr;
  if (strlen(socket_path) > sizeof(addr.sun_path)) {
    return -EINVAL;
  }

  int fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  if (fd == -1) {
    return -errno;
  }

  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

  int ret = TEMP_FAILURE_RETRY(
      connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));
  if (ret == -1) {
    int saved_errno = errno;
    close(fd);
    return -saved_errno;
  }

  return fd;
}

int32_t MidisProcessMsgHeader(int fd, uint32_t* payload_size, uint32_t* type) {
  if (!payload_size || !type) {
    return -EINVAL;
  }

  struct MidisMessageHeader header;
  int bytes = TEMP_FAILURE_RETRY(read(fd, &header, sizeof(header)));
  if (bytes == -1) {
    return -errno;
  }

  if (bytes != sizeof(header)) {
    return -EPROTO;
  }

  *payload_size = header.payload_size;
  *type = header.type;

  return 0;
}

int MidisListDevices(int fd) {
  struct MidisMessageHeader header;
  header.type = REQUEST_LIST_DEVICES;
  header.payload_size = 0;

  int bytes = TEMP_FAILURE_RETRY(write(fd, &header, sizeof(header)));
  if (bytes == -1) {
    return -errno;
  }

  if (bytes != sizeof(header)) {
    return -EPROTO;
  }
  return 0;
}

int MidisProcessListDevices(int fd, uint8_t* buf, size_t buf_len,
                            uint32_t payload_size) {
  if (!buf) {
    return -EINVAL;
  }

  if (buf_len < payload_size) {
    return -EINVAL;
  }

  // Short cut if there is nothing to read.
  if (payload_size == 0) {
    return 0;
  }

  memset(buf, 0, buf_len);

  int bytes = TEMP_FAILURE_RETRY(read(fd, buf, payload_size));
  if (bytes == -1) {
    return -errno;
  }

  if (bytes != payload_size) {
    return -EPROTO;
  }

  uint8_t num_devices = buf[0];
  if ((num_devices * sizeof(struct MidisDeviceInfo)) != (payload_size - 1)) {
    return -EPROTO;
  }

  return bytes;
}

int MidisRequestPort(int fd, const struct MidisRequestPort* port_msg) {
  if (!port_msg) {
    return -EINVAL;
  }

  struct MidisMessageHeader header;
  header.type = REQUEST_PORT;
  header.payload_size = 0;
  int bytes = TEMP_FAILURE_RETRY(write(fd, &header, sizeof(header)));
  if (bytes == -1) {
    return -errno;
  }

  if (bytes != sizeof(header)) {
    return -EPROTO;
  }

  bytes = TEMP_FAILURE_RETRY(write(fd, port_msg, sizeof(*port_msg)));
  if (bytes == -1) {
    return -errno;
  }

  if (bytes != sizeof(*port_msg)) {
    return -EPROTO;
  }
  return 0;
}

int MidisProcessRequestPortResponse(int fd, struct MidisRequestPort* port_msg) {
  if (!port_msg) {
    return -EINVAL;
  }

  memset(port_msg, 0, sizeof(*port_msg));

  struct msghdr msg;
  struct iovec iov;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  iov.iov_base = port_msg;
  iov.iov_len = sizeof(*port_msg);

  int portfd = -1;
  const unsigned int control_size = CMSG_SPACE(sizeof(portfd));
  std::vector<char> control(control_size);
  msg.msg_control = control.data();
  msg.msg_controllen = control.size();

  int rc = TEMP_FAILURE_RETRY(recvmsg(fd, &msg, 0));
  if (rc == -1) {
    return -errno;
  }

  if (rc != sizeof(*port_msg)) {
    return -EPROTO;
  }

  size_t num_fds = 1;
  struct cmsghdr* cmsg;
  for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr;
       cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      memcpy(&portfd, CMSG_DATA(cmsg), num_fds * sizeof(portfd));
      break;
    }
  }

  return portfd;
}

int MidisProcessDeviceAddedRemoved(int fd, struct MidisDeviceInfo* dev_info) {
  if (!dev_info) {
    return -EINVAL;
  }

  int bytes = TEMP_FAILURE_RETRY(read(fd, dev_info, sizeof(*dev_info)));
  if (bytes == -1) {
    return -errno;
  }

  if (bytes != sizeof(*dev_info)) {
    return -EPROTO;
  }
  return 0;
}

#ifdef __cplusplus
}
#endif
