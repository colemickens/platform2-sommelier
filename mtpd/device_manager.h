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

#include <base/macros.h>
#include <base/memory/linked_ptr.h>
#include <base/memory/weak_ptr.h>
#include <base/synchronization/lock.h>
#include <base/threading/simple_thread.h>

#include "file_entry.h"
#include "storage_info.h"

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
  std::vector<std::string> EnumerateStorages();

  // Returns true if |storage_name| is attached.
  bool HasStorage(const std::string& storage_name);

  // Returns storage metadata for |storage_name|.
  const StorageInfo* GetStorageInfo(const std::string& storage_name);

  // Returns storage metadata for |storage_name| by reading device. This method
  // updates all storage info of the device.
  const StorageInfo* GetStorageInfoFromDevice(const std::string& storage_name);

  // Exposed for testing.
  // |storage_name| should be in the form of "usb:bus_location:storage_id".
  // Returns true and fills |usb_bus_str| and |storage_id| on success.
  static bool ParseStorageName(const std::string& storage_name,
                               std::string* usb_bus_str,
                               uint32_t* storage_id);

  // Reads file ids for |file_id| on |storage_name|.
  // |file_id| is the unique identifier for a directory on the given storage.
  // For the root node, pass in |kRootFileId|.
  // On success, returns true and writes the file entry ids of |file_id| into
  // |out|. Otherwise returns false.
  bool ReadDirectoryEntryIds(const std::string& storage_name,
                             uint32_t file_id,
                             std::vector<uint32_t>* out);

  // Reads the metadata for files with |file_ids| on |storage_name|.
  // |file_ids| is a list of identifier for files on |storage_name|.
  // For the root node, pass in |kRootFileId|.
  // On success, returns true and writes the metadata of |file_ids| into
  // |out|. Otherwise returns false.
  // Note: If the metadata for any given id in |file_ids| cannot be retrieved,
  // then the size of |out| may be less than the size of |file_ids|. This is
  // not an error and it is up to the caller to handle this.
  bool GetFileInfo(const std::string& storage_name,
                   const std::vector<uint32_t> file_ids,
                   std::vector<FileEntry>* out);

  // Reads the contents of |file_id| on |storage_name|.
  // Reads |count| bytes starting at |offset|.
  // |file_id| is the unique identifier for a directory on |storage_name|.
  // |file_id| should never refer to the root node.
  // On success, returns true and writes the file contents of |file_id| into
  // |out|. Otherwise returns false.
  bool ReadFileChunk(const std::string& storage_name,
                     uint32_t file_id,
                     uint32_t offset,
                     uint32_t count,
                     std::vector<uint8_t>* out);

  // Copies file of |file_descriptor| to |file_name| in a folder |parent_id|.
  // On success, returns true.
  bool CopyFileFromLocal(const std::string& storage_name,
                         const uint32_t file_descriptor,
                         const uint32_t parent_id,
                         const std::string& file_name);

  // Delete |object_id|. For deleting a directory, the directory should be
  // empty. On success, this method returns true.
  bool DeleteObject(const std::string& storage_name, const uint32_t object_id);

  // Renames |object_id| to |new_name|.
  bool RenameObject(const std::string& storage_name,
                    const uint32_t object_id,
                    const std::string& new_name);

  // Creates a directory |directory_name| on |parent_id|.
  bool CreateDirectory(const std::string& storage_name,
                       const uint32_t parent_id,
                       const std::string& directory_name);

 protected:
  // Used in testing to add dummy storages.
  // Returns whether the test storage has been successfully added.
  // The dummy storage has no physical device backing it, so this should only
  // be used when testing functionality that does not require communicating
  // with a real device.
  bool AddStorageForTest(const std::string& storage_name,
                         const StorageInfo& storage_info);

 private:
  // Key: MTP storage id, Value: metadata for the given storage.
  using MtpStorageMap = std::map<uint32_t, StorageInfo>;
  // (device handle, map of storages on the device, device polling thread)
  struct MtpDevice {
    LIBMTP_mtpdevice_t* first;
    MtpStorageMap second;
    linked_ptr<base::SimpleThread> third;

    MtpDevice() : first(nullptr) {}

    MtpDevice(LIBMTP_mtpdevice_t* d,
              const MtpStorageMap& m,
              base::SimpleThread* t)
        : first(d), second(m), third(t) {}

    MtpDevice(const MtpDevice& rhs)
        : first(rhs.first), second(rhs.second), third(rhs.third) {}
  };
  // Key: device bus location, Value: MtpDevice.
  using MtpDeviceMap = std::map<std::string, MtpDevice>;

  // Reads entries from |device|'s storage with |storage_id|.
  // |file_id| is the unique identifier for a directory on the given storage.
  // For the root node, pass in |kPtpGohRootParent|.
  // On success, returns true and writes the file entries of |file_id| into
  // |out|. Otherwise returns false.
  bool ReadDirectory(LIBMTP_mtpdevice_t* device,
                     uint32_t storage_id,
                     uint32_t file_id,
                     std::vector<FileEntry>* out);

  // Reads the contents of |file_id| from |device|.
  // Reads |count| bytes starting at |offset|.
  // |file_id| is the unique identifier for a file on the given storage.
  // |file_id| should never refer to the root node.
  // On success, returns true and writes the file contents of |file_id| into
  // |out|. Otherwise returns false.
  bool ReadFileChunk(LIBMTP_mtpdevice_t* device,
                     uint32_t file_id,
                     uint32_t offset,
                     uint32_t count,
                     std::vector<uint8_t>* out);

  // Deletes |object_id| on |storage_id| of |mtp_device|. For deleting a
  // directory, the directory should be empty. On success, this method returns
  // true.
  bool DeleteObjectInternal(LIBMTP_mtpdevice_t* mtp_device,
                            const uint32_t storage_id,
                            const uint32_t object_id);

  // Reads the metadata of |file_id| from a storage on |device| with
  // |storage_id|.
  // |file_id| is the unique identifier for a file on the given device.
  // For the root node, pass in |kRootFileId|.
  // On success, returns true and writes the metadata of |file_id| into |out|.
  // Otherwise returns false.
  bool GetFileInfoInternal(LIBMTP_mtpdevice_t* device,
                           uint32_t storage_id,
                           uint32_t file_id,
                           FileEntry* out);

  // Helper function that returns the libmtp device handle and storage id for a
  // given |storage_name|.
  bool GetDeviceAndStorageId(const std::string& storage_name,
                             LIBMTP_mtpdevice_t** mtp_device,
                             uint32_t* storage_id);

  // Callback for udev when something changes for |device|.
  void HandleDeviceNotification(udev_device* device);

  // This is called by a separate thread which blocks in it
  // polling the device specified by |mtp_device| and |usb_name|.
  void PollDevice(LIBMTP_mtpdevice_t* mtp_device,
                  const std::string& usb_bus_name);

  // Iterates through attached devices and find ones that are newly attached.
  // Then populates |device_map_| for the newly attached devices.
  // If this is called as a result of a callback, it came from |source|,
  // which needs to be properly destructed.
  void AddDevices(GSource* source);

  // Re-reads the storage advertised by an already known device
  // on the USB bus.
  void UpdateDevice(const std::string& usb_bus_name);

  // Shared code for both AddDevices and UpdateDevice.
  // |add_update| is set true for add. |usb_bus_name| is only used
  // for update.
  void AddOrUpdateDevices(bool add_update, const std::string& usb_bus_name);

  // Iterates through attached devices and find ones that have been detached.
  // Then removes the detached devices from |device_map_|.
  // If |remove_all| is true, then assumes all devices have been detached.
  void RemoveDevices(bool remove_all);

  // libudev-related items: the main context, the monitoring context to be
  // notified about changes to device states, and the monitoring context's
  // file descriptor.
  udev* const udev_;
  udev_monitor* udev_monitor_;
  int udev_monitor_fd_;

  DeviceEventDelegate* const delegate_;

  // Map of devices and storages. Requires |device_map_lock_| to access.
  MtpDeviceMap device_map_;
  base::Lock device_map_lock_;

  base::WeakPtrFactory<DeviceManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManager);
};

}  // namespace mtpd

#endif  // MTPD_DEVICE_MANAGER_H_
