// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/smbprovider_test_helper.h"

#include <gtest/gtest.h>

#include "smbprovider/proto_bindings/directory_entry.pb.h"

namespace smbprovider {
namespace {

template <typename ProtoType>
ProtoBlob SerializeProtoToBlobAndCheck(const ProtoType& proto) {
  ProtoBlob proto_blob;
  EXPECT_EQ(ERROR_OK, SerializeProtoToBlob(proto, &proto_blob));
  return proto_blob;
}

}  // namespace

MountOptionsProto CreateMountOptionsProto(const std::string& path) {
  MountOptionsProto mount_options;
  mount_options.set_path(path);
  return mount_options;
}

UnmountOptionsProto CreateUnmountOptionsProto(int32_t mount_id) {
  UnmountOptionsProto unmount_options;
  unmount_options.set_mount_id(mount_id);
  return unmount_options;
}

ReadDirectoryOptionsProto CreateReadDirectoryOptionsProto(
    int32_t mount_id, const std::string& directory_path) {
  ReadDirectoryOptionsProto read_directory_options;
  read_directory_options.set_mount_id(mount_id);
  read_directory_options.set_directory_path(directory_path);
  return read_directory_options;
}

GetMetadataEntryOptionsProto CreateGetMetadataOptionsProto(
    int32_t mount_id, const std::string& entry_path) {
  GetMetadataEntryOptionsProto get_metadata_options;
  get_metadata_options.set_mount_id(mount_id);
  get_metadata_options.set_entry_path(entry_path);
  return get_metadata_options;
}

OpenFileOptionsProto CreateOpenFileOptionsProto(int32_t mount_id,
                                                const std::string& file_path,
                                                bool writeable) {
  OpenFileOptionsProto open_file_options;
  open_file_options.set_mount_id(mount_id);
  open_file_options.set_file_path(file_path);
  open_file_options.set_writeable(writeable);
  return open_file_options;
}

CloseFileOptionsProto CreateCloseFileOptionsProto(int32_t mount_id,
                                                  int32_t file_id) {
  CloseFileOptionsProto close_file_options;
  close_file_options.set_mount_id(mount_id);
  close_file_options.set_file_id(file_id);
  return close_file_options;
}

DeleteEntryOptionsProto CreateDeleteEntryOptionsProto(
    int32_t mount_id, const std::string& entry_path, bool recursive) {
  DeleteEntryOptionsProto delete_entry_options;
  delete_entry_options.set_mount_id(mount_id);
  delete_entry_options.set_entry_path(entry_path);
  delete_entry_options.set_recursive(recursive);
  return delete_entry_options;
}

ReadFileOptionsProto CreateReadFileOptionsProto(int32_t mount_id,
                                                int32_t file_id,
                                                int64_t offset,
                                                int32_t length) {
  ReadFileOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_file_id(file_id);
  options.set_offset(offset);
  options.set_length(length);
  return options;
}

CreateFileOptionsProto CreateCreateFileOptionsProto(
    int32_t mount_id, const std::string& file_path) {
  CreateFileOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_file_path(file_path);
  return options;
}

TruncateOptionsProto CreateTruncateOptionsProto(int32_t mount_id,
                                                const std::string& file_path,
                                                int64_t length) {
  TruncateOptionsProto options;
  options.set_mount_id(mount_id);
  options.set_file_path(file_path);
  options.set_length(length);
  return options;
}

ProtoBlob CreateMountOptionsBlob(const std::string& path) {
  return SerializeProtoToBlobAndCheck(CreateMountOptionsProto(path));
}

ProtoBlob CreateUnmountOptionsBlob(int32_t mount_id) {
  return SerializeProtoToBlobAndCheck(CreateUnmountOptionsProto(mount_id));
}

ProtoBlob CreateReadDirectoryOptionsBlob(int32_t mount_id,
                                         const std::string& directory_path) {
  return SerializeProtoToBlobAndCheck(
      CreateReadDirectoryOptionsProto(mount_id, directory_path));
}

ProtoBlob CreateGetMetadataOptionsBlob(int32_t mount_id,
                                       const std::string& entry_path) {
  ProtoBlob proto_blob;
  return SerializeProtoToBlobAndCheck(
      CreateGetMetadataOptionsProto(mount_id, entry_path));
}

ProtoBlob CreateOpenFileOptionsBlob(int32_t mount_id,
                                    const std::string& file_path,
                                    bool writeable) {
  return SerializeProtoToBlobAndCheck(
      CreateOpenFileOptionsProto(mount_id, file_path, writeable));
}

ProtoBlob CreateCloseFileOptionsBlob(int32_t mount_id, int32_t file_id) {
  return SerializeProtoToBlobAndCheck(
      CreateCloseFileOptionsProto(mount_id, file_id));
}

ProtoBlob CreateDeleteEntryOptionsBlob(int32_t mount_id,
                                       const std::string& entry_path,
                                       bool recursive) {
  return SerializeProtoToBlobAndCheck(
      CreateDeleteEntryOptionsProto(mount_id, entry_path, recursive));
}

ProtoBlob CreateReadFileOptionsBlob(int32_t mount_id,
                                    int32_t file_id,
                                    int64_t offset,
                                    int32_t length) {
  return SerializeProtoToBlobAndCheck(
      CreateReadFileOptionsProto(mount_id, file_id, offset, length));
}

ProtoBlob CreateCreateFileOptionsBlob(int32_t mount_id,
                                      const std::string& file_path) {
  return SerializeProtoToBlobAndCheck(
      CreateCreateFileOptionsProto(mount_id, file_path));
}

ProtoBlob CreateTruncateOptionsBlob(int32_t mount_id,
                                    const std::string& file_path,
                                    int64_t length) {
  return SerializeProtoToBlobAndCheck(
      CreateTruncateOptionsProto(mount_id, file_path, length));
}

}  // namespace smbprovider
