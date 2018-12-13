// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/vsock_stream.h"

#include <utility>
#include <vector>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace arc {

VSockStream::VSockStream(base::ScopedFD vsock_fd)
    : vsock_fd_(std::move(vsock_fd)) {}

VSockStream::~VSockStream() = default;

base::Optional<arc_proxy::Message> VSockStream::Read() {
  // TODO(hidehiko): Use fixed size bit int.
  int size = 0;
  if (!base::ReadFromFD(vsock_fd_.get(), reinterpret_cast<char*>(&size),
                        sizeof(size))) {
    PLOG(ERROR) << "Failed to read message size";
    return base::nullopt;
  }
  LOG(INFO) << "Reading: " << size;
  std::vector<char> buf(size);
  if (!base::ReadFromFD(vsock_fd_.get(), buf.data(), buf.size())) {
    PLOG(ERROR) << "Failed to read a proto";
    return base::nullopt;
  }

  arc_proxy::Message message;
  if (!message.ParseFromArray(buf.data(), buf.size())) {
    LOG(ERROR) << "Failed to parse proto message";
    return base::nullopt;
  }

  return message;
}

bool VSockStream::Write(arc_proxy::Message message) {
  // TODO(hidehiko): Use fixed size bit int.
  int size = message.ByteSize();
  if (!base::WriteFileDescriptor(
          vsock_fd_.get(), reinterpret_cast<char*>(&size), sizeof(size))) {
    PLOG(ERROR) << "Failed to write buffer size";
    return false;
  }

  google::protobuf::io::FileOutputStream stream(vsock_fd_.get());
  if (!message.SerializeToZeroCopyStream(&stream)) {
    LOG(ERROR) << "Failed to serialize proto";
    return false;
  }
  return true;
}

}  // namespace arc
