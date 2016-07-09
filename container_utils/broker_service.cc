// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "broker_service.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>  // PATH_MAX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <memory>
#include <string>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <pthread.h>

namespace broker_service {

BrokerService::BrokerService(DBus::Connection& conn, const char* path,
                             const char* name)
    : DBus::ObjectProxy(conn, path, name) {}

void BrokerService::RemoveConnection(int fd) {
  active_requests.erase(fd);
  close(fd);
}

void BrokerService::HandleRequest(const std::unique_ptr<Connection>& conn,
                                  int sockfd) {
  cmsghdr* cmsg;
  msghdr msg = {0};
  iovec iov = {0};
  char dummy = '!';

  LOG(INFO) << __func__ << ": Requesting file descriptor to '" << conn->path
            << "' from permission broker ...";
  // Request permission from permission broker.
  DBus::FileDescriptor fd;
  try {
    fd = this->OpenPath(std::string(conn->path));
  } catch (...) {
    // DBus throws an exception when it the path is denied.
    // Don't break off here, we have to return -1 to the asking process.
    fd = -1;
  }
  LOG(INFO) << __func__ << ": Received file descriptor '" << fd.get()
            << "' from permission broker.";

  iov.iov_base = &dummy;
  iov.iov_len = 1;
  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;

  msg.msg_iovlen = 1;
  char msg_buf[CMSG_SPACE(sizeof(int))] = {0};
  msg.msg_control = msg_buf;
  msg.msg_controllen = sizeof(msg_buf);

  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_len = msg.msg_controllen;
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  int* fdptr = reinterpret_cast<int*>(CMSG_DATA(cmsg));
  if (fd.get() < 0) {
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
  } else {
    fdptr[0] = fd.get();
  }

  int rc = sendmsg(sockfd, &msg, 0);
  if (rc < 0) {
    PLOG(INFO) << __func__ << ": sendmsg";
    return;
  }
  LOG(INFO) << __func__ << ": Sent file descriptor to client.";
  if (fd.get() != -1) {
    close(fd.get());
  }
}

void BrokerService::RunService(const char* sockname, int socknamelen) {
  sockaddr_un addr = {0};
  int maxpath = sizeof(addr.sun_path);
  int pathlen = socknamelen;
  if (pathlen >= maxpath) {
    LOG(FATAL) << __func__ << ": pathlen(" << pathlen << ") >= maxpath("
               << maxpath << ")";
  }

  // Upstart event should be "socket".
  const char* env_var = getenv("UPSTART_EVENTS");
  if (!env_var) {
    LOG(FATAL) << "UPSTART_EVENTS : not called by upstart.";
  }
  if (strcasecmp(env_var, "socket")) {
    // Not a socket event.
    LOG(FATAL) << "UPSTART_EVENTS(" << env_var
               << "): called by upstart, but not a socket event. ";
  }

  // Upstart listens to the socket and gives a good to go socket fd on
  // which we can accept(...).
  env_var = getenv("UPSTART_FDS");
  if (!env_var) {
    LOG(FATAL) << "UPSTART_FDS: cannot get legit socket";
  }
  int upstart_sockfd = atoi(env_var);
  if (upstart_sockfd < 0) {
    LOG(FATAL) << "UPSTART_FDS(" << upstart_sockfd << "): bad socket";
  }

  char recv_buf[256];
  int rc;
  while (true) {
    LOG(INFO) << "Setting up fds for poll(...) ...";

    int num_clients = active_requests.size();
    pollfd fds[num_clients + 1];
    fds[0].fd = upstart_sockfd;
    fds[0].events = POLLIN;
    int i = 1;
    for (auto it = active_requests.begin(); it != active_requests.end();
         ++it, ++i) {
      fds[i].fd = it->first;
      fds[i].events = POLLIN;
    }

    int active_sockets =
        poll(fds, num_clients + 1, active_requests.empty() ? 15000 : -1);
    if (active_sockets <= 0) {
      PLOG(ERROR) << "select failed, exiting cleanly";
      break;
    }

    if (fds[0].revents & POLLIN) {
      int sockfd = accept4(upstart_sockfd, NULL, NULL, SOCK_NONBLOCK);
      if (sockfd < 0) {
        PLOG(ERROR) << __func__ << ": accept";
        continue;
      }
      LOG(INFO) << "Got a new client (sockfd = " << sockfd << ")";
      active_requests[sockfd] = std::unique_ptr<Connection>(new Connection());
    }

    for (int i = 1; i <= num_clients; ++i) {
      int fd = fds[i].fd;
      if ((fds[i].revents & POLLIN) && !(active_requests[fd]->PathOk())) {
        rc = recv(fd, recv_buf, sizeof(recv_buf), 0);
        if (rc < 0) {
          // Some other error occurred.
          PLOG(ERROR) << __func__ << ": recv";
          RemoveConnection(fd);
          continue;
        }
        if (rc + active_requests[fd]->path_len <= PATH_MAX) {
          memcpy(&active_requests[fd]->path[active_requests[fd]->path_len],
                 recv_buf, rc);
          active_requests[fd]->path_len += rc;
          if (active_requests[fd]->PathOk()) {
            HandleRequest(active_requests[fd], fd);
            RemoveConnection(fd);
          }
          continue;
        }
        // Data is too large to fit in path.
        RemoveConnection(fd);
      }
    }
  }

  close(upstart_sockfd);
}

}  // namespace broker_service

using base::FilePath;
using broker_service::BrokerService;
using broker_service::kPermissionBrokerPath;
using broker_service::kPermissionBrokerName;

int main(int argc, char** argv) {
  brillo::FlagHelper::Init(argc, argv, "broker_service");
  brillo::OpenLog("broker_service", true);
  brillo::InitLog(brillo::kLogToSyslog);
  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection conn = DBus::Connection::SystemBus();
  const char kBrokerSocketPath[] = "/run/broker_service/adb";
  constexpr size_t kBrokerSocketPathLen = sizeof(kBrokerSocketPath) - 1;
  BrokerService service(conn, kPermissionBrokerPath, kPermissionBrokerName);
  service.RunService(kBrokerSocketPath, kBrokerSocketPathLen);
  return 0;
}
