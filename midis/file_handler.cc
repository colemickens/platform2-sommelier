// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "midis/file_handler.h"

#include <fcntl.h>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_number_conversions.h>

#include "midis/device.h"

namespace midis {

namespace {

const size_t kMaxReadBuffer = 4096;

}  // namespace

FileHandler::FileHandler(const std::string& path,
                         const DeviceDataCallback& device_data_cb)
    : taskid_(brillo::MessageLoop::kTaskIdNull),
      path_(path),
      device_data_cb_(device_data_cb),
      weak_factory_(this) {}

FileHandler::~FileHandler() { StopMonitoring(); }

std::unique_ptr<FileHandler> FileHandler::Create(
    const std::string& path, uint32_t subdevice_id,
    const DeviceDataCallback& device_data_cb) {
  auto fhandler = base::MakeUnique<FileHandler>(path, device_data_cb);
  if (!fhandler->StartMonitoring(subdevice_id)) {
    return nullptr;
  }
  return fhandler;
}

void FileHandler::HandleDeviceRead(uint32_t subdevice_id) {
  char buffer[kMaxReadBuffer];
  int ret = read(fd_.get(), buffer, kMaxReadBuffer);
  if (ret < 0) {
    PLOG(ERROR) << "Couldn't read device fd: " << fd_.get();
    // TODO(pmalani): Perform some handling to remove the device.
    StopMonitoring();
    return;
  }
  device_data_cb_.Run(buffer, subdevice_id);
}

void FileHandler::StopMonitoring() {
  brillo::MessageLoop::current()->CancelTask(taskid_);
  taskid_ = brillo::MessageLoop::kTaskIdNull;
}

bool FileHandler::StartMonitoring(uint32_t subdevice_id) {
  fd_ = base::ScopedFD(open(path_.c_str(), O_RDWR | O_CLOEXEC));
  taskid_ = brillo::MessageLoop::current()->WatchFileDescriptor(
      FROM_HERE, fd_.get(), brillo::MessageLoop::kWatchRead, true,
      base::Bind(&FileHandler::HandleDeviceRead, weak_factory_.GetWeakPtr(),
                 subdevice_id));
  return true;
}

void FileHandler::SetDeviceDataCbForTesting(const DeviceDataCallback& cb) {
  device_data_cb_ = cb;
}

}  // namespace midis
