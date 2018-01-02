// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/smbprovider.h"

#include <errno.h>

#include <map>
#include <utility>

#include <base/files/file_path.h>
#include <base/memory/ptr_util.h>

#include "smbprovider/constants.h"
#include "smbprovider/mount_manager.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/samba_interface_impl.h"
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

bool IsValidOptions(const MountOptions& options) {
  return options.has_path();
}

bool IsValidOptions(const UnmountOptions& options) {
  return options.has_mount_id();
}

bool IsValidOptions(const ReadDirectoryOptions& options) {
  return options.has_mount_id() && options.has_directory_path();
}

bool IsValidOptions(const GetMetadataEntryOptions& options) {
  return options.has_mount_id() && options.has_entry_path();
}

bool IsValidOptions(const OpenFileOptions& options) {
  return options.has_file_path() && options.has_writeable() &&
         options.has_mount_id();
}

bool IsValidOptions(const CloseFileOptions& options) {
  return options.has_mount_id() && options.has_file_id();
}

// Template specializations to extract the entry path from the various protos
// even when the fields have different names.
template <typename Proto>
std::string GetEntryPath(const Proto& options) {
  // Each new Proto type that uses ParseProtoAndPath must add a specialization
  // below that extracts the field from the proto that represents the path.
  //
  // This will only cause a compile error when a specialization is not defined.
  options.you_must_define_a_specialization_for_GetEntryPath();
}

template <>
std::string GetEntryPath(const ReadDirectoryOptions& options) {
  return options.directory_path();
}

template <>
std::string GetEntryPath(const GetMetadataEntryOptions& options) {
  return options.entry_path();
}

template <>
std::string GetEntryPath(const OpenFileOptions& options) {
  return options.file_path();
}

// Template specializations to map a method name to it's Proto argument.
template <typename Proto>
const char* GetMethodName(const Proto& unused) {
  // Each new Proto type must add a specialization below that maps the options
  // argument type to the corresponding method name.
  //
  // This will only cause a compile error when a specialization is not defined.
  unused.you_must_define_a_specialization_for_GetMethodName();
}

template <>
const char* GetMethodName(const MountOptions& unused) {
  return kMountMethod;
}

template <>
const char* GetMethodName(const UnmountOptions& unused) {
  return kUnmountMethod;
}

template <>
const char* GetMethodName(const GetMetadataEntryOptions& unused) {
  return kGetMetadataEntryMethod;
}

template <>
const char* GetMethodName(const ReadDirectoryOptions& unused) {
  return kReadDirectoryMethod;
}

template <>
const char* GetMethodName(const OpenFileOptions& unused) {
  return kOpenFileMethod;
}

template <>
const char* GetMethodName(const CloseFileOptions& unused) {
  return kCloseFileMethod;
}

void LogAndSetError(const char* operation_name,
                    int32_t mount_id,
                    ErrorType error_received,
                    int32_t* error_code) {
  LOG(ERROR) << "Error performing " << operation_name
             << " from mount id: " << mount_id << ": " << error_received;
  *error_code = static_cast<int32_t>(error_received);
}

// Logs error and sets |error_code|.
template <typename Proto>
void LogAndSetError(Proto options,
                    ErrorType error_received,
                    int32_t* error_code) {
  LogAndSetError(GetMethodName(options), options.mount_id(), error_received,
                 error_code);
}

void LogAndSetDBusParseError(const char* operation_name, int32_t* error_code) {
  LogAndSetError(operation_name, -1, ERROR_DBUS_PARSE_FAILED, error_code);
}

template <typename Proto>
bool ParseOptionsProto(const ProtoBlob& blob,
                       Proto* options,
                       int32_t* error_code) {
  bool is_valid = options->ParseFromArray(blob.data(), blob.size()) &&
                  IsValidOptions(*options);
  if (!is_valid) {
    LogAndSetDBusParseError(GetMethodName(*options), error_code);
  }

  return is_valid;
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
    std::unique_ptr<SambaInterface> samba_interface,
    std::unique_ptr<MountManager> mount_manager,
    size_t buffer_size)
    : org::chromium::SmbProviderAdaptor(this),
      samba_interface_(std::move(samba_interface)),
      dbus_object_(std::move(dbus_object)),
      mount_manager_(std::move(mount_manager)),
      dir_buf_(buffer_size) {}

SmbProvider::SmbProvider(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
    std::unique_ptr<SambaInterface> samba_interface,
    std::unique_ptr<MountManager> mount_manager)
    : SmbProvider(std::move(dbus_object),
                  std::move(samba_interface),
                  std::move(mount_manager),
                  kBufferSize) {}

void SmbProvider::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
}

void SmbProvider::Mount(const ProtoBlob& options_blob,
                        int32_t* error_code,
                        int32_t* mount_id) {
  DCHECK(error_code);
  DCHECK(mount_id);
  *mount_id = -1;

  MountOptions options;
  bool can_mount = ParseOptionsProto(options_blob, &options, error_code) &&
                   CanMountPath(options.path(), error_code);

  if (!can_mount) {
    // ParseOptionsProto() or CanMountPath() already set |error_code|.
    return;
  }

  *mount_id = mount_manager_->AddMount(options.path());
  *error_code = static_cast<int32_t>(ERROR_OK);
}

int32_t SmbProvider::Unmount(const ProtoBlob& options_blob) {
  int32_t error_code;
  UnmountOptions options;
  if (!ParseOptionsProto(options_blob, &options, &error_code) ||
      !RemoveMount(options.mount_id(), &error_code)) {
    return error_code;
  }

  return ERROR_OK;
}

void SmbProvider::ReadDirectory(const ProtoBlob& options_blob,
                                int32_t* error_code,
                                ProtoBlob* out_entries) {
  DCHECK(error_code);
  DCHECK(out_entries);
  out_entries->clear();

  std::string full_path;
  ReadDirectoryOptions options;
  if (!ParseOptionsAndPath(options_blob, &options, &full_path, error_code)) {
    return;
  }

  int32_t dir_id = -1;
  int32_t open_dir_error = samba_interface_->OpenDirectory(full_path, &dir_id);
  if (open_dir_error != 0) {
    LogAndSetError(options, GetErrorFromErrno(open_dir_error), error_code);
    return;
  }
  DirectoryEntryList directory_entries;
  int32_t get_dir_error = GetDirectoryEntries(dir_id, &directory_entries);
  if (get_dir_error != 0) {
    LogAndSetError(options, GetErrorFromErrno(get_dir_error), error_code);
    CloseDirectory(dir_id);
    return;
  }
  *error_code = static_cast<int32_t>(
      SerializeProtoToBlob(directory_entries, out_entries));
  CloseDirectory(dir_id);
}

void SmbProvider::GetMetadataEntry(const ProtoBlob& options_blob,
                                   int32_t* error_code,
                                   ProtoBlob* out_entry) {
  DCHECK(error_code);
  DCHECK(out_entry);
  out_entry->clear();

  std::string full_path;
  GetMetadataEntryOptions options;
  if (!ParseOptionsAndPath(options_blob, &options, &full_path, error_code)) {
    return;
  }

  struct stat stat_info;
  int32_t get_status_error =
      samba_interface_->GetEntryStatus(full_path.c_str(), &stat_info);
  if (get_status_error != 0) {
    LogAndSetError(options, GetErrorFromErrno(get_status_error), error_code);
    return;
  }
  *error_code = GetDirectoryEntryFromStat(full_path, stat_info, out_entry);
}

void SmbProvider::OpenFile(const ProtoBlob& options_blob,
                           int32_t* error_code,
                           int32_t* file_id) {
  DCHECK(error_code);
  DCHECK(file_id);

  std::string full_path;
  OpenFileOptions options;
  if (!ParseOptionsAndPath(options_blob, &options, &full_path, error_code)) {
    return;
  }

  const int32_t flags = options.writeable() ? O_RDWR : O_RDONLY;
  int32_t result = samba_interface_->OpenFile(full_path, flags, file_id);
  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), error_code);
    *file_id = -1;
    return;
  }

  *error_code = static_cast<int32_t>(ERROR_OK);
}

int32_t SmbProvider::CloseFile(const ProtoBlob& options_blob) {
  int32_t error_code;
  CloseFileOptions options;
  if (!ParseOptionsProto(options_blob, &options, &error_code)) {
    return error_code;
  }

  int32_t result = samba_interface_->CloseFile(options.file_id());
  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), &error_code);
    return error_code;
  }

  return static_cast<int32_t>(ERROR_OK);
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

// TODO(zentaro): When the proto's with missing mount_id are landed, this can
// take a generic *Options proto and derive the operation name and entry path
// itself.
bool SmbProvider::GetFullPath(const char* operation_name,
                              int32_t mount_id,
                              const std::string& entry_path,
                              std::string* full_path) const {
  if (!mount_manager_->GetFullPath(mount_id, entry_path, full_path)) {
    LOG(ERROR) << operation_name << " requested unknown mount_id " << mount_id;
    return false;
  }

  return true;
}

template <typename Proto>
bool SmbProvider::ParseOptionsAndPath(const ProtoBlob& blob,
                                      Proto* options,
                                      std::string* full_path,
                                      int32_t* error_code) {
  if (!ParseOptionsProto(blob, options, error_code)) {
    return false;
  }

  if (!GetFullPath(GetMethodName(*options), options->mount_id(),
                   GetEntryPath(*options), full_path)) {
    *error_code = static_cast<int32_t>(ERROR_NOT_FOUND);
    return false;
  }

  return true;
}

bool SmbProvider::CanMountPath(const std::string& mount_root,
                               int32_t* error_code) {
  int32_t dir_id = -1;
  int32_t result = samba_interface_->OpenDirectory(mount_root, &dir_id);
  if (result != 0) {
    LogAndSetError(kMountMethod, -1, GetErrorFromErrno(result), error_code);
    return false;
  }

  CloseDirectory(dir_id);
  return true;
}

void SmbProvider::CloseDirectory(int32_t dir_id) {
  int32_t result = samba_interface_->CloseDirectory(dir_id);
  if (result != 0) {
    LOG(ERROR) << "Error closing directory " << dir_id;
  }
}

bool SmbProvider::RemoveMount(int32_t mount_id, int32_t* error_code) {
  bool removed = mount_manager_->RemoveMount(mount_id);
  if (!removed) {
    *error_code = static_cast<int32_t>(ERROR_NOT_FOUND);
  }

  return removed;
}

}  // namespace smbprovider
