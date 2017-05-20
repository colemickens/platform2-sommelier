// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <midis/client.h>

#include <sys/socket.h>

#include <utility>

#include <base/bind.h>
#include <base/memory/ptr_util.h>
#include <base/posix/eintr_wrapper.h>

#include "midis/constants.h"

namespace midis {

Client::Client(base::ScopedFD fd, DeviceTracker* device_tracker,
               uint32_t client_id, ClientDeletionCallback del_cb)
    : client_fd_(std::move(fd)),
      device_tracker_(device_tracker),
      client_id_(client_id),
      del_cb_(del_cb),
      weak_factory_(this) {
  device_tracker_->AddDeviceObserver(this);
}

Client::~Client() {
  LOG(INFO) << "Deleting client: " << client_id_;
  device_tracker_->RemoveDeviceObserver(this);
  StopMonitoring();
}

std::unique_ptr<Client> Client::Create(base::ScopedFD fd,
                                       DeviceTracker* device_tracker,
                                       uint32_t client_id,
                                       ClientDeletionCallback del_cb) {
  std::unique_ptr<Client> cli(
      new Client(std::move(fd), device_tracker, client_id, del_cb));
  if (!cli->StartMonitoring()) {
    return nullptr;
  }
  LOG(INFO) << "New client created: " << client_id;
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
    TriggerClientDeletion();
    return;
  }

  ret = HANDLE_EINTR(write(client_fd_.get(), payload_buf, payload_size));
  if (ret < 0) {
    PLOG(ERROR) << "SendDevicesList() payload: write to client_fd_failed.";
    TriggerClientDeletion();
  }
}

void Client::TriggerClientDeletion() {
  brillo::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(del_cb_, client_id_));
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

void Client::AddClientToPort() {
  // First read the next message to understand what is payload to retrieve
  struct MidisRequestPort port_msg;
  int ret = HANDLE_EINTR(read(client_fd_.get(), &port_msg, sizeof(port_msg)));
  if (ret != sizeof(port_msg)) {
    LOG(ERROR) << "Read of client fd for MidisRequestPort message failed.";
    TriggerClientDeletion();
    return;
  }

  base::ScopedFD clientfd = device_tracker_->AddClientToReadSubdevice(
      port_msg.card, port_msg.device_num, port_msg.subdevice_num, client_id_);
  if (!clientfd.is_valid()) {
    LOG(ERROR) << "AddClientToReadSubdevice failed.";
    // We don't delete the client here, because this could mean an issue with
    // the device h/w.
    return;
  }

  struct MidisMessageHeader header;
  header.type = REQUEST_PORT_RESPONSE;
  // FD's can only be sent via the cmsg interface.
  header.payload_size = 0;

  ret = HANDLE_EINTR(write(client_fd_.get(), &header, sizeof(header)));
  if (ret != sizeof(header)) {
    PLOG(ERROR) << "SendDevicesList() header: write to client_fd_failed.";
    // TODO(pmalani): remove the client from the device subdevice client list?
    // Or let the device handle it, when it sees the pipe doesn't lead anywhere.
    TriggerClientDeletion();
    return;
  }

  struct msghdr msg = {0};
  struct iovec iov;
  struct cmsghdr* cmsg;

  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  iov.iov_base = &port_msg;
  iov.iov_len = sizeof(struct MidisRequestPort);

  const size_t control_size = CMSG_SPACE(sizeof(clientfd.get()));
  std::vector<char> control(control_size);
  msg.msg_control = control.data();
  msg.msg_controllen = control.size();

  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(clientfd.get()));
  memcpy(CMSG_DATA(cmsg), &clientfd.get(), sizeof(clientfd.get()));

  ssize_t rc = sendmsg(client_fd_.get(), &msg, 0);
  if (rc < 0) {
    PLOG(ERROR) << "sendmsg for client failed.";
    TriggerClientDeletion();
  }
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
      case REQUEST_PORT:
        AddClientToPort();
        break;
      default:
        LOG(ERROR) << "Unknown message: " << header.type;
        break;
    }
  } else {
    PLOG(ERROR) << "read() for client fd failed.";
    TriggerClientDeletion();
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
    TriggerClientDeletion();
    return;
  }

  ret = HANDLE_EINTR(
      write(client_fd_.get(), dev_info, sizeof(struct MidisDeviceInfo)));
  if (ret < 0) {
    PLOG(ERROR) << "NotifyDeviceAdded() payload: write to client_fd_failed.";
    TriggerClientDeletion();
  }
}

}  // namespace midis
