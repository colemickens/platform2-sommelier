// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_PROTO_H_
#define SMBPROVIDER_PROTO_H_

#include <string>
#include <vector>

#include "smbprovider/mount_config.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"

namespace smbprovider {

// Used as buffer for serialized protobufs.
using ProtoBlob = std::vector<uint8_t>;

// Serializes |proto| to the byte array |proto_blob|. Returns ERROR_OK on
// success and ERROR_FAILED on failure.
ErrorType SerializeProtoToBlob(const google::protobuf::MessageLite& proto,
                               ProtoBlob* proto_blob);

// Helper method to check whether a Proto has valid fields.
bool IsValidOptions(const MountOptionsProto& options);
bool IsValidOptions(const UnmountOptionsProto& options);
bool IsValidOptions(const ReadDirectoryOptionsProto& options);
bool IsValidOptions(const GetMetadataEntryOptionsProto& options);
bool IsValidOptions(const OpenFileOptionsProto& options);
bool IsValidOptions(const CloseFileOptionsProto& options);
bool IsValidOptions(const DeleteEntryOptionsProto& options);
bool IsValidOptions(const ReadFileOptionsProto& options);
bool IsValidOptions(const CreateFileOptionsProto& options);
bool IsValidOptions(const TruncateOptionsProto& options);
bool IsValidOptions(const WriteFileOptionsProto& options);
bool IsValidOptions(const CreateDirectoryOptionsProto& options);
bool IsValidOptions(const MoveEntryOptionsProto& options);
bool IsValidOptions(const CopyEntryOptionsProto& options);
bool IsValidOptions(const GetDeleteListOptionsProto& options);
bool IsValidOptions(const GetSharesOptionsProto& options);
bool IsValidOptions(const UpdateMountCredentialsOptionsProto& options);
bool IsValidOptions(const UpdateSharePathOptionsProto& options);

bool IsValidMountConfig(const MountConfigProto& options);

// Helper method to get the entry path from a proto.
std::string GetEntryPath(const ReadDirectoryOptionsProto& options);
std::string GetEntryPath(const GetMetadataEntryOptionsProto& options);
std::string GetEntryPath(const OpenFileOptionsProto& options);
std::string GetEntryPath(const DeleteEntryOptionsProto& options);
std::string GetEntryPath(const CreateFileOptionsProto& options);
std::string GetEntryPath(const TruncateOptionsProto& options);
std::string GetEntryPath(const CreateDirectoryOptionsProto& options);
std::string GetEntryPath(const GetDeleteListOptionsProto& options);
std::string GetEntryPath(const GetSharesOptionsProto& options);

std::string GetSourcePath(const MoveEntryOptionsProto& options);
std::string GetDestinationPath(const MoveEntryOptionsProto& options);
std::string GetSourcePath(const CopyEntryOptionsProto& options);
std::string GetDestinationPath(const CopyEntryOptionsProto& options);

// Helper method to get the corresponding method name for each proto.
const char* GetMethodName(const MountOptionsProto& unused);
const char* GetMethodName(const UnmountOptionsProto& unused);
const char* GetMethodName(const GetMetadataEntryOptionsProto& unused);
const char* GetMethodName(const ReadDirectoryOptionsProto& unused);
const char* GetMethodName(const OpenFileOptionsProto& unused);
const char* GetMethodName(const CloseFileOptionsProto& unused);
const char* GetMethodName(const DeleteEntryOptionsProto& unused);
const char* GetMethodName(const ReadFileOptionsProto& unused);
const char* GetMethodName(const CreateFileOptionsProto& unused);
const char* GetMethodName(const TruncateOptionsProto& unused);
const char* GetMethodName(const WriteFileOptionsProto& unused);
const char* GetMethodName(const CreateDirectoryOptionsProto& unused);
const char* GetMethodName(const MoveEntryOptionsProto& unused);
const char* GetMethodName(const CopyEntryOptionsProto& unused);
const char* GetMethodName(const GetDeleteListOptionsProto& unused);
const char* GetMethodName(const GetSharesOptionsProto& unused);
const char* GetMethodName(const UpdateMountCredentialsOptionsProto& unused);
const char* GetMethodName(const UpdateSharePathOptionsProto& unused);

template <typename Proto>
int32_t GetMountId(const Proto& options) {
  return options.mount_id();
}

template <>
int32_t GetMountId(const MountOptionsProto& unused);

template <>
int32_t GetMountId(const GetSharesOptionsProto& unused);

// Struct mapping to DirectoryEntryProto.
struct DirectoryEntry {
  bool is_directory;
  std::string name;
  std::string full_path;
  int64_t size;
  int64_t last_modified_time;

  DirectoryEntry() = default;

  DirectoryEntry(bool is_directory,
                 const std::string& name,
                 const std::string& full_path,
                 int64_t size,
                 int64_t last_modified_time)
      : is_directory(is_directory),
        name(name),
        full_path(full_path),
        size(size),
        last_modified_time(last_modified_time) {}

  DirectoryEntry(bool is_directory,
                 const std::string& name,
                 const std::string& full_path)
      : DirectoryEntry(is_directory,
                       name,
                       full_path,
                       -1 /* size */,
                       -1 /* last_modified_time */) {}
};

// Converts a vector of DirectoryEnts into a DirectoryEntryListProto.
void SerializeDirEntryVectorToProto(
    const std::vector<DirectoryEntry>& entries_vector,
    DirectoryEntryListProto* entries_proto);

void AddDirectoryEntry(const DirectoryEntry& entry,
                       DirectoryEntryListProto* proto);

void ConvertToProto(const DirectoryEntry& entry, DirectoryEntryProto* proto);

void AddToDeleteList(const std::string& entry_path, DeleteListProto* proto);

void AddToHostnamesProto(const std::string& hostname, HostnamesProto* proto);

template <typename Proto>
MountConfig ConvertToMountConfig(const Proto& option) {
  DCHECK(IsValidMountConfig(option.mount_config()));

  const MountConfigProto& mount_config_proto = option.mount_config();

  return MountConfig(mount_config_proto.enable_ntlm());
}

}  // namespace smbprovider

#endif  // SMBPROVIDER_PROTO_H_
