// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "midis/device.h"

#include <fcntl.h>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>

#include "midis/file_handler.h"

namespace midis {

base::FilePath Device::basedir_ = base::FilePath();

Device::Device(const std::string& name, uint32_t card, uint32_t device,
               uint32_t num_subdevices, uint32_t flags)
    : name_(name),
      card_(card),
      device_(device),
      num_subdevices_(num_subdevices),
      flags_(flags),
      weak_factory_(this) {
  LOG(INFO) << "Device created: " << name_;
}

Device::~Device() { StopMonitoring(); }

std::unique_ptr<Device> Device::Create(const std::string& name, uint32_t card,
                                       uint32_t device, uint32_t num_subdevices,
                                       uint32_t flags) {
  auto dev =
      base::MakeUnique<Device>(name, card, device, num_subdevices, flags);
  if (!dev->StartMonitoring()) {
    dev->StopMonitoring();
    return nullptr;
  }
  return dev;
}

// Cancel all the file watchers and remove the file handlers.
// This function is called if :
// a. Something has gone wrong with the Device monitor and we need to bail
// b. Something has gone wrong while adding the device.
// c. During a graceful shutdown.
void Device::StopMonitoring() { handlers_.clear(); }

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

void Device::HandleReceiveData(const char* buffer, uint32_t subdevice) const {
  LOG(INFO) << "Subdevice: " << subdevice
            << ", The read MIDI info is:" << buffer;
  // TODO(pmalani): We have the buffer, we have the subdevice_id. So,
  // once the client code is in place, we should sent this buffer to all
  // the clients that are subscribed to this subdevice.
}

}  // namespace midis
