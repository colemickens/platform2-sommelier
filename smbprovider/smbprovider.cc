// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/smbprovider.h"

#include <algorithm>
#include <map>
#include <utility>

#include <base/files/file_path.h>
#include <base/memory/ptr_util.h>

#include "smbprovider/constants.h"
#include "smbprovider/iterator/directory_iterator.h"
#include "smbprovider/iterator/post_depth_first_iterator.h"
#include "smbprovider/iterator/share_iterator.h"
#include "smbprovider/mount_manager.h"
#include "smbprovider/proto.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"
#include "smbprovider/samba_interface_impl.h"
#include "smbprovider/smbprovider_helper.h"

namespace smbprovider {

template <typename Proto, typename Iterator>
bool GetEntries(const Proto& options,
                Iterator iterator,
                int32_t* error_code,
                ProtoBlob* out_entries) {
  DCHECK(error_code);
  DCHECK(out_entries);

  DirectoryEntryListProto directory_entries;

  int32_t result = iterator.Init();
  while (result == 0) {
    if (iterator.IsDone()) {
      *error_code = static_cast<int32_t>(
          SerializeProtoToBlob(directory_entries, out_entries));
      return true;
    }
    AddDirectoryEntry(iterator.Get(), &directory_entries);
    result = iterator.Next();
  }

  // The while-loop is only exited if there is an error. A full successful
  // execution will return from inside the above while-loop.
  *error_code = GetErrorFromErrno(result);
  LogOperationError(GetMethodName(options), GetMountId(options),
                    static_cast<ErrorType>(*error_code));
  return false;
}

SmbProvider::SmbProvider(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object,
    std::unique_ptr<SambaInterface> samba_interface,
    std::unique_ptr<MountManager> mount_manager)
    : org::chromium::SmbProviderAdaptor(this),
      samba_interface_(std::move(samba_interface)),
      dbus_object_(std::move(dbus_object)),
      mount_manager_(std::move(mount_manager)) {}

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

  MountOptionsProto options;
  bool can_mount = ParseOptionsProto(options_blob, &options, error_code) &&
                   CanMountPath(options.path(), error_code);

  if (!can_mount) {
    // ParseOptionsProto() or CanMountPath() already set |error_code|.
    return;
  }

  *mount_id = mount_manager_->AddMount(options.path());
  *error_code = static_cast<int32_t>(ERROR_OK);
}

int32_t SmbProvider::Remount(const ProtoBlob& options_blob) {
  int32_t error_code;

  RemountOptionsProto options;
  bool can_remount = ParseOptionsProto(options_blob, &options, &error_code) &&
                     CanMountPath(options.path(), &error_code);

  if (!can_remount) {
    // ParseOptionsProto() or CanMountPath() already set |error_code|.
    return error_code;
  }

  mount_manager_->Remount(options.path(), options.mount_id());
  error_code = static_cast<int32_t>(ERROR_OK);

  return error_code;
}

int32_t SmbProvider::Unmount(const ProtoBlob& options_blob) {
  int32_t error_code;
  UnmountOptionsProto options;
  if (!ParseOptionsProto(options_blob, &options, &error_code) ||
      !RemoveMount(GetMountId(options), &error_code)) {
    return error_code;
  }

  return ERROR_OK;
}

void SmbProvider::ReadDirectory(const ProtoBlob& options_blob,
                                int32_t* error_code,
                                ProtoBlob* out_entries) {
  DCHECK(error_code);
  DCHECK(out_entries);

  ReadDirectoryEntries<ReadDirectoryOptionsProto, DirectoryIterator>(
      options_blob, error_code, out_entries);
}

template <typename Proto, typename Iterator>
void SmbProvider::ReadDirectoryEntries(const ProtoBlob& options_blob,
                                       int32_t* error_code,
                                       ProtoBlob* out_entries) {
  DCHECK(error_code);
  DCHECK(out_entries);
  out_entries->clear();

  std::string full_path;
  Proto options;

  ParseOptionsAndPath(options_blob, &options, &full_path, error_code) &&
      GetEntries(options,
                 GetIterator<Iterator>(full_path, samba_interface_.get()),
                 error_code, out_entries);
}

void SmbProvider::GetMetadataEntry(const ProtoBlob& options_blob,
                                   int32_t* error_code,
                                   ProtoBlob* out_entry) {
  DCHECK(error_code);
  DCHECK(out_entry);
  out_entry->clear();

  std::string full_path;
  GetMetadataEntryOptionsProto options;
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
  *error_code = GetDirectoryEntryProtoFromStat(full_path, stat_info, out_entry);
}

void SmbProvider::OpenFile(const ProtoBlob& options_blob,
                           int32_t* error_code,
                           int32_t* file_id) {
  DCHECK(error_code);
  DCHECK(file_id);

  std::string full_path;
  OpenFileOptionsProto options;
  if (!ParseOptionsAndPath(options_blob, &options, &full_path, error_code)) {
    return;
  }

  if (!OpenFile(options, full_path, error_code, file_id)) {
    *file_id = -1;
    return;
  }

  *error_code = static_cast<int32_t>(ERROR_OK);
}

int32_t SmbProvider::CloseFile(const ProtoBlob& options_blob) {
  int32_t error_code;
  CloseFileOptionsProto options;
  if (!ParseOptionsProto(options_blob, &options, &error_code)) {
    return error_code;
  }

  if (!CloseFile(options, options.file_id(), &error_code)) {
    return error_code;
  }

  return static_cast<int32_t>(ERROR_OK);
}

int32_t SmbProvider::DeleteEntry(const ProtoBlob& options_blob) {
  int32_t error_code;
  DeleteEntryOptionsProto options;
  std::string full_path;
  if (!ParseOptionsAndPath(options_blob, &options, &full_path, &error_code)) {
    return error_code;
  }

  bool is_directory;
  int32_t get_type_result;
  if (!GetEntryType(full_path, &get_type_result, &is_directory)) {
    LogAndSetError(options, GetErrorFromErrno(get_type_result), &error_code);
    return error_code;
  }

  int32_t result;
  if (is_directory) {
    if (options.recursive()) {
      result = RecursiveDelete(full_path);
    } else {
      result = DeleteDirectory(full_path);
    }
  } else {
    result = DeleteFile(full_path);
  }

  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), &error_code);
    return error_code;
  }

  return static_cast<int32_t>(ERROR_OK);
}

void SmbProvider::ReadFile(const ProtoBlob& options_blob,
                           int32_t* error_code,
                           brillo::dbus_utils::FileDescriptor* temp_fd) {
  DCHECK(error_code);
  DCHECK(temp_fd);

  // TODO(allenvic): Investigate having a single shared buffer in the class.
  std::vector<uint8_t> buffer;
  ReadFileOptionsProto options;

  // The functions below will set the error if they fail.
  *error_code = static_cast<int32_t>(ERROR_OK);
  ParseOptionsProto(options_blob, &options, error_code) &&
      Seek(options, error_code) &&
      ReadFileIntoBuffer(options, error_code, &buffer) &&
      WriteTempFile(options, buffer, error_code, temp_fd);
}

int32_t SmbProvider::CreateFile(const ProtoBlob& options_blob) {
  int32_t error_code;
  std::string full_path;
  CreateFileOptionsProto options;
  if (!ParseOptionsAndPath(options_blob, &options, &full_path, &error_code)) {
    return error_code;
  }

  int32_t file_id;
  // CreateFile() gives us back an open file descriptor to the newly created
  // file.
  if (!CreateFile(options, full_path, &file_id, &error_code)) {
    return error_code;
  }

  // Close the file handle from CreateFile().
  if (!CloseFile(options, file_id, &error_code)) {
    // Attempt to delete the file since file will not be usable.
    int32_t unlink_result = samba_interface_->Unlink(full_path);
    if (unlink_result != 0) {
      // Log the unlink error but return the original error.
      LOG(ERROR) << "Error unlinking after error closing file: "
                 << GetErrorFromErrno(unlink_result);
    }
    return error_code;
  }

  return static_cast<int32_t>(ERROR_OK);
}

int32_t SmbProvider::Truncate(const ProtoBlob& options_blob) {
  int32_t error_code;
  std::string full_path;
  TruncateOptionsProto options;
  int32_t file_id;

  const bool result =
      ParseOptionsAndPath(options_blob, &options, &full_path, &error_code) &&
      OpenFile(options, full_path, &error_code, &file_id) &&
      TruncateAndCloseFile(options, file_id, options.length(), &error_code);

  return result ? static_cast<int32_t>(ERROR_OK) : error_code;
}

int32_t SmbProvider::WriteFile(const ProtoBlob& options_blob,
                               const base::ScopedFD& temp_fd) {
  int32_t error_code;
  WriteFileOptionsProto options;
  std::vector<uint8_t> buffer;

  const bool result =
      ParseOptionsProto(options_blob, &options, &error_code) &&
      ReadFromFD(options, temp_fd, &error_code, &buffer) &&
      Seek(options, &error_code) &&
      WriteFileFromBuffer(options, options.file_id(), buffer, &error_code);

  return result ? static_cast<int32_t>(ERROR_OK) : error_code;
}

int32_t SmbProvider::CreateDirectory(const ProtoBlob& options_blob) {
  int32_t error_code;
  CreateDirectoryOptionsProto options;
  std::string full_path;

  const bool result =
      ParseOptionsAndPath(options_blob, &options, &full_path, &error_code) &&
      CreateParentsIfNecessary(options, &error_code) &&
      CreateSingleDirectory(options, full_path, false /* ignore_existing */,
                            &error_code);

  return result ? static_cast<int32_t>(ERROR_OK) : error_code;
}

int32_t SmbProvider::MoveEntry(const ProtoBlob& options_blob) {
  int32_t error_code;
  std::string source_path;
  std::string target_path;
  MoveEntryOptionsProto options;

  const bool success =
      ParseOptionsAndPaths(options_blob, &options, &source_path, &target_path,
                           &error_code) &&
      MoveEntry(options, source_path, target_path, &error_code);

  return success ? static_cast<int32_t>(ERROR_OK) : error_code;
}

int32_t SmbProvider::CopyEntry(const ProtoBlob& options_blob) {
  int32_t error_code;
  std::string source_path;
  std::string target_path;
  CopyEntryOptionsProto options;

  const bool success =
      ParseOptionsAndPaths(options_blob, &options, &source_path, &target_path,
                           &error_code) &&
      CopyEntry(options, source_path, target_path, &error_code);

  return success ? static_cast<int32_t>(ERROR_OK) : error_code;
}

void SmbProvider::GetShares(const ProtoBlob& options_blob,
                            int32_t* error_code,
                            ProtoBlob* shares) {
  DCHECK(error_code);
  DCHECK(shares);

  ReadDirectoryEntries<GetSharesOptionsProto, ShareIterator>(
      options_blob, error_code, shares);
}

template <typename Proto>
bool SmbProvider::GetFullPath(const Proto* options,
                              std::string* full_path) const {
  DCHECK(options);
  DCHECK(full_path);

  const int32_t mount_id = GetMountId(*options);
  const std::string entry_path = GetEntryPath(*options);

  bool success = mount_manager_->GetFullPath(mount_id, entry_path, full_path);
  if (!success) {
    LOG(ERROR) << GetMethodName(*options) << " requested unknown mount_id "
               << mount_id;
  }

  return success;
}

template <>
bool SmbProvider::GetFullPath(const GetSharesOptionsProto* options,
                              std::string* full_path) const {
  DCHECK(options);
  DCHECK(full_path);

  *full_path = GetEntryPath(*options);
  return true;
}

template <typename Proto>
bool SmbProvider::GetFullPaths(const Proto* options,
                               std::string* source_full_path,
                               std::string* target_full_path) const {
  DCHECK(options);
  DCHECK(source_full_path);
  DCHECK(target_full_path);

  const int32_t mount_id = GetMountId(*options);
  const std::string source_path = GetSourcePath(*options);
  const std::string target_path = GetDestinationPath(*options);

  const bool success =
      mount_manager_->GetFullPath(mount_id, source_path, source_full_path) &&
      mount_manager_->GetFullPath(mount_id, target_path, target_full_path);
  if (!success) {
    LOG(ERROR) << GetMethodName(*options) << " requested unknown mount_id "
               << mount_id;
  }

  return success;
}

template <typename Proto>
bool SmbProvider::ParseOptionsAndPath(const ProtoBlob& blob,
                                      Proto* options,
                                      std::string* full_path,
                                      int32_t* error_code) {
  if (!ParseOptionsProto(blob, options, error_code)) {
    return false;
  }

  if (!GetFullPath(options, full_path)) {
    *error_code = static_cast<int32_t>(ERROR_NOT_FOUND);
    return false;
  }

  return true;
}

template <typename Proto>
bool SmbProvider::ParseOptionsAndPaths(const ProtoBlob& blob,
                                       Proto* options,
                                       std::string* source_path,
                                       std::string* target_path,
                                       int32_t* error_code) {
  DCHECK(options);
  DCHECK(source_path);
  DCHECK(target_path);
  DCHECK(error_code);

  if (!ParseOptionsProto(blob, options, error_code)) {
    return false;
  }

  if (!GetFullPaths(options, source_path, target_path)) {
    *error_code = static_cast<int32_t>(ERROR_NOT_FOUND);
    return false;
  }

  return true;
}

bool SmbProvider::GetEntryType(const std::string& full_path,
                               int32_t* error_code,
                               bool* is_directory) {
  struct stat stat_info;
  *error_code = samba_interface_->GetEntryStatus(full_path.c_str(), &stat_info);
  if (*error_code != 0) {
    return false;
  }

  if (IsDirectory(stat_info)) {
    *is_directory = true;
    return true;
  }
  if (IsFile(stat_info)) {
    *is_directory = false;
    return true;
  }
  *error_code = ENOENT;
  return false;
}

template <typename Proto>
bool SmbProvider::Seek(const Proto& options, int32_t* error_code) {
  int32_t result = samba_interface_->Seek(options.file_id(), options.offset());
  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), error_code);
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

bool SmbProvider::ReadFileIntoBuffer(const ReadFileOptionsProto& options,
                                     int32_t* error_code,
                                     std::vector<uint8_t>* buffer) {
  DCHECK(buffer);
  DCHECK(error_code);

  buffer->resize(options.length());
  size_t bytes_read;

  if (!ReadToBuffer(options, options.file_id(), buffer, &bytes_read,
                    error_code)) {
    return false;
  }
  return true;
}

template <typename Proto>
bool SmbProvider::WriteTempFile(const Proto& options,
                                const std::vector<uint8_t>& buffer,
                                int32_t* error_code,
                                brillo::dbus_utils::FileDescriptor* temp_fd) {
  base::ScopedFD scoped_fd = temp_file_manager_.CreateTempFile(buffer);
  if (!scoped_fd.is_valid()) {
    LogAndSetError(options, ERROR_IO, error_code);
    return false;
  }

  *temp_fd = scoped_fd.release();
  return true;
}

template <typename Proto>
bool SmbProvider::WriteFileFromBuffer(const Proto& options,
                                      int32_t file_id,
                                      const std::vector<uint8_t>& buffer,
                                      int32_t* error_code) {
  DCHECK(error_code);

  int32_t result =
      samba_interface_->WriteFile(file_id, buffer.data(), buffer.size());
  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), error_code);
    return false;
  }
  return true;
}

int32_t SmbProvider::RecursiveDelete(const std::string& dir_path) {
  PostDepthFirstIterator it = GetPostOrderIterator(dir_path);
  int32_t it_result = it.Init();
  while (it_result == 0) {
    if (it.IsDone()) {
      return 0;
    }

    int32_t del_result = DeleteDirectoryEntry(it.Get());
    if (del_result != 0) {
      return del_result;
    }

    it_result = it.Next();
  }

  // while-loop is only exited from if there's an iterator error.
  DCHECK_NE(0, it_result);
  return it_result;
}

int32_t SmbProvider::DeleteDirectoryEntry(const DirectoryEntry& entry) {
  if (entry.is_directory) {
    return DeleteDirectory(entry.full_path);
  }
  return DeleteFile(entry.full_path);
}

int32_t SmbProvider::DeleteFile(const std::string& file_path) {
  return samba_interface_->Unlink(file_path.c_str());
}

int32_t SmbProvider::DeleteDirectory(const std::string& dir_path) {
  return samba_interface_->RemoveDirectory(dir_path.c_str());
}

PostDepthFirstIterator SmbProvider::GetPostOrderIterator(
    const std::string& full_path) {
  return PostDepthFirstIterator(full_path, samba_interface_.get());
}

template <typename Proto>
bool SmbProvider::OpenFile(const Proto& options,
                           const std::string& full_path,
                           int32_t* error,
                           int32_t* file_id) {
  int32_t result = samba_interface_->OpenFile(
      full_path, GetOpenFilePermissions(options), file_id);
  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), error);
    return false;
  }
  return true;
}

template <typename Proto>
bool SmbProvider::CloseFile(const Proto& options,
                            int32_t file_id,
                            int32_t* error) {
  int32_t result = samba_interface_->CloseFile(file_id);
  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), error);
    return false;
  }
  return true;
}

template <typename Proto>
bool SmbProvider::TruncateAndCloseFile(const Proto& options,
                                       const int32_t file_id,
                                       int64_t length,
                                       int32_t* error) {
  int32_t truncate_result = samba_interface_->Truncate(file_id, length);
  if (truncate_result != 0) {
    LogAndSetError(options, GetErrorFromErrno(truncate_result), error);
    // Continue to close on error.
  }

  int32_t close_error;
  if (!CloseFile(options, file_id, &close_error)) {
    if (truncate_result == 0) {
      // If Truncate was successful, set error to the close error, otherwise
      // keep the truncate error.
      *error = close_error;
    }
    return false;
  }

  // Return if the truncate was successful.
  return truncate_result == 0;
}

bool SmbProvider::MoveEntry(const MoveEntryOptionsProto& options,
                            const std::string& source_path,
                            const std::string& target_path,
                            int32_t* error) {
  DCHECK(error);
  int32_t result = samba_interface_->MoveEntry(source_path, target_path);
  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), error);
    return false;
  }
  return true;
}

bool SmbProvider::GenerateParentPaths(
    const CreateDirectoryOptionsProto& options,
    int32_t* error_code,
    std::vector<std::string>* parent_paths) {
  DCHECK(error_code);
  DCHECK(parent_paths);
  base::FilePath current_path(options.directory_path());
  DCHECK(current_path.IsAbsolute());

  // Skip the leaf path and start with the lowest parent.
  current_path = current_path.DirName();

  while (current_path.value() != "/") {
    std::string full_path;
    if (!mount_manager_->GetFullPath(
            GetMountId(options), current_path.StripTrailingSeparators().value(),
            &full_path)) {
      *error_code = static_cast<int32_t>(ERROR_NOT_FOUND);
      return false;
    }

    current_path = current_path.DirName();
    parent_paths->push_back(std::move(full_path));
  }

  // Reverse the vector so the top parent will be first.
  std::reverse(parent_paths->begin(), parent_paths->end());
  return true;
}

bool SmbProvider::CreateNestedDirectories(
    const CreateDirectoryOptionsProto& options,
    const std::vector<std::string>& paths,
    int32_t* error_code) {
  DCHECK(error_code);

  for (auto const& path : paths) {
    if (!CreateSingleDirectory(options, path, true /* ignore_existing */,
                               error_code)) {
      return false;
    }
  }
  return true;
}

bool SmbProvider::CreateParentsIfNecessary(
    const CreateDirectoryOptionsProto& options, int32_t* error_code) {
  DCHECK(error_code);

  if (!options.recursive()) {
    // Return true in this case since no parents need to be created.
    return true;
  }

  std::vector<std::string> paths;
  return GenerateParentPaths(options, error_code, &paths) &&
         CreateNestedDirectories(options, paths, error_code);
}

template <typename Proto>
bool SmbProvider::CreateSingleDirectory(const Proto& options,
                                        const std::string& full_path,
                                        bool ignore_existing,
                                        int32_t* error_code) {
  DCHECK(error_code);

  const int32_t result = samba_interface_->CreateDirectory(full_path);
  if (ShouldReportCreateDirError(result, ignore_existing)) {
    LogAndSetError(options, GetErrorFromErrno(result), error_code);
    return false;
  }

  return true;
}

template <typename Proto>
bool SmbProvider::CreateFile(const Proto& options,
                             const std::string& full_path,
                             int32_t* file_id,
                             int32_t* error) {
  DCHECK(file_id);
  DCHECK(error);

  int32_t result = samba_interface_->CreateFile(full_path, file_id);
  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), error);
    return false;
  }
  return true;
}

bool SmbProvider::CopyEntry(const CopyEntryOptionsProto& options,
                            const std::string& source_path,
                            const std::string& target_path,
                            int32_t* error_code) {
  DCHECK(error_code);
  bool is_directory;
  int32_t get_type_result;
  if (!GetEntryType(source_path, &get_type_result, &is_directory)) {
    LogAndSetError(options, GetErrorFromErrno(get_type_result), error_code);
    return false;
  }

  if (is_directory) {
    return CreateSingleDirectory(options, target_path,
                                 false /* ignore_existing */, error_code);
  }

  return CopyFile(options, source_path, target_path, error_code);
}

bool SmbProvider::CopyFile(const CopyEntryOptionsProto& options,
                           const std::string& source_path,
                           const std::string& target_path,
                           int32_t* error_code) {
  DCHECK(error_code);

  int32_t target_file_id;
  int32_t source_file_id;
  bool success =
      CreateFile(options, target_path, &target_file_id, error_code) &&
      OpenFile(options, source_path, error_code, &source_file_id) &&
      CopyData(options, source_file_id, target_file_id, error_code) &&
      CloseFile(options, source_file_id, error_code) &&
      CloseFile(options, target_file_id, error_code);

  return success;
}

bool SmbProvider::CopyData(const CopyEntryOptionsProto& options,
                           int32_t source_fd,
                           int32_t target_fd,
                           int32_t* error_code) {
  DCHECK(error_code);

  std::vector<uint8_t> buffer;
  buffer.resize(kBufferSize);

  size_t bytes_read;
  while (ReadToBuffer(options, source_fd, &buffer, &bytes_read, error_code)) {
    if (bytes_read == 0) {
      // reached end of file successfully.
      return true;
    }

    if (!WriteFileFromBuffer(options, target_fd, buffer, error_code)) {
      return false;
    }
  }

  return false;
}

template <typename Proto>
bool SmbProvider::ReadToBuffer(const Proto& options,
                               int32_t file_id,
                               std::vector<uint8_t>* buffer,
                               size_t* bytes_read,
                               int32_t* error_code) {
  DCHECK(buffer);
  DCHECK(bytes_read);
  DCHECK(error_code);

  int32_t result = samba_interface_->ReadFile(file_id, buffer->data(),
                                              buffer->size(), bytes_read);
  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), error_code);
    return false;
  }

  DCHECK_GE(*bytes_read, 0);
  DCHECK_LE(*bytes_read, buffer->size());
  // Make sure buffer is only as big as bytes_read.
  buffer->resize(*bytes_read);
  return true;
}

void SmbProvider::GetDeleteList(const ProtoBlob& options_blob,
                                int32_t* error_code,
                                brillo::dbus_utils::FileDescriptor* temp_fd,
                                int32_t* bytes_written) {
  DCHECK(error_code);
  DCHECK(temp_fd);
  DCHECK(bytes_written);

  std::string full_path;
  GetDeleteListOptionsProto options;
  if (!ParseOptionsAndPath(options_blob, &options, &full_path, error_code)) {
    return;
  }

  bool is_directory;
  int32_t get_type_result;
  if (!GetEntryType(full_path, &get_type_result, &is_directory)) {
    LogAndSetError(options, GetErrorFromErrno(get_type_result), error_code);
    return;
  }

  DeleteListProto delete_list;
  int32_t result =
      GenerateDeleteList(options, full_path, is_directory, &delete_list);
  if (result != 0) {
    LogAndSetError(options, GetErrorFromErrno(result), error_code);
    return;
  }

  WriteDeleteListToTempFile(options, delete_list, error_code, temp_fd,
                            bytes_written);
}

bool SmbProvider::WriteDeleteListToTempFile(
    const GetDeleteListOptionsProto& options,
    const DeleteListProto& delete_list,
    int32_t* error_code,
    brillo::dbus_utils::FileDescriptor* temp_fd,
    int32_t* bytes_written) {
  DCHECK(error_code);
  DCHECK(temp_fd);
  DCHECK(bytes_written);

  std::vector<uint8_t> buffer;
  *error_code =
      static_cast<int32_t>(SerializeProtoToBlob(delete_list, &buffer));
  if (*error_code != ERROR_OK) {
    return false;
  }

  bool success = WriteTempFile(options, buffer, error_code, temp_fd);
  *bytes_written = success ? buffer.size() : -1;

  return success;
}

int32_t SmbProvider::GenerateDeleteList(
    const GetDeleteListOptionsProto& options,
    const std::string& full_path,
    bool is_directory,
    DeleteListProto* delete_list) {
  DCHECK(delete_list);

  if (!is_directory) {
    // |delete_list| will only contain the relative path to the file.
    AddToDeleteList(GetRelativePath(GetMountId(options), full_path),
                    delete_list);
    return 0;
  }

  PostDepthFirstIterator it = GetPostOrderIterator(full_path);
  int32_t it_result = it.Init();
  while (it_result == 0) {
    if (it.IsDone()) {
      return 0;
    }

    AddToDeleteList(GetRelativePath(GetMountId(options), it.Get().full_path),
                    delete_list);
    it_result = it.Next();
  }

  // while-loop is only exited from if there's an iterator error.
  DCHECK_NE(0, it_result);
  return it_result;
}

std::string SmbProvider::GetRelativePath(int32_t mount_id,
                                         const std::string& entry_path) {
  return mount_manager_->GetRelativePath(mount_id, entry_path);
}

}  // namespace smbprovider
