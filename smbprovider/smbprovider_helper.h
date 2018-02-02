// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SMBPROVIDER_HELPER_H_
#define SMBPROVIDER_SMBPROVIDER_HELPER_H_

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/scoped_file.h>
#include <libsmbclient.h>

#include "smbprovider/proto.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"

namespace dbus {

class FileDescriptor;

}  // namespace dbus

namespace smbprovider {

using PathParts = const std::vector<std::string>;

// Helper method to transform and add |dirent| to a DirectoryEntryListProto.
void AddEntryIfValid(const smbc_dirent& dirent,
                     std::vector<DirectoryEntry>* directory_entries);

// Helper method to advance |dirp| to the next entry.
smbc_dirent* AdvanceDirEnt(smbc_dirent* dirp);

// Helper method that appends a |base_path| to a |relative_path|. Base path may
// or may not contain a trailing separator ('/'). If |relative_path| starts with
// a leading "/", it is stripped before being appended to |base_path|.
std::string AppendPath(const std::string& base_path,
                       const std::string& relative_path);

// Helper method to calculate entry size based on |entry_name|.
size_t CalculateEntrySize(const std::string& entry_name);

// Helper method to get |smbc_dirent| struct from a |buffer|.
smbc_dirent* GetDirentFromBuffer(uint8_t* buffer);

// Helper method to check whether an entry is self (".") or parent ("..").
bool IsSelfOrParentDir(const std::string& entry_name);

// Helper method to check whether or not the entry should be processed on
// GetDirectoryEntries().
bool ShouldProcessEntryType(uint32_t smbc_type);

// Helper method to write the contents of |entry| into the buffer |dirp|.
// |dirlen| is the size of the smbc_dirent entry.
bool WriteEntry(const std::string& entry_name,
                int32_t entry_type,
                size_t buffer_size,
                smbc_dirent* dirp);

// Maps errno to ErrorType.
ErrorType GetErrorFromErrno(int32_t error_code);

// Helper method to determine whether a stat struct represents a Directory.
bool IsDirectory(const struct stat& stat_info);

// Helper method to detemine whether a stat struct represents a File.
bool IsFile(const struct stat& stat_info);

// Helper method to get a valid DBus FileDescriptor |dbus_fd| from a scoped
// file descriptor |fd|.
void GetValidDBusFD(base::ScopedFD* fd, dbus::FileDescriptor* dbus_fd);
void LogAndSetError(const char* operation_name,
                    int32_t mount_id,
                    ErrorType error_received,
                    int32_t* error_code);

// Logs error and sets |error_code|.
template <typename Proto>
void LogAndSetError(Proto options,
                    ErrorType error_received,
                    int32_t* error_code) {
  LogAndSetError(GetMethodName(options), options.mount_id(), error_received,
                 error_code);
}

void LogAndSetDBusParseError(const char* operation_name, int32_t* error_code);

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

// Helper method to get |DirectoryEntryProto| from a struct stat. Returns
// ERROR_OK on success and ERROR_FAILED otherwise.
int32_t GetDirectoryEntryProtoFromStat(const std::string& full_path,
                                       const struct stat& stat_info,
                                       ProtoBlob* proto_blob);

// Helper method to determine if the open file flags are valid. Valid flags
// include: O_RDONLY, O_RDWR, O_WRONLY.
bool IsValidOpenFileFlags(int32_t flags);

// Helper method to read data from a file descriptor |fd| and store the
// result in |buffer|. This reads bytes equal to what is specified in |options|,
// and will fail if it doesn't read that amount. Returns true on success and
// |error| is set on failure.
bool ReadFromFD(const WriteFileOptionsProto& options,
                const dbus::FileDescriptor& fd,
                int32_t* error,
                std::vector<uint8_t>* buffer);

// Gets the correct permissions flag for |options|.
int32_t GetOpenFilePermissions(const OpenFileOptionsProto& options);
int32_t GetOpenFilePermissions(const TruncateOptionsProto& unused);

// Returns the components of a filepath as a vector<std::string>.
PathParts SplitPath(const std::string& full_path);

// Removes smb:// from url. |smb_url| must start with "smb://".
std::string RemoveURLScheme(const std::string& smb_url);

// Returns the file component of a path.|full_path| must be an SMB Url.
std::string GetFileName(const std::string& full_path);

// Returns a string representing the filepath to the directory above the file.
// |full_path| must be an SMB Url.
std::string GetDirPath(const std::string& full_path);

}  // namespace smbprovider

#endif  // SMBPROVIDER_SMBPROVIDER_HELPER_H_
