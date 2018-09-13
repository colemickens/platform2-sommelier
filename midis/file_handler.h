// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_FILE_HANDLER_H_
#define MIDIS_FILE_HANDLER_H_

#include <map>

#include <memory>
#include <string>
#include <utility>

#include <base/callback.h>
#include <base/files/scoped_file.h>
#include <base/memory/weak_ptr.h>
#include <brillo/message_loops/message_loop.h>
#include <gtest/gtest_prod.h>

namespace midis {

// Class which handles file reading and input data handling functionality for a
// particular subdevice.
class FileHandler {
 public:
  using DeviceDataCallback = base::Callback<void(
      const char* buffer, uint32_t subdevice_id, size_t buf_len)>;
  FileHandler(const std::string& path,
              const DeviceDataCallback& device_data_cb);
  ~FileHandler();

  static std::unique_ptr<FileHandler> Create(
      const std::string& path, uint32_t subdevice_id,
      const DeviceDataCallback& device_data_cb);

  // Each FileHandler class has a fd associated with it. This function writes
  // data to that fd.
  void WriteData(const uint8_t* buffer, size_t buf_len);

 private:
  // Callback used to process incoming MIDI data from h/w.
  // This function in turn calls a user supplied callback which determines what
  // is to be done with the read data (send it to clients, print it out, etc.).
  void HandleDeviceRead(uint32_t subdevice_id);
  // Opens a ScopedFD and starts a watch on the resulting FD. This function is
  // also called as a part of the Create() factory method.
  bool StartMonitoring(uint32_t subdevice_id);
  // Cancels the watch on the ScopedFD. This function is also called as a part
  // of the class destructor.
  void StopMonitoring();
  void SetDeviceDataCbForTesting(const DeviceDataCallback& cb);

  base::ScopedFD fd_;
  brillo::MessageLoop::TaskId taskid_;
  std::string path_;
  DeviceDataCallback device_data_cb_;
  base::WeakPtrFactory<FileHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileHandler);
};

}  // namespace midis

#endif  // MIDIS_FILE_HANDLER_H_
