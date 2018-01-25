// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/proto_file_reader.h"

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

  google::protobuf::io::FileInputStream input_stream(file.GetPlatformFile());
  return google::protobuf::TextFormat::Parse(&input_stream, out_proto);
}

}  // namespace modemfwd
