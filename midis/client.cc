// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <midis/client.h>

#include <utility>

#include <base/bind.h>
#include <base/memory/ptr_util.h>

namespace midis {

Client::Client(base::ScopedFD fd)
    : client_fd_(std::move(fd)), weak_factory_(this) {}

Client::~Client() { StopMonitoring(); }

std::unique_ptr<Client> Client::Create(base::ScopedFD fd) {
  auto cli = std::unique_ptr<Client>(new Client(std::move(fd)));
  if (!cli->StartMonitoring()) {
    return nullptr;
  }
  LOG(INFO) << "New client created!";
  return cli;
}

void Client::HandleClientMessages() {
  // TODO(pmalani): Perform message handling for registering for a device.
  // Removing registration for a device, etc.
}

bool Client::StartMonitoring() {
  msg_taskid_ = brillo::MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE, client_fd_.get(), brillo::MessageLoop::kWatchRead, true,
      base::Bind(&Client::HandleClientMessages, weak_factory_.GetWeakPtr()));
  return msg_taskid_ != brillo::MessageLoop::kTaskIdNull;
}

void Client::StopMonitoring() {
  brillo::MessageLoop::current()->CancelTask(msg_taskid_);
  msg_taskid_ = brillo::MessageLoop::kTaskIdNull;
}

}  // namespace midis
