// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SMBPROVIDER_TEST_HELPER_H_
#define SMBPROVIDER_SMBPROVIDER_TEST_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include <authpolicy/proto_bindings/active_directory_info.pb.h>
#include <base/files/file_util.h>

#include "smbprovider/mount_manager.h"
#include "smbprovider/proto.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"

namespace smbprovider {

constexpr char kTestCCacheName[] = "ccache";
constexpr char kTestKrb5ConfName[] = "krb5.conf";

struct MountConfig;
class TempFileManager;

MountOptionsProto CreateMountOptionsProto(const std::string& path,
                                          const std::string& workgroup,
                                          const std::string& username);

MountOptionsProto CreateMountOptionsProto(const std::string& path,
                                          const std::string& workgroup,
                                          const std::string& username,
                                          MountConfig* mount_config);

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

RemountOptionsProto CreateRemountOptionsProto(const std::string& path,
                                              int32_t mount_id,
                                              const MountConfig& mount_config);

// Writes the Credential Cache file contents |krb5cc| and the krb5.conf file
// contents |krb5conf| into a authpolicy::KerberosFiles proto.
authpolicy::KerberosFiles CreateKerberosFilesProto(const std::string& krb5cc,
                                                   const std::string& krb5conf);

UpdateMountCredentialsOptionsProto CreateUpdateMountCredentialsOptionsProto(
    int32_t mount_id,
    const std::string& workgroup,
    const std::string& username);

PremountOptionsProto CreatePremountOptionsProto(const std::string& path);

ProtoBlob CreateMountOptionsBlob(const std::string& path);

ProtoBlob CreateMountOptionsBlob(const std::string& path,
                                 const MountConfig& mount_config);

ProtoBlob CreateMountOptionsBlob(const std::string& path,
                                 const std::string& workgroup,
                                 const std::string& username,
                                 const MountConfig& mount_config);

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

ProtoBlob CreateRemountOptionsBlob(const std::string& path,
                                   int32_t mount_id,
                                   MountConfig mount_config);

ProtoBlob CreateUpdateMountCredentialsOptionsBlob(int32_t mount_id,
                                                  const std::string& workgroup,
                                                  const std::string& username);

ProtoBlob CreatePremountOptionsBlob(const std::string& path);

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

// Expects that the file at |path| contains |expected_contents|.
void ExpectFileEqual(const std::string& path,
                     const std::string expected_contents);

// Expects that the file at |path| does not contain |expected_contents|.
void ExpectFileNotEqual(const std::string& path,
                        const std::string expected_contents);

// Expects that the credentials of the mount with |mount_id| are equal to the
// inputted credentials.
void ExpectCredentialsEqual(MountManager* mount_manager,
                            int32_t mount_id,
                            const std::string& root_path,
                            const std::string& workgroup,
                            const std::string& username,
                            const std::string& password);

// Creates a NetBios Name Query response packet. |hostnames| may contain well
// formed (18 byte) or malformed hostnames. For a well-formed packet,
// |name_length| must be equal to the length of |name|.
std::vector<uint8_t> CreateNetBiosResponsePacket(
    const std::vector<std::vector<uint8_t>>& hostnames,
    uint8_t name_length,
    std::vector<uint8_t> name,
    uint16_t transaction_id,
    uint8_t response_type);
std::vector<uint8_t> CreateNetBiosResponsePacket(
    const std::vector<std::vector<uint8_t>>& hostnames,
    std::vector<uint8_t> name,
    uint16_t transaction_id,
    uint8_t response_type);

// Creates a valid NetBios Hostname as a vector of bytes. |hostname_in| must be
// less than or equal to 15 bytes.
std::vector<uint8_t> CreateValidNetBiosHostname(const std::string& hostname,
                                                uint8_t type);

std::unique_ptr<MountConfigProto> CreateMountConfigProto(bool enable_ntlm);

}  // namespace smbprovider

#endif  // SMBPROVIDER_SMBPROVIDER_TEST_HELPER_H_
