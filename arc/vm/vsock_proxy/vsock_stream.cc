// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/vm/vsock_proxy/vsock_stream.h"

#include <string>
#include <utility>
#include <vector>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

namespace arc {

VSockStream::VSockStream(base::ScopedFD vsock_fd)
    : vsock_fd_(std::move(vsock_fd)) {}

VSockStream::~VSockStream() = default;

bool VSockStream::Read(arc_proxy::VSockMessage* message) {
  uint64_t size = 0;
  if (!base::ReadFromFD(vsock_fd_.get(), reinterpret_cast<char*>(&size),
                        sizeof(size))) {
    PLOG(ERROR) << "Failed to read message size";
    return false;
  }

  std::vector<char> buf(size);
  if (!base::ReadFromFD(vsock_fd_.get(), buf.data(), buf.size())) {
    PLOG(ERROR) << "Failed to read a proto";
    return false;
  }

  if (!message->ParseFromArray(buf.data(), buf.size())) {
    LOG(ERROR) << "Failed to parse proto message";
    return false;
  }

  if (VLOG_IS_ON(1)) {
    std::string text;
    google::protobuf::TextFormat::PrintToString(*message, &text);
    LOG(INFO) << "Reading: " << text;
  }

  return true;
}

bool VSockStream::Write(const arc_proxy::VSockMessage& message) {
  if (VLOG_IS_ON(1)) {
    std::string text;
    google::protobuf::TextFormat::PrintToString(message, &text);
    LOG(INFO) << "Writing: " << text;
  }

  uint64_t size = message.ByteSize();
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
