// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains assorted functions used in mount-related classed.

#include "cryptohome/mount_utils.h"

#include <vector>

#include <base/files/file_util.h>

namespace cryptohome {

bool ReadProtobuf(int in_fd, google::protobuf::MessageLite* message) {
  size_t proto_size = 0;
  if (!base::ReadFromFD(in_fd, reinterpret_cast<char*>(&proto_size),
                        sizeof(proto_size))) {
    PLOG(ERROR) << "Failed to read protobuf size";
    return false;
  }

  std::vector<char> buf(proto_size);
  if (!base::ReadFromFD(in_fd, buf.data(), buf.size())) {
    PLOG(ERROR) << "Failed to read protobuf";
    return false;
  }

  if (!message->ParseFromArray(buf.data(), buf.size())) {
    LOG(ERROR) << "Failed to parse protobuf";
    return false;
  }

  return true;
}

bool WriteProtobuf(int out_fd, const google::protobuf::MessageLite& message) {
  size_t size = message.ByteSizeLong();
  std::vector<char> buf(message.ByteSizeLong());
  if (!message.SerializeToArray(buf.data(), buf.size())) {
    LOG(ERROR) << "Failed to serialize protobuf";
    return false;
  }

  if (!base::WriteFileDescriptor(out_fd, reinterpret_cast<char*>(&size),
                                 sizeof(size))) {
    PLOG(ERROR) << "Failed to write protobuf size";
    return false;
  }

  if (!base::WriteFileDescriptor(out_fd, buf.data(), size)) {
    PLOG(ERROR) << "Failed to write protobuf";
    return false;
  }

  return true;
}

}  // namespace cryptohome
