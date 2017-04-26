// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "midis/client_tracker.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <utility>

#include <base/bind.h>
#include <base/location.h>
#include <base/memory/ptr_util.h>
#include <brillo/message_loops/message_loop.h>

namespace {

const char kSocketPath[] = "/run/midis/midis_socket";
const int kUnixNamedSocketBacklog = 5;

}  // namespace

namespace midis {

ClientTracker::ClientTracker() : client_id_counter_(0), weak_factory_(this) {}

bool ClientTracker::InitClientTracker() {
  server_fd_ =
      base::ScopedFD(socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0));
  if (!server_fd_.is_valid()) {
    LOG(ERROR) << "Error creating midis server socket.";
    return false;
  }
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, kSocketPath, sizeof(addr.sun_path));

  // Calling fchmod before bind prevents the file from being modified.
  // Once bind() completes, we can change the permissions to allow
  // other users in the group to access the socket.
  int rc = fchmod(server_fd_.get(), 0700);
  if (rc < 0) {
    PLOG(ERROR) << "fchmod() on socket failed.";
    return false;
  }

  if (bind(server_fd_.get(), reinterpret_cast<sockaddr*>(&addr),
           sizeof(addr)) == -1) {
    PLOG(ERROR) << "bind() failed.";
    return false;
  }

  // ALlow other group members to access MIDI devices.
  rc = chmod(addr.sun_path, 0770);
  if (rc) {
    PLOG(ERROR) << "chmod on socket failed";
    return false;
  }

  if (listen(server_fd_.get(), kUnixNamedSocketBacklog) == -1) {
    PLOG(ERROR) << "listen() failed.";
    return false;
  }

  LOG(INFO) << "Start client server.";

  auto taskid = brillo::MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE, server_fd_.get(), brillo::MessageLoop::kWatchRead, true,
      base::Bind(&ClientTracker::ProcessClient, weak_factory_.GetWeakPtr(),
                 server_fd_.get()));

  return taskid != brillo::MessageLoop::kTaskIdNull;
}

void ClientTracker::ProcessClient(int fd) {
  auto new_fd = base::ScopedFD(accept(fd, NULL, NULL));
  if (!new_fd.is_valid()) {
    PLOG(ERROR) << "Error in accept()";
    return;
  }

  std::unique_ptr<Client> new_cli =
      Client::Create(std::move(new_fd), device_tracker_);
  if (new_cli) {
    clients_.emplace(client_id_counter_++, std::move(new_cli));
  }
}

}  // namespace midis
