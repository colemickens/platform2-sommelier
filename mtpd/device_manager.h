// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_DEVICE_MANAGER_H_
#define MTPD_DEVICE_MANAGER_H_

#include <glib.h>
#include <libmtp.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/basictypes.h>

#include "mtpd/file_entry.h"
#include "mtpd/storage_info.h"

extern "C" {
struct udev;
struct udev_device;
struct udev_monitor;
}

namespace mtpd {

class DeviceEventDelegate;

class DeviceManager {
 public:
  explicit DeviceManager(DeviceEventDelegate* delegate);
  ~DeviceManager();

  // Returns a file descriptor for monitoring device events.
  int GetDeviceEventDescriptor() const;

  // Processes the available device events.
  void ProcessDeviceEvents();

  // Returns a vector of attached MTP storages.
  std::vector<std::string> EnumerateStorage();

  // Returns true if |storage_name| is attached.
  bool HasStorage(const std::string& storage_name);

  // Returns storage metadata for |storage_name|.
  const StorageInfo* GetStorageInfo(const std::string& storage_name);

  // Exposed for testing.
  // |storage_name| should be in the form of "usb:bus_location:storage_id".
  // Returns true and fills |usb_bus_str| and |storage_id| on success.
  static bool ParseStorageName(const std::string& storage_name,
                               std::string* usb_bus_str,
                               uint32_t* storage_id);

  // Reads entries from |file_path| on |storage_name|.
  // On success, returns true and writes the file entries of |file_path| into
  // |out|. Otherwise returns false.
  bool ReadDirectoryByPath(const std::string& storage_name,
                           const std::string& file_path,
                           DBusFileEntries* out);

  // Reads entries from |file_id| on |storage_name|.
  // |file_id| is the unique identifier for a directory on |storage_name|.
  // On success, returns true and writes the file entries of |file_id| into
  // |out|. Otherwise returns false.
  bool ReadDirectoryById(const std::string& storage_name,
                         uint32_t file_id,
                         DBusFileEntries* out);

  // Reads the contents of |file_path| on |storage_name|.
  // On success, returns true and writes the file contents of |file_path| into
  // |out|. Otherwise returns false.
  bool ReadFileByPath(const std::string& storage_name,
                         const std::string& file_path,
                         std::vector<uint8_t>* out);

  // Reads the contents of |file_id| on |storage_name|.
  // |file_id| is the unique identifier for a directory on |storage_name|.
  // On success, returns true and writes the file contents of |file_id| into
  // |out|. Otherwise returns false.
  bool ReadFileById(const std::string& storage_name,
                         uint32_t file_id,
                         std::vector<uint8_t>* out);

 private:
  // Key: MTP storage id, Value: metadata for the given storage.
  typedef std::map<uint32_t, StorageInfo> MtpStorageMap;
  // (device handle, map of storages on the device)
  typedef std::pair<LIBMTP_mtpdevice_t*, MtpStorageMap> MtpDevice;
  // Key: device bus location, Value: MtpDevice.
  typedef std::map<std::string, MtpDevice> MtpDeviceMap;
  // Function to process path components.
  typedef bool (*ProcessPathComponentFunc)(const LIBMTP_file_t*, size_t,
                                           size_t, uint32_t*);

  // On storage with |storage_id| on |device|, looks up the file id for
  // |file_path| using |process_func| to determine if the components in
  // |file_path| are valid.
  // On success, returns true, and write the file id to |file_id|.
  // Otherwise returns false.
  bool PathToFileId(LIBMTP_mtpdevice_t* device,
                    uint32_t storage_id,
                    const std::string& file_path,
                    ProcessPathComponentFunc process_func,
                    uint32_t* file_id);

  // Reads entries from |device|'s storage with |storage_id|.
  // |file_id| is the unique identifier for a directory on the given storage.
  // On success, returns true and writes the file entries of |file_id| into
  // |out|. Otherwise returns false.
  bool ReadDirectory(LIBMTP_mtpdevice_t* device,
                     uint32_t storage_id,
                     uint32_t file_id,
                     DBusFileEntries* out);

  // Reads the contents of |file_id| from |device|.
  // |file_id| is the unique identifier for a file on the given storage.
  // On success, returns true and writes the file contents of |file_id| into
  // |out|. Otherwise returns false.
  bool ReadFile(LIBMTP_mtpdevice_t* device,
                uint32_t file_id,
                std::vector<uint8_t>* out);

  // Helper function that returns the libmtp device handle and storage id for a
  // given |storage_name|.
  bool GetDeviceAndStorageId(const std::string& storage_name,
                             LIBMTP_mtpdevice_t** mtp_device,
                             uint32_t* storage_id);

  // Callback for udev when something changes for |device|.
  void HandleDeviceNotification(udev_device* device);

  // Iterates through attached devices and find ones that are newly attached.
  // Then populates |device_map_| for the newly attached devices.
  // If this is called as a result of a callback, it came from |source|,
  // which needs to be properly destructed.
  void AddDevices(GSource* source);

  // Iterates through attached devices and find ones that have been detached.
  // Then removes the detached devices from |device_map_|.
  // If |remove_all| is true, then assumes all devices have been detached.
  void RemoveDevices(bool remove_all);

  // libudev-related items: the main context, the monitoring context to be
  // notified about changes to device states, and the monitoring context's
  // file descriptor.
  udev* udev_;
  udev_monitor* udev_monitor_;
  int udev_monitor_fd_;

  DeviceEventDelegate* delegate_;

  // Map of devices and storages.
  MtpDeviceMap device_map_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

}  // namespace mtpd

#endif  // MTPD_DEVICE_MANAGER_H_
