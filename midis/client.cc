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

Client::Client(DeviceTracker* device_tracker,
               uint32_t client_id,
               ClientDeletionCallback del_cb,
               arc::mojom::MidisServerRequest request,
               arc::mojom::MidisClientPtr client_ptr)
    : device_tracker_(device_tracker),
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
}

void Client::TriggerClientDeletion() {
  brillo::MessageLoop::TaskId ret_id = brillo::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(del_cb_, client_id_));
  if (ret_id == brillo::MessageLoop::kTaskIdNull) {
    LOG(ERROR) << "Couldn't schedule the client deletion callback!";
  }
}

void Client::AddClientToPort() {
  // TODO(pmalani): This will be directly implemented as an function inside the
  // interface MidisServer; The generated FD will be returned directly as a
  // response to this callback.
  NOTIMPLEMENTED();
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
  // TODO(pmalani): Implement!
  NOTIMPLEMENTED();
}

void Client::ListDevices(const ListDevicesCallback& callback) {
  // Get all the device information from device_tracker.
  mojo::Array<arc::mojom::MidisDeviceInfoPtr> device_list;
  device_tracker_->ListDevices(&device_list);
  callback.Run(std::move(device_list));
}

}  // namespace midis
