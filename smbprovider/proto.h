// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_PROTO_H_
#define SMBPROVIDER_PROTO_H_

#include <string>
#include <vector>

#include <base/logging.h>

#include "smbprovider/proto_bindings/directory_entry.pb.h"

namespace smbprovider {

// Used as buffer for serialized protobufs.
using ProtoBlob = std::vector<uint8_t>;

// Serializes |proto| to the byte array |proto_blob|. Returns ERROR_OK on
// success and ERROR_FAILED on failure.
template <typename ProtoType>
ErrorType SerializeProtoToBlob(const ProtoType& proto, ProtoBlob* proto_blob) {
  DCHECK(proto_blob);
  proto_blob->resize(proto.ByteSizeLong());
  return proto.SerializeToArray(proto_blob->data(), proto.ByteSizeLong())
             ? ERROR_OK
             : ERROR_FAILED;
}

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

}  // namespace smbprovider

#endif  // SMBPROVIDER_PROTO_H_
