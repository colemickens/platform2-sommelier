// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIS_DEVICE_H_
#define MIDIS_DEVICE_H_

#include <map>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <base/memory/weak_ptr.h>
#include <brillo/message_loops/message_loop.h>
#include <gtest/gtest_prod.h>

namespace midis {

class FileHandler;

class SubDeviceClientFdHolder;

// Class which holds information related to a MIDI device.
// We use the name variable (derived from the ioctl) as a basis
// to arrive at an identifier.
class Device {
 public:
  Device(const std::string& name, const std::string& manufacturer,
         uint32_t card, uint32_t device, uint32_t num_subdevices,
         uint32_t flags);
  ~Device();

  static std::unique_ptr<Device> Create(const std::string& name,
                                        const std::string& manufacturer,
                                        uint32_t card, uint32_t device,
                                        uint32_t num_subdevices,
                                        uint32_t flags);
  const std::string& GetName() const { return name_; }
  const std::string& GetManufacturer() const { return manufacturer_; }
  uint32_t GetCard() const { return card_; }
  uint32_t GetDeviceNum() const { return device_; }
  uint32_t GetNumSubdevices() const { return num_subdevices_; }
  uint32_t GetFlags() const { return flags_; }

  // Adds a client which wishes to read data on a particular subdevice
  // This function should return one end of a pipe file descriptor
  // This will be sent back to the client, and it can listen on that for events.
  //
  // A device can be bidirectional, and so we should also have a watch on the
  // pipe FD so that we can read MIDI events and send them to the MIDI
  // H/W.
  //
  // TODO(pmalani): How do you arbitrate MIDI data from multiple clients. The
  // best approach would be to just allow access to one client for the output.
  // Returns:
  //   A valid base::ScopedFD on success.
  //   An empty base::ScopedFD otherwise.
  base::ScopedFD AddClientToReadSubdevice(uint32_t client_id,
                                          uint32_t subdevice_id);

  // This function is called when a Client is removed from the service for
  // orderly or unorderly reasons (like disconnection). The client is removed
  // from all subdevices.
  void RemoveClientFromDevice(uint32_t client_id);

 private:
  friend class ClientTest;
  friend class DeviceTest;
  friend class UdevHandlerTest;
  FRIEND_TEST(ClientTest, AddClientAndReceiveMessages);
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
  // Callback function which is invoked by the FileHandler object when data is
  // received for a particular subdevice.
  void HandleReceiveData(const char* buffer, uint32_t subdevice,
                         size_t buf_len) const;

  // Callback function which is invoked by SubDeviceClientFdHolder object when
  // data is received from client to be sent to a particular subdevice.
  void WriteClientDataToDevice(uint32_t subdevice_id, const uint8_t* buffer,
                               size_t buf_len);

  std::string name_;
  std::string manufacturer_;
  uint32_t card_;
  uint32_t device_;
  uint32_t num_subdevices_;
  uint32_t flags_;
  // This data structure maps subdevice id's to corresponding file handler
  // objects.
  std::map<uint32_t, std::unique_ptr<FileHandler>> handlers_;
  // This data-structure performs the following map:
  //
  // subdevice ---> (client_1, pipefd_1), (client_2, pipefd_2), ...., (client_n,
  // pipefd_n).
  std::map<uint32_t, std::vector<std::unique_ptr<SubDeviceClientFdHolder>>>
      client_fds_;
  static base::FilePath basedir_;
  base::WeakPtrFactory<Device> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace midis

#endif  // MIDIS_DEVICE_H_
