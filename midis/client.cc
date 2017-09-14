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

Client::Client(base::ScopedFD fd,
               DeviceTracker* device_tracker,
               uint32_t client_id,
               ClientDeletionCallback del_cb,
               arc::mojom::MidisServerRequest request,
               arc::mojom::MidisClientPtr client_ptr)
    : client_fd_(std::move(fd)),
      device_tracker_(device_tracker),
      client_id_(client_id),
      del_cb_(del_cb),
      client_ptr_(std::move(client_ptr)),
      binding_(this, std::move(request)),
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
  std::unique_ptr<Client> cli(new Client(
      std::move(fd), device_tracker, client_id, del_cb, nullptr, nullptr));
  if (!cli->StartMonitoring()) {
    return nullptr;
  }
  LOG(INFO) << "New client created: " << client_id;
  return cli;
}

std::unique_ptr<Client> Client::CreateMojo(
    DeviceTracker* device_tracker,
    uint32_t client_id,
    ClientDeletionCallback del_cb,
    arc::mojom::MidisServerRequest request,
    arc::mojom::MidisClientPtr client_ptr) {
  // Create the client object.
  std::unique_ptr<Client> cli(new Client(base::ScopedFD(),
                                         device_tracker,
                                         client_id,
                                         del_cb,
                                         std::move(request),
                                         std::move(client_ptr)));
  LOG(INFO) << "New Mojo client created: " << client_id;

  return cli;
}

void Client::TriggerClientDeletion() {
  brillo::MessageLoop::TaskId ret_id = brillo::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(del_cb_, client_id_));
  if (ret_id == brillo::MessageLoop::kTaskIdNull) {
    LOG(ERROR) << "Couldn't schedule the client deletion callback!";
  }
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
  struct MidisMessageHeader header;

  ssize_t bytes = HANDLE_EINTR(
      read(client_fd_.get(), &header, sizeof(struct MidisMessageHeader)));
  if (bytes > 0) {
    switch (header.type) {
      case REQUEST_PORT:
        AddClientToPort();
        break;
      case CLOSE_DEVICE:
        HandleCloseDeviceMessage();
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
      FROM_HERE,
      client_fd_.get(),
      brillo::MessageLoop::kWatchRead,
      true,
      base::Bind(&Client::HandleClientMessages, weak_factory_.GetWeakPtr()));
  return msg_taskid_ != brillo::MessageLoop::kTaskIdNull;
}

void Client::StopMonitoring() {
  brillo::MessageLoop::current()->CancelTask(msg_taskid_);
  msg_taskid_ = brillo::MessageLoop::kTaskIdNull;
}

void Client::OnDeviceAddedOrRemoved(const Device& dev, bool added) {
  arc::mojom::MidisDeviceInfoPtr dev_info = arc::mojom::MidisDeviceInfo::New();
  dev_info->card = dev.GetCard();
  dev_info->device_num = dev.GetDeviceNum();
  dev_info->num_subdevices = dev.GetNumSubdevices();
  dev_info->name = dev.GetName();
  dev_info->manufacturer = dev.GetManufacturer();
  if (added) {
    client_ptr_->DeviceAdded(std::move(dev_info));
  } else {
    client_ptr_->DeviceRemoved(std::move(dev_info));
  }
}

void Client::HandleCloseDeviceMessage() {
  struct MidisRequestPort port_msg;
  ssize_t ret =
      HANDLE_EINTR(read(client_fd_.get(), &port_msg, sizeof(port_msg)));
  if (ret != sizeof(port_msg)) {
    LOG(ERROR) << "Read of client fd for MidisRequestPort message failed.";
    TriggerClientDeletion();
    return;
  }

  device_tracker_->RemoveClientFromDevice(
      client_id_, port_msg.card, port_msg.device_num);
}

void Client::ListDevices(const ListDevicesCallback& callback) {
  // Get all the device information from device_tracker.
  mojo::Array<arc::mojom::MidisDeviceInfoPtr> device_list;
  device_tracker_->ListDevices(&device_list);
  callback.Run(std::move(device_list));
}

}  // namespace midis
