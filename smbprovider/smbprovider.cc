// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include <base/files/file_path.h>
#include <base/memory/ptr_util.h>
#include <errno.h>

#include "smbprovider/constants.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/samba_interface_impl.h"
#include "smbprovider/smbprovider.h"
#include "smbprovider/smbprovider_helper.h"

using brillo::dbus_utils::DBusObject;

namespace smbprovider {

namespace {

// Maps errno to ErrorType.
ErrorType GetErrorFromErrno(int32_t error_code) {
  ErrorType error;
  switch (error_code) {
    case EPERM:
    case EACCES:
      error = ERROR_ACCESS_DENIED;
      break;
    case EBADF:
    case ENODEV:
    case ENOENT:
      error = ERROR_NOT_FOUND;
      break;
    case EMFILE:
    case ENFILE:
      error = ERROR_TOO_MANY_OPENED;
      break;
    case ENOTDIR:
      error = ERROR_NOT_A_DIRECTORY;
      break;
    default:
      error = ERROR_FAILED;
      break;
  }
  return error;
}

// Logs error and sets |error_code|.
void LogAndSetError(const std::string& operation_name,
                    int32_t mount_id,
                    ErrorType error_received,
                    int32_t* error_code) {
  LOG(ERROR) << "Error performing " << operation_name
             << " from mount id: " << mount_id << ": " << error_received;
  *error_code = static_cast<int32_t>(error_received);
}

void LogAndSetDBusParseError(const std::string& operation_name,
                             int32_t* error_code) {
  LogAndSetError(operation_name, -1, ERROR_DBUS_PARSE_FAILED, error_code);
}

// Helper method to get |DirectoryEntry| from a struct stat. Returns ERROR_OK
// on success and ERROR_FAILED otherwise.
int32_t GetDirectoryEntryFromStat(const std::string& full_path,
                                  const struct stat& stat_info,
                                  ProtoBlob* proto_blob) {
  DCHECK(proto_blob);
  bool is_directory = S_ISDIR(stat_info.st_mode);
  int64_t size = is_directory ? 0 : stat_info.st_size;
  const base::FilePath path(full_path);

  DirectoryEntry entry;
  entry.set_is_directory(is_directory);
  entry.set_name(path.BaseName().value());
  entry.set_size(size);
  entry.set_last_modified_time(stat_info.st_mtime);
  return static_cast<int32_t>(SerializeProtoToBlob(entry, proto_blob));
}

}  // namespace

SmbProvider::SmbProvider(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
    std::unique_ptr<smbprovider::SambaInterface> samba_interface,
    size_t buffer_size)
    : org::chromium::SmbProviderAdaptor(this),
      samba_interface_(std::move(samba_interface)),
      dbus_object_(std::move(dbus_object)),
      dir_buf_(buffer_size) {}

SmbProvider::SmbProvider(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
    std::unique_ptr<smbprovider::SambaInterface> samba_interface)
    : SmbProvider(
          std::move(dbus_object), std::move(samba_interface), kBufferSize) {}

void SmbProvider::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
}

void SmbProvider::Mount(const ProtoBlob& mount_options_blob,
                        int32_t* error_code,
                        int32_t* mount_id) {
  DCHECK(error_code);
  DCHECK(mount_id);
  MountOptions mount_options;
  if (!mount_options.ParseFromArray(mount_options_blob.data(),
                                    mount_options_blob.size())) {
    *mount_id = -1;
    LogAndSetDBusParseError(kMountMethod, error_code);
    return;
  }
  if (!mount_options.has_path()) {
    *mount_id = -1;
    LogAndSetDBusParseError(kMountMethod, error_code);
    return;
  }
  int32_t dir_id = -1;
  int32_t open_dir_error =
      samba_interface_->OpenDirectory(mount_options.path(), &dir_id);
  if (open_dir_error != 0) {
    *mount_id = -1;
    LogAndSetError(kMountMethod, -1, GetErrorFromErrno(open_dir_error),
                   error_code);
    return;
  }
  DCHECK(mounts_.find(current_mount_id_) == mounts_.end());
  mounts_[current_mount_id_] = mount_options.path();
  CloseDirectory(dir_id);
  *mount_id = current_mount_id_;
  *error_code = static_cast<int32_t>(ERROR_OK);
  current_mount_id_++;
}

int32_t SmbProvider::Unmount(const ProtoBlob& unmount_options_blob) {
  UnmountOptions unmount_options;
  if (!unmount_options.ParseFromArray(unmount_options_blob.data(),
                                      unmount_options_blob.size())) {
    LOG(ERROR) << "Error deserializing UnmountOptions proto";
    return static_cast<int32_t>(ERROR_DBUS_PARSE_FAILED);
  }
  if (!unmount_options.has_mount_id()) {
    LOG(ERROR) << "Error deserializing UnmountOptions proto";
    return static_cast<int32_t>(ERROR_DBUS_PARSE_FAILED);
  }
  ErrorType error = mounts_.erase(unmount_options.mount_id()) == 0
                        ? ERROR_NOT_FOUND
                        : ERROR_OK;
  return static_cast<int32_t>(error);
}

void SmbProvider::ReadDirectory(const ProtoBlob& read_directory_options_blob,
                                int32_t* error_code,
                                ProtoBlob* out_entries) {
  DCHECK(error_code);
  DCHECK(out_entries);
  out_entries->clear();
  ReadDirectoryOptions read_directory_options;
  if (!read_directory_options.ParseFromArray(
          read_directory_options_blob.data(),
          read_directory_options_blob.size())) {
    LogAndSetDBusParseError(kReadDirectoryMethod, error_code);
    return;
  }
  if (!(read_directory_options.has_mount_id() &&
        read_directory_options.has_directory_path())) {
    LogAndSetDBusParseError(kReadDirectoryMethod, error_code);
    return;
  }
  std::string share_path;
  if (!GetMountPathFromId(read_directory_options.mount_id(), &share_path)) {
    LogAndSetError(kReadDirectoryMethod, read_directory_options.mount_id(),
                   ERROR_NOT_FOUND, error_code);
    return;
  }
  int32_t dir_id = -1;
  int32_t open_dir_error = samba_interface_->OpenDirectory(
      AppendPath(share_path, read_directory_options.directory_path()), &dir_id);
  if (open_dir_error != 0) {
    LogAndSetError(kReadDirectoryMethod, read_directory_options.mount_id(),
                   GetErrorFromErrno(open_dir_error), error_code);
    return;
  }
  DirectoryEntryList directory_entries;
  int32_t get_dir_error = GetDirectoryEntries(dir_id, &directory_entries);
  if (get_dir_error != 0) {
    LogAndSetError(kReadDirectoryMethod, read_directory_options.mount_id(),
                   GetErrorFromErrno(get_dir_error), error_code);
    CloseDirectory(dir_id);
    return;
  }
  *error_code = static_cast<int32_t>(
      SerializeProtoToBlob(directory_entries, out_entries));
  CloseDirectory(dir_id);
}

void SmbProvider::GetMetadataEntry(const ProtoBlob& get_metadata_options_blob,
                                   int32_t* error_code,
                                   ProtoBlob* out_entry) {
  DCHECK(error_code);
  DCHECK(out_entry);
  out_entry->clear();
  GetMetadataEntryOptions get_metadata_entry_options;
  if (!get_metadata_entry_options.ParseFromArray(
          get_metadata_options_blob.data(), get_metadata_options_blob.size())) {
    LogAndSetDBusParseError(kGetMetadataEntryMethod, error_code);
    return;
  }
  if (!(get_metadata_entry_options.has_mount_id() &&
        get_metadata_entry_options.has_entry_path())) {
    LogAndSetDBusParseError(kGetMetadataEntryMethod, error_code);
    return;
  }
  std::string share_path;
  if (!GetMountPathFromId(get_metadata_entry_options.mount_id(), &share_path)) {
    LogAndSetError(kGetMetadataEntryMethod,
                   get_metadata_entry_options.mount_id(), ERROR_NOT_FOUND,
                   error_code);
    return;
  }
  const std::string full_path =
      AppendPath(share_path, get_metadata_entry_options.entry_path());
  struct stat stat_info;
  int32_t get_status_error =
      samba_interface_->GetEntryStatus(full_path.c_str(), &stat_info);
  if (get_status_error != 0) {
    LogAndSetError(kGetMetadataEntryMethod,
                   get_metadata_entry_options.mount_id(),
                   GetErrorFromErrno(get_status_error), error_code);
    return;
  }
  *error_code = GetDirectoryEntryFromStat(full_path, stat_info, out_entry);
}

// This is a helper method that has a similar return structure as
// samba_interface_ methods, where it will return errno as an error in case of
// failure.
int32_t SmbProvider::GetDirectoryEntries(int32_t dir_id,
                                         DirectoryEntryList* entries) {
  DCHECK(entries);
  int32_t bytes_read = 0;
  do {
    int32_t result = samba_interface_->GetDirectoryEntries(
        dir_id, GetDirentFromBuffer(dir_buf_.data()), dir_buf_.size(),
        &bytes_read);
    if (result != 0) {
      // The result will be set to errno on failure.
      return result;
    }
    int32_t bytes_left = bytes_read;
    smbc_dirent* dirent = GetDirentFromBuffer(dir_buf_.data());
    while (bytes_left > 0) {
      AddEntryIfValid(*dirent, entries);
      DCHECK_GT(dirent->dirlen, 0);
      DCHECK_GE(bytes_left, dirent->dirlen);
      bytes_left -= dirent->dirlen;
      dirent = AdvanceDirEnt(dirent);
      DCHECK(dirent);
    }
    DCHECK_EQ(bytes_left, 0);
  } while (bytes_read > 0);
  return 0;
}

bool SmbProvider::GetMountPathFromId(int32_t mount_id,
                                     std::string* share_path) const {
  DCHECK(share_path);
  auto mount_iter = mounts_.find(mount_id);
  if (mount_iter == mounts_.end()) {
    return false;
  }
  *share_path = mount_iter->second;
  return true;
}

void SmbProvider::CloseDirectory(int32_t dir_id) {
  int32_t result = samba_interface_->CloseDirectory(dir_id);
  if (result != 0) {
    LOG(ERROR) << "Error closing directory " << dir_id;
  }
}

}  // namespace smbprovider
