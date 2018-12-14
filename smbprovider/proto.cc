// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/proto.h"

#include <dbus/smbprovider/dbus-constants.h>

#include "smbprovider/constants.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"

namespace smbprovider {

bool IsValidOptions(const MountOptionsProto& options) {
  return options.has_path() && options.has_workgroup() &&
         options.has_username() && options.has_mount_config() &&
         IsValidMountConfig(options.mount_config());
}

bool IsValidOptions(const UnmountOptionsProto& options) {
  return options.has_mount_id();
}

bool IsValidOptions(const ReadDirectoryOptionsProto& options) {
  return options.has_mount_id() && options.has_directory_path();
}

bool IsValidOptions(const GetMetadataEntryOptionsProto& options) {
  return options.has_mount_id() && options.has_entry_path();
}

bool IsValidOptions(const OpenFileOptionsProto& options) {
  return options.has_file_path() && options.has_writeable() &&
         options.has_mount_id();
}

bool IsValidOptions(const CloseFileOptionsProto& options) {
  return options.has_mount_id() && options.has_file_id();
}

bool IsValidOptions(const DeleteEntryOptionsProto& options) {
  return options.has_mount_id() && options.has_entry_path() &&
         options.has_recursive();
}

bool IsValidOptions(const ReadFileOptionsProto& options) {
  return options.has_mount_id() && options.has_file_id() &&
         options.has_offset() && options.has_length() &&
         options.offset() >= 0 && options.length() >= 0;
}

bool IsValidOptions(const CreateFileOptionsProto& options) {
  return options.has_mount_id() && options.has_file_path();
}

bool IsValidOptions(const TruncateOptionsProto& options) {
  return options.has_mount_id() && options.has_file_path() &&
         options.has_length() && options.length() >= 0;
}

bool IsValidOptions(const WriteFileOptionsProto& options) {
  return options.has_mount_id() && options.has_file_id() &&
         options.has_offset() && options.has_length() &&
         options.offset() >= 0 && options.length() >= 0;
}

bool IsValidOptions(const CreateDirectoryOptionsProto& options) {
  return options.has_mount_id() && options.has_directory_path() &&
         options.has_recursive();
}

bool IsValidOptions(const MoveEntryOptionsProto& options) {
  return options.has_mount_id() && options.has_source_path() &&
         options.has_target_path();
}

bool IsValidOptions(const CopyEntryOptionsProto& options) {
  return options.has_mount_id() && options.has_source_path() &&
         options.has_target_path();
}

bool IsValidOptions(const GetDeleteListOptionsProto& options) {
  return options.has_mount_id() && options.has_entry_path();
}

bool IsValidOptions(const GetSharesOptionsProto& options) {
  return options.has_server_url();
}

bool IsValidOptions(const RemountOptionsProto& options) {
  // TODO(baileyberro): Add verification for workgroup and username here when
  // SmbProviderClient starts passing them.
  return options.has_path() && options.has_mount_id();
}

bool IsValidOptions(const UpdateMountCredentialsOptionsProto& options) {
  return options.has_mount_id() && options.has_workgroup() &&
         options.has_username();
}

bool IsValidMountConfig(const MountConfigProto& options) {
  return options.has_enable_ntlm();
}

std::string GetEntryPath(const ReadDirectoryOptionsProto& options) {
  return options.directory_path();
}

std::string GetEntryPath(const GetMetadataEntryOptionsProto& options) {
  return options.entry_path();
}

std::string GetEntryPath(const OpenFileOptionsProto& options) {
  return options.file_path();
}

std::string GetEntryPath(const DeleteEntryOptionsProto& options) {
  return options.entry_path();
}

std::string GetEntryPath(const CreateFileOptionsProto& options) {
  return options.file_path();
}

std::string GetEntryPath(const TruncateOptionsProto& options) {
  return options.file_path();
}

std::string GetEntryPath(const CreateDirectoryOptionsProto& options) {
  return options.directory_path();
}

std::string GetEntryPath(const GetDeleteListOptionsProto& options) {
  return options.entry_path();
}

std::string GetEntryPath(const GetSharesOptionsProto& options) {
  return options.server_url();
}

std::string GetSourcePath(const MoveEntryOptionsProto& options) {
  return options.source_path();
}

std::string GetDestinationPath(const MoveEntryOptionsProto& options) {
  return options.target_path();
}

std::string GetSourcePath(const CopyEntryOptionsProto& options) {
  return options.source_path();
}

std::string GetDestinationPath(const CopyEntryOptionsProto& options) {
  return options.target_path();
}

const char* GetMethodName(const MountOptionsProto& unused) {
  return kMountMethod;
}

const char* GetMethodName(const UnmountOptionsProto& unused) {
  return kUnmountMethod;
}

const char* GetMethodName(const GetMetadataEntryOptionsProto& unused) {
  return kGetMetadataEntryMethod;
}

const char* GetMethodName(const ReadDirectoryOptionsProto& unused) {
  return kReadDirectoryMethod;
}

const char* GetMethodName(const OpenFileOptionsProto& unused) {
  return kOpenFileMethod;
}

const char* GetMethodName(const CloseFileOptionsProto& unused) {
  return kCloseFileMethod;
}

const char* GetMethodName(const DeleteEntryOptionsProto& unused) {
  return kDeleteEntryMethod;
}

const char* GetMethodName(const ReadFileOptionsProto& unused) {
  return kReadFileMethod;
}

const char* GetMethodName(const CreateFileOptionsProto& unused) {
  return kCreateFileMethod;
}

const char* GetMethodName(const TruncateOptionsProto& unused) {
  return kTruncateMethod;
}

const char* GetMethodName(const WriteFileOptionsProto& unused) {
  return kWriteFileMethod;
}

const char* GetMethodName(const CreateDirectoryOptionsProto& unused) {
  return kCreateDirectoryMethod;
}

const char* GetMethodName(const MoveEntryOptionsProto& unused) {
  return kMoveEntryMethod;
}

const char* GetMethodName(const CopyEntryOptionsProto& unused) {
  return kCopyEntryMethod;
}

const char* GetMethodName(const GetDeleteListOptionsProto& unused) {
  return kGetDeleteListMethod;
}

const char* GetMethodName(const GetSharesOptionsProto& unused) {
  return kGetSharesMethod;
}

const char* GetMethodName(const RemountOptionsProto& unused) {
  return kRemountMethod;
}

const char* GetMethodName(const UpdateMountCredentialsOptionsProto& unused) {
  return kUpdateMountCredentialsMethod;
}

template <>
int32_t GetMountId(const MountOptionsProto& unused) {
  return -1;
}

template <>
int32_t GetMountId(const GetSharesOptionsProto& unused) {
  return kInternalMountId;
}

void SerializeDirEntryVectorToProto(
    const std::vector<DirectoryEntry>& entries_vector,
    DirectoryEntryListProto* entries_proto) {
  for (const auto& e : entries_vector) {
    AddDirectoryEntry(e, entries_proto);
  }
}

void AddDirectoryEntry(const DirectoryEntry& entry,
                       DirectoryEntryListProto* proto) {
  DCHECK(proto);
  DirectoryEntryProto* new_entry_proto = proto->add_entries();
  ConvertToProto(entry, new_entry_proto);
}

void ConvertToProto(const DirectoryEntry& entry, DirectoryEntryProto* proto) {
  DCHECK(proto);
  proto->set_is_directory(entry.is_directory);
  proto->set_name(entry.name);
  proto->set_size(entry.size);
  proto->set_last_modified_time(entry.last_modified_time);
}

void AddToDeleteList(const std::string& entry_path, DeleteListProto* proto) {
  DCHECK(proto);
  proto->add_entries(entry_path);
}

void AddToHostnamesProto(const std::string& hostname, HostnamesProto* proto) {
  DCHECK(proto);
  proto->add_hostnames(hostname);
}

}  // namespace smbprovider
