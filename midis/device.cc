// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "midis/device.h"

#include <fcntl.h>
#include <sys/socket.h>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>

#include "midis/constants.h"
#include "midis/file_handler.h"
#include "midis/subdevice_client_fd_holder.h"

namespace midis {

base::FilePath Device::basedir_ = base::FilePath();

Device::Device(const std::string& name, const std::string& manufacturer,
               uint32_t card, uint32_t device, uint32_t num_subdevices,
               uint32_t flags)
    : name_(name),
      manufacturer_(manufacturer),
      card_(card),
      device_(device),
      num_subdevices_(num_subdevices),
      flags_(flags),
      weak_factory_(this) {
  LOG(INFO) << "Device created: " << name_;
}

Device::~Device() { StopMonitoring(); }

// Cancel all the file watchers and remove the file handlers.
// This function is called if :
// a. Something has gone wrong with the Device monitor and we need to bail
// b. Something has gone wrong while adding the device.
// c. During a graceful shutdown.
void Device::StopMonitoring() {
  // Cancel all the clients FDs who were listening / writing to this device.
  client_fds_.clear();
  handlers_.clear();
}

bool Device::StartMonitoring() {
  // For each sub-device, we instantiate a fd, and an fd watcher
  // and handler messages from the device in a generic handler.
  for (uint32_t i = 0; i < num_subdevices_; i++) {
    std::string path = base::StringPrintf(
        "%s/dev/snd/midiC%uD%u", basedir_.value().c_str(), card_, device_);

    auto fh = FileHandler::Create(
        path, i,
        base::Bind(&Device::HandleReceiveData, weak_factory_.GetWeakPtr()));
    if (!fh) {
      return false;
    }
    // NOTE: If any initialization of any of the sub-device handlers fails,
    // we mark the StartMonitoring option as failed, and return false.
    // The caller should the invoke Device::StopMonitoring() to cleanup
    // the existing file handlers.
    handlers_.emplace(i, std::move(fh));
  }
  return true;
}

void Device::HandleReceiveData(const char* buffer, uint32_t subdevice,
                               size_t buf_len) const {
  LOG(INFO) << "Device: " << device_ << " Subdevice: " << subdevice
            << ", The read MIDI info is:" << buffer;
  auto list_it = client_fds_.find(subdevice);
  if (list_it != client_fds_.end()) {
    for (const auto& id_fd_entry : list_it->second) {
      id_fd_entry->WriteDeviceDataToClient(buffer, buf_len);
    }
  }
}

void Device::RemoveClientFromDevice(uint32_t client_id) {
  LOG(INFO) << "Removing the client: " << client_id
            << " from all device watchers.";
  for (auto list_it = client_fds_.begin(); list_it != client_fds_.end();) {
    // First remove all clients in a subdevice.
    for (auto it = list_it->second.begin(); it != list_it->second.end();) {
      if (it->get()->GetClientId() == client_id) {
        LOG(INFO) << "Found client: " << client_id << " in list. deleting";
        it = list_it->second.erase(it);
      } else {
        ++it;
      }
    }
    // If no clients remain, remove the subdevice entry from the map.
    if (list_it->second.empty()) {
      client_fds_.erase(list_it++);
    } else {
      ++list_it;
    }
  }

  if (client_fds_.empty()) {
    StopMonitoring();
  }
}

void Device::WriteClientDataToDevice(uint32_t subdevice_id,
                                     const uint8_t* buffer, size_t buf_len) {
  auto handler = handlers_.find(subdevice_id);
  if (handler != handlers_.end()) {
    handler->second->WriteData(buffer, buf_len);
  }
}

base::ScopedFD Device::AddClientToReadSubdevice(uint32_t client_id,
                                                uint32_t subdevice_id) {
  if (client_fds_.empty()) {
    if (!StartMonitoring()) {
      LOG(ERROR) << "Couldn't start monitoring device: " << name_;
      StopMonitoring();
      return base::ScopedFD();
    }
  }

  int sock_fd[2];
  int ret = socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sock_fd);
  if (ret < 0) {
    PLOG(ERROR) << "socketpair for client_id: " << client_id
                << " device_id: " << device_ << " subdevice: " << subdevice_id
                << "failed.";
    return base::ScopedFD();
  }

  base::ScopedFD server_fd(sock_fd[0]);
  base::ScopedFD client_fd(sock_fd[1]);

  auto id_fd_list = client_fds_.find(subdevice_id);
  if (id_fd_list == client_fds_.end()) {
    std::vector<std::unique_ptr<SubDeviceClientFdHolder>> list_entries;

    list_entries.emplace_back(SubDeviceClientFdHolder::Create(
        client_id, subdevice_id, std::move(server_fd),
        base::Bind(&Device::WriteClientDataToDevice,
                   weak_factory_.GetWeakPtr())));

    client_fds_.emplace(subdevice_id, std::move(list_entries));
  } else {
    for (auto const& pair : id_fd_list->second) {
      if (pair->GetClientId() == client_id) {
        LOG(INFO) << "Client id: " << client_id
                  << " already registered to"
                     " subdevice: "
                  << subdevice_id << ".";
        return base::ScopedFD();
      }
    }
    id_fd_list->second.emplace_back(SubDeviceClientFdHolder::Create(
        client_id, subdevice_id, std::move(server_fd),
        base::Bind(&Device::WriteClientDataToDevice,
                   weak_factory_.GetWeakPtr())));
  }

  return client_fd;
}

}  // namespace midis
