// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "helper_process.h"

#include <poll.h>
#include <signal.h>
#include <sys/socket.h>

#include <utility>
#include <vector>

#include <base/process/launch.h>

#include "ipc.pb.h"

namespace imageloader {

void HelperProcess::Start(int argc, char* argv[], const std::string& fd_arg) {
  int control[2];

  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, control) != 0)
    PLOG(FATAL) << "socketpair failed";

  control_fd_.reset(control[0]);
  const int subprocess_fd = control[1];

  CHECK_GE(argc, 1);
  std::vector<std::string> child_argv;
  for (int i = 0; i < argc; i++)
    child_argv.push_back(argv[i]);

  child_argv.push_back(fd_arg + "=" + std::to_string(subprocess_fd));

  base::FileHandleMappingVector fd_mapping;
  fd_mapping.push_back({subprocess_fd, subprocess_fd});

  base::LaunchOptions options;
  options.fds_to_remap = &fd_mapping;

  base::Process p = base::LaunchProcess(child_argv, options);
  CHECK(p.IsValid());
  pid_ = p.Pid();
}

bool HelperProcess::SendMountCommand(int fd, const std::string& path,
                                     const std::string& table) {
  struct msghdr msg = {0};
  char fds[CMSG_SPACE(sizeof(fd))];
  memset(fds, '\0', sizeof(fds));

  MountImage msg_proto;
  msg_proto.set_mount_path(path);
  msg_proto.set_table(table);

  std::string msg_str;
  if (!msg_proto.SerializeToString(&msg_str))
    LOG(FATAL) << "error serializing protobuf";

  // iov takes a non-const pointer.
  char buffer[msg_str.size() + 1];
  memcpy(buffer, msg_str.c_str(), sizeof(buffer));

  struct iovec iov[1];
  iov[0].iov_base = buffer;
  iov[0].iov_len = sizeof(buffer);

  msg.msg_iov = iov;
  msg.msg_iovlen = sizeof(iov) / sizeof(iov[0]);

  msg.msg_control = fds;
  msg.msg_controllen = sizeof(fds);

  struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

  // Move the file descriptor into the payload.
  memmove(CMSG_DATA(cmsg), &fd, sizeof(fd));
  msg.msg_controllen = cmsg->cmsg_len;

  if (sendmsg(control_fd_.get(), &msg, 0) < 0) PLOG(FATAL) << "sendmsg failed";

  return WaitForResponse();
}

bool HelperProcess::WaitForResponse() {
  struct pollfd pfd;
  pfd.fd = control_fd_.get();
  pfd.events = POLLIN;

  int rc = poll(&pfd, 1, /*timeout=*/ 2000);
  PCHECK(rc >= 0 || errno == EINTR);

  if (pfd.revents & POLLIN) {
    char buffer[4096];
    memset(buffer, '\0', sizeof(buffer));
    ssize_t bytes = read(control_fd_.get(), buffer, sizeof(buffer));
    PCHECK(bytes != -1);

    MountResponse response;
    if (!response.ParseFromArray(buffer, bytes)) {
      LOG(FATAL) << "could not deserialize protobuf: " << buffer;
    }
    return response.success();
  }

  return false;
}

}  // namespace imageloader
