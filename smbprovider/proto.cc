// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "smbprovider/proto.h"

#include "smbprovider/proto_bindings/directory_entry.pb.h"

namespace smbprovider {

bool IsValidOptions(const MountOptionsProto& options) {
  return options.has_path();
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

}  // namespace smbprovider
