// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_SERVER_IMPL_H_
#define MTPD_SERVER_IMPL_H_

#include <string>
#include <vector>

#include <base/compiler_specific.h>
#include <base/macros.h>

#include "device_event_delegate.h"
#include "device_manager.h"
#include "file_entry.h"
#include "mtpd_server/mtpd_server.h"

namespace mtpd {

class DeviceManager;

// The D-bus server for the mtpd daemon.
class MtpdServer : public org::chromium::Mtpd_adaptor,
                   public DBus::ObjectAdaptor,
                   public DeviceEventDelegate {
 public:
  explicit MtpdServer(DBus::Connection& connection);
  virtual ~MtpdServer();

  // org::chromium::Mtpd_adaptor implementation.
  std::vector<std::string> EnumerateStorages(DBus::Error& error) override;
  std::vector<uint8_t> GetStorageInfo(const std::string& storageName,
                                      DBus::Error& error) override;
  std::vector<uint8_t> GetStorageInfoFromDevice(const std::string& storageName,
                                                DBus::Error& error) override;
  std::string OpenStorage(const std::string& storageName,
                          const std::string& mode,
                          DBus::Error& error) override;
  void CloseStorage(const std::string& handle, DBus::Error& error) override;
  std::vector<uint32_t> ReadDirectoryEntryIds(const std::string& handle,
                                              const uint32_t& fileId,
                                              DBus::Error& error) override;
  std::vector<uint8_t> GetFileInfo(const std::string& handle,
                                   const std::vector<uint32_t>& fileIds,
                                   DBus::Error& error) override;
  std::vector<uint8_t> ReadFileChunk(const std::string& handle,
                                     const uint32_t& fileId,
                                     const uint32_t& offset,
                                     const uint32_t& count,
                                     DBus::Error& error) override;
  void CopyFileFromLocal(const std::string& handle,
                         const DBus::FileDescriptor& fileDescriptor,
                         const uint32_t& parentId,
                         const std::string& fileName,
                         DBus::Error& error) override;
  void DeleteObject(const std::string& handle,
                    const uint32_t& objectId,
                    DBus::Error& error) override;
  void RenameObject(const std::string& handle,
                    const uint32_t& objectId,
                    const std::string& newName,
                    DBus::Error& error) override;
  void CreateDirectory(const std::string& handle,
                       const uint32_t& parentId,
                       const std::string& directoryName,
                       DBus::Error& error) override;
  bool IsAlive(DBus::Error& error) override;

  // DeviceEventDelegate implementation.
  void StorageAttached(const std::string& storage_name) override;
  void StorageDetached(const std::string& storage_name) override;

  // Returns a file descriptor for monitoring device events.
  int GetDeviceEventDescriptor() const;

  // Processes the available device events.
  void ProcessDeviceEvents();

 private:
  // StorageHandleInfo is a pair of StorageName and Mode.
  typedef std::pair<std::string, std::string> StorageHandleInfo;

  // Handle to StorageHandleInfo map.
  typedef std::map<std::string, StorageHandleInfo> HandleMap;

  // Returns the StorageName for a handle, or an empty string on failure.
  std::string LookupHandle(const std::string& handle);

  // Returns true if the storage is opened with write access.
  bool IsOpenedWithWrite(const std::string& handle);

  HandleMap handle_map_;

  // Device manager needs to be last, so it is the first to be destroyed.
  DeviceManager device_manager_;

  DISALLOW_COPY_AND_ASSIGN(MtpdServer);
};

}  // namespace mtpd

#endif  // MTPD_SERVER_IMPL_H_
