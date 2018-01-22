// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_SMBPROVIDER_TEST_HELPER_H_
#define SMBPROVIDER_SMBPROVIDER_TEST_HELPER_H_

#include <string>

#include "smbprovider/proto.h"
#include "smbprovider/proto_bindings/directory_entry.pb.h"

namespace smbprovider {

MountOptionsProto CreateMountOptionsProto(const std::string& path);

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

ProtoBlob CreateMountOptionsBlob(const std::string& path);

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

}  // namespace smbprovider

#endif  // SMBPROVIDER_SMBPROVIDER_TEST_HELPER_H_
