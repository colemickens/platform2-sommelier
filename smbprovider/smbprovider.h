// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SMBPROVIDER_H_
#define SMBPROVIDER_SMBPROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "dbus_adaptors/org.chromium.SmbProvider.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"

using brillo::dbus_utils::AsyncEventSequencer;

namespace smbprovider {

class DirectoryEntryList;
class SambaInterface;

// Used as buffer for serialized protobufs.
using ProtoBlob = std::vector<uint8_t>;

// Serializes |proto| to the byte array |proto_blob|. Returns ERROR_OK on
// success and ERROR_FAILED on failure.
template <typename ProtoType>
ErrorType SerializeProtoToBlob(const ProtoType& proto, ProtoBlob* proto_blob) {
  DCHECK(proto_blob);
  proto_blob->resize(proto.ByteSizeLong());
  return proto.SerializeToArray(proto_blob->data(), proto.ByteSizeLong())
             ? ERROR_OK
             : ERROR_FAILED;
}

// Implementation of smbprovider's DBus interface. Mostly routes stuff between
// DBus and samba_interface.
class SmbProvider : public org::chromium::SmbProviderAdaptor,
                    public org::chromium::SmbProviderInterface {
 public:
  SmbProvider(std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
              std::unique_ptr<smbprovider::SambaInterface> samba_interface,
              size_t buffer_size);

  SmbProvider(std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
              std::unique_ptr<smbprovider::SambaInterface> samba_interface);

  // org::chromium::SmbProviderInterface: (see org.chromium.SmbProvider.xml).
  void Mount(const ProtoBlob& mount_options_blob,
             int32_t* error_code,
             int32_t* mount_id) override;

  int32_t Unmount(const ProtoBlob& unmount_options_blob) override;

  void ReadDirectory(const ProtoBlob& read_directory_options_blob,
                     int32_t* error_code,
                     ProtoBlob* out_entries) override;

  void GetMetadataEntry(const ProtoBlob& get_metadata_options_blob,
                        int32_t* error_code,
                        ProtoBlob* out_entry) override;

  void OpenFile(const ProtoBlob& open_file_options_blob,
                int32_t* error_code,
                int32_t* file_id) override;

  int32_t CloseFile(const ProtoBlob& close_file_options_blob) override;

  // Register DBus object and interfaces.
  void RegisterAsync(
      const AsyncEventSequencer::CompletionAction& completion_callback);

 private:
  // Gets entries from |samba_interface_| in the directory |dir_id| and places
  // the entries in a buffer. It then transforms the data into a
  // DirectoryEntryList which is passed into |entries|. This should not be be
  // called on a directory that is not open. Returns 0 on success, and errno on
  // failure.
  // |entries| will be empty in case of failure.
  int32_t GetDirectoryEntries(int32_t dir_id, DirectoryEntryList* entries);

  // Helper method to get the corresponding |share_path| to the provided
  // |mount_id| on success. Returns true if if mount info is found.
  // |share_path| will be unmodified in case of failure.
  bool GetMountPathFromId(int32_t mount_id, std::string* share_path) const;

  // Helper method to close the directory with id |dir_id|. Logs an error if the
  // directory fails to close.
  void CloseDirectory(int32_t dir_id);

  std::unique_ptr<smbprovider::SambaInterface> samba_interface_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
  std::map<int32_t, std::string> mounts_;
  int32_t current_mount_id_ = 0;

  // |dir_buf_| is used as the buffer for reading directory entries in
  // GetDirectoryEntries(). Its initial capacity is specified in the
  // constructor.
  std::vector<uint8_t> dir_buf_;

  DISALLOW_COPY_AND_ASSIGN(SmbProvider);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_SMBPROVIDER_H_
