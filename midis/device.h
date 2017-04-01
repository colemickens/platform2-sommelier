// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_DEVICE_H_
#define MIDIS_DEVICE_H_

#include <map>

#include <memory>
#include <string>
#include <utility>

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/memory/weak_ptr.h>
#include <brillo/message_loops/message_loop.h>
#include <gtest/gtest_prod.h>

namespace midis {

class FileHandler;

// Class which holds information related to a MIDI device.
// We use the name variable (derived from the ioctl) as a basis
// to arrive at an identifier.
class Device {
 public:
  Device(const std::string& name, uint32_t card, uint32_t device,
         uint32_t num_subdevices, uint32_t flags);
  ~Device();

  static std::unique_ptr<Device> Create(const std::string& name, uint32_t card,
                                        uint32_t device,
                                        uint32_t num_subdevices,
                                        uint32_t flags);
  std::string GetName() const { return name_; }
  uint32_t GetCard() const { return card_; }
  uint32_t GetDeviceNum() const { return device_; }
  // Callback function which is invoked by the FileHandler object when data is
  // received for a particular subdevice.

 private:
  friend class DeviceTest;
  friend class UdevHandlerTest;
  FRIEND_TEST(DeviceTest, TestHandleDeviceRead);
  FRIEND_TEST(UdevHandlerTest, CreateDevicePositive);
  // This function instantiates a FileHandler for each subdevice. If all the
  // FileHandlers can get initialized successfully, each of them is added to the
  // handlers_ map.
  bool StartMonitoring();
  // Deletes all the FileHandlers created during StartMontoring().
  void StopMonitoring();
  // Helper function to set the base directory to be used for creating and
  // looking for dev node paths. Helpful for testing (where we don't have real
  // h/w, and dev nodes have to be faked).
  static void SetBaseDirForTesting(const base::FilePath& dir) {
    Device::basedir_ = dir;
  }
  void HandleReceiveData(const char* buffer, uint32_t subdevice) const;

  std::string name_;
  uint32_t card_;
  uint32_t device_;
  uint32_t num_subdevices_;
  uint32_t flags_;
  std::map<uint32_t, std::unique_ptr<FileHandler>> handlers_;
  static base::FilePath basedir_;
  base::WeakPtrFactory<Device> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace midis

#endif  // MIDIS_DEVICE_H_
