// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/proto_file_io.h"

#include <utility>

#include <base/files/file.h>
#include <base/macros.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

namespace modemfwd {

bool ReadProtobuf(const base::FilePath& proto_file,
                  google::protobuf::Message* out_proto) {
  DCHECK(out_proto);

  base::File file(proto_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    DLOG(ERROR) << "Could not open \"" << proto_file.value()
                << "\": " << base::File::ErrorToString(file.error_details());
    return false;
  }

  return ReadProtobuf(file.GetPlatformFile(), out_proto);
}

bool ReadProtobuf(int fd, google::protobuf::Message* out_proto) {
  google::protobuf::io::FileInputStream input_stream(fd);
  return google::protobuf::TextFormat::Parse(&input_stream, out_proto);
}

bool WriteProtobuf(const google::protobuf::Message& proto, int fd) {
  google::protobuf::io::FileOutputStream output_stream(fd);
  return google::protobuf::TextFormat::Print(proto, &output_stream);
}

}  // namespace modemfwd
