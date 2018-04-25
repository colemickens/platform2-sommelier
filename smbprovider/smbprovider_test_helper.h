// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SMBPROVIDER_TEST_HELPER_H_
#define SMBPROVIDER_SMBPROVIDER_TEST_HELPER_H_

#include <string>

#include <base/files/file_util.h>

#include "smbprovider/proto.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"

namespace smbprovider {

constexpr char kTestCCacheName[] = "ccache";
constexpr char kTestKrb5ConfName[] = "krb5.conf";

class TempFileManager;

MountOptionsProto CreateMountOptionsProto(const std::string& path,
                                          const std::string& workgroup,
                                          const std::string& username);

UnmountOptionsProto CreateUnmountOptionsProto(int32_t mount_id);

ReadDirectoryOptionsProto CreateReadDirectoryOptionsProto(
    int32_t mount_id, const std::string& directory_path);

GetMetadataEntryOptionsProto CreateGetMetadataOptionsProto(
    int32_t mount_id, const std::string& entry_path);

OpenFileOptionsProto CreateOpenFileOptionsProto(int32_t mount_id,
                                                const std::string& file_path,
                                                bool writeable);

CloseFileOptionsProto CreateCloseFileOptionsProto(int32_t mount_id,
                                                  int32_t file_id);

DeleteEntryOptionsProto CreateDeleteEntryOptionsProto(
    int32_t mount_id, const std::string& entry_path, bool recursive);

ReadFileOptionsProto CreateReadFileOptionsProto(int32_t mount_id,
                                                int32_t file_id,
                                                int64_t offset,
                                                int32_t length);

TruncateOptionsProto CreateTruncateOptionsProto(int32_t mount_id,
                                                const std::string& file_path,
                                                int64_t length);

CreateFileOptionsProto CreateCreateFileOptionsProto(
    int32_t mount_id, const std::string& file_path);

WriteFileOptionsProto CreateWriteFileOptionsProto(int32_t mount_id,
                                                  int32_t file_id,
                                                  int64_t offset,
                                                  int32_t length);

CreateDirectoryOptionsProto CreateCreateDirectoryOptionsProto(
    int32_t mount_id, const std::string& directory_path, bool recursive);

MoveEntryOptionsProto CreateMoveEntryOptionsProto(
    int32_t mount_id,
    const std::string& source_path,
    const std::string& target_path);

CopyEntryOptionsProto CreateCopyEntryOptionsProto(
    int32_t mount_id,
    const std::string& source_path,
    const std::string& target_path);

GetDeleteListOptionsProto CreateGetDeleteListOptionsProto(
    int32_t mount_id, const std::string& entry_path);

GetSharesOptionsProto CreateGetSharesOptionsProto(
    const std::string& server_url);

RemountOptionsProto CreateRemountOptionsProto(const std::string& path,
                                              int32_t mount_id);

ProtoBlob CreateMountOptionsBlob(const std::string& path);

ProtoBlob CreateMountOptionsBlob(const std::string& path,
                                 const std::string& workgroup,
                                 const std::string& username);

ProtoBlob CreateUnmountOptionsBlob(int32_t mount_id);

ProtoBlob CreateReadDirectoryOptionsBlob(int32_t mount_id,
                                         const std::string& directory_path);

ProtoBlob CreateGetMetadataOptionsBlob(int32_t mount_id,
                                       const std::string& entry_path);

ProtoBlob CreateOpenFileOptionsBlob(int32_t mount_id,
                                    const std::string& file_path,
                                    bool writeable);

ProtoBlob CreateCloseFileOptionsBlob(int32_t mount_id, int32_t file_id);

ProtoBlob CreateDeleteEntryOptionsBlob(int32_t mount_id,
                                       const std::string& entry_path,
                                       bool recursive);

ProtoBlob CreateReadFileOptionsBlob(int32_t mount_id,
                                    int32_t file_id,
                                    int64_t offset,
                                    int32_t length);

ProtoBlob CreateCreateFileOptionsBlob(int32_t mount_id,
                                      const std::string& file_path);

ProtoBlob CreateTruncateOptionsBlob(int32_t mount_id,
                                    const std::string& file_path,
                                    int64_t length);

ProtoBlob CreateWriteFileOptionsBlob(int32_t mount_id,
                                     int32_t file_id,
                                     int64_t offset,
                                     int32_t length);

ProtoBlob CreateCreateDirectoryOptionsBlob(int32_t mount_id,
                                           const std::string& directory_path,
                                           bool recursive);

ProtoBlob CreateMoveEntryOptionsBlob(int32_t mount_id,
                                     const std::string& source_path,
                                     const std::string& target_path);

ProtoBlob CreateCopyEntryOptionsBlob(int32_t mount_id,
                                     const std::string& source_path,
                                     const std::string& target_path);

ProtoBlob CreateGetDeleteListOptionsBlob(int32_t mount_id,
                                         const std::string& entry_path);

ProtoBlob CreateGetSharesOptionsBlob(const std::string& server_url);

ProtoBlob CreateRemountOptionsBlob(const std::string& path, int32_t mount_id);

// FakeSamba URL helper methods
inline std::string GetDefaultServer() {
  return "smb://wdshare";
}

inline std::string GetDefaultMountRoot() {
  return "smb://wdshare/test";
}

inline std::string GetDefaultDirectoryPath() {
  return "/path";
}

inline std::string GetDefaultFilePath() {
  return "/path/dog.jpg";
}

inline std::string GetDefaultFullPath(const std::string& relative_path) {
  return GetDefaultMountRoot() + relative_path;
}

inline std::string GetAddedFullDirectoryPath() {
  return GetDefaultMountRoot() + GetDefaultDirectoryPath();
}

inline std::string GetAddedFullFilePath() {
  return GetDefaultMountRoot() + GetDefaultFilePath();
}

// Writes |password| into a file using |temp_manager| with the format of
// "{password_length}{password}".
base::ScopedFD WritePasswordToFile(TempFileManager* temp_manager,
                                   const std::string& password);

std::string CreateKrb5ConfPath(const base::FilePath& temp_dir);

std::string CreateKrb5CCachePath(const base::FilePath& temp_dir);

}  // namespace smbprovider

#endif  // SMBPROVIDER_SMBPROVIDER_TEST_HELPER_H_
