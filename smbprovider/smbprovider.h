// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SMBPROVIDER_H_
#define SMBPROVIDER_SMBPROVIDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <dbus_adaptors/org.chromium.SmbProvider.h>

#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/temp_file_manager.h"

using brillo::dbus_utils::AsyncEventSequencer;

namespace smbprovider {

class DirectoryEntryList;
class MountManager;
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
              std::unique_ptr<SambaInterface> samba_interface,
              std::unique_ptr<MountManager> mount_manager,
              size_t buffer_size);

  SmbProvider(std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
              std::unique_ptr<SambaInterface> samba_interface,
              std::unique_ptr<MountManager> mount_manager);

  // org::chromium::SmbProviderInterface: (see org.chromium.SmbProvider.xml).
  void Mount(const ProtoBlob& options_blob,
             int32_t* error_code,
             int32_t* mount_id) override;

  int32_t Unmount(const ProtoBlob& options_blob) override;

  void ReadDirectory(const ProtoBlob& options_blob,
                     int32_t* error_code,
                     ProtoBlob* out_entries) override;

  void GetMetadataEntry(const ProtoBlob& options_blob,
                        int32_t* error_code,
                        ProtoBlob* out_entry) override;

  void OpenFile(const ProtoBlob& options_blob,
                int32_t* error_code,
                int32_t* file_id) override;

  int32_t CloseFile(const ProtoBlob& options_blob) override;

  int32_t DeleteEntry(const ProtoBlob& options_blob) override;

  void ReadFile(const ProtoBlob& options_blob,
                int32_t* error_code,
                dbus::FileDescriptor* temp_fd) override;

  int32_t CreateFile(const ProtoBlob& options_blob) override;

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

  // Looks up the |mount_id| and appends |entry_path| to the root share path
  // and sets |full_path| to the result. |full_path| will be unmodified on
  // failure.
  // |operation_name| is used for logging purposes only.
  bool GetFullPath(const char* operation_name,
                   int32_t mount_id,
                   const std::string& entry_path,
                   std::string* full_path) const;

  // Parses the raw contents of |blob| into |options| and validates that
  // the required fields are all correctly set.
  // |full_path| will contain the full path, including the mount root, based
  // on the mount id and entry path supplied in |options|.
  // On failure |error_code| will be populated and |options| and |full_path|
  // are undefined.
  template <typename Proto>
  bool ParseOptionsAndPath(const ProtoBlob& blob,
                           Proto* options,
                           std::string* full_path,
                           int32_t* error_code);

  // Tests whether |mount_root| is a valid path to be mounted by attemping
  // to open the directory.
  bool CanMountPath(const std::string& mount_root, int32_t* error_code);

  // Helper method to get the type of an entry. Returns boolean indicating
  // success. Sets is_directory to true for directory, and false for file.
  // Fails when called on non-file, non-directory.
  // On failure, returns false and sets |error_code|.
  bool GetEntryType(const std::string& full_path,
                    int32_t* error_code,
                    bool* is_directory);

  // Helper method to close the directory with id |dir_id|. Logs an error if the
  // directory fails to close.
  void CloseDirectory(int32_t dir_id);

  // Removes |mount_id| from the |mount_manager_| object and sets |error_code|
  // on failure.
  bool RemoveMount(int32_t mount_id, int32_t* error_code);

  // Helper method to read a file with valid |options| and output the results
  // into a |buffer|. This sets |error_code| on failure.
  bool ReadFileIntoBuffer(const ReadFileOptions& options,
                          int32_t* error_code,
                          std::vector<uint8_t>* buffer);

  // Helper method to write data from a |buffer| into a temporary file and
  // outputs the resulting file descriptor into |temp_fd|. |options| is used for
  // logging purposes. This sets |error_code| on failure.
  bool WriteTempFile(const ReadFileOptions& options,
                     const std::vector<uint8_t>& buffer,
                     int32_t* error_code,
                     dbus::FileDescriptor* temp_fd);

  // Helper method to seek given a proto |options|.
  // On failure |error_code| will be populated.
  template <typename Proto>
  bool Seek(const Proto& options, int32_t* error_code);

  std::unique_ptr<SambaInterface> samba_interface_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
  std::unique_ptr<MountManager> mount_manager_;
  TempFileManager temp_file_manager_;

  // |dir_buf_| is used as the buffer for reading directory entries in
  // GetDirectoryEntries(). Its initial capacity is specified in the
  // constructor.
  std::vector<uint8_t> dir_buf_;

  DISALLOW_COPY_AND_ASSIGN(SmbProvider);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_SMBPROVIDER_H_
