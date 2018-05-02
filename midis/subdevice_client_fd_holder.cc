// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "midis/subdevice_client_fd_holder.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/posix/eintr_wrapper.h>

#include "midis/constants.h"

namespace midis {

SubDeviceClientFdHolder::~SubDeviceClientFdHolder() { StopClientMonitoring(); }

SubDeviceClientFdHolder::SubDeviceClientFdHolder(
    uint32_t client_id, uint32_t subdevice_id, base::ScopedFD fd,
    ClientDataCallback client_data_cb)
    : client_id_(client_id),
      subdevice_id_(subdevice_id),
      fd_(std::move(fd)),
      client_data_cb_(client_data_cb),
      queue_(std::make_unique<midi::MidiMessageQueue>(true)),
      weak_factory_(this) {}

std::unique_ptr<SubDeviceClientFdHolder> SubDeviceClientFdHolder::Create(
    uint32_t client_id, uint32_t subdevice_id, base::ScopedFD fd,
    ClientDataCallback client_data_cb) {
  auto holder = std::make_unique<SubDeviceClientFdHolder>(
      client_id, subdevice_id, std::move(fd), client_data_cb);
  if (!holder->StartClientMonitoring()) {
    return nullptr;
  }
  return holder;
}

void SubDeviceClientFdHolder::WriteDeviceDataToClient(const void* buffer,
                                                      size_t buf_len) {
  queue_->Add(reinterpret_cast<const uint8_t*>(buffer), buf_len);
  std::vector<uint8_t> message;
  queue_->Get(&message);
  while (!message.empty()) {
    ssize_t ret =
        HANDLE_EINTR(write(GetRawFd(), message.data(), message.size()));
    if (ret != static_cast<ssize_t>(message.size())) {
      PLOG(ERROR) << "Error writing to client fd.";
    }
    queue_->Get(&message);
  }
}

bool SubDeviceClientFdHolder::StartClientMonitoring() {
  // TODO(pmalani): Should make this conditional on whether the device
  // can accept input.
  pipe_taskid_ = brillo::MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE, fd_.get(), brillo::MessageLoop::kWatchRead, true,
      base::Bind(&SubDeviceClientFdHolder::HandleClientMidiData,
                 weak_factory_.GetWeakPtr()));
  if (pipe_taskid_ == brillo::MessageLoop::kTaskIdNull) {
    LOG(ERROR) << "Client id: " << client_id_
               << " watcher for pipeFD, for output to"
                  " subdevice: "
               << subdevice_id_ << " failed.";
    return false;
  }
  return true;
}

void SubDeviceClientFdHolder::StopClientMonitoring() {
  brillo::MessageLoop::current()->CancelTask(pipe_taskid_);
  pipe_taskid_ = brillo::MessageLoop::kTaskIdNull;
}

void SubDeviceClientFdHolder::HandleClientMidiData() {
  uint8_t buf[kMaxBufSize];
  ssize_t ret = HANDLE_EINTR(read(fd_.get(), buf, sizeof(buf)));
  if (ret < 0) {
    PLOG(ERROR) << "Error reading from pipe fd.";
    return;
  }

  client_data_cb_.Run(subdevice_id_, buf, ret);
}

}  // namespace midis
