// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <midis/client.h>

#include <utility>

#include <base/bind.h>
#include <base/memory/ptr_util.h>
#include <base/posix/eintr_wrapper.h>

namespace midis {

namespace {

const int kMaxBufSize = 1024;

}  // namespace

Client::Client(base::ScopedFD fd, DeviceTracker* device_tracker)
    : client_fd_(std::move(fd)),
      device_tracker_(device_tracker),
      weak_factory_(this) {
  device_tracker_->AddDeviceObserver(this);
}

Client::~Client() {
  StopMonitoring();
  device_tracker_->RemoveDeviceObserver(this);
}

std::unique_ptr<Client> Client::Create(base::ScopedFD fd,
                                       DeviceTracker* device_tracker) {
  auto cli = std::unique_ptr<Client>(new Client(std::move(fd), device_tracker));
  if (!cli->StartMonitoring()) {
    return nullptr;
  }
  LOG(INFO) << "New client created!";
  return cli;
}

void Client::SendDevicesList() {
  uint8_t payload_buf[kMaxBufSize];
  struct MidisMessageHeader header;

  uint32_t payload_size = PrepareDeviceListPayload(payload_buf, kMaxBufSize);

  header.type = LIST_DEVICES_RESPONSE;
  header.payload_size = payload_size;

  int ret = HANDLE_EINTR(
      write(client_fd_.get(), &header, sizeof(struct MidisMessageHeader)));
  if (ret < 0) {
    PLOG(ERROR) << "SendDevicesList() header: write to client_fd_failed.";
    return;
  }

  ret = HANDLE_EINTR(write(client_fd_.get(), payload_buf, payload_size));
  if (ret < 0) {
    PLOG(ERROR) << "SendDevicesList() payload: write to client_fd_failed.";
  }
}

uint32_t Client::PrepareDeviceListPayload(uint8_t* payload_buf,
                                          size_t buf_len) {
  std::vector<MidisDeviceInfo> list;
  if (!device_tracker_) {
    LOG(FATAL) << "Device tracker ptr is nullptr; something bad happened.";
    return 0;
  }
  device_tracker_->ListDevices(&list);
  memset(payload_buf, 0, buf_len);
  // We'll fill num of devices once we are done.
  uint8_t* payload_buf_ptr = &payload_buf[1];

  uint8_t count = 0;
  size_t bytes_written = 1;
  // Prepare the payload to send back.
  for (const auto& device : list) {
    if (count == kMidisMaxDevices) {
      LOG(WARNING) << "Number of devices exceeds max limit!.";
      break;
    }

    // Make sure we avoid buffer overflows.
    if (bytes_written + sizeof(struct MidisDeviceInfo) > buf_len) {
      LOG(WARNING) << "Payload buffer can't accomodate more device entries.";
      break;
    }
    count++;
    bytes_written += sizeof(struct MidisDeviceInfo);

    memcpy(payload_buf_ptr, &device, sizeof(struct MidisDeviceInfo));
    payload_buf_ptr += sizeof(struct MidisDeviceInfo);
  }

  payload_buf[0] = count;

  return bytes_written;
}

void Client::HandleClientMessages() {
  int bytes;
  struct MidisMessageHeader header;

  if ((bytes = HANDLE_EINTR(read(client_fd_.get(), &header,
                                 sizeof(struct MidisMessageHeader)))) > 0) {
    switch (header.type) {
      case REQUEST_LIST_DEVICES:
        SendDevicesList();
        break;
      default:
        LOG(ERROR) << "Unknown message: " << header.type;
        break;
    }
  } else {
    PLOG(ERROR) << "read() for client fd failed.";
  }
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

void Client::OnDeviceAddedOrRemoved(const struct MidisDeviceInfo* dev_info,
                                    bool added) {
  // Prepare the payload.
  struct MidisMessageHeader header;
  header.type = added ? DEVICE_ADDED : DEVICE_REMOVED;
  header.payload_size = sizeof(MidisDeviceInfo);

  int ret = HANDLE_EINTR(
      write(client_fd_.get(), &header, sizeof(struct MidisMessageHeader)));
  if (ret < 0) {
    PLOG(ERROR)
        << "NotifyDeviceAddedOrRemoved() header: write to client_fd failed.";
    return;
  }

  ret = HANDLE_EINTR(
      write(client_fd_.get(), dev_info, sizeof(struct MidisDeviceInfo)));
  if (ret < 0) {
    PLOG(ERROR) << "NotifyDeviceAdded() payload: write to client_fd_failed.";
  }
}

}  // namespace midis
